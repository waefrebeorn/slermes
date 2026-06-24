# Checkpoint 24 — Makefile fix, truncation bug fix, xAI video gen

## Gaps closed

### VG02 (xai video) — PORTED
- `src/tools/video_gen.c:272-370` — xai_video_generate()
- grok-imagine-video, POST https://api.x.ai/v1/videos/generations
- text-to-video, image-to-video (image_url), reference_image_urls (max 7)
- duration clamp: 15s max (10s with refs), aspect ratio mapping (16:9, 9:16, 1:1)
- Provider routing in video_generate_handler (provider=xai)
- Registered xai provider in registry_init_video_gen()

### IA01-IA05 (image gen) — stale claims corrected to PORTED
- IA01 fal: `src/tools/image_gen.c:372-527`
- IA02 krea: `src/tools/image_gen.c:123-252`
- IA03 openai: `src/tools/image_gen.c:35-122`
- IA04 openai-codex: N/A (Python-specific)
- IA05 xai: `src/tools/image_gen.c:254-338`

### VG01 (fal video) — stale claim corrected to PORTED
- `src/tools/video_gen.c:34-238`

## Build fix
- Added CLI_OBJ definition to Makefile (was referenced but never defined)
- Added mcp_serve.o, api_server.o, hermes_env_keys.o to DEPS_OBJ
- Binary now links cleanly

## Truncation bug fix
- Source files had literal `***` (3 asterisks) and `...` (3 dots) in function names
- Fixed using bytes.fromhex() to construct real names from fal_common.c hex analysis
- Real names: ***, ***, fal_er...onse, fal_er...http, fal_vi...able

## Battleship impact
- IA: 3→7 PORTED, 5→0 REAL GAP, 0→1 N/A
- VG: 1→2 PORTED, 1→0 REAL GAP
- Overall: ~77%→~78% PORTED, ~12%→~11% REAL GAP
