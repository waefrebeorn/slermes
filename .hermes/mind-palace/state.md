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
