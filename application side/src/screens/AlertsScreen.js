import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, FlatList, TouchableOpacity, ActivityIndicator } from 'react-native';
import { onValue, ref } from 'firebase/database';
import { database } from '@/utils/firebase';
import { COLORS } from '@/constants/colors';
import { Ionicons } from '@expo/vector-icons';

const AlertsScreen = ({ navigation }) => {
  const [alerts, setAlerts] = useState([]);
  const [loading, setLoading] = useState(true);

  const normalizeTimestampMs = (raw) => {
    const n = typeof raw === 'string' ? Number(raw) : raw;
    if (!Number.isFinite(n) || n <= 0) return null;

    // If it looks like seconds, convert to ms.
    let ms = n;
    if (ms < 100000000000) ms = ms * 1000;

    // Sanity check: avoid obviously wrong dates (e.g., 1970 due to overflow).
    const now = Date.now();
    const minOk = 946684800000; // Jan 1, 2000
    const maxOk = now + 7 * 24 * 60 * 60 * 1000; // allow up to 7 days in future
    if (ms < minOk || ms > maxOk) return null;
    return ms;
  };

  const formatTimestamp = (raw) => {
    const ms = normalizeTimestampMs(raw);
    if (!ms) return 'Unknown';
    return new Date(ms).toLocaleString();
  };

  useEffect(() => {
    const alertsRef = ref(database, '/device-alerts');
    const unsubscribe = onValue(alertsRef, (snapshot) => {
      const data = snapshot.val();
      if (data) {
        // Firebase returns an object, so we convert it to an array
        const alertsList = Object.keys(data)
          .map((key) => {
            const item = data[key] ?? {};
            const timestampMs = normalizeTimestampMs(item.timestamp);
            return {
              id: key,
              ...item,
              timestampMs,
            };
          })
          .sort((a, b) => (b.timestampMs || 0) - (a.timestampMs || 0)); // Sort by newest first
        setAlerts(alertsList);
      } else {
        setAlerts([]);
      }
      setLoading(false);
    });

    // Cleanup subscription on unmount
    return () => unsubscribe();
  }, []);

  const getSeverityColor = (severity) => {
    switch (severity?.toUpperCase()) {
      case 'CRITICAL':
        return COLORS.danger;
      case 'HIGH':
        return COLORS.warning;
      default:
        return COLORS.secondary;
    }
  };

  const renderItem = ({ item }) => (
    <TouchableOpacity style={styles.itemContainer} onPress={() => navigation.navigate('AlertDetail', { alert: item })}>
      <View style={[styles.severityIndicator, { backgroundColor: getSeverityColor(item.severity) }]} />
      <View style={styles.itemContent}>
        <Text style={styles.itemType}>{item.alertType.replace('_', ' ')}</Text>
        <Text style={styles.itemDevice}>Device: {item.deviceId}</Text>
        <Text style={styles.itemTime}>{formatTimestamp(item.timestampMs ?? item.timestamp)}</Text>
      </View>
      {item.imageData?.base64 && <Ionicons name="camera" size={24} color={COLORS.textLight} style={{ marginRight: 10 }} />}
      <Ionicons name="chevron-forward" size={24} color={COLORS.textLight} />
    </TouchableOpacity>
  );

  if (loading) {
    return (
      <View style={styles.centered}>
        <ActivityIndicator size="large" color={COLORS.primary} />
        <Text>Loading Alerts...</Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Live Alerts</Text>
      {alerts.length > 0 ? (
        <FlatList
          data={alerts}
          renderItem={renderItem}
          keyExtractor={item => item.id}
          contentContainerStyle={styles.list}
        />
      ) : (
        <View style={styles.centered}>
          <Text style={styles.noAlertsText}>No active alerts.</Text>
        </View>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
  },
  title: {
    fontSize: 28,
    fontWeight: 'bold',
    padding: 16,
    paddingBottom: 10,
  },
  list: {
    paddingHorizontal: 16,
  },
  itemContainer: {
    backgroundColor: COLORS.white,
    borderRadius: 12,
    padding: 15,
    marginBottom: 10,
    flexDirection: 'row',
    alignItems: 'center',
    shadowColor: COLORS.shadow,
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  severityIndicator: {
    width: 6,
    height: '100%',
    borderRadius: 3,
    marginRight: 15,
  },
  itemContent: {
    flex: 1,
  },
  itemType: {
    fontSize: 16,
    fontWeight: 'bold',
    textTransform: 'capitalize',
  },
  itemDevice: {
    color: COLORS.textLight,
    marginTop: 2,
  },
  itemTime: {
    marginTop: 4,
    color: COLORS.secondary,
    fontSize: 12,
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  noAlertsText: {
    fontSize: 16,
    color: COLORS.textLight,
  }
});

export default AlertsScreen;
