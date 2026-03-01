import { useEffect, useRef } from 'react';
import * as Location from 'expo-location';
import { ref, set, update } from 'firebase/database';
import { database } from '@/utils/firebase';
import { useAuth } from '@/context/AuthContext';
import {
  setBackgroundTaskUid,
  startBackgroundLocationUpdates,
  stopBackgroundLocationUpdates,
} from '@/background/locationTask';

const LOCATION_PATH = '/phone-gps/current';
const STATUS_PATH = '/phone-gps/status';
const MIN_SEND_INTERVAL_MS = 3000;

// Consider "sending" healthy if we successfully wrote recently.
export const LOCATION_HEALTH_WINDOW_MS = 6500;

export default function LocationReporter({ enabled = true, onStatus }) {
  const { user } = useAuth();
  const subscriptionRef = useRef(null);
  const lastSentAtRef = useRef(0);
  const permissionGrantedRef = useRef(false);
  const servicesEnabledRef = useRef(false);
  const lastStatusSentAtRef = useRef(0);
  const lastActiveValueRef = useRef(null);
  const bgStartedRef = useRef(false);

  useEffect(() => {
    let isMounted = true;
    let servicesPollId = null;

    const stop = () => {
      if (subscriptionRef.current) {
        subscriptionRef.current.remove();
        subscriptionRef.current = null;
      }
    };

    const stopBackground = async () => {
      if (!bgStartedRef.current) return;
      try {
        await stopBackgroundLocationUpdates();
      } catch {
        // Ignore.
      }
      bgStartedRef.current = false;
    };

    const emitStatus = (extra = {}) => {
      onStatus?.({
        enabled: !!enabled,
        permissionGranted: permissionGrantedRef.current,
        servicesEnabled: servicesEnabledRef.current,
        lastWriteAt: lastSentAtRef.current,
        ...extra,
      });
    };

    const computeActiveString = () => {
      const canRun = !!enabled && permissionGrantedRef.current && servicesEnabledRef.current;
      if (!canRun) return 'false';
      const last = lastSentAtRef.current;
      if (!last) return 'false';
      return Date.now() - last <= LOCATION_HEALTH_WINDOW_MS ? 'true' : 'false';
    };

    const pushStatusIfNeeded = async (reason, { forceStatusWrite = false } = {}) => {
      const now = Date.now();
      const gpsActive = computeActiveString();
      const changed = gpsActive !== lastActiveValueRef.current;

      // Keep /current.gpsActive fresh ONLY when it changes.
      if (changed) {
        try {
          await update(ref(database, LOCATION_PATH), {
            gpsActive,
            gpsActiveUpdatedAt: now,
          });
        } catch {
          // Ignore current update failures.
        }
      }

      // Avoid burning Firebase writes: only write /status when
      // - gpsActive changes (true <-> false), OR
      // - gpsActive is true AND we explicitly force a status write (e.g. after a real location write)
      if (!changed && !(gpsActive === 'true' && forceStatusWrite)) {
        lastActiveValueRef.current = gpsActive;
        return;
      }

      lastStatusSentAtRef.current = now;
      lastActiveValueRef.current = gpsActive;

      try {
        await set(ref(database, STATUS_PATH), {
          gpsActive,
          enabled: !!enabled,
          permissionGranted: permissionGrantedRef.current,
          servicesEnabled: servicesEnabledRef.current,
          lastWriteAt: lastSentAtRef.current || 0,
          timestamp: now,
          uid: user?.uid ?? null,
          reason: reason ?? null,
        });
      } catch {
        // Ignore status write failures.
      }
    };

    const syncServicesEnabled = async () => {
      try {
        const next = await Location.hasServicesEnabledAsync();
        if (!isMounted) return;
        const changed = next !== servicesEnabledRef.current;
        servicesEnabledRef.current = next;
        if (changed) {
          emitStatus();
        }
        return next;
      } catch {
        return servicesEnabledRef.current;
      }
    };

    const start = async () => {
      try {
        if (!enabled) {
          stop();
          await stopBackground();
          emitStatus({ enabled: false });
          await pushStatusIfNeeded('disabled');
          return;
        }

        // Make uid available to background task.
        try {
          await setBackgroundTaskUid(user?.uid ?? null);
        } catch {
          // Ignore.
        }

        const { status } = await Location.requestForegroundPermissionsAsync();
        permissionGrantedRef.current = status === 'granted';
        await syncServicesEnabled();
        emitStatus();
        await pushStatusIfNeeded(permissionGrantedRef.current ? 'starting' : 'permission_denied');
        if (!permissionGrantedRef.current) return;

        // Request background permission (needed for persistent tracking/notification).
        try {
          const bg = await Location.requestBackgroundPermissionsAsync();
          // If the user denies background permission, we still continue in foreground.
          if (bg?.status === 'granted') {
            try {
              await startBackgroundLocationUpdates();
              bgStartedRef.current = true;
            } catch {
              bgStartedRef.current = false;
            }
          } else {
            await stopBackground();
          }
        } catch {
          // Some environments may throw; keep foreground-only.
        }

        // If Location Services are OFF at OS level, don't start watching.
        if (!servicesEnabledRef.current) {
          stop();
          await stopBackground();
          await pushStatusIfNeeded('services_disabled');
          return;
        }

        // Ensure we don't double-subscribe if enabled toggles quickly.
        stop();

        subscriptionRef.current = await Location.watchPositionAsync(
          {
            accuracy: Location.Accuracy.High,
            timeInterval: 2000,
            distanceInterval: 2,
          },
          async (location) => {
            if (!isMounted) return;
            if (!enabled) return;
            if (!servicesEnabledRef.current) return;
            const now = Date.now();
            if (now - lastSentAtRef.current < MIN_SEND_INTERVAL_MS) return;

            const payload = {
              lat: location?.coords?.latitude ?? null,
              lng: location?.coords?.longitude ?? null,
              accuracy: location?.coords?.accuracy ?? null,
              timestamp: now,
              source: 'phone',
              gpsActive: 'true',
              uid: user?.uid ?? null,
            };

            // Keep it simple: one "current" location object the ESP32 can poll.
            try {
              await set(ref(database, LOCATION_PATH), payload);
              lastSentAtRef.current = now;
              emitStatus({ lastWriteAt: now });
              await pushStatusIfNeeded('location_write_ok', { forceStatusWrite: true });
            } catch {
              emitStatus({ error: 'write_failed' });
              await pushStatusIfNeeded('location_write_failed');
            }
          }
        );
      } catch {
        emitStatus({ error: 'watch_failed' });
        await pushStatusIfNeeded('watch_failed');
      }
    };

    start();

    // Poll OS Location Services so UI turns red quickly when user disables location.
    servicesPollId = setInterval(async () => {
      if (!isMounted) return;

      const prevServices = servicesEnabledRef.current;
      const servicesOn = await syncServicesEnabled();

      // If services changed, write status once (so gpsActive can flip false immediately).
      if (prevServices !== servicesEnabledRef.current) {
        pushStatusIfNeeded('services_change');
      }

      // If we went stale (no recent successful writes), flip gpsActive to false once.
      const computed = computeActiveString();
      if (lastActiveValueRef.current === 'true' && computed === 'false') {
        pushStatusIfNeeded('stale_timeout');
      }

      // If services go OFF, stop watching immediately.
      if (!servicesOn) {
        stop();
        stopBackground();
        return;
      }

      // If services are ON and user enabled reporting but no watcher exists, restart it.
      if (enabled && permissionGrantedRef.current && !subscriptionRef.current) {
        start();
      }
    }, 700);

    return () => {
      isMounted = false;
      if (servicesPollId) clearInterval(servicesPollId);
      stop();

      // Best-effort cleanup.
      stopBackground();
      setBackgroundTaskUid(null);
    };
  }, [user?.uid, enabled, onStatus]);

  return null;
}
