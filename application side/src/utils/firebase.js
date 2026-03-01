import { initializeApp } from 'firebase/app';
import { initializeFirestore } from 'firebase/firestore';
import { getDatabase } from 'firebase/database';
import { initializeAuth, getReactNativePersistence } from 'firebase/auth';
import AsyncStorage from '@react-native-async-storage/async-storage';

// Your web app's Firebase configuration
const firebaseConfig = {
  apiKey: "AIzaSyDFTxaUi04hxf-yL53QfXHwb061MDsXv6g",
  authDomain: "resqfire.firebaseapp.com",
  databaseURL: "https://resqfire-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "resqfire",
  storageBucket: "resqfire.appspot.com",
  messagingSenderId: "985132831798",
  appId: "1:985132831798:web:5eb878c321273b1786b497"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);

// Initialize Auth (ONLY ONCE)
const auth = initializeAuth(app, {
  persistence: getReactNativePersistence(AsyncStorage),
});

// Firestore with long polling
const firestore = initializeFirestore(app, {
  experimentalForceLongPolling: true,
  useFetchStreams: false,
});

// Realtime database
const database = getDatabase(app);

export { app, auth, firestore, database };