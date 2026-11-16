<!-- PARITY:AUTO -->| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Development Guide

Building, testing, and contributing to Slermes.

## Prerequisites

```bash
# Debian/Ubuntu
sudo apt install build-essential libssl-dev libwayland-dev \
                 libxkbcommon-dev libegl1-mesa-dev libgles2-mesa-dev

# macOS
xcode-select --install
brew install openssl

# Windows (cross-compile)
sudo apt install mingw-w64
```

## Build

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)
```

## Quick Iteration

```bash
# Build and run
make -j$(nproc) && ./slermes

# Just compile changed files
make -j$(nproc)

# Full clean rebuild
make clean && make -j$(nproc)
```

## Project Structure

```
slermes/
├── src/
│   ├── cli/          — CLI commands, config, display
│   ├── agent/        — Core agent, providers, context
│   ├── tools/        — Tool handlers (40+)
│   ├── gateway/      — Messaging platforms (14+)
│   │   └── platforms/  — Platform adapters
│   ├── pet/          — Petdex mascot system
│   ├── acp/          — Agent Communication Protocol
│   ├── cron/         — Scheduled tasks
│   ├── provider/     — OAuth providers
│   ├── skills/       — Skills parser
│   └── plugins/      — Plugin system
├── include/          — 127 headers
├── lib/              — 73 libraries
└── tests/            — Test suite
```

## Adding a New CLI Command

1. Add handler in `src/cli/commands.c`:
```c
static void cmd_mycommand(const char *args, agent_state_t *state) {
    // your logic here
}
```

2. Register in the `COMMANDS[]` table:
```c
{.name="/mycommand", .alias=NULL, .description="My new command",
 .category="Tools", .handler=cmd_mycommand},
```

3. Build and test:
```bash
make -j$(nproc)
./slermes
/mycommand
```

## Adding a New Tool

1. Create `src/tools/my_tool.c` with handler:
```c
#include "hermes.h"

char *my_tool_handler(const char *args_json, const char *task_id) {
    // parse JSON args, do work, return JSON result
}
```

2. Register in `src/tools/registry.c`

3. Add `src/tools/my_tool.o` to `TOOL_OBJ` in Makefile

4. Rebuild

## Code Style

The full standard is **[docs/dev/c-standards.md](c-standards.md)**; naming is
**[docs/dev/naming.md](naming.md)**. Both are enforced by gates.

In brief:

- Pure C11 (`-std=c11`): no C++, no VLAs, no GNU extensions
- Snake case; a per-unit prefix on every exported symbol (`st_`, `ai_`, ...)
- `static` for file-internal functions; exported functions declared in
  `include/port_<stem>.h` and nowhere else
- Error returns as `bool` or `NULL` on failure
- JSON for structured data I/O, built with the `json_t` API
- All buffers initialized: `char buf[1024] = {0};`
- Check malloc/calloc returns; every allocation has exactly one owner
- 4 spaces, no tabs, LF endings, one final newline
- Every ported function carries a `PoP:` annotation naming the Python function
  it ports — without one the parity scanner cannot see the work

## Testing

```bash
# Run parity scanner
python3 tests/slermes_parity_battleground.py --json

# Check specific module
python3 tests/slermes_parity_battleground.py --detail --module agent/pet/store.py

# Run functional tests
make test
```

## Parity Scanning

The `slermes_parity_battleground.py` scanner compares Python Hermes features against C11 Slermes implementations. It is a **static** check — it counts functions present/missing, it does not execute anything.

**Output classifications:**
- `PORTED` — C function with PoP annotation (present, not necessarily correct)
- `REAL_GAP` — Missing C implementation (honest gap, not yet ported)
- `PARTIAL` — C fn exists but incomplete
- `STUB` — C fn is a façade (const return / no-op body)

> **The scanner is blind to FAPs.** A fn can be `PORTED` and still produce output
> that diverges from LIVE Python. That behavioral-divergence class is a **FAP
> (Functional Alignment Problem)** and is detected only by the oracle harness:
> `bash tests/oracle/run_oracles.sh` → any `cases: MISMATCH` is a FAP. See
> `docs/fap.md` for the canonical definition and triage. Never call it "drift" or
> "desync".

Adding a PoP annotation:
```c
/* PoP: c_function_name @ python_module:python_function_name */
int c_function_name(const char *arg) { ... }
```

## Contributing

1. Fork the repo
2. Make your changes
3. Run `python3 tests/slermes_parity_battleground.py --json` — verify 0 REAL_GAP
4. Run `make -j$(nproc)` — verify clean build
5. Submit PR

## CI/CD

The CI pipeline builds and tests on:
- Linux x86_64 (native + test)
- macOS arm64
- Linux aarch64/armv7
- Windows x86_64 (cross-compile)

Release workflow triggered by `v*` tag.
