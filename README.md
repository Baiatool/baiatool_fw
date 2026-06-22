# baiatool_fw

Zephyr-based firmware for the Baiatool project, targeting the ESP32 (DevKitC).

## Prerequisites

- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) installed
- `west` installed (`pip install west`)

## Getting started

This repo is the west manifest (T2 topology), so use `west init -m` to bootstrap the workspace.

```bash
west init -m https://github.com/Baiatool/baiatool_fw.git --mr main baiatool-workspace
cd baiatool-workspace
west update
```

This creates the following layout:

```
baiatool-workspace/
├── baiatool_fw/       # this repo (app + manifest)
├── zephyr/
└── modules/
    ├── hal/espressif/
    └── lib/gui/lvgl/
```

## Building

```bash
cd baiatool-workspace/baiatool_fw
west build
```

To flash:

```bash
west flash
```
