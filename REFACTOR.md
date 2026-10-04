# ZeroTier GUI — Ela 界面重构

## 构建

Windows、MSVC x64、Qt 6.11.2（Widgets 和 WidgetsPrivate）、Python 3（编译输出转换）。CLion 自带 CMake/Ninja 可直接使用。
项目直接构建 ElaWidgetTools 库子目录，不构建其示例项目；针对新 Qt 的 QChar 枚举构造做了兼容修改。
Ela 使用 Qt 私有接口，部署时必须使用与构建一致的 Qt DLL。保留 ElaWidgetTools-main 下的许可证。

## 已接入

- 安装引导：显示界面前同步检查本地程序和服务；未安装时只创建独立安装窗口，确认后 winget 安装。检测到安装成功后关闭安装窗口并创建管理主窗口；已安装时直接进入管理界面。
- 导航：概览、网络、Peers、Moon、服务、设置与日志。
- 网络加入/离开、Moon orbit/deorbit 与 .moon 文件导入。
- 服务启动/停止/重启继承主程序管理员权限，通过隐藏的异步 PowerShell 进程执行，并等待目标状态。停止报错但服务已停止时可继续重启；其他失败明确提示。
- 启动主界面时自动启动已停止的 ZeroTier 服务，并等待 Running；最多等待 30 秒，期间禁用管理操作，失败或超时明确提示。已运行时直接读取节点信息，不重复启动。
- Windows 主程序使用 requireAdministrator 清单，启动时直接请求管理员权限；取消 UAC 时不启动。
- 深色/浅色主题同时更新原生控件的文字、日志、表格和表头颜色。
- 从 photos 选取六张明暗主题插画，缩小后嵌入 art.qrc；概览、管理页、设置和安装窗口随主题换图。空列表隐藏空表格并显示插画。
- 插画仅在显示时解码，隐藏时释放；单张解码尺寸限制为 960×600。原图不修改，选图记录位于 assets/art/sources.json。
- 页面数据每 10 秒刷新当前页，结果表按需创建，关闭动画过渡。
- 本次 GUI 成功提交加入的网络另行每 5 秒跟踪，切页及最小化不中断。明确 NOT_FOUND 后自动退出，ACCESS_DENIED 等其他状态持续更新。查询失败或缺少条目不退出；退出失败重新查询后重试，最多三次，提示手动处理。
- 跟踪只覆盖当前进程中加入的网络，不自动退出已有网络；手动退出成功后取消跟踪。
- 命令 15 秒超时，JSON 输出上限 1MB；日志限制行数与字符数。
- 管理页面首次打开才创建控件；已更新但未变化的数据不重建表格，保留选择。
- 操作执行中禁用管理按钮，避免重复操作；超时、输出超限与启动失败给出具体原因。
- 服务停止/重启前确认；服务操作超过两分钟结束等待进程并提示检查实际状态，服务本身可能仍在切换。
- 点击网络行自动填入 ID，便于离开指定网络；安装输出支持跨读取块 UTF-8 解码。

## 验证范围与限制

Release 编译通过，未安装 ZeroTier 时启动存活检查通过。
winget 安装流程已由用户实际验证。通过 UAC 的真实只读查询 info/listnetworks/listpeers/listmoons 均成功，ZeroTier 版本 1.16.2，节点当前离线。实际联网、网络成员变更及服务重启后重连未验证。
支持官方 Windows zerotier-one_x64.exe 的 -q CLI 模式；从 bat 所在目录查找原生程序，不执行 batch wrapper。设置页可指定程序位置。
Moon 导入前重新读取 info.config.settings.homeDir，只接受存在的绝对目录，导入到该目录的 moons.d；目录无效时停止导入。CLI 使用运行时识别的目录，手动设置或重置时清除运行时缓存。导入后确认重启，成功后重新读取列表；取消或失败保留文件并提示。启动获取有效 info 后统计本地 .moon 文件，导入后更新统计。
安装输出按 UTF-8 显示；需要重启的安装退出码会给出提示。
旧 tabs 源码保留作迁移参考，不再参与构建。
用户已取消低于 10MB RAM 的验收目标。

