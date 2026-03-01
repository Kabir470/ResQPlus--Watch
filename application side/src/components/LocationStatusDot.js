import React, { useEffect, useMemo, useState } from 'react';
import { Platform, StyleSheet, TouchableOpacity, View } from 'react-native';
import { COLORS } from '@/constants/colors';
import { LOCATION_HEALTH_WINDOW_MS } from '@/components/LocationReporter';

export default function LocationStatusDot({ enabled, onToggle, lastWriteAt, permissionGranted, servicesEnabled }) {
  const [, setTick] = useState(0);

  // Force re-render periodically so the dot can go red quickly when sending stops.
  useEffect(() => {
    const id = setInterval(() => setTick((t) => (t + 1) % 1000000), 800);
    return () => clearInterval(id);
  }, []);

  const isHealthy = useMemo(() => {
    if (!enabled) return false;
    if (!permissionGranted) return false;
    if (!servicesEnabled) return false;
    if (!lastWriteAt) return false;
    return Date.now() - lastWriteAt <= LOCATION_HEALTH_WINDOW_MS;
  }, [enabled, permissionGranted, servicesEnabled, lastWriteAt]);

  return (
    <TouchableOpacity
      accessibilityRole="button"
      accessibilityLabel={enabled ? 'Turn off location sending' : 'Turn on location sending'}
      onPress={onToggle}
      style={styles.touch}
      hitSlop={{ top: 14, left: 14, right: 14, bottom: 14 }}
      activeOpacity={0.8}
    >
      <View
        style={[
          styles.fab,
          { backgroundColor: isHealthy ? COLORS.success : COLORS.danger },
        ]}
      />
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  touch: {
    position: 'absolute',
    right: 18,
    // Lift above bottom tab bar (typical chat/AI floating button placement)
    bottom: 104,
    zIndex: 999,
  },
  fab: {
    width: 56,
    height: 56,
    borderRadius: 28,
    borderWidth: 3,
    borderColor: COLORS.white,
    elevation: 6,
    shadowColor: COLORS.shadow,
    shadowOpacity: Platform.OS === 'ios' ? 0.25 : 0,
    shadowRadius: Platform.OS === 'ios' ? 6 : 0,
    shadowOffset: Platform.OS === 'ios' ? { width: 0, height: 4 } : { width: 0, height: 0 },
  },
});
