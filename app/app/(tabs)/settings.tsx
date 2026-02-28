import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  TextInput,
  Switch,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  Alert,
  ActivityIndicator,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { useSettings } from '../../src/context/SettingsContext';

export default function SettingsScreen() {
  const { settings, updateSettings, baseUrl } = useSettings();
  const [ipInput, setIpInput] = useState(settings.deviceIp);
  const [testing, setTesting] = useState(false);
  const [status, setStatus] = useState<any>(null);

  useEffect(() => {
    setIpInput(settings.deviceIp);
  }, [settings.deviceIp]);

  const saveIp = () => {
    const trimmed = ipInput.trim();
    if (trimmed && trimmed !== settings.deviceIp) {
      updateSettings({ deviceIp: trimmed });
      setStatus(null);
    }
  };

  const testConnection = async () => {
    setTesting(true);
    setStatus(null);
    try {
      const res = await fetch(`${baseUrl}/status`, {
        signal: AbortSignal.timeout(5000),
      });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      setStatus(data);
    } catch (err: any) {
      Alert.alert(
        'Connection Failed',
        `Could not reach BirdFeeder at ${baseUrl}\n\n${err.message}`,
        [{ text: 'OK' }]
      );
    } finally {
      setTesting(false);
    }
  };

  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {/* Device Connection */}
      <Text style={styles.sectionTitle}>Device Connection</Text>
      <View style={styles.card}>
        <Text style={styles.label}>ESP32 IP Address</Text>
        <View style={styles.ipRow}>
          <TextInput
            style={styles.ipInput}
            value={ipInput}
            onChangeText={setIpInput}
            onBlur={saveIp}
            onSubmitEditing={saveIp}
            placeholder="192.168.1.100"
            placeholderTextColor="#475569"
            keyboardType="numeric"
            autoCapitalize="none"
            autoCorrect={false}
          />
          <TouchableOpacity
            style={styles.testButton}
            onPress={testConnection}
            disabled={testing}
          >
            {testing ? (
              <ActivityIndicator size="small" color="#38bdf8" />
            ) : (
              <Ionicons name="pulse" size={20} color="#38bdf8" />
            )}
          </TouchableOpacity>
        </View>
        <Text style={styles.hint}>
          Find this on your router's connected devices page
        </Text>

        {status && (
          <View style={styles.statusCard}>
            <View style={styles.statusRow}>
              <Ionicons name="checkmark-circle" size={20} color="#22c55e" />
              <Text style={styles.statusConnected}>Connected</Text>
            </View>
            <View style={styles.statusGrid}>
              <View style={styles.statusItem}>
                <Text style={styles.statusValue}>
                  {status.battery_pct?.toFixed(0)}%
                </Text>
                <Text style={styles.statusLabel}>Battery</Text>
              </View>
              <View style={styles.statusItem}>
                <Text style={styles.statusValue}>
                  {status.battery_v?.toFixed(2)}V
                </Text>
                <Text style={styles.statusLabel}>Voltage</Text>
              </View>
              <View style={styles.statusItem}>
                <Text style={styles.statusValue}>
                  {Math.floor((status.uptime_s || 0) / 60)}m
                </Text>
                <Text style={styles.statusLabel}>Uptime</Text>
              </View>
            </View>
          </View>
        )}
      </View>

      {/* Notifications */}
      <Text style={styles.sectionTitle}>Notifications</Text>
      <View style={styles.card}>
        <View style={styles.switchRow}>
          <View style={styles.switchInfo}>
            <Text style={styles.switchLabel}>Push Notifications</Text>
            <Text style={styles.switchDesc}>
              Get alerted when threats are detected
            </Text>
          </View>
          <Switch
            value={settings.notificationsEnabled}
            onValueChange={(val) =>
              updateSettings({ notificationsEnabled: val })
            }
            trackColor={{ false: '#334155', true: '#0369a1' }}
            thumbColor={settings.notificationsEnabled ? '#38bdf8' : '#64748b'}
          />
        </View>
      </View>

      {/* About */}
      <Text style={styles.sectionTitle}>About</Text>
      <View style={styles.card}>
        <View style={styles.aboutRow}>
          <Text style={styles.aboutLabel}>Version</Text>
          <Text style={styles.aboutValue}>1.0.0</Text>
        </View>
        <View style={styles.aboutRow}>
          <Text style={styles.aboutLabel}>Firmware API</Text>
          <Text style={styles.aboutValue}>{baseUrl}</Text>
        </View>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0f172a',
  },
  content: {
    padding: 16,
    paddingBottom: 40,
  },
  sectionTitle: {
    color: '#64748b',
    fontSize: 12,
    fontWeight: '700',
    textTransform: 'uppercase',
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
  ipRow: {
    flexDirection: 'row',
    gap: 8,
  },
  ipInput: {
    flex: 1,
    backgroundColor: '#0f172a',
    borderRadius: 8,
    paddingHorizontal: 14,
    paddingVertical: 10,
    color: '#e2e8f0',
    fontSize: 16,
    fontFamily: 'monospace',
    borderWidth: 1,
    borderColor: '#334155',
  },
  testButton: {
    backgroundColor: '#0f172a',
    borderRadius: 8,
    width: 44,
    alignItems: 'center',
    justifyContent: 'center',
    borderWidth: 1,
    borderColor: '#334155',
  },
  hint: {
    color: '#475569',
    fontSize: 11,
    marginTop: 8,
    marginLeft: 2,
  },
  statusCard: {
    backgroundColor: '#0f172a',
    borderRadius: 8,
    padding: 12,
    marginTop: 12,
  },
  statusRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    marginBottom: 10,
  },
  statusConnected: {
    color: '#22c55e',
    fontWeight: '600',
    fontSize: 14,
  },
  statusGrid: {
    flexDirection: 'row',
    justifyContent: 'space-around',
  },
  statusItem: {
    alignItems: 'center',
  },
  statusValue: {
    color: '#e2e8f0',
    fontSize: 18,
    fontWeight: '700',
  },
  statusLabel: {
    color: '#64748b',
    fontSize: 11,
    marginTop: 2,
  },
  switchRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  switchInfo: {
    flex: 1,
    marginRight: 16,
  },
  switchLabel: {
    color: '#e2e8f0',
    fontSize: 15,
    fontWeight: '600',
  },
  switchDesc: {
    color: '#64748b',
    fontSize: 12,
    marginTop: 2,
  },
  aboutRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: '#334155',
  },
  aboutLabel: {
    color: '#94a3b8',
    fontSize: 14,
  },
  aboutValue: {
    color: '#e2e8f0',
    fontSize: 14,
    fontFamily: 'monospace',
  },
});
