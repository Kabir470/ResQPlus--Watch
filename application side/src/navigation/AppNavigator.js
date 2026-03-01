import React, { useCallback, useMemo, useState } from 'react';
import { Alert, Linking, Platform } from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';

// Constants
import { COLORS } from '@/constants/colors';

// Screens
import HomeScreen from '@/screens/HomeScreen';
import MapScreen from '@/screens/MapScreen';
import AlertsScreen from '@/screens/AlertsScreen';
import ProfileScreen from '@/screens/ProfileScreen';
import LoginScreen from '@/screens/LoginScreen';
import SignupScreen from '@/screens/SignupScreen';
import DeviceDashboardScreen from '@/screens/DeviceDashboardScreen';
import ReportIncidentScreen from '@/screens/ReportIncidentScreen';
import AdminNoticesScreen from '@/screens/AdminNoticesScreen';
import FeedScreen from '@/screens/FeedScreen';
import PostScreen from '@/screens/PostScreen';
import CreateNoticeScreen from '@/screens/CreateNoticeScreen';
import AlertDetailScreen from '@/screens/AlertDetailScreen';
import WeaponAlertsScreen from '@/screens/WeaponAlertsScreen';
import WatchAlertsScreen from '@/screens/WatchAlertsScreen';
import LocationReporter from '@/components/LocationReporter';
import LocationStatusDot from '@/components/LocationStatusDot';

// Register background location task (must be imported before starting updates)
import '@/background/locationTask';

const Stack = createNativeStackNavigator();
const Tab = createBottomTabNavigator();

// Logged-in user flow
function MainTabNavigator() {
  return (
    <Tab.Navigator
      screenOptions={({ route }) => ({
        tabBarIcon: ({ focused, color, size }) => {
          let iconName;

          if (route.name === 'Home') {
            iconName = focused ? 'home' : 'home-outline';
          } else if (route.name === 'Map') {
            iconName = focused ? 'map' : 'map-outline';
          } else if (route.name === 'Alerts') {
            iconName = focused ? 'notifications' : 'notifications-outline';
          } else if (route.name === 'Weapon') {
            iconName = focused ? 'shield' : 'shield-outline';
          } else if (route.name === 'Watch') {
            iconName = focused ? 'watch' : 'watch-outline';
          } else if (route.name === 'Dashboard') {
            iconName = focused ? 'speedometer' : 'speedometer-outline';
          } else if (route.name === 'Profile') {
            iconName = focused ? 'person' : 'person-outline';
          } else if (route.name === 'Feed') {
            iconName = focused ? 'list' : 'list-outline';
          }

          return <Ionicons name={iconName} size={size} color={color} />;
        },
        tabBarActiveTintColor: COLORS.primary,
        tabBarInactiveTintColor: COLORS.textLight,
        headerShown: false,
      })}
    >
      <Tab.Screen name="Home" component={HomeScreen} />
      <Tab.Screen name="Feed" component={FeedScreen} />
      <Tab.Screen name="Weapon" component={WeaponAlertsScreen} />
      <Tab.Screen name="Map" component={MapScreen} />
      <Tab.Screen name="Alerts" component={AlertsScreen} />
      <Tab.Screen name="Watch" component={WatchAlertsScreen} />
      <Tab.Screen name="Dashboard" component={DeviceDashboardScreen} />
      <Tab.Screen name="Profile" component={ProfileScreen} />
    </Tab.Navigator>
  );
}

// Main app navigator
import { useAuth } from '@/context/AuthContext';
const AppNavigator = () => {
  const { isAuthenticated } = useAuth();

  const [locationEnabled, setLocationEnabled] = useState(true);
  const [locationStatus, setLocationStatus] = useState({
    permissionGranted: false,
    servicesEnabled: false,
    lastWriteAt: 0,
  });

  const onLocationStatus = useCallback((status) => {
    setLocationStatus((prev) => ({
      ...prev,
      ...status,
    }));
  }, []);

  const openLocationSettings = useCallback(async () => {
    // Best-effort: on Android we can deep-link to Location Services settings.
    if (Platform.OS === 'android') {
      try {
        // Optional dependency: expo-intent-launcher
        // If not installed, we'll fall back to app settings.
        // eslint-disable-next-line global-require
        const IntentLauncher = require('expo-intent-launcher');
        await IntentLauncher.startActivityAsync(IntentLauncher.ActivityAction.LOCATION_SOURCE_SETTINGS);
        return;
      } catch {
        // fall through
      }
    }

    try {
      await Linking.openSettings();
    } catch {
      // ignore
    }
  }, []);

  const handleLocationFabPress = useCallback(() => {
    const servicesEnabled = !!locationStatus.servicesEnabled;
    const permissionGranted = !!locationStatus.permissionGranted;

    // If the user is trying to enable sending but OS location services are OFF, prompt them.
    if (!servicesEnabled) {
      Alert.alert(
        'Turn on Location Services',
        'Location Services are turned off. Turn them on to send GPS to Firebase.',
        [
          { text: 'Cancel', style: 'cancel' },
          { text: 'Open Settings', onPress: openLocationSettings },
        ]
      );
      return;
    }

    // If permission is missing, prompt to open app settings.
    if (!permissionGranted) {
      Alert.alert(
        'Allow Location Permission',
        'Location permission is not granted. Allow it to send GPS to Firebase.',
        [
          { text: 'Cancel', style: 'cancel' },
          { text: 'Open Settings', onPress: openLocationSettings },
        ]
      );
      return;
    }

    setLocationEnabled((v) => !v);
  }, [locationStatus.permissionGranted, locationStatus.servicesEnabled, openLocationSettings]);

  const resolvedFabEnabled = useMemo(() => {
    // The UI dot uses this to decide green/red. Keep it tied to the toggle state.
    return locationEnabled;
  }, [locationEnabled]);

  return (
    <NavigationContainer>
      {isAuthenticated ? (
        <>
          <LocationReporter enabled={locationEnabled} onStatus={onLocationStatus} />
          <LocationStatusDot
            enabled={resolvedFabEnabled}
            permissionGranted={!!locationStatus.permissionGranted}
            servicesEnabled={!!locationStatus.servicesEnabled}
            lastWriteAt={locationStatus.lastWriteAt}
            onToggle={handleLocationFabPress}
          />
        </>
      ) : null}
      <Stack.Navigator>
        {isAuthenticated ? (
          <>
            <Stack.Screen name="Main" component={MainTabNavigator} options={{ headerShown: false }} />
            <Stack.Screen name="ReportIncident" component={ReportIncidentScreen} />
            <Stack.Screen name="AdminNotices" component={AdminNoticesScreen} />
            <Stack.Screen name="Post" component={PostScreen} />
            <Stack.Screen name="CreateNotice" component={CreateNoticeScreen} />
            <Stack.Screen name="AlertDetail" component={AlertDetailScreen} options={{ title: 'Alert Details' }} />
          </>
        ) : (
          <>
            <Stack.Screen name="Login" component={LoginScreen} options={{ headerShown: false }} />
            <Stack.Screen name="Signup" component={SignupScreen} options={{ headerShown: false }}/>
          </>
        )}
      </Stack.Navigator>
    </NavigationContainer>
  );
};

export default AppNavigator;
