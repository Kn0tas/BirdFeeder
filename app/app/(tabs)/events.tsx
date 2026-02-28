import React, { useState, useCallback } from 'react';
import {
  View,
  Text,
  FlatList,
  Image,
  TouchableOpacity,
  StyleSheet,
  RefreshControl,
  Modal,
  Dimensions,
  ActivityIndicator,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { useFocusEffect } from 'expo-router';
import { useSettings } from '../../src/context/SettingsContext';

interface DetectionEvent {
  id: number;
  species: string;
  confidence: number;
  timestamp: number;
}

const speciesIcons: Record<string, string> = {
  crow: '🐦‍⬛',
  magpie: '🐦',
  squirrel: '🐿️',
  background: '🌿',
  unknown: '❓',
};

const speciesColors: Record<string, string> = {
  crow: '#ef4444',
  magpie: '#f97316',
  squirrel: '#eab308',
  background: '#22c55e',
  unknown: '#64748b',
};

function formatTimestamp(unixSeconds: number): string {
  const d = new Date(unixSeconds * 1000);
  const now = new Date();
  const diff = Math.floor((now.getTime() - d.getTime()) / 1000);

  if (diff < 60) return `${diff}s ago`;
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
  return d.toLocaleDateString();
}

export default function EventsScreen() {
  const { baseUrl } = useSettings();
  const [events, setEvents] = useState<DetectionEvent[]>([]);
  const [refreshing, setRefreshing] = useState(false);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedEvent, setSelectedEvent] = useState<DetectionEvent | null>(
    null
  );

  const fetchEvents = useCallback(async () => {
    try {
      setError(null);
      const res = await fetch(`${baseUrl}/events`);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data: DetectionEvent[] = await res.json();
      setEvents(data);
    } catch (err: any) {
      setError(err.message || 'Connection failed');
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  }, [baseUrl]);

  useFocusEffect(
    useCallback(() => {
      setLoading(true);
      fetchEvents();
    }, [fetchEvents])
  );

  const onRefresh = () => {
    setRefreshing(true);
    fetchEvents();
  };

  const renderEvent = ({ item }: { item: DetectionEvent }) => (
    <TouchableOpacity
      style={styles.eventCard}
      onPress={() => setSelectedEvent(item)}
      activeOpacity={0.7}
    >
      <View style={styles.eventIcon}>
        <Text style={styles.eventEmoji}>
          {speciesIcons[item.species] || speciesIcons.unknown}
        </Text>
      </View>
      <View style={styles.eventInfo}>
        <Text style={styles.eventSpecies}>
          {item.species.charAt(0).toUpperCase() + item.species.slice(1)}
        </Text>
        <Text style={styles.eventTime}>{formatTimestamp(item.timestamp)}</Text>
      </View>
      <View style={styles.eventRight}>
        <View
          style={[
            styles.confidenceBadge,
            {
              backgroundColor:
                (speciesColors[item.species] || speciesColors.unknown) + '22',
            },
          ]}
        >
          <Text
            style={[
              styles.confidenceText,
              {
                color:
                  speciesColors[item.species] || speciesColors.unknown,
              },
            ]}
          >
            {(item.confidence * 100).toFixed(0)}%
          </Text>
        </View>
        <Ionicons name="chevron-forward" size={16} color="#475569" />
      </View>
    </TouchableOpacity>
  );

  if (loading) {
    return (
      <View style={styles.centerContainer}>
        <ActivityIndicator size="large" color="#38bdf8" />
        <Text style={styles.loadingText}>Loading events...</Text>
      </View>
    );
  }

  if (error) {
    return (
      <View style={styles.centerContainer}>
        <Ionicons name="cloud-offline" size={48} color="#ef4444" />
        <Text style={styles.errorText}>Cannot connect</Text>
        <Text style={styles.errorSub}>{error}</Text>
        <TouchableOpacity style={styles.retryButton} onPress={onRefresh}>
          <Text style={styles.retryText}>Retry</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <FlatList
        data={events}
        keyExtractor={(item) => `event-${item.id}`}
        renderItem={renderEvent}
        contentContainerStyle={events.length === 0 ? styles.emptyList : styles.list}
        refreshControl={
          <RefreshControl
            refreshing={refreshing}
            onRefresh={onRefresh}
            tintColor="#38bdf8"
            colors={['#38bdf8']}
          />
        }
        ListEmptyComponent={
          <View style={styles.emptyContainer}>
            <Ionicons name="leaf-outline" size={48} color="#334155" />
            <Text style={styles.emptyText}>No detections yet</Text>
            <Text style={styles.emptySubtext}>
              Events will appear here when threats are detected
            </Text>
          </View>
        }
      />

      {/* Snapshot modal */}
      <Modal
        visible={!!selectedEvent}
        transparent
        animationType="fade"
        onRequestClose={() => setSelectedEvent(null)}
      >
        <TouchableOpacity
          style={styles.modalOverlay}
          activeOpacity={1}
          onPress={() => setSelectedEvent(null)}
        >
          <View style={styles.modalContent}>
            {selectedEvent && (
              <>
                <Image
                  source={{
                    uri: `${baseUrl}/events/${selectedEvent.id}.jpg`,
                  }}
                  style={styles.snapshotImage}
                  resizeMode="contain"
                />
                <View style={styles.modalInfo}>
                  <Text style={styles.modalSpecies}>
                    {speciesIcons[selectedEvent.species] || '❓'}{' '}
                    {selectedEvent.species.charAt(0).toUpperCase() +
                      selectedEvent.species.slice(1)}
                  </Text>
                  <Text style={styles.modalConfidence}>
                    Confidence: {(selectedEvent.confidence * 100).toFixed(1)}%
                  </Text>
                </View>
              </>
            )}
          </View>
        </TouchableOpacity>
      </Modal>
    </View>
  );
}

const { width } = Dimensions.get('window');

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0f172a',
  },
  centerContainer: {
    flex: 1,
    backgroundColor: '#0f172a',
    alignItems: 'center',
    justifyContent: 'center',
    padding: 32,
  },
  list: {
    paddingVertical: 8,
  },
  emptyList: {
    flex: 1,
  },
  eventCard: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1e293b',
    marginHorizontal: 12,
    marginVertical: 4,
    borderRadius: 12,
    padding: 14,
    gap: 12,
  },
  eventIcon: {
    width: 44,
    height: 44,
    borderRadius: 22,
    backgroundColor: '#334155',
    alignItems: 'center',
    justifyContent: 'center',
  },
  eventEmoji: {
    fontSize: 22,
  },
  eventInfo: {
    flex: 1,
  },
  eventSpecies: {
    color: '#e2e8f0',
    fontSize: 16,
    fontWeight: '600',
  },
  eventTime: {
    color: '#64748b',
    fontSize: 12,
    marginTop: 2,
  },
  eventRight: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  confidenceBadge: {
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 12,
  },
  confidenceText: {
    fontSize: 13,
    fontWeight: '700',
  },
  emptyContainer: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
    padding: 32,
  },
  emptyText: {
    color: '#475569',
    fontSize: 18,
    fontWeight: '600',
    marginTop: 16,
  },
  emptySubtext: {
    color: '#334155',
    fontSize: 13,
    textAlign: 'center',
    marginTop: 8,
  },
  loadingText: {
    color: '#94a3b8',
    marginTop: 12,
    fontSize: 14,
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
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0, 0, 0, 0.85)',
    alignItems: 'center',
    justifyContent: 'center',
  },
  modalContent: {
    width: width - 32,
    backgroundColor: '#1e293b',
    borderRadius: 16,
    overflow: 'hidden',
  },
  snapshotImage: {
    width: '100%',
    height: width - 32,
    backgroundColor: '#0f172a',
  },
  modalInfo: {
    padding: 16,
    alignItems: 'center',
    gap: 4,
  },
  modalSpecies: {
    color: '#e2e8f0',
    fontSize: 20,
    fontWeight: '700',
  },
  modalConfidence: {
    color: '#94a3b8',
    fontSize: 14,
  },
});
