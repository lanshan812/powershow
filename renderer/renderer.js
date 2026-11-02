'use strict';
const listEl = document.getElementById('list');
const gearEl = document.getElementById('gear');
const refreshEl = document.getElementById('refresh');
const settingsEl = document.getElementById('settings');
const opacityEl = document.getElementById('opacity');
const opacityValEl = document.getElementById('opacity-val');

// 细线风格设备图标（stroke 图标，跟随文字色）
const ICONS = {
  mouse: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round">
    <rect x="7" y="3" width="10" height="18" rx="5"/><line x1="12" y1="7" x2="12" y2="10"/></svg>`,
  keyboard: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round">
    <rect x="3" y="7" width="18" height="10" rx="2"/><line x1="7" y1="11" x2="7" y2="11"/><line x1="11" y1="11" x2="11" y2="11"/><line x1="15" y1="11" x2="15" y2="11"/><line x1="9" y1="14" x2="15" y2="14"/></svg>`,
  headset: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round">
    <path d="M4 14v-2a8 8 0 0 1 16 0v2"/><rect x="3" y="14" width="4" height="6" rx="2"/><rect x="17" y="14" width="4" height="6" rx="2"/></svg>`,
  gamepad: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round">
    <path d="M6 9h12a4 4 0 0 1 4 4l-1 5a2 2 0 0 1-3.6.8L15.5 16h-7L6.6 18.8A2 2 0 0 1 3 18l-1-5a4 4 0 0 1 4-4z"/><line x1="8" y1="12" x2="8" y2="14"/><line x1="7" y1="13" x2="9" y2="13"/><circle cx="16" cy="12" r="0.4"/><circle cx="18" cy="13" r="0.4"/></svg>`,
  device: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round">
    <rect x="5" y="8" width="14" height="9" rx="2"/><line x1="19" y1="11" x2="21" y2="11"/><line x1="8" y1="11" x2="8" y2="14"/></svg>`,
};

// iOS 风格电池胶囊
function batt(level) {
  const cls = level < 15 ? 'low' : level < 35 ? 'warn' : '';
  const w = Math.max(2, (40 - 4 - 2) * level / 100); // 内部可用宽度
  return `
    <div class="batt ${cls}">
      <div class="body"><div class="fill" style="width:${w.toFixed(1)}px"></div></div>
      <div class="cap"></div>
    </div>`;
}

function render(devices) {
  listEl.innerHTML = devices
    .map((d) => `
      <div class="device">
        <span class="name" title="${d.name}">${d.name}</span>
        ${batt(d.level)}
        <span class="pct ${d.level < 15 ? 'low' : ''}">${d.level}%</span>
      </div>`)
    .join('');
}

// 立即刷新：转一圈动画 + 重新拉取
refreshEl.addEventListener('click', async (e) => {
  e.stopPropagation();
  refreshEl.classList.remove('spinning');
  void refreshEl.offsetWidth; // 重启动画
  refreshEl.classList.add('spinning');
  try {
    render(await window.battery.fetch());
  } catch (err) {
    listEl.innerHTML = `<div class="error">${err.message || err}</div>`;
  }
});

// 设置面板：齿轮切换显示，滑杆调透明度
gearEl.addEventListener('click', (e) => {
  e.stopPropagation();
  settingsEl.classList.toggle('hidden');
});

// 点击面板外区域自动收起
document.addEventListener('click', (e) => {
  if (!settingsEl.classList.contains('hidden') && !settingsEl.contains(e.target)) {
    settingsEl.classList.add('hidden');
  }
});

// bgAlpha 只作用于卡片背景（CSS 变量），文字与电池胶囊保持不透明
function applyBgAlpha(alpha) {
  if (alpha != null) {
    document.documentElement.style.setProperty('--bg-alpha', alpha);
  }
}

opacityEl.addEventListener('input', () => {
  const alpha = Number(opacityEl.value) / 100;
  opacityValEl.textContent = `${opacityEl.value}%`;
  applyBgAlpha(alpha);
  window.battery.setSettings({ bgAlpha: alpha });
});

window.battery.getSettings().then(({ bgAlpha }) => {
  const pct = Math.round((bgAlpha ?? 0.78) * 100);
  opacityEl.value = pct;
  opacityValEl.textContent = `${pct}%`;
  applyBgAlpha(bgAlpha);
});

window.battery.onData(render);
window.battery.onError((msg) => {
  listEl.innerHTML = `<div class="error">${msg}</div>`;
});

window.battery.fetch()
  .then(render)
  .catch((e) => { listEl.innerHTML = `<div class="error">${e.message || e}</div>`; });
