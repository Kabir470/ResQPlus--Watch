import React, { useState } from 'react';
import { View, Text, TextInput, StyleSheet, TouchableOpacity, ActivityIndicator, Alert } from 'react-native';
import { collection, addDoc, serverTimestamp } from 'firebase/firestore';
import { firestore, auth } from '@/utils/firebase';
import { COLORS } from '@/constants/colors';

const PostScreen = ({ navigation }) => {
  const [content, setContent] = useState('');
  const [loading, setLoading] = useState(false);

  const handlePost = async () => {
    if (!content.trim()) {
      return Alert.alert('Error', 'Post content cannot be empty.');
    }
    setLoading(true);
    try {
      const user = auth.currentUser;
      await addDoc(collection(firestore, 'posts'), {
        content: content,
        authorId: user?.uid || 'anon',
        authorName: user?.displayName || 'Anonymous User',
        timestamp: serverTimestamp(),
        likes: 0,
        commentsCount: 0,
        likedBy: [],
      });
      navigation.goBack();
    } catch (error) {
      Alert.alert('Error', 'Could not submit your post. Please try again.');
      console.error(error);
    } finally {
      setLoading(false);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Create a Post</Text>
      <TextInput
        style={styles.input}
        placeholder="What's on your mind?"
        multiline
        value={content}
        onChangeText={setContent}
      />
      <TouchableOpacity style={styles.postButton} onPress={handlePost} disabled={loading}>
        {loading ? (
          <ActivityIndicator color={COLORS.white} />
        ) : (
          <Text style={styles.postButtonText}>Post</Text>
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
    textAlignVertical: 'top',
    minHeight: 150,
    marginBottom: 20,
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

export default PostScreen;
