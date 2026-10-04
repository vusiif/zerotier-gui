# ZeroTier GUI

Windows C++ / Qt 6 / ElaWidgetTools 图形客户端，用于执行本机 ZeroTier CLI 命令。

源码与反馈：[GitHub](https://github.com/vusiif/zerotier-gui) · [Gitee](https://gitee.com/vusiif/zerotier-gui)。当前开发分支为 `dev-net`。

## 使用

运行发布目录中的 `zerotier_gui.exe`，同目录的 DLL 和插件目录必须保留。程序会请求管理员权限。
未安装 ZeroTier 时只显示安装窗口，可使用 winget 安装；安装完成后进入管理界面。

- 网络：加入、离开、持续跟踪本次加入的网络；NOT_FOUND 自动退出。
- 网络设置：四项地址/路由/DNS 开关，保存前检查变化，保存后回读；支持属性和 IP 查询。
- Peers：链路分类、首选路径、收发间隔及全部路径提示。DIRECT/RELAY 不代表当前可达。
- Moon：导入文件到服务实际 homeDir 下的 `moons.d`，统计本地文件，并确认重启。
- 服务：启动、停止、重启；缓存清理会暂停服务并保留 `peers.d` 备份。
- 帮助：项目链接、CLI 帮助/版本、完整诊断报告预览与导出。
- 设置：程序/数据目录、日志导出、确认后通过 winget 卸载。

卸载、停止服务和缓存维护会中断连接。诊断报告含 IP、节点和本地配置信息，分享前请检查内容。

## 构建与测试

已验证 Windows x64、MSVC、Qt 6.11.2、CMake、Ninja、Python 3。Qt 需要 Widgets、WidgetsPrivate 和 Concurrent；Ela 使用 Qt 私有接口，运行时 Qt 库必须与构建版本一致。
CLion 中选择 MSVC 工具链及 Release 配置，设置 Qt 安装前缀即可。也可在 Visual Studio 开发者终端运行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64 -DZEROTIER_GUI_BUILD_TESTS=ON
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure
```

无需 `photos` 或 Ela 示例项目：运行图片已在 `assets/art`，所需 Ela 源码、字体与资源已纳入仓库。
构建目录、`dist`、旧发布包、本机凭据和原始照片不提交到 Git。

## 发布与本机验证

在 MSVC 开发者终端中使用与构建一致的 Qt kit：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Publish.ps1 -BuildDirectory build -QtDirectory C:/Qt/6.11.2/msvc2022_64
```

脚本部署 Qt/Ela DLL 和插件、许可证、说明与 SHA256 清单到 `dist`，保留旧目录为时间戳备份。

`scripts/VerifyInstalled.ps1` 在管理员终端中执行本机只读 CLI 查询，仅保存摘要到忽略的构建目录。
`scripts/VerifyRelease.ps1` 在服务已经运行时检查 `dist` 主窗口创建和正常关闭；仅对该检查进程排除 Qt SDK 路径，不更改系统 PATH，也不执行网络修改或服务停止操作。

## 验收状态

已验证干净 Git 源码构建、十组自动测试和发布包启动。
本机 ZeroTier 1.16.2 的只读查询成功，但节点当前 OFFLINE；没有可用测试网络和 Moon 节点，实际组网、真实设置修改、Moon 连通性和重启后恢复仍未验收。
卸载、缓存维护等改变状态的流程使用模拟后端验证，没有卸载本机 ZeroTier。

详细结果见 [测试报告](TEST_REPORT.md) 和 [开发说明](REFACTOR.md)。macOS 尚未实现。

## 单 EXE 静态版

Windows x64 静态版已使用 QtBase 6.11.2、静态 Ela 和 MSVC `/MT` 编译，并通过十组测试及仅含 EXE 的隔离目录启动检查。运行时无需旁边的 Qt/Ela/VC 运行时 DLL，ZeroTier 仍由启动检测和 winget 流程单独安装。

构建方法见 [静态构建说明](packaging/STATIC.md)。`scripts/BuildStatic.ps1` 构建静态 Qt、程序和测试；`scripts/publish_static.py` 生成独立 EXE、SHA256 及静态发布支持 ZIP。发布时同时提供支持 ZIP，内含 Qt 源码、许可证、程序对象文件和已验证的重新链接脚本；它不需要放在 EXE 旁边才能运行。

ElaWidgetTools 的 MIT 许可证保留在 `ElaWidgetTools-main/LICENSE`；发布包附带 Qt 许可说明。
