<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Slermes — Project State

The sentinel above is the live scanner verdict, owned by `make parity`.
This file keeps no hand-typed parity counts — they go stale the moment they
are written. For the ranked work list see
[WORK_DASHBOARD.md](WORK_DASHBOARD.md); for the honest numbers' method see
[parity-summary.md](parity-summary.md) and [PARITY_LEDGER](../PARITY_LEDGER.md).

## What is true structurally

- **One native C11 binary** (`slermes` / `slermes.exe`), zero Python at
  runtime. Entry point `src/main.c` dispatches CLI / gateway / cron / acp /
  mcp-serve / api-server.
- **Main-only port** → `origin main` (no feature branches; stale tips under `archive/*`). Python quarry:
  `C:/Users/eman5/hermes-agent` (upstream `NousResearch/hermes-agent`). C
  work is never pushed back upstream.
- **Whole-ecosystem mission** — agent, tools, platforms, plugins, and every
  third-party dependency gets SLERMed from scratch in C11. No N/A, nothing
  "not doable".
- **Verification stack** — battleground scanner (static) → differential
  oracle (behavioral) → watercup trace-cups (path parity) → build health +
  port-object gates. A port is parity-closed only when oracle AND watercup
  say MATCH.

## Re-establish live state (discover, don't synthesize)

```bash
cd C:/Users/eman5/slermes
git branch --show-current && git log --oneline -3 && git status --short
make parity                                   # scan + watercup + walkway
python tests/verify_build_health.py --no-link # MSYS2 env child
bash tests/run_all_oracles.sh                 # ORACLE_JOBS=1 on MSYS2
bash tests/run_slv_tests.sh
python tests/parity_reality.py                # body-level reality
python scripts/gen_port_stages.py             # module funnel S0-S6
```

Dev-box environment facts (Smart App Control, winlibs toolchain, retry
doctrine): [POST_REBOOT_RESUME.md](../POST_REBOOT_RESUME.md).

## History

Session-by-session history through the latest rewrite is archived in
[archive/state-history-pre-the latest run.md](archive/state-history-pre-the latest run.md)
and `.hermes/museum/`. Those are historical notes, not current state — never
quote their counts as live.
