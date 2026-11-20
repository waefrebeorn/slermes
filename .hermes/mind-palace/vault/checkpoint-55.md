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

# Checkpoint 55 — AG44 extract_image_refs PORTED

**Battleship:** v81→v82

## What was done

### AG44: extract_image_refs → full C implementation

- New function `extract_image_refs()` in `src/agent/image_routing.c:430-618`
- Port of Python `agent/image_routing.py:extract_image_refs()`
- Scans free-form text for image references:
  - Local paths (absolute `/` or home-relative `~/`) with image extensions
  - URLs (`http://` / `https://`) with image extensions
- Skips matches inside fenced code blocks (``````) and inline backticks (`)
- Deduplicated, order-preserving output via malloc'd arrays
- Supports image extensions: .png, .jpg, .jpeg, .gif, .webp, .bmp, .tiff, .tif, .heic, .heif
- Strips trailing punctuation from URLs (.,;:!?)]>)

### File:line evidence
- Implementation: `src/agent/image_routing.c:430-618`
- Header declaration: `include/image_routing.h:120-125`
- Extension helper: `image_routing.c:409-418`

### Build/Test
- Clean compile, 0 errors
- 4/4 tests passing

### Classification Changes
- AG44 image_routing: 2/3 → 3/3 public functions — **PARTIAL → PORTED**