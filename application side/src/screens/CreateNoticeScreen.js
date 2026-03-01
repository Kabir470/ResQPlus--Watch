import React, { useState } from 'react';
import { View, Text, TextInput, StyleSheet, TouchableOpacity, ActivityIndicator, Alert } from 'react-native';
import { collection, addDoc, serverTimestamp } from 'firebase/firestore';
import { firestore, auth } from '@/utils/firebase';
import { COLORS } from '@/constants/colors';

const CreateNoticeScreen = ({ navigation }) => {
  const [title, setTitle] = useState('');
  const [message, setMessage] = useState('');
  const [loading, setLoading] = useState(false);

  const handlePostNotice = async () => {
    if (!title.trim() || !message.trim()) {
      return Alert.alert('Error', 'Title and message cannot be empty.');
    }
    setLoading(true);
    try {
      const user = auth.currentUser;
      await addDoc(collection(firestore, 'notices'), {
        title: title,
        message: message,
        issuedBy: user.displayName || 'Government Authority',
        authorId: user.uid,
        timestamp: serverTimestamp(),
      });
      navigation.goBack();
    } catch (error) {
      Alert.alert('Error', 'Could not post the notice. Please try again.');
      console.error(error);
    } finally {
      setLoading(false);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Post a Government Notice</Text>
      <TextInput
        style={styles.input}
        placeholder="Notice Title"
        value={title}
        onChangeText={setTitle}
      />
      <TextInput
        style={[styles.input, styles.textArea]}
        placeholder="Notice Message"
        multiline
        value={message}
        onChangeText={setMessage}
      />
      <TouchableOpacity style={styles.postButton} onPress={handlePostNotice} disabled={loading}>
        {loading ? (
          <ActivityIndicator color={COLORS.white} />
        ) : (
          <Text style={styles.postButtonText}>Post Notice</Text>
        )}
      </TouchableOpacity>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
    padding: 20,
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
    marginBottom: 20,
  },
  input: {
    backgroundColor: COLORS.white,
    borderRadius: 10,
    padding: 15,
    fontSize: 16,
    marginBottom: 20,
  },
  textArea: {
    textAlignVertical: 'top',
    minHeight: 200,
  },
  postButton: {
    backgroundColor: COLORS.primary,
    paddingVertical: 15,
    borderRadius: 10,
    alignItems: 'center',
  },
  postButtonText: {
    color: COLORS.white,
    fontSize: 16,
    fontWeight: '600',
  },
});

export default CreateNoticeScreen;
