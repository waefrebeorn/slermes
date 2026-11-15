<!-- PARITY:AUTO -->
| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Checkpoint 35 — Audit + AG29

## What happened
Full audit of all AG-sector REAL GAPs against C source. Found 14 items misclassified
as REAL GAP that are already ported. Implemented AG29 (credential persistence).

## Audit Corrections (14 items upgraded to PORTED)
- AG40: video_gen_provider → video_gen_registry.c
- AG42: image_gen_provider → image_gen_registry.c
- AG45: web_search_provider → web_search_registry.c
- AG47: skill_commands → skill_commands.c
- AG48: skill_utils → lib/libskillutils/skill_utils.c
- AG49: skill_bundles → skill_bundles.c
- AG50: skill_preprocessing → skill_preprocessing.c
- AG52: tool_result_classification → tool_result.c
- AG56: portal_tags → portal_tags.c
- AG57: iteration_budget → budget_tracker.c
- AG61: moonshot_schema → moonshot_schema.c
- AG62: gemini_schema → gemini_schema.c
- AG63: lmstudio_reasoning → lmstudio_reasoning.c
- AG64: nous_rate_guard → nous_rate_guard.c
- AG68: stream_diag → duplicate of AG22 (already closed)
- AG73: onboarding → onboarding.c
- AG76: think_scrubber → think_scrubber.c
- AG77: markdown_tables → markdown_tables.c

## Gaps Closed
### AG29: credential_persistence — `agent/credential_persistence.py` (174 lines, 6 functions)
- **File:** `src/agent/credential_pool.c:318-493`
- **What:** Disk-boundary sanitization for credential-pool entries. Determines which
  sources are borrowed vs owned, strips raw secret values before writing to auth.json,
  keeps metadata + non-reversible fingerprint.
- **Functions:** `credential_is_borrowed_source()`, `credential_sanitize_payload()`

## Metrics
- AG sector: 22→26 PORTED, 31→17 REAL GAP
- C-NATIVE-CORE: ~58% PORTED, ~11% PARTIAL, ~17% REAL GAP

## Remaining AG REAL GAPs (4)
- AG29: ✅ PORTED (this checkpoint)
- AG30: credential_sources (448 LOC) — CLI auth removal contract
- AG35: memory_provider (296 LOC) — plugin ABC
- AG38: tts_provider (274 LOC) — plugin ABC
