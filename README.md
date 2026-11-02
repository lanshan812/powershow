# PowerShow

轻量的桌面悬浮电量卡片，实时显示本机蓝牙外设（键盘 / 鼠标 / 耳机 / 手柄）的剩余电量。

基于 **Tauri 2**（Rust + 系统 WebView）构建：主程序仅 ~3.6 MB，Linux deb 包 1.5 MB，内存占用约为 Electron 版的一半。

## 特性

- **磨砂悬浮卡片**：无边框、不进任务栏、不抢焦点，顶部区域可拖动
- **iOS 风格电池胶囊**：电量 <35% 变黄、<15% 变红
- **自动深浅双主题**：跟随系统 `prefers-color-scheme`
- **背景透明度可调**：只调背景，文字与电量显示保持清晰
- **立即刷新**：标题栏刷新按钮 + 60 秒自动刷新 + 托盘菜单刷新
- **系统托盘**：刷新 / 显示隐藏 / 退出

## 下载安装

前往 [Releases](https://github.com/lanshan812/powershow/releases) 下载对应平台：

| 平台 | 文件 | 使用方式 |
|---|---|---|
| Windows | `PowerShow_x.x.x_x64-setup.exe` | 直接安装 |
| Windows（免安装） | `PowerShow-x.x.x-portable.zip` | 解压后双击 `PowerShow.exe` |
| Linux | `powershow_x.x.x_amd64.deb` | `sudo dpkg -i` 安装 |
| Linux | `PowerShow_x.x.x_amd64.AppImage` | `chmod +x` 后直接运行 |

> Windows 依赖系统自带的 WebView2（Win10/11 已内置）。
> Linux 依赖 webkit2gtk-4.1 等系统库，deb 包会自动安装依赖；AppImage 已自带这些库所以体积较大。

## 从源码构建

需要 Rust 工具链与平台依赖（Linux：`libwebkit2gtk-4.1-dev libgtk-3-dev libayatana-appindicator3-dev`）。

```bash
cd src-tauri
cargo build --release        # 产出 target/release/powershow
cargo test --release         # 采集逻辑测试
npx @tauri-apps/cli build    # 打包 AppImage / deb
```

推送 `v*` 标签会自动触发 GitHub Actions 构建 Windows 安装包与免安装 zip，并发布到 Releases。

## 项目结构

```
ui/                    # 前端（HTML + CSS + JS）
  index.html
  style.css            # 磨砂卡片、电池胶囊、深浅双主题
  renderer.js          # 列表渲染、设置、刷新交互（Tauri invoke）
src-tauri/
  src/lib.rs           # 电量采集（upower / PowerShell）、设置持久化、托盘
  src/main.rs
  tauri.conf.json      # 窗口（透明/无边框/不置顶）、打包配置
  capabilities/        # Tauri 权限
  icons/
legacy/                # v1 Electron 实现（保留参考）
```

## 电量采集原理

- **Linux**：读取 `upower --dump`，蓝牙外设电量经 bluez 的 Battery1 接口转发到 UPower
- **Windows**：`Win32_Battery`（蓝牙 HID over GATT 的虚拟电池），取 `EstimatedChargeRemaining`

> 蓝牙外设电量由设备固件估算（通常基于电压），会在唤醒/负载变化时跳动，属正常现象。

## License

MIT