# Virtual Audio Router

Route one Windows audio source to multiple physical output devices simultaneously — Bluetooth, USB, AUX, HDMI, and more.

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│  Python / PySide6 GUI  (gui/)                                │
│  • Device selection    • Profiles    • System tray           │
└──────────────────────┬───────────────────────────────────────┘
                       │  pybind11 (bindings/)
┌──────────────────────▼───────────────────────────────────────┐
│  C++ Audio Engine  (engine/)                                 │
│  AudioEngine → DeviceManager → AudioRouter → OutputDevice(s) │
│               BufferManager   LatencyManager  Resampler      │
│               SyncManager     ThreadPool      Logger         │
└──────────────────────────────────────────────────────────────┘
                       │  WASAPI / MMDevice API
┌──────────────────────▼───────────────────────────────────────┐
│  Windows Audio Stack                                         │
│  • Bluetooth Speaker   • USB DAC   • HDMI   • AUX           │
└──────────────────────────────────────────────────────────────┘
```

### Key Principles

| Rule | Why |
|---|---|
| Python never touches audio data | GIL latency, unsafe for real-time |
| pybind11 surface is a thin façade | Only `AudioEngine` is exposed |
| Every API returns `Result<T, E>` | No silent failures, no exceptions on audio threads |
| Logger is always async | Disk I/O must never block an audio callback |
| All outputs implement `IDevice` | Future WDK virtual driver drops in without changing the router |

---

## Project Structure

```
VirtualAudioRouter/
├── engine/          C++20 audio engine (static library)
│   ├── include/var/ Public headers (AudioEngine, DeviceManager, etc.)
│   └── src/         Implementations
├── bindings/        pybind11 Python extension module (var_engine.pyd)
├── core/            Header-only shared types (Result.h, Types.h, Constants.h)
├── gui/             Python/PySide6 application
│   ├── main.py      Entry point
│   ├── app/         UI components
│   ├── core/        Engine bridge, settings, logger
│   └── resources/   Dark theme QSS, icons
├── tests/           GoogleTest unit tests
├── config/          Default settings JSON
├── driver/          (Phase 8) Future WDK virtual audio driver
└── installer/       (Future) NSIS/WiX installer
```

---

## Prerequisites

| Requirement | Version |
|---|---|
| Windows | 10 / 11 (22H2+ recommended) |
| Visual Studio | 2022 with "Desktop Development with C++" |
| Windows SDK | 10.0.22621 or later |
| CMake | 3.25+ |
| Python | 3.11 or 3.12 |
| Git | Any recent version |

---

## Build Instructions

### 1. Install Python dependencies

```powershell
cd gui
pip install -r requirements.txt
```

### 2. Configure the C++ build

```powershell
# From project root — first run downloads pybind11 and GoogleTest via FetchContent
cmake --preset debug-windows
```

### 3. Build the engine + bindings

```powershell
cmake --build --preset debug-build
```

This produces `var_engine.pyd` in the `gui/` directory.

### 4. Run tests

```powershell
ctest --preset debug-test
```

### 5. Launch the GUI

```powershell
cd gui
python main.py
```

> **Note:** If the C++ build is not complete, the GUI runs in **Mock Mode** (DEV MODE badge visible). All UI functionality works with simulated device data.

---

## Development Phases

| Phase | Status | Description |
|---|---|---|
| 1 | ✅ **Current** | Architecture, skeleton, build system, GUI |
| 2 | ⬜ Planned | Real WASAPI device enumeration |
| 3 | ⬜ Planned | Open one device, play sine wave |
| 4 | ⬜ Planned | Multi-device simultaneous playback |
| 5 | ⬜ Planned | Latency compensation + drift correction |
| 6 | ⬜ Planned | WASAPI loopback capture |
| 7 | ⬜ Planned | Settings, profiles, auto-start, reconnect |
| 8 | ⬜ Planned | WDK virtual audio driver abstraction |

---

## Threading Model

```
UI Thread           → Qt event loop, pybind11 calls (non-blocking)
Control Thread      → Engine lifecycle, device management
Audio Capture Thread → WASAPI loopback read (Phase 6)
Output Thread[0..N] → One per output device, real-time priority (Phase 3)
Device Monitor Thread → Hot-plug detection (Phase 2)
Logger Thread       → Async disk write, never blocks audio
```

---

## License

MIT — see LICENSE file.
