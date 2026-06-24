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
