'use strict';
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('battery', {
  onData: (cb) => ipcRenderer.on('battery-data', (_e, devices) => cb(devices)),
  onError: (cb) => ipcRenderer.on('battery-error', (_e, msg) => cb(msg)),
  fetch: () => ipcRenderer.invoke('get-batteries'),
  getSettings: () => ipcRenderer.invoke('get-settings'),
  setSettings: (patch) => ipcRenderer.invoke('set-settings', patch),
});
