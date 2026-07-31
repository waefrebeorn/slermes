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

- Pure C11 (`-std=c11`): No C++, no VLAs, no GNU extensions
- Snake_case for functions and variables
- `static` for file-internal functions
- Error returns as `bool` or `NULL` on failure
- JSON for structured data I/O
- All buffers initialized: `char buf[1024] = {0};`
- Check malloc/calloc returns

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
