import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, FlatList, TouchableOpacity, ActivityIndicator } from 'react-native';
import { onValue, ref } from 'firebase/database';
import { database } from '@/utils/firebase';
import { COLORS } from '@/constants/colors';
import { Ionicons } from '@expo/vector-icons';

const WeaponAlertsScreen = ({ navigation }) => {
  const [alerts, setAlerts] = useState([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const alertsRef = ref(database, '/emergency-alerts');
    const unsubscribe = onValue(alertsRef, (snapshot) => {
      const data = snapshot.val();
      if (data) {
        const alertsList = Object.keys(data).map(key => ({
          id: key,
          ...data[key]
        })).sort((a, b) => b.timestamp - a.timestamp);
        setAlerts(alertsList);
      } else {
        setAlerts([]);
      }
      setLoading(false);
    });

    return () => unsubscribe();
  }, []);

  const getSeverityColor = (severity) => {
    switch (severity?.toUpperCase()) {
      case 'CRITICAL': return COLORS.danger;
      case 'HIGH': return COLORS.warning;
      default: return COLORS.secondary;
    }
  };

  const renderItem = ({ item }) => (
    <TouchableOpacity style={styles.itemContainer} onPress={() => navigation.navigate('AlertDetail', { alert: item })}>
      <View style={[styles.severityIndicator, { backgroundColor: getSeverityColor(item.severity) }]} />
      <View style={styles.itemContent}>
        <Text style={styles.itemType}>{item.alertType.replace('_', ' ')}</Text>
        <Text style={styles.itemDevice}>Device: {item.deviceId}</Text>
        <Text style={styles.itemTime}>{new Date(item.timestamp).toLocaleString()}</Text>
      </View>
      {item.imageData?.base64 && <Ionicons name="camera" size={24} color={COLORS.textLight} style={{ marginRight: 10 }} />}
      <Ionicons name="chevron-forward" size={24} color={COLORS.textLight} />
    </TouchableOpacity>
  );

  if (loading) {
    return (
      <View style={styles.centered}>
        <ActivityIndicator size="large" color={COLORS.primary} />
        <Text>Loading Weapon Alerts...</Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Weapon Detections</Text>
      {alerts.length > 0 ? (
        <FlatList
          data={alerts}
          renderItem={renderItem}
          keyExtractor={item => item.id}
          contentContainerStyle={styles.list}
        />
      ) : (
        <View style={styles.centered}>
          <Text style={styles.noAlertsText}>No weapon alerts detected.</Text>
        </View>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: COLORS.background },
  title: { fontSize: 28, fontWeight: 'bold', padding: 16, paddingBottom: 10 },
  list: { paddingHorizontal: 16 },
  itemContainer: { backgroundColor: COLORS.white, borderRadius: 12, padding: 15, marginBottom: 10, flexDirection: 'row', alignItems: 'center', shadowColor: COLORS.shadow, shadowOffset: { width: 0, height: 2 }, shadowOpacity: 0.1, shadowRadius: 4, elevation: 3 },
  severityIndicator: { width: 6, height: '100%', borderRadius: 3, marginRight: 15 },
  itemContent: { flex: 1 },
  itemType: { fontSize: 16, fontWeight: 'bold', textTransform: 'capitalize' },
  itemDevice: { color: COLORS.textLight, marginTop: 2 },
  itemTime: { marginTop: 4, color: COLORS.secondary, fontSize: 12 },
  centered: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  noAlertsText: { fontSize: 16, color: COLORS.textLight }
});

export default WeaponAlertsScreen;