## 自动验证与发布

命令执行模拟测试覆盖成功、非零退出、stderr、输出上限、超时、并发保护、启动失败和 Windows -q 参数。
开启 CMake 选项 ZEROTIER_GUI_BUILD_TESTS 后运行 CTest，共十组：maintenance、diagnostic_report、peer_presentation、network_settings、moon_service、network_monitor、service_startup、command_runner、ui_smoke、functional。详见 TEST_REPORT.md。

Peers 页新增缓存清理：读取 info 的 homeDir，确认中断连接后停止服务，只在确认为 Stopped 时将 peers.d 改名为 peers.d.backup-UUID，再启动服务并确认 Running。备份原样保留，不遍历或删除内容；拒绝根目录、相对目录、符号链接/Windows 重解析点和错误类型。失败时尝试恢复此前运行的服务，并明确显示恢复失败或保留的备份路径。设置页新增 winget 卸载入口，使用 uninstall --id ZeroTier.ZeroTierOne --exact --source winget --silent --accept-source-agreements --disable-interactivity；确认说明连接中断和配置保留取决于卸载器，不主动删除数据目录。超时提示安装器可能仍运行；成功还需本地检测程序/服务消失。确认卸载后禁用管理操作，保留日志，退出重开由启动检测进入安装窗口。两项功能仅用模拟服务/卸载器和临时目录测试；不宣称真实卸载/重连验收完成。

「帮助与诊断」页提供入门说明、GitHub https://github.com/vusiif/zerotier-gui 与 Gitee https://gitee.com/vusiif/zerotier-gui 两个项目链接、CLI -h/-v 查询及 dump 报告预览/导出。Windows CLI 通常在桌面生成 zerotier_dump.txt，生成前提示可能覆盖文件及报告内的 IP/节点/本地配置。成功后读取返回路径中的报告或完整控制台输出；拒绝旧文件、非法路径、空/格式错误或大于 4MB 的文件。失败清除缓存，避免导出旧报告；导出使用 QSaveFile，报告不会自动上传。CLI 帮助只有 stderr 输出时也能正确展示，富文本链接颜色随主题更新。本机 -h/-v 已实际验证；dump 仅通过临时目录和模拟 CLI 验证，未覆盖真实桌面报告。

Peers 页面显示链路 DIRECT/RELAY、距首选路径上次发送和接收的毫秒数。分类参考官方 CLI 的 preferred/tunneled 逻辑，不代表当前节点可达；缺失必要字段显示未知。路径列显示首选路径，悬停查看所有路径及活动/过期标志；负延迟标为不可用，未知版本不伪装正常。零时间显示无记录，非法时间显示未知，未来时间显示时钟异常；JSON 不变时仍更新相对时间并保留选择。依据：https://github.com/zerotier/ZeroTierOne/blob/dev/one.cpp 的 peers 分支。

网络页的「网络设置 / 属性查询」读取 allowManaged/allowGlobal/allowDefault/allowDNS 四项开关；保存前确认变更并重新读取，若外部设置变化则停止提交并加载最新值。逐项 set 后回读验证，失败时停止后续提交并展示实际状态；读回失败禁止再次用旧值保存，需重新读取。get 支持当前 JSON 的属性名及 ip/ip4/ip6/ip6plane/ip6prefix；CLI 退出码为 0 的属性错误仍显示失败。设置窗口期间暂停后台查询与管理操作。
界面测试以模拟命令和离屏窗口验证安装窗口检测与关闭、按需创建、导航、服务状态及忙碌按钮恢复，不安装或修改真实 ZeroTier。
MSVC 编译输出由 scripts/msvc_launcher.py 转为 UTF-8，并将中文 showIncludes 前缀转换为 Ninja 可识别的英文前缀；避免缺少英文语言包时遗漏头文件依赖。
发布脚本：powershell -ExecutionPolicy Bypass -File scripts/Publish.ps1
发布脚本使用独立暂存目录，成功后生成 dist，保留旧 dist 为带时间的备份。
不包含构建缓存；带 Ela DLL、Qt 依赖、许可说明及 SHA256 清单。
