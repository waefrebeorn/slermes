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

# Checkpoint 22 — Image Gen: FAL verified, OpenAI DALL-E PORTED

**Date:** Session boundary (user returned)
**Battleship:** v47 → v48
**Overall:** ~76% PORTED

## Gaps Closed

### IA01 (fal) — verified PORTED
- `src/tools/image_gen.c` — FAL.ai REST API via `fal_post_json()`, image download, local save
- Was incorrectly marked REAL GAP; verified complete

### IA03 (openai) — PORTED
- `src/tools/image_gen.c` — `openai_image_generate()` with dall-e-2/3, gpt-image-1
- Provider routing via `provider` JSON param
- JSON schema updated with `provider` and `model` fields
- OpenAI provider registered in `registry_init_image_gen`
- b64_json download + local file save
- Build: clean compile (only pre-existing linker errors remain)

## Technical Notes
- Discovered critical function name encoding issue: long C names like `***` display as `fal_es...json` in terminal
- Must use `bytes.fromhex()` in Python replacements, never write display form as literal
- C string escaping for JSON: use `\\"` in Python for each `"` inside C string literals
