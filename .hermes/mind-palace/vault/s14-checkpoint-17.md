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

# Checkpoint 17 — Battleship Reclassification Round 2

## TD Sector → FULLY PORTED
All 8 TD gaps verified as already implemented in C code.

## Provider Fields (PR04-PR07) → PORTED
Added env_vars, auth_type, display_name, signup_url, default_aux_model to provider_t.

## AL09 → PORTED
Truncated response continuation implemented.

## GW16 → PARTIAL
`src/api_server.c` (1232 lines) covers core endpoints. Missing: /v1/responses, /v1/runs, session messages/fork.

## Summary of All Sectors

| Sector | Status | REAL GAP |
|--------|--------|----------|
| AL | PARTIAL | AL07 |
| CL | PORTED | 4 small |
| GW | PORTED | 15 (mostly cosmetic) |
| TD | PORTED | 0 |
| PR | PORTED | 8 (PR01/PR08/PR09 are XL) |
| CR | PORTED | 9 |
| SK | PORTED | 3 |
| ME | PORTED | 2 |
| SE | PORTED | 9 |
| RD | PORTED | 0 |
| MS | AUDITED | ~5 (down from 21) |
| PL | AUDITED | 2 (PL14, PL18) |
| TU | AUDITED | 3 |
| AC | AUDITED | 4 |
