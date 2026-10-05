# ZeroTier GUI

基于 Qt 6 Widgets 的 Windows ZeroTier 管理工具。

## 使用

- 左侧分隔线可左右拖动，调整导航栏与表格的占比；汉堡按钮可折叠侧栏，折叠后保留图标和提示。
- 右上角支持深浅主题切换与窗口置顶，选择自动保存。深色主题同时覆盖表格、输入框和弹窗文字。
- 界面不显示操作日志。日志写入用户本地应用数据目录下的 `zerotier-gui.log`，约 1 MiB 上限，超出后保留最近记录；错误仍通过页面提示或弹窗显示。
- 启动时先检查本机安装：未安装仅显示独立的 winget 安装窗口，检测到安装完成后再创建管理界面；已安装直接进入主界面。安装窗口不显示日志。
- 网络页面可打开“网络设置”，读取/修改 allowManaged、allowGlobal、allowDefault、allowDNS，查询属性及 IP。保存前确认并检查配置冲突，保存后回读验证。
- 本次 GUI 加入的网络在后台跟踪，即使切换页面也继续检查。只有明确的 NOT_FOUND 才会自动退出；授权等待、查询失败和其他已有网络不会因此退出。
- 启动后异步检查服务，服务停止时尝试启动并等待就绪。服务/安装/网络修改操作防重入，执行中阻止关闭窗口；查询和表格浏览仍可继续。
- Moon 支持 orbit/deorbit；文件导入读取 `info.config.settings.homeDir`，原子写入 `moons.d`，确认重启后等待实际服务状态。失败保留已导入文件并提示。
- 帮助页面提供 GitHub/Gitee、CLI 用法/版本及诊断导出。导出前确认敏感内容与 CLI 本地报告更新，不自动上传。
- 列表每 2 秒异步静默更新。刷新保留选择、滚动位置、列宽、列顺序和输入内容；服务暂时不可用时保留上次成功读取的数据。
- 表格内按住鼠标左键拖动可横向、纵向浏览；也可使用滚动条和鼠标滚轮。表头支持排序、调整列宽与拖动列顺序。
- Peers、Networks、Moon 的名称默认采用 ID。选中一项后点击“编辑名称”，或双击名称单元格编辑；名称保存在本机，不修改远端网络名称。留空恢复默认名称。
- 选中一项后点击右下角“详细信息”，在非模态浮窗查看完整字段及原始 JSON。
- 服务页面支持启动、停止、重启以及通过 winget 安装或卸载。安装接受软件包和来源协议；卸载成功后删除 `%ProgramData%\ZeroTier`，包括节点身份和网络配置。
- Windows MSVC 发行版启动时自动请求管理员授权，以读取 ZeroTier 服务的认证文件并执行网络和服务操作。
- Moon 文件复制到官方的 `moons.d` 目录，随后重启服务；列表使用 `zerotier-cli -j listmoons` 读取。

## 编译

需要 Qt 6、CMake 和支持 C++17 的编译器。Windows 推荐 MSVC 2022。

```powershell
cmake -S . -B cmake-build-release -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build cmake-build-release --config Release --parallel
C:/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe --release cmake-build-release/Release/zerotier_gui.exe
```

### 单 EXE 编译

使用以 `-static -static-runtime -release` 编译的 Qt 6 SDK，再构建本项目：

```powershell
cmake -S . -B cmake-build-single -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/static"
cmake --build cmake-build-single --config Release --parallel
```

静态 Qt SDK 会自动启用静态 MSVC 运行库，并将平台插件链接进程序。生成的 EXE 不需要运行 `windeployqt`，也不需要在旁边放置 Qt 或 MSVC DLL。ZeroTier 服务仍需要单独安装，可通过程序中的 winget 安装入口完成。

## 回归测试

```powershell
cmake -S . -B cmake-build-release -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" -DZEROTIER_BUILD_TESTS=ON
cmake --build cmake-build-release --config Release --parallel
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
$env:QT_PLUGIN_PATH = "C:/Qt/6.8.3/msvc2022_64/plugins"
ctest --test-dir cmake-build-release -C Release --output-on-failure
```

默认测试包含窗口按钮、主题、单文件日志、表格交互与模拟 CLI 查询，不调用系统 ZeroTier。日志位于构建目录的 `test-results.txt`，界面截图位于 `screenshots/`。测试用的 `zerotier-cli.exe` 不能打包进正式发行目录。

PowerShell 集成测试通过 `-DZEROTIER_TEST_POWERSHELL=ON` 单独启用，默认关闭。它会启动 PowerShell，可能触发本机行为检测；卸载清理测试仅操作构建目录中的临时测试数据，不卸载真实 ZeroTier。本次 UI 恢复验证未运行这两项集成测试。

## 发布目录与安装包

在 MSVC 开发者终端构建 Release 后运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Publish.ps1 -BuildDirectory cmake-build-ui-restore -QtDirectory C:/Qt/6.11.2/msvc2022_64
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Package.ps1 -Version 0.2.0
```

Publish 使用与构建相同的动态 Qt SDK，部署 DLL、平台插件、许可证和说明，生成 SHA256 清单；旧 dist 改名备份。Package 调用已安装的 WinRAR 和 Inno Setup，拒绝覆盖同版本安装包。分发完整 dist，或任选一个生成的安装 EXE；不要发送 CMake 缓存、测试 CLI 或构建目录。

服务页恢复 Peers 缓存维护：读取 CLI 实际 homeDir，停止服务后将 peers.d 改名备份，再确认服务恢复运行。节点身份、网络和 Moon 配置保留。服务、帮助和安装页使用内嵌背景，随深浅主题切换；表格页保留可用空间。Peers 根据 preferred/tunneled 判断连接类型，缺失字段显示未知，路径原始字段可在提示和详情中查看。

界面按钮与侧栏使用随主题变色的矢量图标，提供侧栏收放、页面/主题过渡和按钮悬停缓动；轮询更新保持静默。
