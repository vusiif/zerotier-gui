# 项目结构文档

> **基线版本**: v1.0  
> **当前阶段**: Phase 1 — 网络管理  
> **最后更新**: 2026-05-31

---

## ⚠️ 关于参考项目

当前工作区中的代码为**参考实现**。本文档描述的是 Phase 1 **目标系统的推荐架构**，不保证与现有代码一致。实现时可以自由重构文件组织、类层次、命名等，只需遵循本文档定义的模块职责和分层原则即可。**以下目录结构中标注 `[远期]` 的文件在 Phase 1 不创建，仅展示扩展方向。**

---

## 一、源码目录结构

```
zerotier-gui/
├── CMakeLists.txt                  # 顶层 CMake 构建脚本
├── main.cpp                        # 应用程序入口，初始化 QApplication
│
├── MainWindow.h                    # 主窗口头文件
├── MainWindow.cpp                  # 主窗口实现：Tab 管理、输出控制台、状态栏、托盘
│
├── tabs/                           # 功能标签页（每个 Tab 对应一个功能模块，可按阶段扩展）
│   ├── ServiceTab.h / .cpp         # [Phase 1] 服务管理：启动/停止/重启、状态检查
│   ├── InfoTab.h / .cpp            # [Phase 1] 节点信息：info 命令 JSON 树形展示
│   ├── PeersTab.h / .cpp           # [Phase 1] Peers 列表：listpeers 表格展示、缓存清理
│   ├── NetworkTab.h / .cpp         # [Phase 1] 网络管理：join/leave/list、状态追踪、get/set
│   ├── MoonTab.h / .cpp            # [Phase 1] Moon 管理：.moon 文件导入与列表
│   ├── ChatTab.h / .cpp            # [Phase 2 远期] 文字聊天
│   ├── VoiceTab.h / .cpp           # [Phase 3 远期] 语音通话
│   └── FileTab.h / .cpp            # [Phase 4 远期] 文件传输管理
│
├── core/                           # 核心工具类层
│   ├── ZtClient.h / .cpp           # ZT 命令行客户端封装
│   │                               #   - findZt(): 定位 zerotier-cli 路径
│   │                               #   - 统一的 QProcess 管理
│   │                               #   - JSON 响应解析辅助
│   │                               #   - 错误处理与重试
│   ├── SystemTray.h / .cpp         # 系统托盘管理
│   │                               #   - 托盘图标创建/更新（在线/离线）
│   │                               #   - 右键上下文菜单
│   │                               #   - 单击还原窗口
│   │                               #   - 气泡通知
│   ├── PortChecker.h / .cpp        # 端口占用检测
│   │                               #   - 检测 9993 端口是否被占用
│   │                               #   - 识别占用进程
│   └── Installer.h / .cpp          # ZT 安装/卸载管理
│                                   #   - 下载 MSI 安装包（QNetworkAccessManager）
│                                   #   - 静默安装
│                                   #   - winget 卸载
│                                   #   - 安装状态检测
│
├── resources/                      # Qt 资源文件
│   ├── icons/
│   │   ├── app.ico                 # 应用图标
│   │   ├── online.png              # 在线状态图标
│   │   ├── offline.png             # 离线状态图标
│   │   └── tray.ico                # 托盘图标
│   └── resources.qrc               # Qt 资源描述文件
│
└── .docs/                          # 项目基线文档（本目录）
    ├── feature-list.md             # 功能清单
    ├── tech-stack.md               # 技术栈文档
    ├── project-structure.md        # 项目结构文档（本文档）
    └── api-design.md               # API / 接口设计文档
```

---

## 二、模块职责划分

### 2.1 MainWindow（主窗口）

| 职责 | 说明 |
|------|------|
| Tab 容器管理 | 持有 QTabWidget，延迟创建各 Tab 内容 |
| 输出控制台 | 统一的命令输出日志区域（QTextEdit） |
| 状态栏 | 显示 ZT 版本、在线状态、网络数、Peers 数 |
| 系统托盘 | 创建 SystemTray 实例，处理最小化/还原 |
| 全局定时器 | 驱动各 Tab 的自动刷新 |
| 管理员权限 | 启动时检测并请求 UAC 提权 |

### 2.2 tabs/（功能标签页）

> **扩展机制**: 每个功能模块对应一个 Tab 类，遵循统一构造签名 `XxxTab(MainWindow*, QWidget* parent)`。新增标签页只需：① 在 `tabs/` 下创建类文件；② 在主窗口的 Tab 注册逻辑中添加新条目；③ 在 CMakeLists.txt 中添加源文件。**无需修改现有 Tab 代码**。远期 Phase 2/3/4 的 ChatTab、VoiceTab、FileTab 均按此模式接入。

