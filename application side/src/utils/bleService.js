/**
 * BLE Service – manages Bluetooth Low Energy connection to the ResQ+ ESP32-S3 watch.
 *
 * Protocol:
 *   • The watch acts as a **BLE peripheral** (GATT server).
 *   • It advertises a custom service with UUID  SERVICE_UUID.
 *   • Inside that service there is a **notify** characteristic (ALERT_CHAR_UUID).
 *   • Every time the user taps the "Alert" button on the watch, the ESP32 writes
 *     an incremented value (or a small payload) to that characteristic and sends
 *     a BLE notification.
 *   • This module subscribes to those notifications so the app can react instantly.
 *
 * Make sure the ESP32 firmware uses the SAME UUIDs defined below.
 */

import { Platform, PermissionsAndroid } from 'react-native';

// Lazy-require so the app doesn't crash if native module is missing (Expo Go).
let BleManager = null;
try {
  BleManager = require('react-native-ble-plx').BleManager;
} catch {
  // native module unavailable – handled below
}

// ── UUIDs (must match ESP32 firmware) ──────────────────────────────────────────
export const SERVICE_UUID        = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
export const ALERT_CHAR_UUID     = 'beb5483e-36e1-4688-b7f5-ea07361b26a8'; // TX (notify)
export const RX_CHAR_UUID        = 'beb5483f-36e1-4688-b7f5-ea07361b26a8'; // RX (write)

// ── BLE availability flag ──────────────────────────────────────────────────────
export const isBleAvailable = !!BleManager;

// ── Portable Base64 encode (works on all React Native / Hermes versions) ───────
const B64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
function toBase64(str) {
  // If native btoa exists, use it
  if (typeof btoa === 'function') {
    try { return btoa(str); } catch { /* fallback below */ }
  }
  let out = '';
  for (let i = 0; i < str.length; i += 3) {
    const a = str.charCodeAt(i);
    const b = i + 1 < str.length ? str.charCodeAt(i + 1) : 0;
    const c = i + 2 < str.length ? str.charCodeAt(i + 2) : 0;
    out += B64_CHARS[a >> 2];
    out += B64_CHARS[((a & 3) << 4) | (b >> 4)];
    out += i + 1 < str.length ? B64_CHARS[((b & 0x0f) << 2) | (c >> 6)] : '=';
    out += i + 2 < str.length ? B64_CHARS[c & 0x3f] : '=';
  }
  return out;
}

// ── Singleton manager ──────────────────────────────────────────────────────────
let manager = null;

export function getBleManager() {
  if (!BleManager) {
    throw new Error(
      'BLE is not available. react-native-ble-plx requires a native build (Expo Dev Client). ' +
      'It does not work inside Expo Go.',
    );
  }
  if (!manager) {
    manager = new BleManager();
  }
  return manager;
}

// ── Android runtime permissions (API 31+) ──────────────────────────────────────
export async function requestBlePermissions() {
  if (Platform.OS === 'ios') return true;

  const apiLevel = Platform.Version;

  if (apiLevel >= 31) {
    // Android 12+
    const results = await PermissionsAndroid.requestMultiple([
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
      PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
    ]);
    return Object.values(results).every(
      (r) => r === PermissionsAndroid.RESULTS.GRANTED,
    );
  }

  // Android < 12
  const granted = await PermissionsAndroid.request(
    PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
  );
  return granted === PermissionsAndroid.RESULTS.GRANTED;
}

// ── Scan for the ResQ+ watch (auto-connect to first match) ─────────────────────
/**
 * Scans for a BLE peripheral advertising SERVICE_UUID.
 * Returns a Promise that resolves with the first matching device, or rejects
 * after `timeoutMs` milliseconds.
 */
export function scanForWatch(timeoutMs = 15000) {
  const ble = getBleManager();

  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      ble.stopDeviceScan();
      reject(new Error('Watch not found – scan timed out'));
    }, timeoutMs);

    ble.startDeviceScan([SERVICE_UUID], null, (error, device) => {
      if (error) {
        clearTimeout(timer);
        reject(error);
        return;
      }
      if (device) {
        clearTimeout(timer);
        ble.stopDeviceScan();
        resolve(device);
      }
    });
  });
}

