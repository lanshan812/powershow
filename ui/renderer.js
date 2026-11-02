'use strict';
const { invoke } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;

const listEl = document.getElementById('list');
const gearEl = document.getElementById('gear');
const refreshEl = document.getElementById('refresh');
const settingsEl = document.getElementById('settings');
const opacityEl = document.getElementById('opacity');
const opacityValEl = document.getElementById('opacity-val');

// iOS 风格电池胶囊
function batt(level) {
  const cls = level < 15 ? 'low' : level < 35 ? 'warn' : '';
  const w = Math.max(2, (40 - 4 - 2) * level / 100);
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

async function refresh() {
  try {
    render(await invoke('get_batteries'));
  } catch (err) {
    listEl.innerHTML = `<div class="error">${err}</div>`;
  }
}

refreshEl.addEventListener('click', async (e) => {
  e.stopPropagation();
  refreshEl.classList.remove('spinning');
  void refreshEl.offsetWidth;
  refreshEl.classList.add('spinning');
  await refresh();
});

gearEl.addEventListener('click', (e) => {
  e.stopPropagation();
  settingsEl.classList.toggle('hidden');
});

document.addEventListener('click', (e) => {
  if (!settingsEl.classList.contains('hidden') && !settingsEl.contains(e.target)) {
    settingsEl.classList.add('hidden');
  }
});

function applyBgAlpha(alpha) {
  if (alpha != null) {
    document.documentElement.style.setProperty('--bg-alpha', alpha);
  }
}

let bgAlpha = null;

opacityEl.addEventListener('input', () => {
  bgAlpha = Number(opacityEl.value) / 100;
  opacityValEl.textContent = `${opacityEl.value}%`;
  applyBgAlpha(bgAlpha);
  invoke('set_settings', { settings: { bg_alpha: bgAlpha } });
});

(async () => {
  const s = await invoke('get_settings').catch(() => ({}));
  bgAlpha = s.bg_alpha ?? null;
  const pct = Math.round((bgAlpha ?? 0.78) * 100);
  opacityEl.value = pct;
  opacityValEl.textContent = `${pct}%`;
  applyBgAlpha(bgAlpha);

  await refresh();
  setInterval(refresh, 60 * 1000);
  listen('refresh-request', refresh);
})();