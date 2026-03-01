import React from 'react';
import { View, Text, TextInput, Button, StyleSheet } from 'react-native';

const ReportIncidentScreen = () => {
  return (
    <View style={styles.container}>
      <Text style={styles.title}>Report an Incident</Text>
      <TextInput placeholder="Incident Type (e.g., Accident, Fire)" style={styles.input} />
      <TextInput placeholder="Description" style={[styles.input, styles.textArea]} multiline />
      <Button title="Upload Image" onPress={() => {}} />
      <View style={{ marginTop: 16 }}>
        <Button title="Submit Report" onPress={() => {}} />
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
  },
  title: {
    fontSize: 24,
    marginBottom: 16,
  },
  input: {
    height: 40,
    borderColor: 'gray',
    borderWidth: 1,
    marginBottom: 12,
    padding: 8,
  },
  textArea: {
    height: 100,
  },
});

export default ReportIncidentScreen;
