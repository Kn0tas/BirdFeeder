import React, { useState, useEffect, useCallback, useRef } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  ActivityIndicator,
  Dimensions,
  FlatList,
  Image,
  TextInput,
  Switch,
  ScrollView,
  Alert,
  Modal,
  RefreshControl,
  Platform,
  StatusBar as RNStatusBar,
  Animated,
} from 'react-native';
import { StatusBar } from 'expo-status-bar';
import { WebView } from 'react-native-webview';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { SafeAreaProvider, SafeAreaView, useSafeAreaInsets } from 'react-native-safe-area-context';

/* ── Settings persistence ─────────────────────────────────────────── */

const SETTINGS_KEY = 'birdfeeder_settings';
const DEFAULT_IP = '192.168.0.21';

function useSettings() {
  const [deviceIp, setDeviceIp] = useState(DEFAULT_IP);
  const [notificationsEnabled, setNotificationsEnabled] = useState(true);
  const [loaded, setLoaded] = useState(false);

  useEffect(() => {
    AsyncStorage.getItem(SETTINGS_KEY).then((stored) => {
      if (stored) {
        try {
          const parsed = JSON.parse(stored);
          if (parsed.deviceIp) setDeviceIp(parsed.deviceIp);
          if (parsed.notificationsEnabled !== undefined)
            setNotificationsEnabled(parsed.notificationsEnabled);
        } catch {}
      }
      setLoaded(true);
    });
  }, []);

  const save = useCallback(
    (ip: string, notif: boolean) => {
      AsyncStorage.setItem(
        SETTINGS_KEY,
        JSON.stringify({ deviceIp: ip, notificationsEnabled: notif })
      );
    },
    []
  );

  return {
    deviceIp,
    setDeviceIp: (ip: string) => {
      setDeviceIp(ip);
      save(ip, notificationsEnabled);
    },
    notificationsEnabled,
    setNotificationsEnabled: (v: boolean) => {
      setNotificationsEnabled(v);
      save(deviceIp, v);
    },
    baseUrl: `http://${deviceIp}`,
    wsUrl: `ws://${deviceIp}/ws`,
    loaded,
  };
}

/* ── WebSocket hook ───────────────────────────────────────────────── */

interface WsDetection {
  type: string;
  species: string;
  confidence: number;
  snap_id: number;
  timestamp: number;
}

function useWebSocket(
  wsUrl: string,
  onDetection: (d: WsDetection) => void
) {
  const [connected, setConnected] = useState(false);
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  const connect = useCallback(() => {
    if (!wsUrl) return; // Don't connect with empty URL
    if (wsRef.current) {
      try { wsRef.current.close(); } catch {}
    }

    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      setConnected(true);
      if (reconnectTimer.current) {
        clearTimeout(reconnectTimer.current);
        reconnectTimer.current = null;
      }
    };

    ws.onmessage = (evt: MessageEvent) => {
      try {
        const data = JSON.parse(evt.data) as WsDetection;
        if (data.type === 'detection') {
          onDetection(data);
        }
      } catch {}
    };

    ws.onerror = () => {
      setConnected(false);
    };

    ws.onclose = () => {
      setConnected(false);
      wsRef.current = null;
      /* Auto-reconnect after 3 seconds */
      reconnectTimer.current = setTimeout(connect, 3000);
    };
  }, [wsUrl, onDetection]);

  useEffect(() => {
    connect();
    return () => {
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
      if (wsRef.current) {
        try { wsRef.current.close(); } catch {}
      }
    };
  }, [connect]);

  return connected;
}

/* ── Toast notification ───────────────────────────────────────────── */

function Toast({ message, visible }: { message: string; visible: boolean }) {
  const slideAnim = useRef(new Animated.Value(-100)).current;

  useEffect(() => {
    if (visible) {
      Animated.sequence([
        Animated.timing(slideAnim, {
          toValue: 0,
          duration: 300,
          useNativeDriver: true,
        }),
        Animated.delay(3000),
        Animated.timing(slideAnim, {
          toValue: -100,
          duration: 300,
          useNativeDriver: true,
        }),
      ]).start();
    }
  }, [visible, message, slideAnim]);

  if (!visible) return null;

  return (
    <Animated.View
      style={[
        styles.toast,
        { transform: [{ translateY: slideAnim }] },
      ]}
    >
      <Text style={styles.toastText}>{message}</Text>
    </Animated.View>
  );
}

