# installer/ — Reserved for Installer

This directory will contain the **Windows installer** script (NSIS or WiX Toolset).

## Planned installer contents

- Embeds the Python runtime (or requires Python 3.11+)
- Bundles `var_engine.pyd` and all Qt DLLs
- Registers the app for Windows startup (optional)
- Installs the virtual audio driver (Phase 8, requires admin + driver signing)
- Creates Start Menu shortcut
- Adds system tray autostart entry
