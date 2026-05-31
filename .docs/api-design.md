# API / 接口设计文档

> **基线版本**: v1.0  
> **当前阶段**: Phase 1 — 网络管理  
> **最后更新**: 2026-05-31

---

## 一、概述

本文档定义两层接口：

1. **外部接口**: GUI 程序与 ZeroTier CLI (`zerotier-cli`) 之间的命令调用协议——这是**必须遵守的契约**，不受代码重构影响
2. **内部接口**: 各模块之间的推荐 C++ 类接口签名——这是**推荐设计**，重构时只要保持相同的能力边界，具体类名、方法签名可调整

> **扩展性说明**: ZtClient 作为通用命令执行器，不仅服务于 Phase 1 的 ZT CLI 命令，远期 Phase 2/3/4 的聊天、语音、文件等 Tab 也可复用 ZtClient 或直接通过 MainWindow 暴露的接口获取 ZT 网络状态（如本机虚拟 IP、在线 Peers 列表等），无需修改核心层代码。

---

## 二、外部接口 — ZeroTier CLI 命令封装

### 2.1 ZtClient 核心接口

```cpp
// core/ZtClient.h

#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>

class ZtClient : public QObject {
    Q_OBJECT

public:
    explicit ZtClient(QObject *parent = nullptr);

    // 定位 zerotier-cli 可执行文件路径
    static QString findZt();

    // 异步执行命令，完成时通过信号返回原始文本输出
    void exec(const QStringList &args);

    // 异步执行命令并自动解析为 JSON，完成时通过信号返回
    void execJson(const QStringList &args);

    // 检查是否有命令正在执行
    bool isBusy() const;

signals:
    // 命令执行完成（原始文本）
    void commandFinished(const QString &output);
    // 命令执行完成（JSON Object）
    void jsonObjectReady(const QJsonObject &obj);
    // 命令执行完成（JSON Array）
    void jsonArrayReady(const QJsonArray &arr);
    // 命令执行出错
    void commandError(const QString &errorMessage);
    // 实时标准输出（用于进度显示等）
    void readyReadStandardOutput(const QString &text);

private:
    QProcess *m_process;
};
```

### 2.2 ZT 命令映射表

以下列出所有需要封装的 ZT CLI 命令及对应的 ZtClient 调用方式：

| 功能 | 命令 | 参数 | 返回格式 | 对应方法 |
|------|------|------|---------|---------|
| 获取节点信息 | `info` | `-j` | JSON Object | `execJson({"-j", "info"})` |
| 获取简易状态 | `info` | — | 纯文本 | `exec({"info"})` |
| 获取版本号 | `-v` | — | 纯文本 | `exec({"-v"})` |
| Peers 列表 (JSON) | `listpeers` | `-j` | JSON Array | `execJson({"-j", "listpeers"})` |
| Peers 列表 (详细) | `peers` | `-j` | JSON Array | `execJson({"-j", "peers"})` |
| 网络列表 | `listnetworks` | `-j` | JSON Array | `execJson({"-j", "listnetworks"})` |
| 加入网络 | `join` | `<nwid>` | 纯文本 | `exec({"join", nwid})` |
| 离开网络 | `leave` | `<nwid>` | 纯文本 | `exec({"leave", nwid})` |
| 获取网络设置 | `get` | `<nwid> <setting>` | 纯文本 | `exec({"get", nwid, setting})` |
| 设置网络属性 | `set` | `<nwid> <setting>` | 纯文本 | `exec({"set", nwid, setting})` |
| 调试转储 | `dump` | — | 纯文本 | `exec({"dump"})` |
| 帮助信息 | `-h` | — | 纯文本 | `exec({"-h"})` |

### 2.3 info 命令 JSON 响应结构（关键字段）

```json
{
  "address": "4ca65a9b1f",           // ZT 节点 ID（10位16进制）
  "version": "1.16.1",               // ZT 版本号
  "online": false,                   // 是否在线
  "tcpFallbackActive": false,        // TCP 回退是否激活
  "clock": 1779340412210,            // 时钟（毫秒时间戳）
  "planetWorldId": 149604618,        // 行星世界 ID
  "planetWorldTimestamp": 1567191349589,
  "publicIdentity": "4ca65a9b1f:0:...", // 完整公钥身份
  "config": {
    "settings": {
      "homeDir": "C:\\ProgramData\\ZeroTier\\One",  // 主目录
      "primaryPort": 9993,            // 主端口
      "secondaryPort": 50426,         // 备用端口
      "tertiaryPort": 53077,          // 第三端口
      "listeningOn": ["..."],         // 监听地址列表
      "surfaceAddresses": ["..."],    // 表面地址列表
      "portMappingEnabled": true,
      "allowTcpFallbackRelay": true,
      "forceTcpRelay": false
    }
  }
}
```

### 2.4 listpeers 命令 JSON 响应结构

```json
[{
  "address": "778cde7190",           // Peer ZT ID
  "isBonded": false,
  "latency": -488,                   // 延迟（负数=不可达）
  "role": "PLANET",                  // PLANET | MOON | LEAF
  "tunneled": false,
  "version": "-1.-1.-1",
  "paths": [{
    "active": true,
    "address": "103.195.103.66/9993", // IP:Port
    "expired": false,
    "lastReceive": 1779343539006,
    "lastSend": 0,
    "preferred": true
  }]
}]
```

