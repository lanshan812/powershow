use serde::{Deserialize, Serialize};
use std::process::Command;

#[derive(Serialize)]
pub struct Device {
    pub id: String,
    pub name: String,
    pub level: i32,
    #[serde(rename = "type")]
    pub kind: String,
}

fn classify(name: &str) -> &'static str {
    let n = name.to_lowercase();
    if n.contains("keyboard") || name.contains("键盘") {
        "keyboard"
    } else if n.contains("mouse") || name.contains("鼠标") || n.contains("trackpad") {
        "mouse"
    } else if n.contains("headset")
        || n.contains("headphone")
        || name.contains("耳机")
        || n.contains("earbud")
        || n.contains("freebuds")
    {
        "headset"
    } else {
        "device"
    }
}

#[cfg(target_os = "linux")]
fn collect() -> Result<Vec<Device>, String> {
    let out = Command::new("upower")
        .arg("--dump")
        .output()
        .map_err(|e| format!("无法执行 upower: {e}"))?;
    let dump = String::from_utf8_lossy(&out.stdout);
    let mut devices = Vec::new();
    let mut cur: Option<Device> = None;
    let mut has_level = false;

    for line in dump.lines() {
        let Some((key, value)) = line.trim().split_once(':') else {
            continue;
        };
        let (key, value) = (key.trim(), value.trim());
        match key {
            "Device" => {
                if let Some(d) = cur.take() {
                    if has_level {
                        devices.push(d);
                    }
                }
                has_level = false;
            }
            "native-path" if value.contains("/org/bluez/") => {
                cur = Some(Device {
                    id: value.to_string(),
                    name: String::new(),
                    level: 0,
                    kind: "device".into(),
                });
            }
            "native-path" => {} // 内置电池/AC 等，忽略
            "model" => {
                if let Some(d) = cur.as_mut() {
                    d.name = value.to_string();
                    d.kind = classify(value).into();
                }
            }
            "percentage" => {
                if let Some(d) = cur.as_mut() {
                    let lv = value.trim_end_matches('%').trim().parse::<f32>().unwrap_or(-1.0);
                    if lv >= 0.0 {
                        d.level = lv.round() as i32;
                        has_level = true;
                    }
                }
            }
            _ => {}
        }
    }
    if let Some(d) = cur.take() {
        if has_level {
            devices.push(d);
        }
    }
    if devices.is_empty() {
        return Err("未发现上报电量的蓝牙外设".into());
    }
    Ok(devices)
}

#[cfg(target_os = "windows")]
fn ps(script: &str) -> Result<String, String> {
    let out = Command::new("powershell.exe")
        .args(["-NoProfile", "-NonInteractive", "-Command", script])
        .output()
        .map_err(|e| format!("PowerShell 执行失败: {e}"))?;
    Ok(String::from_utf8_lossy(&out.stdout).to_string())
}

#[cfg(target_os = "windows")]
fn collect() -> Result<Vec<Device>, String> {
    let script = "Get-CimInstance Win32_Battery | Select-Object Name, EstimatedChargeRemaining | ConvertTo-Json -Compress";
    let mut devices = Vec::new();
    if let Ok(out) = ps(script) {
        if let Ok(v) = serde_json::from_str::<serde_json::Value>(out.trim()) {
            let arr = match v {
                serde_json::Value::Array(a) => a,
                other => vec![other],
            };
            for item in arr {
                if let Some(level) = item.get("EstimatedChargeRemaining").and_then(|x| x.as_i64()) {
                    let name = item
                        .get("Name")
                        .and_then(|x| x.as_str())
                        .unwrap_or("蓝牙设备")
                        .to_string();
                    devices.push(Device {
                        id: name.clone(),
                        kind: classify(&name).into(),
                        level: level as i32,
                        name,
                    });
                }
            }
        }
    }
    if devices.is_empty() {
        return Err("未发现上报电量的蓝牙外设".into());
    }
    Ok(devices)
}

#[tauri::command]
fn get_batteries() -> Result<Vec<Device>, String> {
    collect()
}

#[derive(Serialize, Deserialize, Default)]
pub struct Settings {
    pub bg_alpha: Option<f64>,
}

fn state_path(app: &tauri::AppHandle) -> std::path::PathBuf {
    use tauri::Manager;
    let dir = app.path().app_config_dir().unwrap_or_default();
    let _ = std::fs::create_dir_all(&dir);
    dir.join("state.json")
}

#[tauri::command]
fn get_settings(app: tauri::AppHandle) -> Settings {
    std::fs::read_to_string(state_path(&app))
        .ok()
        .and_then(|s| serde_json::from_str(&s).ok())
        .unwrap_or_default()
}

#[tauri::command]
fn set_settings(app: tauri::AppHandle, settings: Settings) -> Result<(), String> {
    let p = state_path(&app);
    let json = serde_json::to_string(&settings).map_err(|e| e.to_string())?;
    std::fs::write(p, json).map_err(|e| e.to_string())
}

pub fn run() {
    use tauri::menu::{Menu, MenuItem};
    use tauri::tray::TrayIconBuilder;
    use tauri::{Emitter, Manager};

    tauri::Builder::default()
        .setup(|app| {
            let refresh = MenuItem::with_id(app, "refresh", "立即刷新", true, None::<&str>)?;
            let toggle = MenuItem::with_id(app, "toggle", "显示/隐藏", true, None::<&str>)?;
            let quit = MenuItem::with_id(app, "quit", "退出", true, None::<&str>)?;
            let menu = Menu::with_items(app, &[&refresh, &toggle, &quit])?;

            TrayIconBuilder::new()
                .icon(app.default_window_icon().unwrap().clone())
                .tooltip("PowerShow")
                .menu(&menu)
                .show_menu_on_left_click(false)
                .on_menu_event(|app, event| match event.id.as_ref() {
                    "refresh" => {
                        let _ = app.emit("refresh-request", ());
                    }
                    "toggle" => {
                        if let Some(w) = app.get_webview_window("main") {
                            let _ = if w.is_visible().unwrap_or(false) {
                                w.hide()
                            } else {
                                w.show()
                            };
                        }
                    }
                    "quit" => app.exit(0),
                    _ => {}
                })
                .on_tray_icon_event(|tray, event| {
                    if let tauri::tray::TrayIconEvent::Click {
                        button: tauri::tray::MouseButton::Left,
                        button_state: tauri::tray::MouseButtonState::Up,
                        ..
                    } = event
                    {
                        let app = tray.app_handle();
                        if let Some(w) = app.get_webview_window("main") {
                            let _ = if w.is_visible().unwrap_or(false) {
                                w.hide()
                            } else {
                                w.show()
                            };
                        }
                    }
                })
                .build(app)?;
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            get_batteries,
            get_settings,
            set_settings
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
#[cfg(test)]
mod tests {
    #[test]
    fn collects_devices() {
        let d = super::collect().expect("采集失败");
        for x in &d {
            println!("{} {}% [{}]", x.name, x.level, x.kind);
        }
        assert!(!d.is_empty());
    }
}
