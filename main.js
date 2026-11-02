'use strict';
const { app, BrowserWindow, Tray, Menu, ipcMain, screen, nativeImage } = require('electron');
const path = require('path');
const fs = require('fs');

const isWin = process.platform === 'win32';
const isLinux = process.platform === 'linux';
const collector = isWin ? require('./collectors/windows') : require('./collectors/linux');

const REFRESH_MS = 60 * 1000; // 电量变化慢，1 分钟足够
const STATE_FILE = path.join(app.getPath('userData'), 'state.json');

let win = null;
let tray = null;
let timer = null;

function loadState() {
  try { return JSON.parse(fs.readFileSync(STATE_FILE, 'utf8')); } catch { return {}; }
}
function saveState(patch) {
  const s = { ...loadState(), ...patch };
  try { fs.writeFileSync(STATE_FILE, JSON.stringify(s)); } catch {}
}

function createWindow() {
  const state = loadState();
  const { workArea } = screen.getPrimaryDisplay();

  win = new BrowserWindow({
    width: 300,
    height: 190,
    x: state.x ?? workArea.x + workArea.width - 330,
    y: state.y ?? workArea.y + 60,
    frame: false,
    transparent: true,
    resizable: false,
    alwaysOnTop: false,          // 桌面组件化：不置顶，被其他窗口正常遮挡
    skipTaskbar: true,
    hasShadow: true,
    minimizable: false,
    maximizable: false,
    fullscreenable: false,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  // 桌面组件化：所有工作区可见、不抢焦点
  win.setVisibleOnAllWorkspaces(true, { visibleOnFullScreen: true });
  if (isLinux) {
    win.setSkipTaskbar(true);
  }

  win.loadFile(path.join(__dirname, 'renderer', 'index.html'));

  // 记住拖动后的位置
  const savePos = () => { const [x, y] = win.getPosition(); saveState({ x, y }); };
  win.on('moved', savePos);
  win.on('close', savePos);
}

function pushBattery() {
  collector.getBatteries()
    .then((devices) => win?.webContents.send('battery-data', devices))
    .catch((err) => win?.webContents.send('battery-error', String(err.message || err)));
}

function createTray() {
  // 16x16 空图标即可（Wayland 下托盘由扩展渲染）
  const icon = nativeImage.createEmpty();
  tray = new Tray(icon);
  tray.setToolTip('蓝牙电量卡片');
  tray.setContextMenu(Menu.buildFromTemplate([
    { label: '立即刷新', click: pushBattery },
    { label: '显示/隐藏', click: () => (win.isVisible() ? win.hide() : win.show()) },
    { type: 'separator' },
    { label: '退出', click: () => app.quit() },
  ]));
  tray.on('click', () => (win.isVisible() ? win.hide() : win.show()));
}

// renderer 就绪后主动拉取一次，避免首推消息早于监听器注册而丢失
ipcMain.handle('get-batteries', () => collector.getBatteries());

// 设置：bgAlpha 仅调整卡片背景透明度（文字/电池不受影响），由 renderer 应用到 CSS 变量
ipcMain.handle('get-settings', () => ({ bgAlpha: loadState().bgAlpha ?? null }));
ipcMain.handle('set-settings', (_e, patch) => {
  saveState(patch);
  return loadState();
});

app.whenReady().then(() => {
  createWindow();
  createTray();
  pushBattery();
  timer = setInterval(pushBattery, REFRESH_MS);
});

app.on('window-all-closed', () => { /* 有托盘，不退出 */ });
app.on('before-quit', () => clearInterval(timer));
