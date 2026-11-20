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

# Checkpoint 36 — AG29 + AG30

## Gaps Closed

### AG29: credential_persistence — `agent/credential_persistence.py` (174 lines, 6 functions)
- **File:** `src/agent/credential_pool.c:318-493`
- **Functions:** `credential_is_borrowed_source()`, `credential_sanitize_payload()`
- **What:** Disk-boundary sanitization for credential-pool entries. Determines which
  sources are borrowed vs owned, strips raw secret values before writing to auth.json,
  keeps metadata + non-reversible fingerprint.

### AG30: credential_sources — `agent/credential_sources.py` (448 lines, 14 functions)
- **File:** `src/agent/credential_sources.c` (new)
- **Functions:** Full RemovalStep registry pattern with 9 registered steps:
  copilot, env, claude_code, hermes_pkce, nous_device_code, codex_device_code,
  xai_oauth, qwen_cli, custom_config
- **What:** Unified removal contract for every credential source. Each source registers
  a RemovalStep with match/remove functions. Registry searched in order, first match wins.
  Handles .env cleanup, OAuth file deletion, auth.json provider clearing, and source
  suppression.

## Metrics
- AG sector: 36→38 PORTED, 17→15 REAL GAP
- C-NATIVE-CORE: ~59% PORTED, ~11% PARTIAL, ~16% REAL GAP

## Files Modified
- src/agent/credential_pool.c (AG29)
- src/agent/credential_sources.c (new, AG30)
- include/credential_pool.h (AG29 declarations)
- include/credential_sources.h (new, AG30 declarations)
- Makefile (added credential_sources.o)
- .hermes/mind-palace/battleship-v40.md (v57)
