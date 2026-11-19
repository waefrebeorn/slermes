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

# Next Session: Function + Feature Parity — Steamroll All Remaining REAL_GAPs

## Goal: Close the remaining REAL_GAPs and SLERM the whole ecosystem.
Rewriting from scratch in C is the point; there is NO N/A, no "out of scope",
and nothing "not doable". Every Python feature AND every third-party
dependency is work. Agentic work takes minutes — keep taking concrete bites.

> Live parity counts are owned by `make parity` and stamped into the sentinel
> above. Never hand-type a count here; run the tools and read the sentinel.

## The loop (one module at a time, finish it)

1. `python tests/slermes_parity_battleground.py --detail` filtered to the
   module (or `scripts/parity_coverage.py --by-module` to pick it).
2. Grep `src/ include/ lib/` for each symbol BEFORE writing — behavior under
   another name is a naming gap, the cheapest real fix.
3. Implement real C logic, or expose the existing function under the name
   callers use. Never an empty body with a PoP comment.
4. Prove it: `bash tests/run_one_oracle.sh <name> ...` and a watercup gate
   for pure helpers. Wire new suites into the standing gates.
5. Build, run the gates, commit the bite to `integrate`.

## Honest scoreboards

- Scanner (`make parity`) — feature-level PoP claims; a pointer, not a verdict.
- `tests/parity_reality.py` — body-level reality; placeholders don't count.
- Oracle + watercup — the only behavioral proof. Parity-closed = BOTH MATCH.

A falling gap count with `void f(void){}` behind it is scoreboard gaming, not
progress. The only honest parity number is the fraction of features with a C
body that does real work.

## Dependency front

whisper.cpp → c11_whisper (active); SQLite engine, ncurses (retire via the
Lua UI), ctranslate2, and the long-horizon crypto/inflate/regex/raster
targets. See docs/third-party.md. Same rule: reference in, from-scratch C11
out, oracle/watercup proof, no escape hatches.

No choices. No questions. Never stop between gaps.