| 类名 | 阶段 | 职责描述 |
|------|------|---------|
| ServiceTab | Phase 1 | ZT 服务生命周期管理：启动/停止/重启、状态指示、安装/卸载入口 |
| InfoTab | Phase 1 | 节点信息展示：info 命令 JSON 解析与结构化视图、端口占用检测入口 |
| PeersTab | Phase 1 | Peers 列表管理：listpeers 表格展示、角色/延迟过滤排序、缓存清理 |
| NetworkTab | Phase 1 | 网络管理：join/leave/list、状态追踪定时器、get/set 设置面板、详情面板 |
| MoonTab | Phase 1 | Moon 节点管理：.moon 文件导入、moon.d 目录列表 |
| ChatTab | Phase 2（远期） | 文字聊天：基于 ZT 虚拟 IP 的 P2P TCP 通信 |
| VoiceTab | Phase 3（远期） | 语音通话：基于 ZT 虚拟 IP 的音频流传输 |
| FileTab | Phase 4（远期） | 文件传输：基于 ZT 虚拟局域网的文件共享 |

### 2.3 core/（工具类层）

| 类名 | 职责 | 关键方法 |
|------|------|---------|
| ZtClient | 封装 zerotier-cli 命令执行 | `exec(args)`, `execJson(args)`, `findZt()` |
| SystemTray | 系统托盘管理 | `updateStatus(bool online)`, `showNotification(text)` |
| PortChecker | 端口占用检测 | `isPort9993Available()`, `getOccupyingProcess()` |
| Installer | ZT 软件安装管理 | `download()`, `install()`, `uninstall()`, `isInstalled()` |

---

## 三、CMake 构建配置（推荐）

```cmake
cmake_minimum_required(VERSION 3.16)
project(zerotier_gui LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(SOURCES
    main.cpp
    MainWindow.cpp
    tabs/ServiceTab.cpp
    tabs/InfoTab.cpp
    tabs/PeersTab.cpp
    tabs/NetworkTab.cpp
    tabs/MoonTab.cpp
    core/ZtClient.cpp
    core/SystemTray.cpp
    core/PortChecker.cpp
    core/Installer.cpp
)

set(HEADERS
    MainWindow.h
    tabs/ServiceTab.h
    tabs/InfoTab.h
    tabs/PeersTab.h
    tabs/NetworkTab.h
    tabs/MoonTab.h
    core/ZtClient.h
    core/SystemTray.h
    core/PortChecker.h
    core/Installer.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} resources/resources.qrc)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Network)
```

> **说明**: 以上为 Phase 1 推荐配置。Qt6::Network 用于 Installer 的 HTTP 下载；如使用其他下载方式可省略。具体文件列表以实际实现为准。

## 四、扩展性设计要点

### 4.1 标签页扩展机制

新增一个标签页的步骤（以未来 Phase 2 ChatTab 为例）：

1. 创建 `tabs/ChatTab.h` 和 `tabs/ChatTab.cpp`，继承 `QWidget`，实现构造函数 `ChatTab(MainWindow*, QWidget* parent)`
2. 在主窗口的 Tab 管理代码中注册新标签页（名称 + 创建工厂）
3. 在 CMakeLists.txt 的 SOURCES 和 HEADERS 中添加新文件

**无需修改**任何现有 Tab 的代码。MainWindow 通过 `appendOutput()`、服务操作、状态栏更新等公共接口向所有 Tab 提供统一服务。

### 4.2 Phase 2+ 技术预留

为支持远期聊天/语音/文件功能，当前架构做以下预留：

- MainWindow 提供 ZT 虚拟 IP 信息查询接口（通过 InfoTab 获取本机 ZT IP），供后续 Chat/Voice/File Tab 使用
- ZtClient 作为通用命令执行器，远期 Tab 可通过它执行自定义 ZT 命令或直接使用 QProcess 进行 P2P 通信
- 标签页之间通过 MainWindow 中介实现松耦合，避免直接依赖

### 4.3 数据流

```
用户操作 → Tab Widget → ZtClient → QProcess → zerotier-cli → 系统
                                    ↓
                              stdout/stderr
                                    ↓
                              JSON 解析 (ZtClient)
                                    ↓
                           ┌───────┴───────┐
                           ↓               ↓
                      Tab UI 更新     输出控制台
                           ↓
                      MainWindow (状态栏/托盘更新)
```

### 定时刷新流程

```
MainWindow QTimer (全局)
    │
    ├──→ ServiceTab: 检查服务状态 (info)
    ├──→ InfoTab: 刷新节点信息 (info -j)
    ├──→ PeersTab: 刷新 Peers 列表 (listpeers -j)
    ├──→ NetworkTab: 刷新网络列表 + 状态追踪 (listnetworks -j)
    └──→ MoonTab: 刷新 Moon 列表
         │
         └──→ MainWindow: 聚合状态 → 状态栏/托盘更新
```

### 网络状态追踪流程（F-09）

```
用户点击 Join → ZtClient.exec("join", nwid)
    │
    └──→ NetworkTab 启动专属定时器 (间隔 3s)
         │
         └──→ 每次 tick: ZtClient.execJson("listnetworks", "-j")
              │
              ├── status == "OK"         → 绿色标记，停止追踪定时器
              ├── status == "ACCESS_DENIED" → 黄色标记，提示等待认证，继续追踪
              └── status == "NOT_FOUND"  → 红色标记，自动 leave，停止追踪
```
