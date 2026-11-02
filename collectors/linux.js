'use strict';
// Linux：用 upower --dump 枚举蓝牙外设电量（bluetoothd -> upower 转发）
const { execFile } = require('child_process');

function run(cmd, args) {
  return new Promise((resolve, reject) => {
    execFile(cmd, args, { timeout: 8000, maxBuffer: 1 << 20 }, (err, stdout) => {
      if (err) reject(err); else resolve(stdout);
    });
  });
}

// 中文/英文环境的类别关键词都覆盖
const KEYBOARD_RE = /keyboard|键盘/i;
const MOUSE_RE = /mouse|鼠标|trackpad|触控板/i;
const HEADSET_RE = /headset|headphone|耳机|耳麦|earbud/i;

function classify(model) {
  if (KEYBOARD_RE.test(model)) return 'keyboard';
  if (MOUSE_RE.test(model)) return 'mouse';
  if (HEADSET_RE.test(model)) return 'headset';
  return 'device';
}

function parseDump(dump) {
  const devices = [];
  let cur = null;
  for (const line of dump.split('\n')) {
    const m = line.match(/^\s*([A-Za-z][\w-]*):\s*(.*)$/);
    if (!m) continue;
    const [, key, value] = m;
    if (key === 'Device') { cur = null; continue; } // 新设备块开始（如 DisplayDevice 无 native-path）
    if (key === 'native-path') {
      // 只有蓝牙设备进入采集；遇到其他设备（AC/内置电池）重置，避免串数据
      cur = /\/org\/bluez\//.test(value)
        ? { id: value.trim(), name: '', level: null, type: 'device', isBattery: false }
        : null;
      if (cur) devices.push(cur);
      continue;
    }
    if (!cur) continue;
    if (key === 'model') cur.name = value.trim();
    else if (key === 'percentage') { cur.level = Math.round(parseFloat(value)); cur.isBattery = true; }
    else if (key === 'type') cur.type = classify(cur.name || value.trim());
    else if (key === 'warning-level' && /last/i.test(value) && cur.level != null) {
      cur.level = 1; // upower 有时只给 last 级别不给百分比
    }
  }
  return devices
    .filter((d) => d.isBattery && d.level != null)
    .map(({ id, name, level, type }) => ({ id, name: name || '蓝牙设备', level, type }));
}

async function getBatteries() {
  const dump = await run('upower', ['--dump']);
  const list = parseDump(dump);
  if (!list.length) throw new Error('未发现上报电量的蓝牙外设');
  return list;
}

module.exports = { getBatteries };
