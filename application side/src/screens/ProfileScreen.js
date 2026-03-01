import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity, ScrollView, Image } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { SafeAreaView } from 'react-native-safe-area-context';
import { useAuth } from '@/context/AuthContext';
import { COLORS } from '@/constants/colors';

const ProfileScreen = ({ navigation }) => {
  const { logout } = useAuth();

  const menuItems = [
    { title: 'Account', items: [
      { icon: 'person-outline', label: 'Edit Profile', screen: 'EditProfile' },
      { icon: 'call-outline', label: 'Emergency Contacts', screen: 'EmergencyContacts' },
    ]},
    { title: 'Settings', items: [
      { icon: 'notifications-outline', label: 'Notifications', screen: 'Settings' },
      { icon: 'shield-checkmark-outline', label: 'Privacy & Security', screen: 'PrivacySecurity' },
    ]},
    { title: 'Help & Support', items: [
      { icon: 'help-circle-outline', label: 'Help Center', screen: 'HelpCenter' },
      { icon: 'document-text-outline', label: 'Terms & Conditions', screen: 'Terms' },
      { icon: 'information-circle-outline', label: 'About', screen: 'About' },
    ]},
  ];

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView showsVerticalScrollIndicator={false}>
        <View style={styles.header}>
          <Image source={require('@/assets/icon.png')} style={styles.avatar} />
          <Text style={styles.name}>User Name</Text>
          <Text style={styles.email}>user.email@example.com</Text>
        </View>

        {menuItems.map((section, index) => (
          <View key={index} style={styles.section}>
            <Text style={styles.sectionTitle}>{section.title}</Text>
            <View style={styles.menuCard}>
              {section.items.map((item, itemIndex) => (
                <TouchableOpacity key={itemIndex} style={styles.menuItem} onPress={() => item.screen && navigation.navigate(item.screen)}>
                  <View style={styles.menuItemLeft}>
                    <Ionicons name={item.icon} size={22} color={COLORS.primary} />
                    <Text style={styles.menuItemLabel}>{item.label}</Text>
                  </View>
                  <Ionicons name="chevron-forward" size={20} color={COLORS.textLight} />
                </TouchableOpacity>
              ))}
            </View>
          </View>
        ))}

        <TouchableOpacity style={styles.logoutButton} onPress={logout}>
          <Ionicons name="log-out-outline" size={22} color={COLORS.danger} />
          <Text style={styles.logoutText}>Logout</Text>
        </TouchableOpacity>
      </ScrollView>
    </SafeAreaView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
  },
  header: {
    alignItems: 'center',
    padding: 20,
    backgroundColor: COLORS.white,
    marginBottom: 10,
  },
  avatar: {
    width: 100,
    height: 100,
    borderRadius: 50,
    marginBottom: 10,
  },
  name: {
    fontSize: 22,
    fontWeight: 'bold',
  },
  email: {
    color: COLORS.textLight,
  },
  section: {
    marginBottom: 20,
    paddingHorizontal: 15,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    marginBottom: 10,
    marginLeft: 5,
  },
  menuCard: {
    backgroundColor: COLORS.white,
    borderRadius: 12,
  },
  menuItem: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    padding: 15,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
  },
  menuItemLeft: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  menuItemLabel: {
    marginLeft: 15,
    fontSize: 16,
  },
  logoutButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: COLORS.white,
    marginHorizontal: 15,
    paddingVertical: 15,
    borderRadius: 12,
    marginTop: 20,
  },
  logoutText: {
    color: COLORS.danger,
    marginLeft: 10,
    fontWeight: '600',
    fontSize: 16,
  },
});

export default ProfileScreen;
