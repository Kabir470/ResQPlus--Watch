import React, { useState, useEffect, useRef, useMemo } from 'react';
import { View, StyleSheet, Text, ActivityIndicator, TextInput, TouchableOpacity, Platform } from 'react-native';
import MapView, { Marker, Callout } from 'react-native-maps';
import { onValue, ref } from 'firebase/database';
import { database } from '@/utils/firebase';
import { COLORS } from '@/constants/colors';
import * as Location from 'expo-location';
import { Ionicons } from '@expo/vector-icons';
import Constants from 'expo-constants';

const MapScreen = ({ navigation }) => {
  const [allAlerts, setAllAlerts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [userLocation, setUserLocation] = useState(null);
  const [mapType, setMapType] = useState('standard');
  const [selectedAlert, setSelectedAlert] = useState(null);
  const mapViewRef = useRef(null);
  const markerRefs = useRef({});

  const hasAndroidMapsApiKey = useMemo(() => {
    if (Platform.OS !== 'android') return true;
    const expoConfig = Constants?.expoConfig;
    const key = expoConfig?.android?.config?.googleMaps?.apiKey;
    return typeof key === 'string' && key.trim().length > 0;
  }, []);

  useEffect(() => {
    // Request location permissions
    (async () => {
      let { status } = await Location.requestForegroundPermissionsAsync();
      if (status !== 'granted') {
        console.log('Permission to access location was denied');
        return;
      }
      let location = await Location.getCurrentPositionAsync({});
      setUserLocation(location.coords);
    })();

    const deviceAlertsRef = ref(database, '/device-alerts');

    const unsubscribe = onValue(deviceAlertsRef, (snapshot) => {
      const data = snapshot.val() || {};
      const list = Object.keys(data).map(key => ({ id: key, ...data[key] }));
      setAllAlerts(list); // Directly set the alerts from the single source
      setLoading(false);
    });

    return () => {
      unsubscribe();
    };
  }, []);

  const filteredAlerts = useMemo(() => {
    if (!searchQuery) {
      return allAlerts;
    }
    return allAlerts.filter(alert =>
      String(alert?.alertType ?? '').toLowerCase().includes(searchQuery.toLowerCase()) ||
      String(alert?.deviceId ?? '').toLowerCase().includes(searchQuery.toLowerCase())
    );
  }, [allAlerts, searchQuery]);

  const coerceNumber = (value) => {
    const n = typeof value === 'number' ? value : Number.parseFloat(String(value));
    return Number.isFinite(n) ? n : null;
  };
  
  const zoomIn = () => {
    mapViewRef.current?.getCamera().then(cam => {
      cam.altitude /= 2;
      mapViewRef.current?.animateCamera(cam);
    });
  };

  const zoomOut = () => {
    mapViewRef.current?.getCamera().then(cam => {
      cam.altitude *= 2;
      mapViewRef.current?.animateCamera(cam);
    });
  };

  const centerOnUser = () => {
    if (userLocation) {
      mapViewRef.current?.animateToRegion({
        ...userLocation,
        latitudeDelta: 0.05,
        longitudeDelta: 0.05,
      });
    }
  };

  const toggleMapType = () => {
    setMapType(prev => (prev === 'standard' ? 'satellite' : 'standard'));
  };

  const getMarkerColor = (severity) => {
    switch (severity?.toUpperCase()) {
      case 'CRITICAL':
        return COLORS.danger;
      case 'HIGH':
        return COLORS.warning;
      default:
        return COLORS.secondary;
    }
  };

  const formatAlertType = (value) => String(value ?? '').replace(/_/g, ' ').trim();

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
    if (!ms) return '';
    return new Date(ms).toLocaleString();
  };

  const getNearbyType = (alert) =>
    alert?.nearbyType ??
    alert?.nearbyServiceType ??
    (alert?.nearbyMedical || alert?.medicalPhone ? 'Medical' : null);

  const getNearbyName = (alert) =>
    alert?.nearbyName ??
    alert?.nearbyServiceName ??
    alert?.nearbyMedical ??
    alert?.nearbyMedicalName ??
    alert?.medical?.name ??
    null;

  const getNearbyPhone = (alert) =>
    alert?.nearbyPhone ??
    alert?.nearbyServicePhone ??
    alert?.medicalPhone ??
    alert?.nearbyMedicalPhone ??
    alert?.medical?.phone ??
    null;

  const getContact = (alert) =>
    alert?.contact ??
    alert?.contactPhone ??
    alert?.contactNumber ??
    null;

  const formatBoolLike = (v) => {
    if (v === true || v === 1 || v === '1' || v === 'true') return 'true';
    if (v === false || v === 0 || v === '0' || v === 'false') return 'false';
    return null;
  };

  const getPopupLines = (alert) => {
    const nearbyType = getNearbyType(alert);
    const nearbyName = getNearbyName(alert);
    const nearbyPhone = getNearbyPhone(alert);
    const contact = getContact(alert);
    const gpsActive = formatBoolLike(alert?.location?.gpsActive);
    const ageMsRaw = alert?.location?.ageMs;
    const ageMs = Number.isFinite(Number(ageMsRaw)) ? `${Math.round(Number(ageMsRaw))} ms` : null;
    const ts = formatTimestamp(alert?.timestampMs ?? alert?.timestamp);

    return [
      { label: 'Severity', value: alert?.severity ?? null },
      { label: 'Device', value: alert?.deviceId ?? null },
      { label: 'Timestamp', value: ts || null },
      nearbyName ? { label: `Nearby ${nearbyType ?? 'Service'}`, value: nearbyName } : null,
      nearbyPhone ? { label: `${nearbyType ?? 'Service'} Phone`, value: nearbyPhone } : null,
      contact ? { label: 'Contact', value: contact } : null,
      gpsActive ? { label: 'GPS Active', value: gpsActive } : null,
      ageMs ? { label: 'GPS Age', value: ageMs } : null,
    ].filter(Boolean);
  };

  if (loading) {
    return (
      <View style={styles.centered}>
        <ActivityIndicator size="large" color={COLORS.primary} />
        <Text>Loading Map Data...</Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      {hasAndroidMapsApiKey ? (
        <MapView
          ref={mapViewRef}
          style={styles.map}
          mapType={mapType}
          initialRegion={{
            latitude: 23.8103, // Centered on Dhaka
            longitude: 90.4125,
            latitudeDelta: 0.5,
            longitudeDelta: 0.5,
          }}
          showsUserLocation={true}
          onPress={() => setSelectedAlert(null)}
        >
          {filteredAlerts
            .map(alert => {
              const latitude = coerceNumber(alert?.location?.lat);
              const longitude = coerceNumber(alert?.location?.lng);
              if (latitude == null || longitude == null) return null;

              return (
                <Marker
                  key={alert.id}
                  ref={(r) => {
                    if (r) markerRefs.current[alert.id] = r;
                  }}
                  coordinate={{ latitude, longitude }}
                  pinColor={getMarkerColor(alert.severity)}
                  onPress={() => {
                    if (Platform.OS === 'android') {
                      setSelectedAlert(alert);
                      return;
                    }

                    // iOS can be flaky about opening callouts automatically.
                    const marker = markerRefs.current[alert.id];
                    requestAnimationFrame(() => marker?.showCallout?.());
                  }}
                >
                  {Platform.OS === 'ios' ? (
                    <Callout>
                      <View style={styles.calloutContainer}>
                        <Text style={styles.calloutTitle}>{formatAlertType(alert.alertType)}</Text>
                        {getPopupLines(alert).map((row) => (
                          <Text key={row.label}>
                            {row.label}: {String(row.value)}
                          </Text>
                        ))}
                      </View>
                    </Callout>
                  ) : null}
                </Marker>
              );
            })}
        </MapView>
      ) : (
        <View style={styles.centered}>
          <Text style={styles.mapsErrorTitle}>Google Maps not configured</Text>
          <Text style={styles.mapsErrorText}>
            This APK is missing an Android Google Maps API key, so opening the map crashes.
          </Text>
          <Text style={styles.mapsErrorText}>
            Create an EAS env var `GOOGLE_MAPS_API_KEY` with visibility `sensitive` (or `plaintext`) and rebuild.
          </Text>
        </View>
      )}

      {Platform.OS === 'android' && selectedAlert ? (
        <View style={styles.androidPopupWrap} pointerEvents="box-none">
          <View style={styles.androidPopupCard}>
            <View style={styles.androidPopupHeader}>
              <Text style={styles.androidPopupTitle} numberOfLines={2}>
                {formatAlertType(selectedAlert.alertType)}
              </Text>
              <TouchableOpacity
                onPress={() => setSelectedAlert(null)}
                accessibilityLabel="Close"
                style={styles.androidPopupClose}
              >
                <Ionicons name="close" size={18} color={COLORS.text} />
              </TouchableOpacity>
            </View>

            {getPopupLines(selectedAlert).map((row) => (
              <Text key={row.label} style={styles.androidPopupLine}>
                <Text style={styles.androidPopupLabel}>{row.label}: </Text>
                {String(row.value)}
              </Text>
            ))}

            <TouchableOpacity
              style={styles.androidPopupLink}
              onPress={() => {
                setSelectedAlert(null);
                navigation.navigate('AlertDetail', { alert: selectedAlert });
              }}
            >
              <Text style={styles.androidPopupLinkText}>Open Alert Details</Text>
              <Ionicons name="chevron-forward" size={16} color={COLORS.primary} />
            </TouchableOpacity>
          </View>
        </View>
      ) : null}

      <View style={styles.searchContainer}>
        <Ionicons name="search" size={20} color={COLORS.textLight} />
        <TextInput
          style={styles.searchInput}
          placeholder="Filter by type or device ID..."
          value={searchQuery}
          onChangeText={setSearchQuery}
        />
      </View>

      <View style={styles.mapControls}>
        <TouchableOpacity style={styles.controlButton} onPress={toggleMapType}>
          <Ionicons name={mapType === 'standard' ? 'map-outline' : 'planet-outline'} size={24} color={COLORS.primary} />
        </TouchableOpacity>
        <TouchableOpacity style={styles.controlButton} onPress={centerOnUser}>
          <Ionicons name="locate-outline" size={24} color={COLORS.primary} />
        </TouchableOpacity>
        <TouchableOpacity style={styles.controlButton} onPress={zoomIn}>
          <Ionicons name="add-outline" size={24} color={COLORS.primary} />
        </TouchableOpacity>
        <TouchableOpacity style={styles.controlButton} onPress={zoomOut}>
          <Ionicons name="remove-outline" size={24} color={COLORS.primary} />
        </TouchableOpacity>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  map: {
    ...StyleSheet.absoluteFillObject,
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  mapsErrorTitle: {
    fontWeight: 'bold',
    fontSize: 16,
    marginBottom: 6,
    color: COLORS.text,
  },
  mapsErrorText: {
    textAlign: 'center',
    color: COLORS.textLight,
    paddingHorizontal: 24,
    marginBottom: 4,
  },
  searchContainer: {
    position: 'absolute',
    top: 50,
    left: 20,
    right: 20,
    backgroundColor: 'white',
    borderRadius: 12,
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 15,
    elevation: 5,
  },
  searchInput: {
    flex: 1,
    height: 50,
    marginLeft: 10,
  },
  mapControls: {
    position: 'absolute',
    top: 120,
    right: 20,
    alignItems: 'center',
  },
  controlButton: {
    backgroundColor: 'white',
    borderRadius: 30,
    padding: 10,
    marginBottom: 10,
    elevation: 5,
  },
  calloutContainer: {
    width: 200,
    padding: 5,
  },
  calloutTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    textTransform: 'capitalize',
  },
  androidPopupWrap: {
    position: 'absolute',
    left: 15,
    right: 15,
    bottom: 90,
  },
  androidPopupCard: {
    backgroundColor: 'white',
    borderRadius: 12,
    padding: 12,
    elevation: 5,
  },
  androidPopupHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    marginBottom: 8,
  },
  androidPopupTitle: {
    fontSize: 14,
    fontWeight: 'bold',
    flex: 1,
    marginRight: 10,
  },
  androidPopupClose: {
    width: 28,
    height: 28,
    borderRadius: 14,
    alignItems: 'center',
    justifyContent: 'center',
  },
  androidPopupLine: {
    color: COLORS.text,
    marginBottom: 4,
  },
  androidPopupLabel: {
    fontWeight: '600',
  },
  androidPopupLink: {
    marginTop: 6,
    paddingTop: 8,
    borderTopWidth: 1,
    borderTopColor: COLORS.border,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
  },
  androidPopupLinkText: {
    color: COLORS.primary,
    fontWeight: '600',
  },
});

export default MapScreen;