### 2.5 listnetworks 命令 JSON 响应结构

```json
[{
  "id": "2030210660b1d788",          // 网络 ID（16位16进制）
  "nwid": "2030210660b1d788",        // 同上
  "name": "sky1ine",                 // 网络名称
  "mac": "8a:cb:6f:8e:c3:27",        // 虚拟 MAC 地址
  "status": "OK",                    // OK | NOT_FOUND | ACCESS_DENIED
  "type": "PRIVATE",                 // PRIVATE | PUBLIC
  "mtu": 2800,
  "assignedAddresses": ["10.10.10.132/24"], // 分配的 IP
  "portDeviceName": "ethernet_32774",
  "routes": [{
    "target": "10.10.10.0/24",
    "via": null
  }],
  "dns": {
    "domain": "",
    "servers": []
  }
}]
```

### 2.6 服务管理命令（通过 Windows net 命令）

| 操作 | 命令 | 说明 |
|------|------|------|
| 启动服务 | `net start ZeroTierOneService` | 需管理员权限 |
| 停止服务 | `net stop ZeroTierOneService` | 可能返回错误109（可忽略） |
| 重启服务 | 先 stop 再 start | 无单独命令 |
| 设置手动启动 | `sc config ZeroTierOneService start= demand` | 需管理员权限 |

---

## 三、内部接口 — 核心工具类

### 3.1 SystemTray（系统托盘）

```cpp
// core/SystemTray.h

#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>

class MainWindow;

class SystemTray : public QObject {
    Q_OBJECT

public:
    explicit SystemTray(MainWindow *mainWindow);

    // 更新托盘状态
    void updateStatus(bool online);
    // 显示气泡通知
    void showNotification(const QString &title, const QString &message);

signals:
    void startServiceRequested();
    void stopServiceRequested();
    void showWindowRequested();
    void quitRequested();

private:
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_statusAction;
    QAction *m_startAction;
    QAction *m_stopAction;
    QAction *m_showAction;
    QAction *m_quitAction;
    MainWindow *m_mainWindow;

    void setupMenu();
    void setupIcons();
};
```

### 3.2 PortChecker（端口检测）

```cpp
// core/PortChecker.h

#pragma once

#include <QObject>

class PortChecker : public QObject {
    Q_OBJECT

public:
    explicit PortChecker(QObject *parent = nullptr);

    // 检查 9993 端口是否可用（未被占用）
    // 返回 true 表示端口可用
    bool isPort9993Available();

    // 获取占用 9993 端口的进程名称
    QString getOccupyingProcessName();

    // 异步检测（通过 QProcess 执行 netstat）
    void checkAsync();

signals:
    void portAvailable();
    void portOccupied(const QString &processName);
    void checkError(const QString &error);

private:
    // 通过 Windows netstat 命令检测
    // netstat -ano | findstr :9993
};
```

### 3.3 Installer（安装管理）

```cpp
// core/Installer.h

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class Installer : public QObject {
    Q_OBJECT

public:
    explicit Installer(QObject *parent = nullptr);

    // 检查 ZT 是否已安装
    static bool isZeroTierInstalled();

    // 获取已安装的 ZT 版本
    static QString installedVersion();

    // 下载 MSI 安装包
    void downloadInstaller();

    // 安装已下载的 MSI
    void install();

    // 卸载 ZT（winget）
    void uninstall();

    // 取消当前操作
    void cancel();

signals:
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &filePath);
    void downloadError(const QString &error);
    void installFinished(bool success, const QString &message);
    void uninstallFinished(bool success, const QString &message);

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply = nullptr;
    QString m_downloadPath;
};
```

---

## 四、内部接口 — Tab 与 MainWindow 交互

### 4.1 各 Tab 的通用构造签名

```cpp
// 所有 Tab 的构造函数签名保持一致：
explicit XxxTab(MainWindow *mainWindow, QWidget *parent = nullptr);
```

### 4.2 MainWindow 提供给 Tab 的公共接口

| 方法 | 说明 |
|------|------|
| `void appendOutput(const QString &text)` | 向底部输出控制台追加日志 |
| `void setStatusText(const QString &text)` | 更新状态栏文字（新增） |
| `void updateTrayStatus(bool online)` | 更新托盘图标状态（新增） |

### 4.3 Tab 实现应遵循的约定

- 每个 Tab 拥有自己的 `QProcess *m_proc`（或通过 `ZtClient` 管理）
- Tab 在首次切换到其时通过 `MainWindow::onTabChanged` 延迟创建（已有机制，保留）
- 支持定时刷新的 Tab 应提供 `startAutoRefresh(int intervalMs)` / `stopAutoRefresh()` 方法

---

## 五、网络设置 get/set 支持列表

基于 ZeroTier 文档，支持以下常用网络设置项：

| 设置项 | 说明 | 示例值 |
|--------|------|--------|
| `allowDNS` | 是否允许 DNS 配置 | true / false |
| `allowDefault` | 是否允许默认路由 | true / false |
| `allowGlobal` | 是否允许全局 IP | true / false |
| `allowManaged` | 是否允许管理地址 | true / false |
| `bridge` | 是否桥接模式 | true / false |
| `broadcastEnabled` | 是否启用广播 | true / false |
| `mtu` | MTU 大小 | 2800 |
| *(其他 JSON 属性)* | 通过 `get/set` 直接操作 | — |
