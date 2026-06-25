Review or generate code following Zephyr RTOS coding guidelines (https://docs.zephyrproject.org/latest/contribute/coding_guidelines/index.html) with project-specific overrides.

Apply ALL rules below when writing or reviewing C code for this project.

---

## Naming & Identifiers

- `snake_case` for all functions, variables, and file names
- `ALL_CAPS` for macros and Kconfig symbols
- Module-prefix ALL public symbols: `wifi_`, `storage_`, `sensor_`, etc.
- External identifiers must be unique across translation units
- Internal linkage: declare with `static`
- No identifier shadowing between scopes

## Types

- `typedef` IS ALLOWED — use for size/signedness aliases (e.g. `typedef uint32_t foo_handle_t`) and for struct/enum aliases when it improves readability
- Prefer fixed-width types: `uint8_t`, `int16_t`, `uint32_t`, etc. over `int`, `unsigned`, `long`
- No bit-fields unless mapping hardware registers
- No `restrict` qualifier
- No variable-length arrays (VLAs)

## Memory Allocation

- Dynamic allocation IS ALLOWED — prefer `k_malloc`/`k_free` (Zephyr heap) over raw `malloc`/`free`
- Prefer static or slab allocation (`K_MEM_SLAB_DEFINE`) when object lifetime is bounded and known at compile time
- ALWAYS check allocation return value before use; handle `NULL` explicitly
- Every allocation must have a corresponding free path; no leaks

## Control Flow

- Every `if`/`else if` chain MUST end with `else`
- Every `switch`:
  - Must have a `default` label
  - Each case must end with unconditional `break` (or explicit `/* falls through */` comment)
  - Must have at least 2 case clauses
- No recursion
- `for` loop counters: integer types only, never float

## Functions

- All non-`void` functions: explicit `return` statement on EVERY path
- No variadic functions (`<stdarg.h>` prohibited)
- Validate ALL external/user input at system boundaries
- Check return values of ALL library and Zephyr API calls

## Preprocessor & Headers

- Macro parameters: always wrap in parentheses — `#define SQUARE(x) ((x) * (x))`
- Mandatory include guards: `#ifndef FOO_H_` / `#define FOO_H_` / `#endif /* FOO_H_ */`
- Include paths: use `<zephyr/kernel.h>`, `<zephyr/device.h>`, etc. — NOT bare `<kernel.h>`
- No conditional compilation of full function declarations or struct declarations in headers; field-level `#ifdef` inside structs is OK
- Do NOT redefine or `#undef` common macros: `MIN`, `MAX`, `ARRAY_SIZE`, `CONTAINER_OF`, `BUILD_ASSERT`

## Comments & Dead Code

- No nested `/* /* */ */` comments
- No commented-out code blocks
- No unreachable code, dead code, unused declarations, or unused parameters
- Comment WHY, not WHAT — one short line max; no multi-paragraph docstrings

## Language & Compiler Extensions

- C11 standard only; no C++ features
- Prefer `__attribute__((...))` over GCC `#pragma`
- Use Zephyr macros when available: `BUILD_ASSERT`, `CONTAINER_OF`, `ARRAY_SIZE`, `IS_ENABLED`, `UTIL_AND`
- No `<stdio.h>`, `<setjmp.h>`, or `<tgmath.h>` in kernel/driver code

## Inclusive Language

- Banned: `master`/`slave`, `blacklist`/`whitelist`, `sanity`
- Use: `primary`/`secondary`, `allowlist`/`denylist`, `coherence`/`confidence`

## Zephyr-Specific Patterns

- Threads: define with `K_THREAD_DEFINE` or `k_thread_create`; each service owns one thread
- IPC: use `k_msgq` or `k_event` — never shared globals between services
- Kconfig: add `CONFIG_BAIATOOL_<FEATURE>` for every new service/feature
- Shell commands: register via `SHELL_CMD_REGISTER` in `src/endpoints/`
- Error returns: use standard errno values (`-EINVAL`, `-ENOMEM`, `-EIO`, etc.)

---

$ARGUMENTS