/* ── Tab bar ──────────────────────────────────────────────────────── */

type TabId = 'live' | 'events' | 'settings';

const TABS: { id: TabId; label: string; icon: string }[] = [
  { id: 'live', label: 'Live Feed', icon: '📹' },
  { id: 'events', label: 'Events', icon: '📋' },
  { id: 'settings', label: 'Settings', icon: '⚙️' },
];

function TabBar({
  active,
  onSelect,
}: {
  active: TabId;
  onSelect: (t: TabId) => void;
}) {
  const insets = useSafeAreaInsets();
  return (
    <View style={[styles.tabBar, { paddingBottom: Math.max(insets.bottom, 6) }]}>
      {TABS.map((tab) => (
        <TouchableOpacity
          key={tab.id}
          style={styles.tab}
          onPress={() => onSelect(tab.id)}
        >
          <Text style={styles.tabIcon}>{tab.icon}</Text>
          <Text
            style={[
              styles.tabLabel,
              active === tab.id && styles.tabLabelActive,
            ]}
          >
            {tab.label}
          </Text>
        </TouchableOpacity>
      ))}
    </View>
  );
}

/* ── Live Feed Screen ─────────────────────────────────────────────── */

type StreamMode = 'live' | 'ai';

function LiveFeedScreen({ baseUrl }: { baseUrl: string }) {
  const [mode, setMode] = useState<StreamMode>('live');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(false);
  const [streamKey, setStreamKey] = useState(Date.now());

  const streamUrl = `${baseUrl}/stream?mode=${mode}&t=${streamKey}`;
  const { width } = Dimensions.get('window');
  const streamHeight = mode === 'live' ? (width * 3) / 4 : width;

  const toggleMode = () => {
    setMode((prev) => (prev === 'live' ? 'ai' : 'live'));
    setStreamKey(Date.now());
    setLoading(true);
    setError(false);
  };

  return (
    <View style={styles.screen}>
      <Text style={styles.header}>BirdFeeder Live</Text>

      <View style={[styles.streamContainer, { height: streamHeight }]}>
        {error ? (
          <View style={styles.centerBox}>
            <Text style={{ fontSize: 40 }}>📡</Text>
            <Text style={styles.errorText}>Cannot connect to BirdFeeder</Text>
            <Text style={styles.subText}>
              Check the device is on and the IP is correct
            </Text>
            <TouchableOpacity
              style={styles.retryBtn}
              onPress={() => {
                setStreamKey(Date.now());
                setError(false);
                setLoading(true);
              }}
            >
              <Text style={styles.btnText}>Retry</Text>
            </TouchableOpacity>
          </View>
        ) : (
          <>
            <WebView
              key={streamKey}
              source={{ html: `<html><body style="margin:0;background:#1e293b"><img src="${streamUrl}" style="width:100%;height:auto"/></body></html>` }}
              style={{ flex: 1, backgroundColor: '#1e293b' }}
              scrollEnabled={false}
              javaScriptEnabled={false}
              mediaPlaybackRequiresUserAction={false}
              onLoadEnd={() => setLoading(false)}
              onError={() => setError(true)}
              onHttpError={() => setError(true)}
            />
            {loading && (
              <View style={styles.loadingOverlay}>
                <ActivityIndicator size="large" color="#38bdf8" />
                <Text style={styles.subText}>Connecting...</Text>
              </View>
            )}
          </>
        )}
      </View>

      <View style={styles.controls}>
        <View style={styles.row}>
          <Text style={{ fontSize: 18 }}>{mode === 'live' ? '👁️' : '🧠'}</Text>
          <Text style={styles.modeLabel}>
            {mode === 'live' ? 'Live View (QVGA)' : 'AI View (96×96)'}
          </Text>
        </View>
        <TouchableOpacity style={styles.switchBtn} onPress={toggleMode}>
          <Text style={styles.switchText}>
            Switch to {mode === 'live' ? 'AI' : 'Live'}
          </Text>
        </TouchableOpacity>
      </View>

      <View style={styles.statusRow}>
        <View
          style={[styles.dot, error ? styles.dotRed : styles.dotGreen]}
        />
        <Text style={styles.subText}>
          {error ? 'Disconnected' : loading ? 'Connecting...' : 'Streaming'}
        </Text>
        <Text style={[styles.subText, { marginLeft: 'auto', fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace' }]}>
          {baseUrl}
        </Text>
      </View>
    </View>
  );
}

/* ── Events Screen ────────────────────────────────────────────────── */

interface DetectionEvent {
  id: number;
  species: string;
  confidence: number;
  timestamp: number;
}

const speciesEmoji: Record<string, string> = {
  crow: '🐦‍⬛',
  magpie: '🐦',
  squirrel: '🐿️',
  unknown: '❓',
};

function timeAgo(ts: number): string {
  const diff = Math.floor(Date.now() / 1000 - ts);
  if (diff < 60) return `${diff}s ago`;
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
  return new Date(ts * 1000).toLocaleDateString();
}

function EventsScreen({ baseUrl, refreshTrigger }: { baseUrl: string; refreshTrigger: number }) {
  const [events, setEvents] = useState<DetectionEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [selected, setSelected] = useState<DetectionEvent | null>(null);
  const { width } = Dimensions.get('window');

  const fetchEvents = useCallback(async () => {
    try {
      setError(null);
      const res = await fetch(`${baseUrl}/events`);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      setEvents(await res.json());
    } catch (e: any) {
      setError(e.message || 'Connection failed');
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  }, [baseUrl]);

  useEffect(() => {
    fetchEvents();
  }, [fetchEvents, refreshTrigger]);

  if (loading) {
    return (
      <View style={[styles.screen, styles.centerBox]}>
        <Text style={styles.header}>Detection Events</Text>
        <ActivityIndicator size="large" color="#38bdf8" />
      </View>
    );
  }

  if (error) {
    return (
      <View style={[styles.screen, styles.centerBox]}>
        <Text style={styles.header}>Detection Events</Text>
        <Text style={{ fontSize: 40 }}>📡</Text>
        <Text style={styles.errorText}>Cannot connect</Text>
        <Text style={styles.subText}>{error}</Text>
        <TouchableOpacity style={styles.retryBtn} onPress={fetchEvents}>
          <Text style={styles.btnText}>Retry</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <View style={styles.screen}>
      <Text style={styles.header}>Detection Events</Text>
      <FlatList
        data={events}
        keyExtractor={(item) => `ev-${item.id}`}
        contentContainerStyle={
          events.length === 0 ? { flex: 1, justifyContent: 'center' } : { paddingVertical: 8 }
        }
        refreshControl={
          <RefreshControl
            refreshing={refreshing}
            onRefresh={() => {
              setRefreshing(true);
              fetchEvents();
            }}
            tintColor="#38bdf8"
            colors={['#38bdf8']}
          />
        }
        ListEmptyComponent={
          <View style={{ alignItems: 'center', padding: 32 }}>
            <Text style={{ fontSize: 40 }}>🌿</Text>
            <Text style={styles.subText}>No detections yet</Text>
          </View>
        }
        renderItem={({ item }) => (
          <TouchableOpacity
            style={styles.eventCard}
            onPress={() => setSelected(item)}
          >
            <Text style={{ fontSize: 28 }}>
              {speciesEmoji[item.species] || '❓'}
            </Text>
            <View style={{ flex: 1, marginLeft: 12 }}>
              <Text style={styles.eventSpecies}>
                {item.species.charAt(0).toUpperCase() + item.species.slice(1)}
              </Text>
              <Text style={styles.subText}>{timeAgo(item.timestamp)}</Text>
            </View>
            <Text style={styles.confidence}>
              {(item.confidence * 100).toFixed(0)}%
            </Text>
          </TouchableOpacity>
        )}
      />

      <Modal
        visible={!!selected}
        transparent
        animationType="fade"
        onRequestClose={() => setSelected(null)}
      >
        <TouchableOpacity
          style={styles.modalOverlay}
          activeOpacity={1}
          onPress={() => setSelected(null)}
        >
          {selected && (
            <View style={styles.modalContent}>
              <Image
                source={{ uri: `${baseUrl}/events/${selected.id}.jpg` }}
                style={{ width: width - 32, height: width - 32 }}
                resizeMode="contain"
              />
              <Text style={[styles.eventSpecies, { textAlign: 'center', marginTop: 12 }]}>
                {speciesEmoji[selected.species] || '❓'}{' '}
                {selected.species.charAt(0).toUpperCase() +
                  selected.species.slice(1)}{' '}
                — {(selected.confidence * 100).toFixed(1)}%
              </Text>
            </View>
          )}
        </TouchableOpacity>
      </Modal>
    </View>
  );
}

/* ── Settings Screen ──────────────────────────────────────────────── */

function SettingsScreen({
  deviceIp,
  setDeviceIp,
  notificationsEnabled,
  setNotificationsEnabled,
  baseUrl,
  wsConnected,
}: {
  deviceIp: string;
  setDeviceIp: (ip: string) => void;
  notificationsEnabled: boolean;
  setNotificationsEnabled: (v: boolean) => void;
  baseUrl: string;
  wsConnected: boolean;
}) {
  const [ipInput, setIpInput] = useState(deviceIp);
  const [testing, setTesting] = useState(false);
  const [status, setStatus] = useState<any>(null);

  const saveIp = () => {
    const trimmed = ipInput.trim();
    if (trimmed && trimmed !== deviceIp) {
      setDeviceIp(trimmed);
      setStatus(null);
    }
  };

  const testConnection = async () => {
    setTesting(true);
    setStatus(null);
    try {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 5000);
      const res = await fetch(`http://${ipInput.trim()}/status`, {
        signal: controller.signal,
      });
      clearTimeout(timeout);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      setStatus(await res.json());
      if (ipInput.trim() !== deviceIp) setDeviceIp(ipInput.trim());
    } catch (err: any) {
      Alert.alert('Connection Failed', err.message || 'Could not reach device');
    } finally {
      setTesting(false);
    }
  };

  return (
    <ScrollView style={styles.screen} contentContainerStyle={{ padding: 16 }}>
      <Text style={styles.header}>Settings</Text>

      <Text style={styles.sectionTitle}>DEVICE CONNECTION</Text>
      <View style={styles.card}>
        <Text style={styles.label}>ESP32 IP Address</Text>
        <View style={styles.row}>
          <TextInput
            style={styles.ipInput}
            value={ipInput}
            onChangeText={setIpInput}
            onBlur={saveIp}
            onSubmitEditing={saveIp}
            placeholder="192.168.0.21"
            placeholderTextColor="#475569"
            keyboardType="numeric"
            autoCapitalize="none"
            autoCorrect={false}
          />
          <TouchableOpacity
            style={styles.testBtn}
            onPress={testConnection}
            disabled={testing}
          >
            {testing ? (
              <ActivityIndicator size="small" color="#38bdf8" />
            ) : (
              <Text style={{ fontSize: 20 }}>📡</Text>
            )}
          </TouchableOpacity>
        </View>

        {status && (
          <View style={styles.statusCard}>
            <Text style={[styles.modeLabel, { color: '#22c55e' }]}>
              ✅ Connected
            </Text>
            <View style={[styles.row, { justifyContent: 'space-around', marginTop: 8 }]}>
              <View style={{ alignItems: 'center' }}>
                <Text style={styles.statValue}>
                  {status.battery_pct?.toFixed(0)}%
                </Text>
                <Text style={styles.subText}>Battery</Text>
              </View>
              <View style={{ alignItems: 'center' }}>
                <Text style={styles.statValue}>
                  {status.battery_v?.toFixed(2)}V
                </Text>
                <Text style={styles.subText}>Voltage</Text>
              </View>
              <View style={{ alignItems: 'center' }}>
                <Text style={styles.statValue}>
                  {Math.floor((status.uptime_s || 0) / 60)}m
                </Text>
                <Text style={styles.subText}>Uptime</Text>
              </View>
            </View>
          </View>
        )}
      </View>

      <Text style={styles.sectionTitle}>NOTIFICATIONS</Text>
      <View style={styles.card}>
        <View style={[styles.row, { justifyContent: 'space-between' }]}>
          <View style={{ flex: 1 }}>
            <Text style={styles.eventSpecies}>Push Notifications</Text>
            <Text style={styles.subText}>Alert when threats are detected</Text>
          </View>
          <Switch
            value={notificationsEnabled}
            onValueChange={setNotificationsEnabled}
            trackColor={{ false: '#334155', true: '#0369a1' }}
            thumbColor={notificationsEnabled ? '#38bdf8' : '#64748b'}
          />
        </View>
      </View>

      <Text style={styles.sectionTitle}>ABOUT</Text>
      <View style={styles.card}>
        <View style={[styles.row, { justifyContent: 'space-between', paddingVertical: 6 }]}>
          <Text style={styles.subText}>Version</Text>
          <Text style={styles.label}>1.0.0</Text>
        </View>
        <View style={[styles.row, { justifyContent: 'space-between', paddingVertical: 6 }]}>
          <Text style={styles.subText}>API</Text>
          <Text style={styles.label}>{baseUrl}</Text>
        </View>
        <View style={[styles.row, { justifyContent: 'space-between', paddingVertical: 6 }]}>
          <Text style={styles.subText}>WebSocket</Text>
          <View style={[styles.row, { gap: 6 }]}>
            <View style={[styles.dot, wsConnected ? styles.dotGreen : styles.dotRed]} />
            <Text style={styles.label}>{wsConnected ? 'Connected' : 'Disconnected'}</Text>
          </View>
        </View>
      </View>
    </ScrollView>
  );
}

/* ── Main App ─────────────────────────────────────────────────────── */

export default function App() {
  const [activeTab, setActiveTab] = useState<TabId>('live');
  const settings = useSettings();
  const [toastMsg, setToastMsg] = useState('');
  const [toastVisible, setToastVisible] = useState(false);
  const [eventRefresh, setEventRefresh] = useState(0);

  const handleDetection = useCallback((d: WsDetection) => {
    const species = d.species.charAt(0).toUpperCase() + d.species.slice(1);
    const conf = (d.confidence * 100).toFixed(0);
    setToastMsg(`🚨 ${speciesEmoji[d.species] || '❓'} ${species} detected (${conf}%)`);
    setToastVisible(true);
    /* Trigger events refresh */
    setEventRefresh((n) => n + 1);
    /* Reset toast visibility after animation */
    setTimeout(() => setToastVisible(false), 3600);
  }, []);

  const wsConnected = useWebSocket(
    settings.loaded ? settings.wsUrl : '',
    handleDetection
  );

  if (!settings.loaded) {
    return (
      <SafeAreaView style={[styles.screen, styles.centerBox]}>
        <StatusBar style="light" />
        <ActivityIndicator size="large" color="#38bdf8" />
      </SafeAreaView>
    );
  }

  return (
    <SafeAreaProvider>
    <SafeAreaView style={styles.container}>
      <StatusBar style="light" />
      <Toast message={toastMsg} visible={toastVisible} />
      <View style={{ flex: 1 }}>
        {activeTab === 'live' && (
          <LiveFeedScreen baseUrl={settings.baseUrl} />
        )}
        {activeTab === 'events' && (
          <EventsScreen baseUrl={settings.baseUrl} refreshTrigger={eventRefresh} />
        )}
        {activeTab === 'settings' && (
          <SettingsScreen
            deviceIp={settings.deviceIp}
            setDeviceIp={settings.setDeviceIp}
            notificationsEnabled={settings.notificationsEnabled}
            setNotificationsEnabled={settings.setNotificationsEnabled}
            baseUrl={settings.baseUrl}
            wsConnected={wsConnected}
          />
        )}
      </View>
      <TabBar active={activeTab} onSelect={setActiveTab} />
    </SafeAreaView>
    </SafeAreaProvider>
  );
}

/* ── Styles ────────────────────────────────────────────────────────── */

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0f172a',
    paddingTop: Platform.OS === 'android' ? RNStatusBar.currentHeight : 0,
  },
  screen: {
    flex: 1,
    backgroundColor: '#0f172a',
  },
  header: {
    color: '#e2e8f0',
    fontSize: 22,
    fontWeight: '700',
    paddingHorizontal: 16,
    paddingTop: 12,
    paddingBottom: 8,
  },
  centerBox: {
    alignItems: 'center',
    justifyContent: 'center',
  },
  /* Tab bar */
  tabBar: {
    flexDirection: 'row',
    backgroundColor: '#0f172a',
    borderTopWidth: 1,
    borderTopColor: '#1e293b',
    paddingBottom: 6,
    paddingTop: 8,
  },
  tab: {
    flex: 1,
    alignItems: 'center',
    gap: 2,
  },
  tabIcon: {
    fontSize: 20,
  },
  tabLabel: {
    color: '#64748b',
    fontSize: 11,
    fontWeight: '600',
  },
  tabLabelActive: {
    color: '#38bdf8',
  },
  /* Stream */
  streamContainer: {
    width: '100%',
    backgroundColor: '#1e293b',
    position: 'relative',
  },
  loadingOverlay: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: 'rgba(15,23,42,0.85)',
    alignItems: 'center',
    justifyContent: 'center',
  },
  controls: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: '#1e293b',
    borderBottomWidth: 1,
    borderBottomColor: '#334155',
  },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  modeLabel: {
    color: '#38bdf8',
    fontWeight: '600',
    fontSize: 15,
  },
  switchBtn: {
    backgroundColor: '#334155',
    paddingHorizontal: 14,
    paddingVertical: 8,
    borderRadius: 8,
  },
  switchText: {
    color: '#e2e8f0',
    fontSize: 13,
    fontWeight: '500',
  },
  statusRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 16,
    paddingVertical: 10,
    gap: 8,
  },
  dot: { width: 8, height: 8, borderRadius: 4 },
  dotGreen: { backgroundColor: '#22c55e' },
  dotRed: { backgroundColor: '#ef4444' },
  /* Events */
  eventCard: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1e293b',
    marginHorizontal: 12,
    marginVertical: 4,
    borderRadius: 12,
    padding: 14,
  },
  eventSpecies: {
    color: '#e2e8f0',
    fontSize: 16,
    fontWeight: '600',
  },
  confidence: {
    color: '#38bdf8',
    fontSize: 14,
    fontWeight: '700',
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.85)',
    alignItems: 'center',
    justifyContent: 'center',
  },
  modalContent: {
    backgroundColor: '#1e293b',
    borderRadius: 16,
    overflow: 'hidden',
    padding: 16,
  },
  /* Settings */
  sectionTitle: {
    color: '#64748b',
    fontSize: 12,
    fontWeight: '700',
    letterSpacing: 1,
    marginTop: 20,
    marginBottom: 8,
    marginLeft: 4,
  },
  card: {
    backgroundColor: '#1e293b',
    borderRadius: 12,
    padding: 16,
  },
  label: {
    color: '#94a3b8',
    fontSize: 13,
    fontWeight: '500',
    marginBottom: 8,
  },
  ipInput: {
    flex: 1,
    backgroundColor: '#0f172a',
    borderRadius: 8,
    paddingHorizontal: 14,
    paddingVertical: 10,
    color: '#e2e8f0',
    fontSize: 16,
    borderWidth: 1,
    borderColor: '#334155',
  },
  testBtn: {
    backgroundColor: '#0f172a',
    borderRadius: 8,
    width: 44,
    height: 44,
    alignItems: 'center',
    justifyContent: 'center',
    borderWidth: 1,
    borderColor: '#334155',
  },
  statusCard: {
    backgroundColor: '#0f172a',
    borderRadius: 8,
    padding: 12,
    marginTop: 12,
  },
  statValue: {
    color: '#e2e8f0',
    fontSize: 18,
    fontWeight: '700',
  },
  /* Common */
  errorText: {
    color: '#ef4444',
    fontSize: 18,
    fontWeight: '700',
    marginTop: 12,
  },
  subText: {
    color: '#94a3b8',
    fontSize: 13,
    marginTop: 4,
  },
  retryBtn: {
    marginTop: 16,
    backgroundColor: '#1e293b',
    paddingHorizontal: 24,
    paddingVertical: 10,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#334155',
  },
  btnText: {
    color: '#38bdf8',
    fontWeight: '600',
  },
  /* Toast */
  toast: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    zIndex: 100,
    backgroundColor: '#dc2626',
    paddingTop: Platform.OS === 'android' ? (RNStatusBar.currentHeight || 0) + 8 : 50,
    paddingBottom: 12,
    paddingHorizontal: 16,
    alignItems: 'center',
  },
  toastText: {
    color: '#fff',
    fontSize: 15,
    fontWeight: '700',
  },
});
