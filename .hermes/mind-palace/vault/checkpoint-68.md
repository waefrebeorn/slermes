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

# Checkpoint 68 — CP68/v95: AG25 recover_with_credential_pool ported (17/24)

## Changes

**Ported `recover_with_credential_pool()` from `agent/agent_runtime_helpers.py:544-723` (180L)** to `src/agent/credential_pool.c`.

### Implementation
- Created `credential_pool_recover()` — wraps `credential_pool_report()` with:
  - **Retry counting**: first 429 hit → CRED_RECOVER_RETRY, second → CRED_RECOVER_ROTATE
  - **Billing detection**: 402 status + error-body patterns (`usage_limit_reached`, `gousagelimit`)
  - **Auth**: 401/403 → mark exhausted, rotate to next key
  - **Cross-provider guard**: skip pool mutation when provider name doesn't match
  - **Pool exhaustion**: CRED_RECOVER_FALLBACK when no usable entries remain
- Added `cred_recover_t` enum (CRED_RECOVER_NONE/RETRY/ROTATE/FALLBACK)

### Evidence
- `src/agent/credential_pool.c:499-580` — implementation
- `include/credential_pool.h:116-134` — declaration + cred_recover_t enum

### Impact
- **AG25:** 16/24 → 17/24 (71%)
- **Build:** Clean, 0 errors
- **Tests:** 4/4 pass
- **Commit:** `0a2b1c0a2`
