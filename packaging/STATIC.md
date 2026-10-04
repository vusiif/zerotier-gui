# Standalone Windows executable

`ZEROTIER_GUI_STATIC=ON` requires a static Qt kit built with the static MSVC runtime. It also builds ElaWidgetTools statically and imports the Windows platform and JPEG plugins. The dynamic build remains the default.

Qt source used for the initial build: QtBase 6.11.2, unmodified official source archive:
https://download.qt.io/official_releases/qt/6.11/6.11.2/submodules/qtbase-everywhere-src-6.11.2.zip

SHA256: `8f8c16703a8170b235361aacdf0ec97d2445ae4e3e3d127eb1576f498269ef79`.

Extract the source into `.docs-tmp/qt-static`, then run `scripts/BuildStatic.ps1` in a shell that can run the installed MSVC compiler. The script creates a separate Qt kit, app build and test build. It uses Release, `/MT`, bundled image/compression/font libraries and no OpenSSL dependency. Windows Schannel remains available; no ZeroTier service is embedded in the executable.

The standalone application still needs Windows system DLLs, Windows PowerShell for service maintenance, and winget for installing/uninstalling ZeroTier. ZeroTier is installed separately through the existing startup screen.

Qt is used under LGPL v3; ElaWidgetTools is MIT licensed. Keep the accompanying notices, Qt source and application relinking materials available alongside published binaries. The static support archive provides the Qt source, libraries and compiled application objects to allow relinking against a modified compatible Qt. Debugging modifications to Qt is not restricted. See the included LGPL/GPL license texts and the original third-party notices in the Qt source.

The static support archive is development/licensing material; it is not needed beside the EXE to run it. Do not rename a self-extracting archive as a standalone build: the standalone build must have no Qt/Ela/VC redistributable DLL imports and must pass startup in a directory containing only its EXE.
