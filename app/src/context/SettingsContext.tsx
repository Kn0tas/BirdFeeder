import React, { createContext, useContext, useState, useEffect } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';

interface Settings {
  deviceIp: string;
  notificationsEnabled: boolean;
}

interface SettingsContextType {
  settings: Settings;
  updateSettings: (updates: Partial<Settings>) => void;
  baseUrl: string;
}

const defaultSettings: Settings = {
  deviceIp: '192.168.1.100',
  notificationsEnabled: true,
};

const SettingsContext = createContext<SettingsContextType>({
  settings: defaultSettings,
  updateSettings: () => {},
  baseUrl: '',
});

export function useSettings() {
  return useContext(SettingsContext);
}

export function SettingsProvider({ children }: { children: React.ReactNode }) {
  const [settings, setSettings] = useState<Settings>(defaultSettings);

  useEffect(() => {
    AsyncStorage.getItem('birdfeeder_settings').then((stored) => {
      if (stored) {
        try {
          setSettings({ ...defaultSettings, ...JSON.parse(stored) });
        } catch {}
      }
    });
  }, []);

  const updateSettings = (updates: Partial<Settings>) => {
    setSettings((prev) => {
      const next = { ...prev, ...updates };
      AsyncStorage.setItem('birdfeeder_settings', JSON.stringify(next));
      return next;
    });
  };

  const baseUrl = `http://${settings.deviceIp}`;

  return (
    <SettingsContext.Provider value={{ settings, updateSettings, baseUrl }}>
      {children}
    </SettingsContext.Provider>
  );
}