// ── Scan ALL nearby BLE devices (for device picker) ────────────────────────────
/**
 * Scans for ALL nearby BLE devices (not filtered by service UUID).
 * Calls `onDeviceFound(device)` for each unique device discovered.
 * Returns a stop function – call it to end the scan.
 *
 * @param {function} onDeviceFound – called with a device object { id, name, rssi, raw }
 * @param {function} [onError]     – called on scan error
 * @returns {function} stopScan    – call to stop scanning
 */
export function scanAllDevices(onDeviceFound, onError) {
  const ble = getBleManager();
  const seen = new Set();

  ble.startDeviceScan(null, { allowDuplicates: false }, (error, device) => {
    if (error) {
      onError?.(error);
      return;
    }
    if (device && !seen.has(device.id)) {
      seen.add(device.id);
      onDeviceFound({
        id: device.id,
        name: device.name || device.localName || null,
        rssi: device.rssi,
        raw: device,                 // keep the full device object for connecting
      });
    }
  });

  // Return a stop function
  return () => {
    try {
      ble.stopDeviceScan();
    } catch {
      // ignore
    }
  };
}

// ── Stop any ongoing scan ──────────────────────────────────────────────────────
export function stopScan() {
  try {
    const ble = getBleManager();
    ble.stopDeviceScan();
  } catch {
    // ignore
  }
}

// ── Connect & subscribe ────────────────────────────────────────────────────────
/**
 * Connects to the given BLE device, discovers services/characteristics, and
 * subscribes to notifications on the alert characteristic.
 *
 * @param {object}   device      – BLE device object from scan
 * @param {function} onAlert     – called with (decodedValue: string) on each notification
 * @returns {{ device, subscription }}  – call subscription.remove() to unsubscribe
 */
export async function connectAndSubscribe(device, onAlert) {
  const connected = await device.connect({ autoConnect: false });
  await connected.discoverAllServicesAndCharacteristics();

  const subscription = connected.monitorCharacteristicForService(
    SERVICE_UUID,
    ALERT_CHAR_UUID,
    (error, characteristic) => {
      if (error) {
        console.warn('[BLE] notification error', error.message);
        return;
      }
      // characteristic.value is Base-64 encoded
      const raw = characteristic?.value;
      if (raw) {
        try {
          const decoded = atob(raw);          // decode base-64 → string
          onAlert(decoded);
        } catch {
          onAlert(raw);
        }
      } else {
        onAlert('1');                          // fallback: treat as +1 alert
      }
    },
  );

  return { device: connected, subscription };
}

// ── Send current time to the watch ────────────────────────────────────────────────
/**
 * Writes the current epoch time and timezone offset to the watch via the RX
 * characteristic. The watch uses this to set its internal clock.
 *
 * Format: "TIME:<epoch_seconds>:<tz_offset_minutes>"
 * e.g.    "TIME:1709251200:330"  (for UTC+5:30)
 */
export async function sendTimeToWatch(device) {
  const doSend = async () => {
    const epochSec = Math.floor(Date.now() / 1000);
    const tzOffsetMin = -new Date().getTimezoneOffset(); // JS gives inverted sign
    const payload = `TIME:${epochSec}:${tzOffsetMin}`;
    const encoded = toBase64(payload);

    console.log('[BLE] Sending time payload:', payload, '  b64:', encoded);

    await device.writeCharacteristicWithResponseForService(
      SERVICE_UUID,
      RX_CHAR_UUID,
      encoded,
    );
    console.log('[BLE] Time sent successfully');
  };

  // Try up to 3 times with a short delay between attempts
  for (let attempt = 1; attempt <= 3; attempt++) {
    try {
      // Small delay to let GATT settle after connection
      await new Promise((r) => setTimeout(r, 500 * attempt));
      await doSend();
      return; // success
    } catch (err) {
      console.warn(`[BLE] sendTimeToWatch attempt ${attempt} failed:`, err.message);
      if (attempt === 3) {
        console.warn('[BLE] All time-sync attempts failed');
      }
    }
  }
}

// ── Disconnect helper ──────────────────────────────────────────────────────────
export async function disconnectWatch(device) {
  try {
    const isConnected = await device.isConnected();
    if (isConnected) {
      await device.cancelConnection();
    }
  } catch {
    // already disconnected – ignore
  }
}
