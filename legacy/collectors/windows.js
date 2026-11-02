'use strict';
// Windows：蓝牙外设电量会作为 Win32_Battery 实例出现（蓝牙 HID over GATT 电量上报）。
// 备用方案：PowerShell PnP 设备的 BatteryReportValue。
const { execFile } = require('child_process');

function ps(script) {
  return new Promise((resolve, reject) => {
    execFile('powershell.exe',
      ['-NoProfile', '-NonInteractive', '-Command', script],
      { timeout: 15000, maxBuffer: 1 << 20, windowsHide: true },
      (err, stdout) => { if (err) reject(err); else resolve(stdout); });
  });
}

function classify(model) {
  if (/keyboard|键盘/i.test(model)) return 'keyboard';
  if (/mouse|鼠标|trackpad/i.test(model)) return 'mouse';
  if (/headset|headphone|耳机|earbud/i.test(model)) return 'headset';
  return 'device';
}

// 主方案：Win32_Battery（含蓝牙外设虚拟电池）
async function viaWin32Battery() {
  const out = await ps(`
    Get-CimInstance Win32_Battery |
      Select-Object Name, EstimatedChargeRemaining |
      ConvertTo-Json -Compress`);
  if (!out.trim()) return [];
  const arr = Array.isArray(JSON.parse(out)) ? JSON.parse(out) : [JSON.parse(out)];
  return arr
    .filter((b) => b.EstimatedChargeRemaining != null)
    .map((b) => ({
      id: b.Name,
      name: b.Name || '蓝牙设备',
      level: Math.round(b.EstimatedChargeRemaining),
      type: classify(b.Name || ''),
    }));
}

// 备用方案：PnP 电量属性（部分设备不出现在 Win32_Battery）
async function viaPnp() {
  const out = await ps(`
    Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like 'BTHENUM*' } | ForEach-Object {
      $p = Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName 'DEVPKEY_Device_BatteryReportValue' -ErrorAction SilentlyContinue
      if ($p -and $p.Data -le 100) {
        [pscustomobject]@{ Name = $_.FriendlyName; Level = $p.Data }
      }
    } | ConvertTo-Json -Compress`);
  if (!out.trim()) return [];
  const arr = Array.isArray(JSON.parse(out)) ? JSON.parse(out) : [JSON.parse(out)];
  return arr.map((b) => ({
    id: b.Name,
    name: b.Name || '蓝牙设备',
    level: Math.round(b.Level),
    type: classify(b.Name || ''),
  }));
}

async function getBatteries() {
  let list = [];
  try { list = await viaWin32Battery(); } catch { /* 降级 */ }
  if (!list.length) {
    try { list = await viaPnp(); } catch { /* 全部失败 */ }
  }
  if (!list.length) throw new Error('未发现上报电量的蓝牙外设');
  return list;
}

module.exports = { getBatteries };
