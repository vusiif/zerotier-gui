# ZeroTier GUI

基于 Qt 6 Widgets 的 Windows ZeroTier 管理工具。

## 使用

- 左侧分隔线可左右拖动，调整导航栏与表格的占比；操作日志默认折叠。
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

测试使用独立的模拟 CLI 和临时设置，不调用系统 ZeroTier。卸载清理测试只在构建目录下生成的临时目录中执行，检查失败时保留数据、成功后删除数据。日志位于构建目录的 `test-results.txt`，界面截图位于 `screenshots/`。测试用的 `zerotier-cli.exe` 不能打包进正式发行目录。
