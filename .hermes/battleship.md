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

# Battleship — REAL_GAP Elimination Battleground

The sentinel above is the live scanner verdict (`make parity` owns it).
**Rewriting from scratch in C is the point.** Stubs = REAL_GAP. There is no
N/A: every unported feature is work, and nothing is "not doable" — we do it
because we are doing it in C11.

## The battleground map (derive it live, never from prose)

```bash
python tests/slermes_parity_battleground.py --json        # full scan
python tests/slermes_parity_battleground.py --detail      # per-module gaps
python scripts/parity_coverage.py --dirs                  # per-area rollup
python scripts/parity_coverage.py --fanin                 # rank by reuse
```

The scanner's REAL_GAP list is regenerated into
[docs/real-gap-list.md](../docs/real-gap-list.md) by `make parity` — that
file, not this one, is the function-level map. Mission areas follow the
Python package tree: `hermes_cli`, `agent`, `gateway`, `tools`, `cron`,
`plugins`, `pm`, `tui_gateway`, `acp_adapter` — each with C homes under
`src/<area>/` named for the module they port.

## Resolution protocol

1. Pick the next module from the live gap list (match the axis to the ask:
   fan-in for reuse, short-gap count for throughput, gaps×LOC for the hole).
2. For each REAL_GAP function:
   a. Read the LIVE Python source in the quarry.
   b. Grep `src/ include/ lib/` — behavior under another name is a naming
      gap; expose it, don't double-code.
   c. Implement real C logic (never hermes_log + return NULL).
   d. Prove it: oracle (`tests/run_one_oracle.sh`) and watercup gate for
      pure helpers.
3. Build, run the gates, commit the bite.
4. Re-scan: verify the gap actually closed with a real body behind it. A
   falling gap count with an empty body behind it is scoreboard gaming, not
   progress — `tests/parity_reality.py` is the honest scoreboard.

No choices. No questions. Never stop between gaps.

## Usage-level candidates

[battleship-user.md](battleship-user.md) carries mined usage-gap candidates
(from the Python test suite, issues, and config docs). They are candidates,
not verified state — check each against the live code before working it.
