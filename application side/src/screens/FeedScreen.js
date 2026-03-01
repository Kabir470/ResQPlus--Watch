import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, FlatList, ActivityIndicator, TouchableOpacity, Share, Alert, Modal, TextInput } from 'react-native';
import { collection, onSnapshot, query, orderBy, doc, updateDoc, arrayUnion, arrayRemove, increment, addDoc, serverTimestamp, getDocs } from 'firebase/firestore';
import { firestore, auth } from '@/utils/firebase';
import { COLORS } from '@/constants/colors';
import { Ionicons } from '@expo/vector-icons';

const FeedScreen = ({ navigation }) => {
  const [posts, setPosts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [commentModal, setCommentModal] = useState({ visible: false, postId: null, text: '' });

  useEffect(() => {
     console.log('Firebase project ID at runtime:', firestore.app.options.projectId); // for debugging.

    // --- TEST WRITE BLOCK ---
    const testWrite = async () => {
      try {
        await addDoc(collection(firestore, 'testCollection'), {
          message: 'Test document from app',
          timestamp: serverTimestamp(),
        });
        console.log('Successfully wrote a test document!');
      } catch (e) {
        console.error('Failed to write test document:', e);
      }
    };
    testWrite();
    // --- end of TEST WRITE BLOCK ---
    const postsQuery = query(collection(firestore, 'posts'), orderBy('timestamp', 'desc'));

    (async () => {
      try {
        const snap = await getDocs(postsQuery);
        const initial = snap.docs.map(d => ({ id: d.id, likes: 0, commentsCount: 0, likedBy: [], ...d.data() }));
        setPosts(initial);
        setLoading(false);
      } catch (e) {
        console.error('Initial posts fetch failed:', e);
        setError('Failed to load feed');
        setLoading(false);
      }
    })();

    const unsubscribe = onSnapshot(
      postsQuery,
      (snapshot) => {
          console.log('Firebase project ID at runtime (onSnapshot):', firestore.app.options.projectId); // for debugging.
          const postsData = snapshot.docs.map(d => ({
          id: d.id,
          likes: 0,
          commentsCount: 0,
          likedBy: [],
          ...d.data(),
        }));
        setPosts(postsData);
        setError(null);
      },
      (err) => {
        setError('Failed to load feed');
        console.error('Feed onSnapshot error:', err);
      }
    );

    return () => unsubscribe();
  }, []);

  const toggleLike = useCallback(async (post) => {
    try {
      const userId = auth.currentUser?.uid;
      if (!userId) {
        Alert.alert('Sign in required', 'Please log in to like posts.');
        return;
      }

      const alreadyLiked = Array.isArray(post.likedBy) && post.likedBy.includes(userId);
      const ref = doc(firestore, 'posts', post.id);

     
      setPosts((prev) => prev.map(p => p.id === post.id ? {
        ...p,
        likes: (p.likes || 0) + (alreadyLiked ? -1 : 1),
        likedBy: alreadyLiked ? (p.likedBy || []).filter(id => id !== userId) : [ ...(p.likedBy || []), userId ],
      } : p));

      await updateDoc(ref, {
        likes: increment(alreadyLiked ? -1 : 1),
        likedBy: alreadyLiked ? arrayRemove(userId) : arrayUnion(userId),
      });
    } catch (e) {
      console.error('Like toggle failed:', e);
      Alert.alert('Error', 'Could not update like.');
    }
  }, []);

  const openCommentModal = useCallback((postId) => {
    setCommentModal({ visible: true, postId, text: '' });
  }, []);

  const submitComment = useCallback(async () => {
    try {
      const { postId, text } = commentModal;
      const trimmed = text.trim();
      if (!postId || !trimmed) {
        return;
      }
      const user = auth.currentUser;
      const authorName = user?.displayName || 'Anonymous User';

    // add comments...
      await addDoc(collection(firestore, 'posts', postId, 'comments'), {
        text: trimmed,
        authorId: user?.uid || 'anon',
        authorName,
        timestamp: serverTimestamp(),
      });

      // for comment count
      await updateDoc(doc(firestore, 'posts', postId), {
        commentsCount: increment(1),
      });

      // Local optimistic bump
      setPosts((prev) => prev.map(p => p.id === postId ? { ...p, commentsCount: (p.commentsCount || 0) + 1 } : p));

      setCommentModal({ visible: false, postId: null, text: '' });
    } catch (e) {
      console.error('Submit comment failed:', e);
      Alert.alert('Error', 'Could not add comment.');
    }
  }, [commentModal]);

  const sharePost = useCallback(async (post) => {
    try {
      const message = `${post.authorName || 'Anonymous'} posted:\n\n${post.content || ''}`;
      await Share.share({ message });
    } catch (e) {
      // ignore errors from Share
    }
  }, []);

  const renderItem = ({ item }) => {
    const ts = item.timestamp?.toDate ? item.timestamp?.toDate() : null;
    const timeText = ts ? new Date(ts).toLocaleString() : 'Just now';
    const likes = item.likes || 0;
    const commentsCount = item.commentsCount || 0;
    return (
      <View style={styles.postContainer}>
        <Text style={styles.postAuthor}>{item.authorName || 'Anonymous'}</Text>
        <Text style={styles.postContent}>{item.content}</Text>
        <View style={styles.actionsRow}>
          <TouchableOpacity style={styles.actionBtn} onPress={() => toggleLike(item)}>
            <Ionicons name="heart" size={20} color={COLORS.danger} />
            <Text style={styles.actionText}>{likes}</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.actionBtn} onPress={() => openCommentModal(item.id)}>
            <Ionicons name="chatbubble-ellipses" size={20} color={COLORS.secondary} />
            <Text style={styles.actionText}>{commentsCount}</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.actionBtn} onPress={() => sharePost(item)}>
            <Ionicons name="share-social" size={20} color={COLORS.accent} />
            <Text style={styles.actionText}>Share</Text>
          </TouchableOpacity>
        </View>
        <Text style={styles.postTime}>{timeText}</Text>
      </View>
    );
  };

  if (loading) {
    return (
      <View style={styles.centered}>
        <ActivityIndicator size="large" color={COLORS.primary} />
        <Text>Loading Feed...</Text>
      </View>
    );
  }

  if (!error && posts.length === 0) {
    return (
      <View style={styles.centered}>
        <Ionicons name="layers-outline" size={40} color={COLORS.textLight} />
        <Text style={{ marginTop: 8 }}>No posts yet</Text>
        <TouchableOpacity style={[styles.retryBtn]} onPress={() => navigation.navigate('Post')}>
          <Text style={{ color: COLORS.white }}>Create your first post</Text>
        </TouchableOpacity>
      </View>
    );
  }

  if (error) {
    return (
      <View style={styles.centered}>
        <Ionicons name="alert-circle" size={40} color={COLORS.warning} />
        <Text style={{ marginTop: 8 }}>{error}</Text>
        <TouchableOpacity style={[styles.retryBtn]} onPress={() => {
          setLoading(true);
          setError(null);
        }}>
          <Text style={{ color: COLORS.white }}>Retry</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.headerTitle}>Community Feed</Text>
        <TouchableOpacity onPress={() => navigation.navigate('Post')}>
            <Ionicons name="add-circle" size={32} color={COLORS.primary} />
        </TouchableOpacity>
      </View>
      <FlatList
        data={posts}
        renderItem={renderItem}
        keyExtractor={item => item.id}
        contentContainerStyle={styles.list}
      />

      {/* Comment modal */}
      <Modal transparent visible={commentModal.visible} animationType="fade" onRequestClose={() => setCommentModal({ visible: false, postId: null, text: '' })}>
        <View style={styles.modalBackdrop}>
          <View style={styles.modalCard}>
            <Text style={styles.modalTitle}>Add a comment</Text>
            <TextInput
              style={styles.modalInput}
              placeholder="Write a comment..."
              value={commentModal.text}
              onChangeText={(t) => setCommentModal((prev) => ({ ...prev, text: t }))}
              multiline
            />
            <View style={styles.modalActions}>
              <TouchableOpacity style={[styles.modalBtn, { backgroundColor: COLORS.lightGray }]} onPress={() => setCommentModal({ visible: false, postId: null, text: '' })}>
                <Text>Cancel</Text>
              </TouchableOpacity>
              <TouchableOpacity style={[styles.modalBtn, { backgroundColor: COLORS.primary }]} onPress={submitComment}>
                <Text style={{ color: COLORS.white }}>Submit</Text>
              </TouchableOpacity>
            </View>
          </View>
        </View>
      </Modal>
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
    backgroundColor: COLORS.white,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
  },
  headerTitle: {
    fontSize: 24,
    fontWeight: 'bold',
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  list: {
    padding: 10,
  },
  postContainer: {
    backgroundColor: COLORS.white,
    borderRadius: 12,
    padding: 15,
    marginBottom: 10,
    shadowColor: COLORS.shadow,
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  postAuthor: {
    fontWeight: 'bold',
    fontSize: 16,
    marginBottom: 5,
  },
  postContent: {
    fontSize: 14,
    lineHeight: 20,
    marginBottom: 10,
  },
  actionsRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 6,
  },
  actionBtn: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 4,
    paddingHorizontal: 8,
    borderRadius: 8,
    backgroundColor: COLORS.lightGray,
    marginRight: 12,
  },
  actionText: {
    fontSize: 13,
    color: COLORS.text,
    marginLeft: 6,
  },
  postTime: {
    fontSize: 12,
    color: COLORS.textLight,
    textAlign: 'right',
  },
  retryBtn: {
    marginTop: 12,
    backgroundColor: COLORS.primary,
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 8,
  },
  modalBackdrop: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.4)',
    justifyContent: 'center',
    alignItems: 'center',
    padding: 24,
  },
  modalCard: {
    width: '100%',
    backgroundColor: COLORS.white,
    borderRadius: 12,
    padding: 16,
  },
  modalTitle: {
    fontSize: 18,
    fontWeight: '600',
    marginBottom: 8,
  },
  modalInput: {
    minHeight: 80,
    borderWidth: 1,
    borderColor: COLORS.border,
    borderRadius: 8,
    padding: 10,
    marginBottom: 12,
  },
  modalActions: {
    flexDirection: 'row',
    justifyContent: 'flex-end',
    gap: 12,
  },
  modalBtn: {
    paddingHorizontal: 14,
    paddingVertical: 10,
    borderRadius: 8,
  },
});

export default FeedScreen;
