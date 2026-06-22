# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**baiatool_fw** — Zephyr RTOS firmware for ESP32 (esp32_devkitc/esp32/procpu).

## Build

```bash
# From repo root, with Zephyr env sourced
west build -b esp32_devkitc/esp32/procpu

# Flash
west flash

# Monitor serial
west espressif monitor
```

Source Zephyr env first if not in shell profile:
```bash
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
```

## Code Style — Zephyr Coding Guidelines (mandatory)

All code must follow https://docs.zephyrproject.org/latest/contribute/coding_guidelines/index.html

Key rules enforced in this project:

- **No `typedef`** — declare structs/enums with full `struct`/`enum` tags, never typedef them
- **No dynamic memory allocation** — no `malloc`, `k_malloc`, `k_heap_alloc`, or equivalents; use static allocation, kernel objects, or memory slabs defined at compile time
- **Naming**: `snake_case` for functions and variables; `ALL_CAPS` for macros and Kconfig symbols
- **Prefixing**: module-prefix all public symbols (e.g. `wifi_connect`, `sensor_read`)
- **Headers**: use `<zephyr/...>` include paths, not bare `<kernel.h>`
- **C standard**: C11, no C++ features
- **`__attribute__`** over GCC-specific pragmas; use Zephyr macros (`BUILD_ASSERT`, `CONTAINER_OF`, etc.) when available

## Architecture

```
src/
  main.c            — entry point, minimal: init services, start threads
  services/         — long-running subsystems (wifi, ble, storage…)
  endpoints/        — request/response handlers (shell cmds, HTTP routes…)
  components/       — reusable logic shared across services/endpoints
  drivers/          — board-specific hardware abstraction (thin wrappers)
boards/             — board overlays and DTS fragments
```

Each service owns its own thread and Kconfig symbol. Services communicate via Zephyr message queues (`k_msgq`) or event flags (`k_event`) — never via shared globals.

## Kconfig

Feature flags live in `Kconfig`. Add a `CONFIG_BAIATOOL_<FEATURE>` symbol for each new service. Enable in `prj.conf`.

## Shell Integration

Zephyr shell (`CONFIG_SHELL=y`) is the runtime interface. Endpoints in `src/endpoints/` register shell commands via `SHELL_CMD_REGISTER`. No interactive CLI framework outside of Zephyr shell.
