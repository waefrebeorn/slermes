<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# State — mind palace

The sentinel above is the live scanner verdict (`make parity` owns it). This
file keeps no hand-typed counts — they go stale the moment they are written.

## Re-establish state

```bash
cd C:/Users/eman5/slermes
git log --oneline -3 && git status --short
make parity
python tests/verify_build_health.py --no-link   # MSYS2 env child
bash tests/run_all_oracles.sh                   # ORACLE_JOBS=1
bash tests/run_slv_tests.sh
python tests/parity_reality.py
```

## Structural truths

- Single native C11 binary, zero Python at runtime; main-only port to `origin main`.
- Verification stack: scanner → diff oracle → watercup → build/port gates.
  Parity-closed = oracle AND watercup MATCH.
- Whole-ecosystem mission incl. third-party dependencies; no N/A.

Historical session states: `archive/`, `vault/`, and `../museum/`.
