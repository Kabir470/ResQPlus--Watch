import { useEffect } from 'react';
import { Alert } from 'react-native';
import * as Updates from 'expo-updates';

const useAppUpdateCheck = () => {
  const { isUpdateAvailable, isUpdatePending, fetchUpdateAsync, reloadAsync } = Updates.useUpdates();

  useEffect(() => {
    if (isUpdateAvailable) {
      const fetchAndApplyUpdate = async () => {
        try {
          await fetchUpdateAsync();
          await reloadAsync();
        } catch (error) {
          Alert.alert("Update Error", "Could not download or apply the latest update.");
        }
      };
      
      Alert.alert(
        "Update Available",
        "A new version of the app is available. Would you like to update now?",
        [
          { text: "Later", style: "cancel" },
          { text: "Update Now", onPress: fetchAndApplyUpdate },
        ]
      );
    }
  }, [isUpdateAvailable, fetchUpdateAsync, reloadAsync]);
};

export default useAppUpdateCheck;
