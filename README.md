# baiatool_fw

Zephyr-based firmware for the Baiatool project, targeting the ESP32 (DevKitC).

## Prerequisites

- Python 3.12+
- CMake >= 3.20 and Ninja

The Zephyr SDK and board tools are installed automatically during setup (`west sdk install` and `west packages pip --install`).

## Setup

All commands run from the workspace root (parent of `baiatool_fw/`).

### 1. Create workspace

**From a local clone (development):**

```bash
python3 -m venv .venv
source .venv/bin/activate      # Windows: .venv\Scripts\activate
pip install west
west init -l baiatool_fw/
west update
```

**From scratch (fresh clone):**

```bash
mkdir baiatool-workspace && cd baiatool-workspace
python3 -m venv .venv
source .venv/bin/activate
pip install west
west init -m https://github.com/Baiatool/baiatool_fw.git --mr main .
west update
```

Workspace layout after `west update`:

```
baiatool-workspace/
├── .venv/
├── baiatool_fw/           # app + manifest
├── zephyr/                # Zephyr v4.4.0
└── modules/
    ├── hal/espressif/
    ├── lib/gui/lvgl/
    ├── crypto/mbedtls/
    └── crypto/tf-psa-crypto/
```

### 2. Install dependencies

```bash
pip install -r baiatool_fw/requirements.txt   # Zephyr base deps
west sdk install                               # Zephyr SDK toolchain
west packages pip --install                    # esptool + board tools
west blobs fetch hal_espressif                 # WiFi/BT binary blobs
```

## Building

```bash
west build -p always baiatool_fw -b esp32_devkitc/esp32/procpu
```

Output binary: `build/zephyr/zephyr.bin`

## Flashing

```bash
west flash
```

Connect the ESP32 DevKitC via USB before flashing.

## Serial monitor

```bash
west espressif monitor
```

Or any serial terminal at 115200 baud on the ESP32 USB port.

## Project structure

```
baiatool_fw/
├── src/
│   ├── components/    # reusable hardware components
│   ├── drivers/       # device drivers
│   ├── endpoints/     # communication endpoints
│   ├── services/      # application services
│   ├── shell/         # debug shell commands
│   └── main.c
├── includes/          # public headers
├── boards/            # board-specific overlays
├── prj.conf           # Kconfig configuration
├── CMakeLists.txt
├── requirements.txt   # Python deps (install after west update)
└── west.yml           # west manifest
```
