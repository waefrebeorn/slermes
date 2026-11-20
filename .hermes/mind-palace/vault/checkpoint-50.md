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

# Checkpoint 50 — Name Parity Batch 2

**Committed:** 30af060d9
**Parity:** 26 → 39 exact matches

## Renamed Functions (13 total, 64 replacements, 17 files)

| Old C Name | New C Name (matches Python) | Files |
|------------|----------------------------|-------|
| `state_load` | `load_state` | 4 files |
| `state_save` | `save_state` | 4 files |
| `provider_register` | `register_provider` | 3 files |
| `models_dev_lookup_context` | `lookup_models_dev_context` | 1 file |
| `rate_limit_parse_headers` | `parse_rate_limit_headers` | 2 files |
| `skill_extract_conditions` | `extract_skill_conditions` | 2 files |
| `skill_extract_config_vars` | `extract_skill_config_vars` | 2 files |
| `skill_extract_description` | `extract_skill_description` | 2 files |
| `bedrock_get_context_length` | `get_bedrock_context_length` | 1 file |
| `bedrock_is_anthropic_model` | `is_anthropic_bedrock_model` | 1 file |
| `credential_is_borrowed_source` | `is_borrowed_credential_source` | 2 files |
| `skill_is_excluded_path` | `is_excluded_skill_path` | 2 files |
| `skill_iter_index_files` | `iter_skill_index_files` | 2 files |

## Verification
- ✅ All old names eliminated from C source
- ✅ Build: clean compile, 0 errors
- ✅ Tests: 4/4 pass
- ✅ No duplicate symbols

## Cumulative Name Parity (cp48+cp49+cp50)

| Batch | Renamed | Method | Files |
|-------|---------|--------|-------|
| cp48 | 6 | Direct rename (init_agent, classify_api_error, etc.) | 20 |
| cp49 | 0 | Name parity mapping document | — |
| cp50 | 13 | Reorder renames (state_load→load_state, etc.) | 17 |
| **Total** | **19** | | **37** |

**Exact parity: 39 / 1051 (3.7%)**
**Build: Clean, 4/4 pass**
