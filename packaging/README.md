# Windows packages

Run `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Package.ps1 -Version 0.1.0` after publishing a Release build to `dist`.

The script validates the payload SHA256 manifest and generates two packages in the ignored `packages` directory:

- `*-SFX.exe`: WinRAR self-extracting archive. Extract to a writable folder and run `zerotier_gui.exe`. Install the included `vc_redist.x64.exe` if needed. No shortcut or uninstaller is created.
- `*-Setup.exe`: Inno Setup installer with Chinese/English UI, Program Files installation, Start menu shortcut, optional desktop shortcut and an uninstaller. Installs the bundled Visual C++ runtime when no installed x64 runtime is detected. ZeroTier installation remains handled by the GUI's winget installation screen. Uninstalling this GUI does not uninstall ZeroTier or its configuration.

The initial package version is `0.1.0`; use `-Version` for subsequent versions. Existing EXE outputs are preserved, and the script refuses to overwrite them. The SHA256 JSON contains checksums for both outputs. Share either EXE on its own.

These are unsigned installer/archive executables, not a statically linked single executable application. Package compilation and archive integrity are checked; clean-machine installation, shortcut creation, runtime installation and uninstallation still require acceptance testing.
