import * as TaskManager from 'expo-task-manager';
import * as Location from 'expo-location';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { ref, set } from 'firebase/database';
import { database } from '@/utils/firebase';

export const BACKGROUND_LOCATION_TASK = 'RESQPLUS_BACKGROUND_LOCATION';

const LOCATION_PATH = '/phone-gps/current';

const UID_STORAGE_KEY = 'resqplus:locationReporterUid';

TaskManager.defineTask(BACKGROUND_LOCATION_TASK, async ({ data, error }) => {
  if (error) {
    return;
  }

  const locations = data?.locations;
  if (!Array.isArray(locations) || locations.length === 0) return;

  const location = locations[locations.length - 1];
  const now = Date.now();

  const uid = await AsyncStorage.getItem(UID_STORAGE_KEY);

  const payload = {
    lat: location?.coords?.latitude ?? null,
    lng: location?.coords?.longitude ?? null,
    accuracy: location?.coords?.accuracy ?? null,
    timestamp: now,
    source: 'phone',
    gpsActive: 'true',
    uid: uid ?? null,
  };

  try {
    await set(ref(database, LOCATION_PATH), payload);
  } catch {
    // Ignore background write failures.
  }
});

export async function startBackgroundLocationUpdates() {
  const hasStarted = await Location.hasStartedLocationUpdatesAsync(BACKGROUND_LOCATION_TASK);
  if (hasStarted) return;

  await Location.startLocationUpdatesAsync(BACKGROUND_LOCATION_TASK, {
    accuracy: Location.Accuracy.High,
    timeInterval: 5000,
    distanceInterval: 5,
    pausesUpdatesAutomatically: false,
    showsBackgroundLocationIndicator: true,
    foregroundService: {
      notificationTitle: 'ResQPlus is sending location',
      notificationBody: 'Location sharing is active for emergencies.',
    },
  });
}

export async function stopBackgroundLocationUpdates() {
  const hasStarted = await Location.hasStartedLocationUpdatesAsync(BACKGROUND_LOCATION_TASK);
  if (!hasStarted) return;
  await Location.stopLocationUpdatesAsync(BACKGROUND_LOCATION_TASK);
}

export async function setBackgroundTaskUid(uid) {
  if (!uid) {
    await AsyncStorage.removeItem(UID_STORAGE_KEY);
    return;
  }
  await AsyncStorage.setItem(UID_STORAGE_KEY, uid);
}
