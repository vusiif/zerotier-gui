# 技术栈文档

> **基线版本**: v1.0  
> **当前阶段**: Phase 1 — 网络管理  
> **最后更新**: 2026-05-31

---

## ⚠️ 关于参考项目

当前工作区中的代码为**参考实现**，使用了 Qt6 + CMake + MSVC 技术栈。本文档基于该技术栈制定，但如果重构时有充分理由，允许替换等价技术（如 Qt 版本升级、改用其他构建系统等），只需在下游设计阶段与 Advisor 达成一致即可。

---

## 一、核心技术选型

| 层面 | 技术 | 版本 | 选型理由 |
|------|------|------|---------|
| **编程语言** | C++ | C++17 | 与 Qt 框架天然契合；系统级能力（服务管理、端口检测）；高性能 |
| **GUI 框架** | Qt6 | ≥6.5 | 成熟稳定的跨平台 GUI 框架；丰富的 Widgets 组件；QSystemTrayIcon、QNetworkAccessManager 等开箱即用 |
| **构建系统** | CMake | ≥3.16 | Qt6 官方推荐的构建系统；简洁的依赖管理 |
| **编译器** | MSVC 2022 | 64-bit | Windows 平台标准编译器，与 Qt 二进制兼容 |
| **JSON 解析** | QJsonDocument (Qt6) | — | 内置模块，零额外依赖；解析 `zerotier-cli -j` 输出 |
| **进程管理** | QProcess (Qt6) | — | 执行 zerotier-cli 命令行；异步非阻塞 |
| **系统托盘** | QSystemTrayIcon (Qt6) | — | 跨平台托盘 API |
| **HTTP 下载** | QNetworkAccessManager (Qt6) | — | 下载 ZeroTier MSI 安装包，支持进度回调 |
| **Windows API** | Win32 API | — | UAC 提权（ShellExecuteEx）、服务管理（net/sc）、端口检测（netstat） |

---

## 二、外部依赖

### 2.1 编译期依赖

| 依赖 | 用途 | 获取方式 |
|------|------|---------|
| Qt 6.8.3 (msvc2022_64) | GUI 框架 | 官方安装器或 vcpkg |
| Windows SDK | Win32 API 头文件 | Visual Studio 2022 自带 |

### 2.2 运行时依赖

| 依赖 | 用途 | 备注 |
|------|------|------|
| Qt6::Widgets | GUI 组件 | 动态链接 |
| `zerotier-cli` | ZeroTier 命令行工具 | ZeroTier One 安装后自带 |
| `winget` | 卸载 ZeroTier | Windows 10 1809+ 自带 |
| `curl` | 下载安装包 | Windows 10 1803+ 自带 |
| `net` / `sc` | Windows 服务管理 | 系统自带 |

### 2.3 不引入的第三方库

- **不引入** cpp-httplib / libcurl 等 HTTP 库：下载功能通过 Qt 内置 QNetworkAccessManager 实现
- **不引入** nlohmann/json 等 JSON 库：使用 Qt 自带的 QJsonDocument
- **不引入** spdlog / fmt 等日志库：日志量小，使用 Qt 的 qDebug 即可

---

## 三、架构模式

```
┌─────────────────────────────────────────┐
│              MainWindow                  │
│  ┌─────────────────────────────────┐    │
│  │         QTabWidget              │    │
│  │  ┌──────┐ ┌──────┐ ┌──────┐   │    │
│  │  │Service│ │ Info │ │Peers │   │    │
│  │  │ Tab  │ │ Tab  │ │ Tab  │   │    │
│  │  └──────┘ └──────┘ └──────┘   │    │
│  │  ┌──────┐ ┌──────┐ ┌──────┐   │    │
│  │  │Netw. │ │ Moon │ │(Fut.)│   │    │
│  │  │ Tab  │ │ Tab  │ │ Chat │   │    │
│  │  └──────┘ └──────┘ └──────┘   │    │
│  └─────────────────────────────────┘    │
│  ┌─────────────────────────────────┐    │
│  │       Output Console            │    │
│  └─────────────────────────────────┘    │
│  ┌─────────────────────────────────┐    │
│  │         Status Bar              │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
         │                  │
         ▼                  ▼
   ┌──────────┐    ┌──────────────┐
   │ ZtClient │    │ SystemTray   │
   │ (命令执行) │    │ (托盘管理)    │
   └──────────┘    └──────────────┘
         │
         ▼
   ┌──────────┐
   │ QProcess │
   └──────────┘
```

