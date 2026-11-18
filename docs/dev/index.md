<!-- PARITY:AUTO -->| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Development Guide

Building, testing, and contributing to Slermes. The enforced C standard is
[c-standards.md](c-standards.md); naming is [naming.md](naming.md); every
verification gate and its invocation is [gates.md](gates.md). This guide is
the workflow overview.

## Prerequisites

**Windows (primary dev platform)** — MSYS2 (bash, make, python) plus the
winlibs GCC bundle under `C:/winlibs`. The repo ships the env recipe:
`build/winlibs-env.sh` puts winlibs first, sets library paths, and unsets
`GCC_EXEC_PREFIX`/`COMPILER_PATH` (MSYS2's own backend can be refused by
security policy; winlibs is the working toolchain).

**Linux**

```bash
sudo apt install build-essential libssl-dev libwayland-dev \
                 libxkbcommon-dev libegl1-mesa-dev libgles2-mesa-dev
```

**macOS**

```bash
xcode-select --install
brew install openssl
```

All three platforms must build — a change that compiles on one only is not
done. Platform-specific code goes behind `include/compat/` shims.

## Build

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc) slermes        # Linux/macOS
```

Windows — everything in ONE MSYS2 bash child (this box drops exported env
vars before native children see them):

```bash
/c/msys64/usr/bin/bash -lc 'source /c/Users/eman5/slermes/build/winlibs-env.sh; \
  cd /c/Users/eman5/slermes && make -j8 slermes \
  USERPROFILE="C:\Users\eman5" LOCALAPPDATA="C:\Users\eman5\AppData\Local"'
```

- Use the explicit target `slermes`. Bare `make` historically built only the
  first object and exited 0 without compiling anything.
- `USERPROFILE`/`LOCALAPPDATA` are **make variables**, not exports (ccache
  refuses to run without them).
- Build profiles: `make PROFILE=dev|strict|analyzer|sanitize|release`
  (`release` `-O3` is the default; `strict` is the bar for new work;
  `sanitize` refuses loudly when the toolchain lacks the runtimes — a
  sanitizer that did not run is not a pass). Switching profiles requires
  `make clean` (the `.profile` stamp guards it).
- ccache is wired into `CC` and warm; do not install another.
- One build at a time: concurrent makes contend for ccache and `.o` files
  and produce failures that read like code defects.

## Project structure

```
slermes/
├── src/
│   ├── main.c        — entry point (argv dispatch)
│   ├── cli/          — CLI loop, slash commands, config, display, TUI units
│   ├── hermes_cli/   — ports of the hermes_cli Python package
│   ├── agent/        — agent loop, providers, credentials, context
│   ├── tools/        — tool handlers + registry.c (registry_dispatch)
│   ├── gateway/      — messaging platforms + relay + runtime
│   ├── plugins/      — native C plugins (plugin_*.c → .so)
│   ├── cron/         — scheduler
│   ├── pet/          — Petdex mascots
│   ├── lv/ lua/      — slerm-Lua interpreter + Lua terminal host
│   └── ...
├── include/          — unit headers (port_<stem>.h) + compat/ shims
├── lib/              — our C libraries + vendored SLERM targets
├── build/            — config.mk, objects.mk (cupboards), rules.mk, env scripts
├── tests/            — oracles, watercup/, gates, mission8 suites
└── scripts/          — parity pipeline, audits, generators
```

`build/objects.mk` is cupboard-organized: one `CUPBOARD_*` variable per
subsystem. After editing it run `python scripts/organize_objects.py` (it
refuses to write unless the expanded object multiset is identical).
**Registered is not reachable** — confirm a new object is on the link line:
`make -n slermes | grep -c '<obj>.o'`.

## Porting a Python module (the loop)

1. Name the unit for the Python module: `src/<py_top_dir>/port_<stem>.c`,
   `include/port_<stem>.h`, `tests/t_port_<stem>.c`,
   `tests/sta_oracle_<stem>.py` (see [naming.md](naming.md)).
2. Grep `src/ include/ lib/` for the symbol **before writing anything** —
   behavior under another name is a naming gap, the cheapest real fix.
3. Port pure helpers first; annotate every ported function:
   `/* PoP: <c_function> @ <python_file>:<python_function> */` — C name
   first, and the body must do real work. A PoP on an empty body is a lie.
4. Write the harness + oracle and run them:
   `bash tests/run_one_oracle.sh <stem> ...` (DEPS table in
   `tests/run_all_oracles.sh`). For pure helpers add a watercup gate
   (`tests/watercup/gates/<fn>/`: gate.json + harness.c + twin.py).
5. Register the object in the right cupboard; register the oracle in
   `tests/oracle/registry.json`.
6. Build, run the gates, commit the bite to `integrate`.

## Adding a CLI command / tool

- Slash command: handler + registration in the command registry
  (`src/cli/commands.c`, `src/hermes_cli/command_registry.c`), then build
  and exercise it in the running CLI.
- Tool: handler in `src/tools/`, registered via `tools_init_all()` /
  `registry.c` (`registry_dispatch` is the real dispatcher — prove the tool
  by dispatching it with real arguments, not by counting registrations).

## Testing & gates

Run before every commit (full invocations and pitfalls: [gates.md](gates.md)):

```bash
python tests/verify_build_health.py --no-link   # sources + port-object gate
python tests/verify_c_standards.py              # machine-checkable C standard
python tests/verify_naming.py                   # unit/file naming
bash tests/run_all_oracles.sh                   # differential oracle gate
bash tests/run_slv_tests.sh                     # slerm-Lua suites
make parity                                     # refresh every parity sentinel
```

A compile-only gate is not a build gate; a green total is meaningless unless
it counts; a suite that did not run is not a pass.

## Parity scanning

`slermes_parity_battleground.py` compares Python Hermes features against the
C tree by name + PoP annotation. It is **static** — it never executes
anything. Classes: PORTED / REAL_GAP / PARTIAL (no N/A — every unported
feature is work). The scanner is blind to FAPs; only the oracle and watercup
catch behavioral divergence ([fap.md](../fap.md)).

```bash
python scripts/parity_truth.py                          # fail-closed live scan
python tests/slermes_parity_battleground.py --detail    # per-module gaps
python scripts/parity_coverage.py --dirs                # per-area rollup
```

## Contributing

1. Work on `integrate`; one self-contained bite per commit.
2. Behavior first, annotations last; prove with an oracle, not a comment.
3. Run the gates above; a red gate is the next task, not something to build
   on top of.
4. Commit source only — never rebuilt binaries, `.testbin/` artifacts, or
   gate logs. Push to `origin integrate` (never the port branch to `main`).

## CI/CD

`.github/workflows/` carries the upstream-derived pipelines (lint, tests,
release, supply-chain audits) plus `sync-upstream.yml`. The port's
verification gates run on the dev box as above; release engineering is
downstream of parity (see [ROADMAP](../../ROADMAP.md)).
