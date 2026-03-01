import React from 'react';
import { View, Text, StyleSheet, ScrollView, Image, Linking, TouchableOpacity } from 'react-native';
import MapView, { Marker } from 'react-native-maps';
import { COLORS } from '@/constants/colors';
import { Ionicons } from '@expo/vector-icons';
import { Platform } from 'react-native';
import Constants from 'expo-constants';

const AlertDetailScreen = ({ route }) => {
  const { alert } = route.params;

  const toNumberOrNull = (v) => {
    const n = typeof v === 'string' ? Number(v) : v;
    return Number.isFinite(n) ? n : null;
  };

  const formatBoolLike = (v) => {
    if (v === true || v === 1 || v === '1' || v === 'true') return 'true';
    if (v === false || v === 0 || v === '0' || v === 'false') return 'false';
    return 'Unknown';
  };

  const formatAgeMs = (v) => {
    const n = toNumberOrNull(v);
    if (n === null) return 'Unknown';
    return `${Math.round(n)} ms`;
  };

  const latNum = toNumberOrNull(alert.location?.lat);
  const lngNum = toNumberOrNull(alert.location?.lng);
  const hasCoords =
    latNum !== null &&
    lngNum !== null &&
    !(latNum === 0 && lngNum === 0);

  const hasAndroidMapsApiKey = (() => {
    if (Platform.OS !== 'android') return true;
    const expoConfig = Constants?.expoConfig;
    const key = expoConfig?.android?.config?.googleMaps?.apiKey;
    return typeof key === 'string' && key.trim().length > 0;
  })();

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

  const openInMaps = () => {
    const url = alert.location?.mapsLink;
    if (url) {
      Linking.openURL(url);
    }
  };

  const nearbyType =
    alert.nearbyType ??
    alert.nearbyServiceType ??
    (alert.nearbyMedical || alert.medicalPhone ? 'Medical' : null);

  const nearbyName =
    alert.nearbyName ??
    alert.nearbyServiceName ??
    alert.nearbyMedical ??
    alert.nearbyMedicalName ??
    alert.medical?.name ??
    null;

  const nearbyPhone =
    alert.nearbyPhone ??
    alert.nearbyServicePhone ??
    alert.medicalPhone ??
    alert.nearbyMedicalPhone ??
    alert.medical?.phone ??
    null;

  const contactValue =
    alert.contact ??
    alert.contactPhone ??
    alert.contactNumber ??
    null;

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>{String(alert?.alertType ?? 'ALERT').replace(/_/g, ' ')}</Text>
        <View style={[styles.severityBadge, { backgroundColor: getSeverityColor(alert.severity) }]}>
          <Text style={styles.severityText}>{alert.severity}</Text>
        </View>
      </View>

      {alert.imageData && alert.imageData.base64 && alert.imageData.base64.length > 100 ? (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Captured Image</Text>
          <Image
            source={{ uri: `data:image/jpeg;base64,${alert.imageData.base64}` }}
            style={styles.image}
            resizeMode="contain"
          />
        </View>
      ) : (
        <View style={styles.card}>
            <Text style={styles.cardTitle}>No Image Captured</Text>
        </View>
      )}


      <View style={styles.card}>
        <Text style={styles.cardTitle}>Alert Details</Text>
        <InfoRow icon="time-outline" label="Timestamp" value={formatTimestamp(alert.timestampMs ?? alert.timestamp)} />
        <InfoRow icon="hardware-chip-outline" label="Device ID" value={alert.deviceId} />
        {nearbyName != null && (
          <InfoRow
            icon="business-outline"
            label={`Nearby ${nearbyType ?? 'Service'}`}
            value={String(nearbyName)}
          />
        )}
        {nearbyPhone != null && (
          <InfoRow
            icon="call-outline"
            label={`${nearbyType ?? 'Service'} Phone`}
            value={String(nearbyPhone)}
          />
        )}
        {contactValue != null && (
          <InfoRow icon="call-outline" label="Contact" value={String(contactValue)} />
        )}
        {alert.description && <InfoRow icon="information-circle-outline" label="Description" value={alert.description} />}
      </View>

      {hasCoords && (
        <View style={styles.card}>
            <Text style={styles.cardTitle}>Location</Text>
            <InfoRow icon="navigate-outline" label="Latitude" value={String(latNum)} />
            <InfoRow icon="navigate-outline" label="Longitude" value={String(lngNum)} />
            {alert.location?.source && <InfoRow icon="compass-outline" label="Source" value={String(alert.location.source)} />}
            {'gpsActive' in (alert.location ?? {}) && (
              <InfoRow icon="locate-outline" label="GPS Active" value={formatBoolLike(alert.location?.gpsActive)} />
            )}
            {'gpsFixTimestampMs' in (alert.location ?? {}) && (
              <InfoRow
                icon="time-outline"
                label="GPS Fix Time"
                value={formatTimestamp(alert.location?.gpsFixTimestampMs)}
              />
            )}
            {'ageMs' in (alert.location ?? {}) && (
              <InfoRow icon="speedometer-outline" label="GPS Age" value={formatAgeMs(alert.location?.ageMs)} />
            )}
            {hasAndroidMapsApiKey ? (
              <MapView
                style={styles.map}
                initialRegion={{
                    latitude: latNum,
                    longitude: lngNum,
                    latitudeDelta: 0.01,
                    longitudeDelta: 0.01,
                }}
                scrollEnabled={false}
              >
                <Marker coordinate={{ latitude: latNum, longitude: lngNum }} />
              </MapView>
            ) : (
              <View style={styles.mapsErrorBox}>
                <Text style={styles.mapsErrorText}>Google Maps not configured for Android build.</Text>
                <Text style={styles.mapsErrorText}>
                  Add EAS env var `GOOGLE_MAPS_API_KEY` with visibility `sensitive` (or `plaintext`) and rebuild.
                </Text>
              </View>
            )}
            <TouchableOpacity style={styles.mapLink} onPress={openInMaps}>
            <Text style={styles.mapLinkText}>Open in Google Maps</Text>
            <Ionicons name="open-outline" size={16} color={COLORS.primary} />
            </TouchableOpacity>
        </View>
      )}

      

    </ScrollView>
  );
};

