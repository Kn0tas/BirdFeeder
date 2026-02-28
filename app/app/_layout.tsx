import { Slot } from 'expo-router';
import { StatusBar } from 'expo-status-bar';
import { View } from 'react-native';
import { SettingsProvider } from '../src/context/SettingsContext';

export default function RootLayout() {
  return (
    <SettingsProvider>
      <StatusBar style="light" />
      <View style={{ flex: 1 }}>
        <Slot />
      </View>
    </SettingsProvider>
  );
}
