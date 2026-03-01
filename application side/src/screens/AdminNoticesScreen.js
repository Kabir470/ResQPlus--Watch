import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, FlatList, ActivityIndicator, TouchableOpacity } from 'react-native';
import { collection, onSnapshot, query, orderBy, doc } from 'firebase/firestore';
import { firestore, auth } from '@/utils/firebase';
import { COLORS } from '@/constants/colors';
import { Ionicons } from '@expo/vector-icons';
import { useAuth } from '@/context/AuthContext';

const AdminNoticesScreen = ({ navigation }) => {
  const { user } = useAuth();
  const [notices, setNotices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [userRole, setUserRole] = useState('user');

  useEffect(() => {
    // Fetch user role
    if (user) {
      const userDocRef = doc(firestore, 'users', user.uid);
      const unsubscribeUser = onSnapshot(userDocRef, (doc) => {
        if (doc.exists()) {
          setUserRole(doc.data().role || 'user');
        }
      });
      return () => unsubscribeUser();
    }
  }, [user]);

  useEffect(() => {
    const noticesQuery = query(collection(firestore, 'notices'), orderBy('timestamp', 'desc'));
    
    const unsubscribe = onSnapshot(noticesQuery, (snapshot) => {
      const noticesData = snapshot.docs.map(doc => ({
        id: doc.id,
        ...doc.data()
      }));
      setNotices(noticesData);
      setLoading(false);
    });

    return () => unsubscribe();
  }, []);

  const renderItem = ({ item }) => (
    <View style={styles.itemContainer}>
      <Text style={styles.itemTitle}>{item.title}</Text>
      <Text style={styles.itemMessage}>{item.message}</Text>
      <Text style={styles.itemIssuer}>Issued by: {item.issuedBy}</Text>
      <Text style={styles.itemDate}>{new Date(item.timestamp?.toDate()).toLocaleString()}</Text>
    </View>
  );

  if (loading) {
    return (
      <View style={styles.centered}>
        <ActivityIndicator size="large" color={COLORS.primary} />
        <Text>Loading Notices...</Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Government Notices</Text>
        {userRole === 'government' && (
          <TouchableOpacity onPress={() => navigation.navigate('CreateNotice')}>
            <Ionicons name="add-circle" size={32} color={COLORS.primary} />
          </TouchableOpacity>
        )}
      </View>
      <FlatList
        data={notices}
        renderItem={renderItem}
        keyExtractor={item => item.id}
        contentContainerStyle={styles.list}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: 16,
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  list: {
    paddingHorizontal: 16,
  },
  itemContainer: {
    backgroundColor: COLORS.white,
    padding: 16,
    marginBottom: 10,
    borderRadius: 12,
  },
  itemTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    marginBottom: 5,
  },
  itemMessage: {
    fontSize: 14,
    lineHeight: 20,
    marginBottom: 10,
  },
  itemIssuer: {
    fontSize: 12,
    color: COLORS.textLight,
  },
  itemDate: {
    marginTop: 4,
    color: 'gray',
    textAlign: 'right',
    fontSize: 12,
  },
});

export default AdminNoticesScreen;
