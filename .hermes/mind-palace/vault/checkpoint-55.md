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