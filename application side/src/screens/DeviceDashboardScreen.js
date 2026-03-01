import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView, ActivityIndicator, TouchableOpacity } from 'react-native';
import { onValue, ref, query, orderByChild, limitToLast, equalTo } from 'firebase/database';
import { database } from '@/utils/firebase';
import { COLORS } from '@/constants/colors';
import { Ionicons } from '@expo/vector-icons';

const DEVICE_ID = "ESP32_IOT_002"; // The device we are monitoring

const DeviceDashboardScreen = ({ navigation }) => {
  const [latestAlert, setLatestAlert] = useState(null);
  const [loading, setLoading] = useState(true);

  const toNumberOrNull = (v) => {
    const n = typeof v === 'string' ? Number(v) : v;
    return Number.isFinite(n) ? n : null;
  };

  const normalizeTimestampMs = (raw) => {
    const n = typeof raw === 'string' ? Number(raw) : raw;
    if (!Number.isFinite(n) || n <= 0) return null;
    let ms = n;
    if (ms < 100000000000) ms = ms * 1000;
    const now = Date.now();
    const minOk = 946684800000; // Jan 1, 2000
    const maxOk = now + 7 * 24 * 60 * 60 * 1000;
    if (ms < minOk || ms > maxOk) return null;
    return ms;
  };

  const formatTimestamp = (raw) => {
    const ms = normalizeTimestampMs(raw);
    if (!ms) return 'Unknown';
    return new Date(ms).toLocaleString();
  };

  const formatNumFixed = (raw, digits) => {
    const n = toNumberOrNull(raw);
    if (n === null) return 'N/A';
    return n.toFixed(digits);
  };

  useEffect(() => {
    // Query to get the most recent alert for our specific device
    const alertsRef = query(
      ref(database, '/device-alerts'),
      orderByChild('deviceId'),
      equalTo(DEVICE_ID),
      limitToLast(1)
    );

    const unsubscribe = onValue(alertsRef, (snapshot) => {
      const data = snapshot.val();
      if (data) {
        const key = Object.keys(data)[0];
        setLatestAlert({ id: key, ...data[key] });
      } else {
        setLatestAlert(null);
      }
      setLoading(false);
    });

    return () => unsubscribe();
  }, []);

  if (loading) {
    return (
      <View style={styles.centered}>
        <ActivityIndicator size="large" color={COLORS.primary} />
        <Text>Loading Device Data...</Text>
      </View>
    );
  }

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>Device Dashboard</Text>
      <Text style={styles.subtitle}>Live status for device: {DEVICE_ID}</Text>

      {latestAlert ? (
        <>
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Last Known Status</Text>
            <InfoRow icon="alert-circle-outline" label="Last Event" value={(latestAlert.alertType ?? 'Unknown').replace('_', ' ')} />
            <InfoRow icon="pulse-outline" label="Severity" value={latestAlert.severity ?? 'Unknown'} />
            <InfoRow icon="time-outline" label="Last Update" value={formatTimestamp(latestAlert.timestampMs ?? latestAlert.timestamp)} />
            <InfoRow
              icon="location-outline"
              label="Location"
              value={() => {
                const lat = toNumberOrNull(latestAlert.location?.lat);
                const lng = toNumberOrNull(latestAlert.location?.lng);
                if (lat === null || lng === null) return 'Unknown';
                return `${lat.toFixed(4)}, ${lng.toFixed(4)}`;
              }}
            />
          </View>

          {latestAlert.mpu && (
            <View style={styles.card}>
              <Text style={styles.cardTitle}>MPU6050 Sensor Data</Text>
              <Text style={styles.sensorSubTitle}>Accelerometer (m/s²)</Text>
              <SensorRow label="X-Axis" value={formatNumFixed(latestAlert.mpu?.accX, 2)} />
              <SensorRow label="Y-Axis" value={formatNumFixed(latestAlert.mpu?.accY, 2)} />
              <SensorRow label="Z-Axis" value={formatNumFixed(latestAlert.mpu?.accZ, 2)} />
              <Text style={styles.sensorSubTitle}>Gyroscope (°/s)</Text>
              <SensorRow label="X-Axis" value={formatNumFixed(latestAlert.mpu?.gyroX, 2)} />
              <SensorRow label="Y-Axis" value={formatNumFixed(latestAlert.mpu?.gyroY, 2)} />
              <SensorRow label="Z-Axis" value={formatNumFixed(latestAlert.mpu?.gyroZ, 2)} />
            </View>
          )}
        </>
      ) : (
        <View style={styles.card}>
           <Text style={styles.noDataText}>No data received from this device yet.</Text>
        </View>
      )}

      <TouchableOpacity style={styles.fullWidthButton} onPress={() => navigation.navigate('Alerts')}>
        <Ionicons name="archive-outline" size={22} color={COLORS.primary} />
        <Text style={styles.buttonText}>View Full Alert History</Text>
      </TouchableOpacity>
    </ScrollView>
  );
};

// Helper components for displaying rows
const InfoRow = ({ icon, label, value }) => (
  <View style={styles.infoRow}>
    <Ionicons name={icon} size={20} color={COLORS.primary} style={styles.icon} />
    <Text style={styles.label}>{label}:</Text>
    <Text style={styles.value}>{typeof value === 'function' ? value() : value}</Text>
  </View>
);

const SensorRow = ({ label, value }) => (
    <View style={styles.sensorRow}>
        <Text style={styles.sensorLabel}>{label}:</Text>
        <Text style={styles.sensorValue}>{value}</Text>
    </View>
);

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
    padding: 16,
  },
  title: {
    fontSize: 28,
    fontWeight: 'bold',
  },
  subtitle: {
    fontSize: 14,
    color: COLORS.textLight,
    marginBottom: 20,
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  card: {
    backgroundColor: COLORS.white,
    borderRadius: 12,
    padding: 20,
    marginBottom: 20,
    shadowColor: COLORS.shadow,
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  cardTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    marginBottom: 15,
  },
  infoRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 10,
  },
  icon: {
    marginRight: 10,
  },
  label: {
    fontWeight: '600',
    fontSize: 16,
  },
  value: {
    marginLeft: 8,
    fontSize: 16,
    color: COLORS.textLight,
  },
  sensorSubTitle: {
    fontSize: 16,
    fontWeight: '600',
    marginTop: 10,
    marginBottom: 5,
    borderTopWidth: 1,
    borderTopColor: COLORS.border,
    paddingTop: 10,
  },
  sensorRow: {
      flexDirection: 'row',
      justifyContent: 'space-between',
      paddingVertical: 4,
      paddingHorizontal: 10,
  },
  sensorLabel: {
      fontSize: 15,
      color: COLORS.text,
  },
  sensorValue: {
      fontSize: 15,
      color: COLORS.primary,
      fontWeight: 'bold',
  },
  noDataText: {
    textAlign: 'center',
    color: COLORS.textLight,
    fontSize: 16,
    padding: 20,
  },
  fullWidthButton: {
    backgroundColor: COLORS.white,
    borderRadius: 12,
    padding: 15,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    borderWidth: 1,
    borderColor: COLORS.primary,
  },
  buttonText: {
    color: COLORS.primary,
    fontWeight: '600',
    marginLeft: 10,
    fontSize: 16,
  }
});

export default DeviceDashboardScreen;
