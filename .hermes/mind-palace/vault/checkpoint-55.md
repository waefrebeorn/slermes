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