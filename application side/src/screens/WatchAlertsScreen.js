import React, { useState, useEffect, useRef, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
  FlatList,
  Vibration,
  Platform,
  ActivityIndicator,
  Alert,
  Modal,
  SafeAreaView,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { COLORS } from '@/constants/colors';
import {
  getBleManager,
  requestBlePermissions,
  scanAllDevices,
  stopScan,
  connectAndSubscribe,
  disconnectWatch,
  sendTimeToWatch,
  isBleAvailable,
} from '@/utils/bleService';

// ── Helpers ────────────────────────────────────────────────────────────────────
const timestamp = () => new Date().toLocaleTimeString();

// ── Component ──────────────────────────────────────────────────────────────────
const WatchAlertsScreen = () => {
  // Connection state
  const [status, setStatus] = useState('disconnected'); // disconnected | scanning | connecting | connected
  const [deviceName, setDeviceName] = useState(null);

  // Device picker
  const [pickerVisible, setPickerVisible] = useState(false);
  const [foundDevices, setFoundDevices] = useState([]); // [{id, name, rssi, raw}]
  const stopScanRef = useRef(null);

  // Alerts state
  const [alertCount, setAlertCount] = useState(0);
  const [alertLog, setAlertLog] = useState([]); // [{id, time, message}]

  // Refs to survive re-renders
  const deviceRef = useRef(null);
  const subRef = useRef(null);

  // ── Cleanup on unmount ─────────────────────────────────────────────────────
  useEffect(() => {
    return () => {
      stopScanRef.current?.();
      subRef.current?.remove();
      if (deviceRef.current) disconnectWatch(deviceRef.current);
    };
  }, []);

  // ── BLE state listener (adapter on/off) ────────────────────────────────────
  useEffect(() => {
    if (!isBleAvailable) return;
    let stateSubscription;
    try {
      const ble = getBleManager();
      stateSubscription = ble.onStateChange((state) => {
        if (state === 'PoweredOff') {
          setStatus('disconnected');
          setDeviceName(null);
          closePicker();
        }
      }, true);
    } catch {
      // native module missing – ignore
    }
    return () => stateSubscription?.remove();
  }, []);

  // ── Handle incoming alert from the watch ───────────────────────────────────
  const handleAlert = useCallback((value) => {
    // Check for reset command from watch (e.g. "Alert:0")
    if (value && value.includes('Alert:0')) {
      setAlertCount(0);
      setAlertLog([]);
      return;
    }

    // Parse count from "Alert:N" format if present
    let count = null;
    const match = value && value.match(/Alert:(\d+)/);
    if (match) {
      count = parseInt(match[1], 10);
    }

    if (count !== null) {
      setAlertCount(count);
    } else {
      setAlertCount((prev) => prev + 1);
    }
    setAlertLog((prev) => [
      { id: String(Date.now()), time: timestamp(), message: value || 'Alert' },
      ...prev,
    ]);
    Vibration.vibrate(Platform.OS === 'android' ? [0, 300, 100, 300] : 400);
  }, []);

  // ── Open device picker & start scanning ───────────────────────────────────
  const openPicker = useCallback(async () => {
    try {
      const granted = await requestBlePermissions();
      if (!granted) {
        Alert.alert('Permission Denied', 'Bluetooth permissions are required.');
        return;
      }

      const ble = getBleManager();
      const state = await ble.state();
      if (state !== 'PoweredOn') {
        Alert.alert('Bluetooth Off', 'Please turn on Bluetooth first.');
        return;
      }

      setFoundDevices([]);
      setPickerVisible(true);
      setStatus('scanning');

      const stop = scanAllDevices(
        (device) => {
          setFoundDevices((prev) => {
            // Avoid duplicates (shouldn't happen but be safe)
            if (prev.some((d) => d.id === device.id)) return prev;
            return [...prev, device];
          });
        },
        (error) => {
          console.warn('[BLE] scan error', error.message);
        },
      );
      stopScanRef.current = stop;

      // Auto-stop scan after 30 seconds
      setTimeout(() => {
        stop();
      }, 30000);
    } catch (err) {
      Alert.alert('Error', err.message);
    }
  }, []);

  // ── Close picker & stop scan ──────────────────────────────────────────────
  const closePicker = useCallback(() => {
    stopScanRef.current?.();
    stopScanRef.current = null;
    setPickerVisible(false);
    if (status === 'scanning') setStatus('disconnected');
  }, [status]);

  // ── User selects a device from the picker ─────────────────────────────────
  const handleSelectDevice = useCallback(async (device) => {
    // Stop scanning
    stopScanRef.current?.();
    stopScanRef.current = null;
    setPickerVisible(false);
    setStatus('connecting');
    setDeviceName(device.name || device.id);

    try {
      const { device: connected, subscription } = await connectAndSubscribe(
        device.raw,
        handleAlert,
      );
      deviceRef.current = connected;
      subRef.current = subscription;

      // Send current time to the watch so it can display the correct clock
      await sendTimeToWatch(connected);

      connected.onDisconnected(() => {
        setStatus('disconnected');
        setDeviceName(null);
        deviceRef.current = null;
        subRef.current = null;
      });

      setStatus('connected');
    } catch (err) {
      console.warn('[WatchAlerts] connection error', err.message);
      setStatus('disconnected');
      setDeviceName(null);
      Alert.alert('Connection Failed', err.message);
    }
  }, [handleAlert]);

  // ── Disconnect ────────────────────────────────────────────────────────────
  const handleDisconnect = useCallback(async () => {
    subRef.current?.remove();
    if (deviceRef.current) await disconnectWatch(deviceRef.current);
    deviceRef.current = null;
    subRef.current = null;
    setStatus('disconnected');
    setDeviceName(null);
  }, []);

  // ── Reset count ───────────────────────────────────────────────────────────
  const handleResetCount = useCallback(() => {
    setAlertCount(0);
    setAlertLog([]);
  }, []);

  // ── Status colour helpers ─────────────────────────────────────────────────
  const statusColor =
    status === 'connected'
      ? COLORS.success
      : status === 'scanning' || status === 'connecting'
        ? COLORS.warning
        : COLORS.danger;
  const statusLabel =
    status === 'connected'
      ? `Connected · ${deviceName || 'Watch'}`
      : status === 'connecting'
        ? `Connecting to ${deviceName || '…'}`
        : status === 'scanning'
          ? 'Scanning…'
          : 'Disconnected';

  // ── Render ────────────────────────────────────────────────────────────────

  // Show friendly message when BLE native module is missing (Expo Go)
  if (!isBleAvailable) {
    return (
      <View style={[styles.container, { justifyContent: 'center', alignItems: 'center' }]}>
        <Ionicons name="bluetooth-outline" size={64} color={COLORS.mediumGray} />
        <Text style={[styles.title, { textAlign: 'center', marginTop: 16 }]}>BLE Not Available</Text>
        <Text style={{ color: COLORS.textLight, textAlign: 'center', marginTop: 8, paddingHorizontal: 30 }}>
          You're running in Expo Go which doesn't support Bluetooth.{'\n\n'}
          Build a development APK to enable BLE:{'\n'}
          <Text style={{ fontWeight: 'bold' }}>npx expo run:android</Text>
        </Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      {/* Header */}
      <Text style={styles.title}>Watch Alerts</Text>

      {/* Connection card */}
      <View style={styles.card}>
        <View style={styles.row}>
          <Ionicons name="watch-outline" size={28} color={COLORS.primary} />
          <View style={{ marginLeft: 12, flex: 1 }}>
            <Text style={styles.deviceName}>{deviceName ?? 'ResQ+ Watch'}</Text>
            <View style={styles.statusRow}>
              <View style={[styles.statusDot, { backgroundColor: statusColor }]} />
              <Text style={[styles.statusText, { color: statusColor }]}>{statusLabel}</Text>
            </View>
          </View>
          {status === 'connected' && (
            <View style={styles.liveBadge}>
              <Text style={styles.liveBadgeText}>LIVE</Text>
            </View>
          )}
        </View>

        {(status === 'scanning' || status === 'connecting') && (
          <ActivityIndicator size="small" color={COLORS.primary} style={{ marginTop: 10 }} />
        )}

        <Text style={styles.modeLabel}>
          <Ionicons name="bluetooth-outline" size={13} color={COLORS.textLight} />
          {'  '}Via Bluetooth Low Energy
        </Text>

        <View style={styles.buttonRow}>
          {status !== 'connected' ? (
            <TouchableOpacity
              style={[styles.btn, styles.connectBtn]}
              onPress={openPicker}
              disabled={status === 'scanning' || status === 'connecting'}
            >
              <Ionicons name="bluetooth-outline" size={18} color="#fff" />
              <Text style={styles.btnText}>
                {status === 'scanning'
                  ? 'Scanning…'
                  : status === 'connecting'
                    ? 'Connecting…'
                    : 'Connect'}
              </Text>
            </TouchableOpacity>
          ) : (
            <TouchableOpacity style={[styles.btn, styles.disconnectBtn]} onPress={handleDisconnect}>
              <Ionicons name="close-circle-outline" size={18} color="#fff" />
              <Text style={styles.btnText}>Disconnect</Text>
            </TouchableOpacity>
          )}
        </View>
      </View>

      {/* Alert counter */}
      <View style={styles.counterCard}>
        <Text style={styles.counterLabel}>Alert Count</Text>
        <Text style={styles.counterValue}>{alertCount}</Text>
        <TouchableOpacity onPress={handleResetCount} style={styles.resetBtn}>
          <Ionicons name="refresh-outline" size={16} color={COLORS.textLight} />
          <Text style={styles.resetText}>Reset</Text>
        </TouchableOpacity>
      </View>

      {/* Alert log */}
      <Text style={styles.sectionTitle}>Alert Log</Text>
      {alertLog.length === 0 ? (
        <View style={styles.emptyContainer}>
          <Ionicons name="notifications-off-outline" size={48} color={COLORS.mediumGray} />
          <Text style={styles.emptyText}>No alerts yet</Text>
          <Text style={styles.emptySubtext}>
            {status === 'connected'
              ? 'Waiting for alerts from your watch…'
              : 'Tap Connect to scan for nearby devices'}
          </Text>
        </View>
      ) : (
        <FlatList
          data={alertLog}
          keyExtractor={(item) => item.id}
          renderItem={({ item, index }) => (
            <View style={styles.logItem}>
              <View style={styles.logBadge}>
                <Text style={styles.logBadgeText}>{alertLog.length - index}</Text>
              </View>
              <View style={{ flex: 1, marginLeft: 12 }}>
                <Text style={styles.logMessage}>{item.message}</Text>
                <Text style={styles.logTime}>{item.time}</Text>
              </View>
              <Ionicons name="alert-circle" size={22} color={COLORS.danger} />
            </View>
          )}
          contentContainerStyle={{ paddingBottom: 20 }}
        />
      )}

      {/* ── Device Picker Modal ────────────────────────────────────────────── */}
      <Modal
        visible={pickerVisible}
        animationType="slide"
        transparent
        onRequestClose={closePicker}
      >
        <View style={styles.modalOverlay}>
          <SafeAreaView style={styles.modalContainer}>
            {/* Header */}
            <View style={styles.modalHeader}>
              <Text style={styles.modalTitle}>Available Devices</Text>
              <TouchableOpacity onPress={closePicker} hitSlop={{ top: 10, bottom: 10, left: 10, right: 10 }}>
                <Ionicons name="close" size={24} color={COLORS.text} />
              </TouchableOpacity>
            </View>

            {/* Scanning indicator */}
            <View style={styles.scanningRow}>
              <ActivityIndicator size="small" color={COLORS.primary} />
              <Text style={styles.scanningText}>Scanning for nearby BLE devices…</Text>
            </View>

            {/* Device list */}
            {foundDevices.length === 0 ? (
              <View style={styles.modalEmpty}>
                <Ionicons name="search-outline" size={40} color={COLORS.mediumGray} />
                <Text style={styles.modalEmptyText}>Searching…</Text>
                <Text style={styles.modalEmptySubtext}>Make sure your watch is powered on</Text>
              </View>
            ) : (
              <FlatList
                data={foundDevices.sort((a, b) => (b.rssi || -999) - (a.rssi || -999))}
                keyExtractor={(item) => item.id}
                renderItem={({ item }) => (
                  <TouchableOpacity
                    style={styles.deviceItem}
                    onPress={() => handleSelectDevice(item)}
                    activeOpacity={0.7}
                  >
                    <View style={styles.deviceIconWrap}>
                      <Ionicons
                        name={item.name ? 'bluetooth' : 'help-circle-outline'}
                        size={22}
                        color={item.name ? COLORS.primary : COLORS.textLight}
                      />
                    </View>
                    <View style={{ flex: 1, marginLeft: 12 }}>
                      <Text style={styles.deviceItemName}>
                        {item.name || 'Unknown Device'}
                      </Text>
                      <Text style={styles.deviceItemId}>{item.id}</Text>
                    </View>
                    <View style={styles.rssiWrap}>
                      <Ionicons
                        name="cellular-outline"
                        size={14}
                        color={
                          (item.rssi || -999) > -60
                            ? COLORS.success
                            : (item.rssi || -999) > -80
                              ? COLORS.warning
                              : COLORS.danger
                        }
                      />
                      <Text style={styles.rssiText}>{item.rssi ?? '?'} dBm</Text>
                    </View>
                    <Ionicons name="chevron-forward" size={18} color={COLORS.textLight} />
                  </TouchableOpacity>
                )}
                contentContainerStyle={{ paddingBottom: 20 }}
                ItemSeparatorComponent={() => <View style={styles.separator} />}
              />
            )}

            <Text style={styles.deviceCountText}>
              {foundDevices.length} device{foundDevices.length !== 1 ? 's' : ''} found
            </Text>
          </SafeAreaView>
        </View>
      </Modal>
    </View>
  );
};

// ── Styles ──────────────────────────────────────────────────────────────────────
const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
    padding: 16,
  },
  title: {
    fontSize: 26,
    fontWeight: 'bold',
    color: COLORS.text,
    marginBottom: 16,
  },

  // Connection card
  card: {
    backgroundColor: COLORS.white,
    borderRadius: 14,
    padding: 18,
    marginBottom: 16,
    elevation: 3,
    shadowColor: COLORS.shadow,
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
  },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  deviceName: {
    fontSize: 17,
    fontWeight: '600',
    color: COLORS.text,
  },
  statusRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginTop: 4,
  },
  statusDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
    marginRight: 6,
  },
  statusText: {
    fontSize: 13,
    fontWeight: '500',
  },
  modeLabel: {
    fontSize: 12,
    color: COLORS.textLight,
    marginTop: 10,
  },
  liveBadge: {
    backgroundColor: COLORS.success,
    paddingHorizontal: 8,
    paddingVertical: 3,
    borderRadius: 6,
  },
  liveBadgeText: {
    color: '#fff',
    fontSize: 11,
    fontWeight: 'bold',
  },
  buttonRow: {
    flexDirection: 'row',
    marginTop: 14,
  },
  btn: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 10,
    paddingHorizontal: 20,
    borderRadius: 10,
  },
  connectBtn: {
    backgroundColor: COLORS.primary,
  },
  disconnectBtn: {
    backgroundColor: COLORS.textLight,
  },
  btnText: {
    color: '#fff',
    fontWeight: '600',
    marginLeft: 6,
    fontSize: 15,
  },

  // Counter card
  counterCard: {
    backgroundColor: COLORS.white,
    borderRadius: 14,
    padding: 20,
    alignItems: 'center',
    marginBottom: 16,
    elevation: 3,
    shadowColor: COLORS.shadow,
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
  },
  counterLabel: {
    fontSize: 14,
    color: COLORS.textLight,
    marginBottom: 4,
  },
  counterValue: {
    fontSize: 56,
    fontWeight: 'bold',
    color: COLORS.primary,
  },
  resetBtn: {
    flexDirection: 'row',
    alignItems: 'center',
    marginTop: 8,
  },
  resetText: {
    color: COLORS.textLight,
    marginLeft: 4,
    fontSize: 13,
  },

  // Section
  sectionTitle: {
    fontSize: 18,
    fontWeight: '700',
    color: COLORS.text,
    marginBottom: 10,
  },

  // Empty state
  emptyContainer: {
    alignItems: 'center',
    marginTop: 30,
  },
  emptyText: {
    fontSize: 16,
    fontWeight: '600',
    color: COLORS.textLight,
    marginTop: 10,
  },
  emptySubtext: {
    fontSize: 13,
    color: COLORS.mediumGray,
    marginTop: 4,
    textAlign: 'center',
  },

  // Log items
  logItem: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: COLORS.white,
    borderRadius: 10,
    padding: 14,
    marginBottom: 8,
    elevation: 1,
    shadowColor: COLORS.shadow,
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.06,
    shadowRadius: 2,
  },
  logBadge: {
    width: 30,
    height: 30,
    borderRadius: 15,
    backgroundColor: COLORS.primary,
    justifyContent: 'center',
    alignItems: 'center',
  },
  logBadgeText: {
    color: '#fff',
    fontWeight: 'bold',
    fontSize: 13,
  },
  logMessage: {
    fontSize: 15,
    fontWeight: '500',
    color: COLORS.text,
  },
  logTime: {
    fontSize: 12,
    color: COLORS.textLight,
    marginTop: 2,
  },

  // ── Device Picker Modal ──────────────────────────────────────────────────
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.5)',
    justifyContent: 'flex-end',
  },
  modalContainer: {
    backgroundColor: COLORS.white,
    borderTopLeftRadius: 20,
    borderTopRightRadius: 20,
    maxHeight: '80%',
    paddingBottom: Platform.OS === 'android' ? 20 : 0,
  },
  modalHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: 18,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
  },
  modalTitle: {
    fontSize: 20,
    fontWeight: 'bold',
    color: COLORS.text,
  },
  scanningRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 18,
    paddingVertical: 10,
    backgroundColor: COLORS.lightGray,
  },
  scanningText: {
    marginLeft: 10,
    fontSize: 13,
    color: COLORS.textLight,
  },
  modalEmpty: {
    alignItems: 'center',
    paddingVertical: 40,
  },
  modalEmptyText: {
    fontSize: 16,
    fontWeight: '600',
    color: COLORS.textLight,
    marginTop: 12,
  },
  modalEmptySubtext: {
    fontSize: 13,
    color: COLORS.mediumGray,
    marginTop: 4,
  },
  deviceItem: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 14,
    paddingHorizontal: 18,
  },
  deviceIconWrap: {
    width: 40,
    height: 40,
    borderRadius: 20,
    backgroundColor: COLORS.lightGray,
    justifyContent: 'center',
    alignItems: 'center',
  },
  deviceItemName: {
    fontSize: 15,
    fontWeight: '600',
    color: COLORS.text,
  },
  deviceItemId: {
    fontSize: 11,
    color: COLORS.textLight,
    marginTop: 2,
  },
  rssiWrap: {
    flexDirection: 'row',
    alignItems: 'center',
    marginRight: 8,
  },
  rssiText: {
    fontSize: 11,
    color: COLORS.textLight,
    marginLeft: 3,
  },
  separator: {
    height: 1,
    backgroundColor: COLORS.border,
    marginLeft: 70,
  },
  deviceCountText: {
    textAlign: 'center',
    fontSize: 12,
    color: COLORS.textLight,
    paddingVertical: 10,
    borderTopWidth: 1,
    borderTopColor: COLORS.border,
  },
});

export default WatchAlertsScreen;