const InfoRow = ({ icon, label, value }) => (
  <View style={styles.infoRow}>
    <Ionicons name={icon} size={20} color={COLORS.primary} style={styles.icon} />
    <Text style={styles.label}>{label}:</Text>
    <Text style={styles.value}>{value}</Text>
  </View>
);

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
  },
  header: {
    padding: 20,
    backgroundColor: COLORS.white,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
  },
  title: {
    fontSize: 28,
    fontWeight: 'bold',
    textTransform: 'capitalize',
  },
  severityBadge: {
    position: 'absolute',
    top: 20,
    right: 20,
    paddingHorizontal: 12,
    paddingVertical: 5,
    borderRadius: 15,
  },
  severityText: {
    color: COLORS.white,
    fontWeight: 'bold',
    fontSize: 12,
  },
  card: {
    backgroundColor: COLORS.white,
    margin: 15,
    borderRadius: 12,
    padding: 20,
  },
  cardTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    marginBottom: 15,
  },
  image: {
    width: '100%',
    height: 250,
    borderRadius: 8,
    backgroundColor: COLORS.lightGray,
  },
  infoRow: {
    flexDirection: 'row',
    alignItems: 'flex-start',
    marginBottom: 12,
  },
  icon: {
    marginRight: 10,
    marginTop: 2,
  },
  label: {
    fontWeight: '600',
    fontSize: 16,
  },
  value: {
    marginLeft: 8,
    fontSize: 16,
    color: COLORS.text,
    flex: 1,
  },
  map: {
    height: 200,
    borderRadius: 8,
  },
  mapLink: {
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'center',
      paddingTop: 10,
  },
  mapLinkText: {
      color: COLORS.primary,
      fontWeight: '600',
      marginRight: 5,
  },
  mapsErrorBox: {
    backgroundColor: COLORS.background,
    borderRadius: 8,
    padding: 12,
  },
  mapsErrorText: {
    color: COLORS.textLight,
    textAlign: 'center',
  },
});

export default AlertDetailScreen;