### 架构说明

- **MainWindow**: 主窗口，持有 Tab 容器、输出控制台、状态栏、系统托盘
- **各 Tab 类**: 独立的功能模块，每个 Tab 拥有自己的 QProcess 实例（或通过 ZtClient），通过 MainWindow 的 `appendOutput()` 输出日志
- **ZtClient**: （新增）抽取 ZT 命令执行为独立工具类，封装 findZt()、命令构建、JSON 解析等通用逻辑，减少各 Tab 中的重复代码
- **SystemTray**: （新增）系统托盘管理类，负责托盘图标、右键菜单、状态更新
- **扩展性**: 新增 Tab 只需在 tabs/ 目录添加类文件 + 在 MainWindow 注册，不修改现有代码

---

## 四、远期技术预留（Phase 2+，仅参考）

以下技术选型仅作为远期架构参考，**不纳入当前 Phase 1 的依赖和实现**：

| 远期功能 | 可能的技术方案 | Qt 模块 |
|---------|--------------|---------|
| 文字聊天 | TCP socket 直连 ZT 虚拟 IP | `Qt6::Network`（QTcpServer/QTcpSocket） |
| 语音通话 | 音频采集/播放/编码/传输 | `Qt6::Multimedia` 或第三方（如 Opus） |
| 文件传输 | TCP 流式传输 + 断点续传 | `Qt6::Network` |
| 聊天记录 | 本地 SQLite 存储 | `Qt6::Sql` |

> **当前 Phase 1 不引入** `Qt6::Network`（除 Installer 下载用的 QNetworkAccessManager）、`Qt6::Multimedia`、`Qt6::Sql` 等模块。

---

## 五、项目文件结构（规划）

```
zerotier-gui/
├── CMakeLists.txt              # 构建配置
├── main.cpp                    # 入口
├── MainWindow.h / .cpp         # 主窗口
├── tabs/                       # 标签页实现（按阶段扩展）
│   ├── ServiceTab.h / .cpp     # [Phase 1] 服务管理
│   ├── InfoTab.h / .cpp        # [Phase 1] 节点信息
│   ├── PeersTab.h / .cpp       # [Phase 1] Peers 列表
│   ├── NetworkTab.h / .cpp     # [Phase 1] 网络管理
│   ├── MoonTab.h / .cpp        # [Phase 1] Moon 管理
│   ├── ChatTab.h / .cpp        # [Phase 2 远期] 文字聊天
│   ├── VoiceTab.h / .cpp       # [Phase 3 远期] 语音通话
│   └── FileTab.h / .cpp        # [Phase 4 远期] 文件传输
├── core/                       # （新增）核心工具类
│   ├── ZtClient.h / .cpp       # ZT 命令执行与 JSON 解析
│   ├── SystemTray.h / .cpp     # 系统托盘管理
│   ├── PortChecker.h / .cpp    # 端口占用检测
│   └── Installer.h / .cpp      # ZT 安装/卸载管理
├── resources/                  # （新增）资源文件
│   ├── icons/                  # 图标（在线/离线/托盘）
│   └── resources.qrc           # Qt 资源文件
└── .docs/                      # 项目文档
    ├── feature-list.md
    ├── tech-stack.md
    ├── project-structure.md
    └── api-design.md
```
