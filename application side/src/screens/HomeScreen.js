import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity, ScrollView } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { COLORS } from '@/constants/colors';

const HomeScreen = ({ navigation }) => {
  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.headerTitle}>Dashboard</Text>
        <TouchableOpacity onPress={() => navigation.navigate('Profile')}>
          <Ionicons name="person-circle-outline" size={30} color={COLORS.primary} />
        </TouchableOpacity>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Device Status</Text>
        <View style={styles.statusRow}>
          <Ionicons name="location-outline" size={20} color={COLORS.success} />
          <Text style={styles.statusText}>Location: Live</Text>
        </View>
        <View style={styles.statusRow}>
          <Ionicons name="battery-half-outline" size={20} color={COLORS.warning} />
          <Text style={styles.statusText}>Battery: 75%</Text>
        </View>
        <TouchableOpacity style={styles.detailsButton} onPress={() => navigation.navigate('Dashboard')}>
          <Text style={styles.detailsButtonText}>View Full Dashboard</Text>
        </TouchableOpacity>
      </View>

      <Text style={styles.sectionTitle}>Quick Actions</Text>
      <View style={styles.quickActionsContainer}>
        <TouchableOpacity style={styles.actionButton} onPress={() => navigation.navigate('Map')}>
          <Ionicons name="map-outline" size={30} color={COLORS.primary} />
          <Text style={styles.actionText}>Alerts Map</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.actionButton} onPress={() => navigation.navigate('ReportIncident')}>
          <Ionicons name="megaphone-outline" size={30} color={COLORS.primary} />
          <Text style={styles.actionText}>Report Incident</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.actionButton} onPress={() => navigation.navigate('AdminNotices')}>
          <Ionicons name="reader-outline" size={30} color={COLORS.primary} />
          <Text style={styles.actionText}>Govt. Notices</Text>
        </TouchableOpacity>
      </View>
      
      <Text style={styles.sectionTitle}>Recent Alerts</Text>
      {/* Placeholder for recent alerts list */}
      <View style={styles.alertItem}>
        <Ionicons name="warning-outline" size={24} color={COLORS.danger} />
        <View style={styles.alertTextContainer}>
          <Text style={styles.alertType}>Accident Reported</Text>
          <Text style={styles.alertLocation}>Highway A, near exit 4</Text>
        </View>
      </View>

    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
    padding: 16,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 20,
  },
  headerTitle: {
    fontSize: 28,
    fontWeight: 'bold',
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
  statusRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 10,
  },
  statusText: {
    marginLeft: 10,
    fontSize: 16,
  },
  detailsButton: {
    marginTop: 15,
    backgroundColor: COLORS.lightGray,
    padding: 10,
    borderRadius: 8,
    alignItems: 'center',
  },
  detailsButtonText: {
    color: COLORS.primary,
    fontWeight: '600',
  },
  sectionTitle: {
    fontSize: 20,
    fontWeight: 'bold',
    marginBottom: 15,
  },
  quickActionsContainer: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    marginBottom: 20,
  },
  actionButton: {
    alignItems: 'center',
    padding: 10,
  },
  actionText: {
    marginTop: 5,
    fontWeight: '600',
    color: COLORS.text,
  },
  alertItem: {
    backgroundColor: COLORS.white,
    borderRadius: 12,
    padding: 15,
    flexDirection: 'row',
    alignItems: 'center',
  },
  alertTextContainer: {
    marginLeft: 15,
  },
  alertType: {
    fontSize: 16,
    fontWeight: 'bold',
  },
  alertLocation: {
    color: COLORS.textLight,
  },
});

export default HomeScreen;
