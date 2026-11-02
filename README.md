# PowerShow

轻量的桌面悬浮电量卡片，实时显示本机蓝牙外设（键盘 / 鼠标 / 耳机 / 手柄）的剩余电量。

单进程、纯 **Qt Widgets + QPainter 自绘**实现：主程序仅 **86 KB**，内存占用约 **20–37 MB**，启动近乎瞬时。

![screenshot](docs/screenshot.png)

## 特性

- **悬浮卡片**：无边框、不被窗口遮挡（桌面组件定位），顶部拖动移动位置
- **iOS 风格电池胶囊**：电量 <35% 变黄、<15% 变红
- **自动深浅双主题**：跟随系统配色
- **背景透明度可调**：齿轮面板滑杆，只调卡片背景，文字与电量显示保持清晰
- **手动刷新**：点击刷新按钮即时拉取（带旋转动画），默认每 60 秒自动刷新
- **位置记忆**：拖动后的位置自动保存，越界自动回退到主屏右上角
- **系统托盘**：立即刷新 / 显示隐藏 / 退出

## 下载安装

前往 [Releases](https://github.com/lanshan812/powershow/releases) 下载对应平台：

| 平台 | 文件 | 使用方式 |
|---|---|---|
| Windows | `PowerShow-qt-*-win-x64.zip` | 解压后双击 `PowerShow.exe`（Qt 运行库已内置） |
| Linux | `PowerShow-qt-*-linux-x64.deb` | `sudo dpkg -i` 安装（依赖系统 Qt6） |
| Linux | `PowerShow-qt-*-linux-x64.tar.gz` | 解压后直接运行 |

> 蓝牙外设需支持电量上报（蓝牙 HID over GATT 的 Battery Service）。电量由设备固件估算（通常基于电压），唤醒或负载变化时跳变属正常现象。

## 从源码构建

依赖：CMake ≥ 3.16、Qt 6 Widgets。

```bash
# Debian / Ubuntu
sudo apt install qt6-base-dev cmake build-essential libgl1-mesa-dev libxkbcommon-dev

cd qt
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/powershow
```

调试命令：

```bash
./build/powershow --dump   # 打印当前采集到的设备与电量后退出
./build/powershow --shot   # 渲染自身画面到 /tmp/card_self.png（无桌面环境时验证绘制）
```

Windows 构建需要 Qt 6（MSVC 或 MinGW 均可），`windeployqt` 打包运行时依赖。

## 项目结构

```
qt/
  src/main.cpp          # 全部逻辑：采集、自绘卡片、交互、托盘（约 700 行）
  CMakeLists.txt
  powershow.desktop     # Linux 桌面入口
  assets/icon.png       # 应用图标
docs/screenshot.png
.github/workflows/release-qt.yml   # 打标签自动构建两平台产物并发布 Release
```

## 实现说明

- **电量采集**
  - Linux：解析 `upower --dump` 中 `native-path` 为 `/org/bluez/*` 的蓝牙设备
  - Windows：读取 `Win32_Battery` 的 `EstimatedChargeRemaining`（蓝牙外设虚拟电池）
- **窗口**：`FramelessWindowHint` + 半透明背景，`QPainter` 绘制圆角卡片、电池胶囊与图标
- **平台细节**
  - Linux 检测到 `DISPLAY` 时优先使用 XWayland（`QT_QPA_PLATFORM=xcb`）——Wayland 协议下客户端无法控制窗口位置
  - Windows 使用系统托盘与原生窗口消息

## 版本历史

| 版本 | 实现 | 内存 (PSS) | 主程序 |
|---|---|---|---|
| v3（当前） | Qt Widgets 自绘 | ~20–37 MB | 86 KB |
| v2 | Tauri 2（Rust + WebKit） | ~71 MB | 3.6 MB |
| v1 | Electron | ~291 MB | — |

## License

MIT