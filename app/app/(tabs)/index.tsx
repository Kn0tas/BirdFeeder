import React, { useState, useCallback } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  ActivityIndicator,
  Dimensions,
} from 'react-native';
import { WebView } from 'react-native-webview';
import { Ionicons } from '@expo/vector-icons';
import { useFocusEffect } from 'expo-router';
import { useSettings } from '../../src/context/SettingsContext';

type StreamMode = 'live' | 'ai';

export default function LiveFeedScreen() {
  const { baseUrl } = useSettings();
  const [mode, setMode] = useState<StreamMode>('live');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(false);
  const [streamKey, setStreamKey] = useState(Date.now());

  // Refresh stream when tab is focused
  useFocusEffect(
    useCallback(() => {
      setStreamKey(Date.now());
      setLoading(true);
      setError(false);
    }, [])
  );

  const streamUrl = `${baseUrl}/stream?mode=${mode}&t=${streamKey}`;

  const toggleMode = () => {
    setMode((prev) => (prev === 'live' ? 'ai' : 'live'));
    setStreamKey(Date.now());
    setLoading(true);
    setError(false);
  };

  const { width } = Dimensions.get('window');
  const streamHeight = mode === 'live' ? (width * 480) / 640 : width;

  const mjpegHtml = `<html><body style="margin:0;background:#1e293b"><img src="${streamUrl}" style="width:100%;height:auto"/></body></html>`;

  return (
    <View style={styles.container}>
      {/* Stream viewer */}
      <View style={[styles.streamContainer, { height: streamHeight }]}>
        {error ? (
          <View style={styles.errorContainer}>
            <Ionicons name="cloud-offline" size={48} color="#ef4444" />
            <Text style={styles.errorText}>Cannot connect to BirdFeeder</Text>
            <Text style={styles.errorSub}>
              Check that the device is on and the IP is correct in Settings
            </Text>
            <TouchableOpacity
              style={styles.retryButton}
              onPress={() => {
                setStreamKey(Date.now());
                setError(false);
                setLoading(true);
              }}
            >
              <Text style={styles.retryText}>Retry</Text>
            </TouchableOpacity>
          </View>
        ) : (
          <>
            <WebView
              key={streamKey}
              source={{ html: mjpegHtml }}
              style={[styles.webview, { backgroundColor: '#1e293b' }]}
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
                <Text style={styles.loadingText}>Connecting...</Text>
              </View>
            )}
          </>
        )}
      </View>

      {/* Controls */}
      <View style={styles.controls}>
        <TouchableOpacity style={styles.modeButton} onPress={toggleMode}>
          <Ionicons
            name={mode === 'live' ? 'eye' : 'hardware-chip'}
            size={22}
            color="#38bdf8"
          />
          <Text style={styles.modeText}>
            {mode === 'live' ? 'Live View (VGA)' : 'AI View (96×96)'}
          </Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={styles.switchButton}
          onPress={toggleMode}
        >
          <Ionicons name="swap-horizontal" size={20} color="#e2e8f0" />
          <Text style={styles.switchText}>
            Switch to {mode === 'live' ? 'AI' : 'Live'}
          </Text>
        </TouchableOpacity>
      </View>

      {/* Status bar */}
      <View style={styles.statusBar}>
        <View style={[styles.dot, error ? styles.dotRed : styles.dotGreen]} />
        <Text style={styles.statusText}>
          {error ? 'Disconnected' : loading ? 'Connecting...' : 'Streaming'}
        </Text>
        <Text style={styles.ipText}>{baseUrl}</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0f172a',
  },
  streamContainer: {
    width: '100%',
    backgroundColor: '#1e293b',
    position: 'relative',
  },
  webview: {
    flex: 1,
    backgroundColor: 'transparent',
  },
  loadingOverlay: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: 'rgba(15, 23, 42, 0.85)',
    alignItems: 'center',
    justifyContent: 'center',
  },
  loadingText: {
    color: '#94a3b8',
    marginTop: 12,
    fontSize: 14,
  },
  errorContainer: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
    padding: 32,
  },
  errorText: {
    color: '#ef4444',
    fontSize: 18,
    fontWeight: '700',
    marginTop: 16,
  },
  errorSub: {
    color: '#94a3b8',
    fontSize: 13,
    textAlign: 'center',
    marginTop: 8,
  },
  retryButton: {
    marginTop: 20,
    backgroundColor: '#1e293b',
    paddingHorizontal: 24,
    paddingVertical: 10,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#334155',
  },
  retryText: {
    color: '#38bdf8',
    fontWeight: '600',
  },
  controls: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 16,
    paddingVertical: 14,
    backgroundColor: '#1e293b',
    borderBottomWidth: 1,
    borderBottomColor: '#334155',
  },
  modeButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  modeText: {
    color: '#38bdf8',
    fontWeight: '600',
    fontSize: 15,
  },
  switchButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
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
  statusBar: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 16,
    paddingVertical: 10,
    backgroundColor: '#0f172a',
    gap: 8,
  },
  dot: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  dotGreen: {
    backgroundColor: '#22c55e',
  },
  dotRed: {
    backgroundColor: '#ef4444',
  },
  statusText: {
    color: '#94a3b8',
    fontSize: 12,
    flex: 1,
  },
  ipText: {
    color: '#64748b',
    fontSize: 11,
    fontFamily: 'monospace',
  },
});
