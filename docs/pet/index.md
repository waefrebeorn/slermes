# Petdex Mascot System

Animated sprites that display alongside the agent's status in the TUI. Built entirely in C11.

## Overview

The pet system provides a visual indicator of the agent's state through animated pixel-art sprites. Pets are downloaded from [petdex.dev](https://petdex.dev) or installed from local files.

**Files:** `src/pet/` (6 C files, 1 header, 1,500+ lines)

## Commands

```
/pet                    # Show active pet info
/pet info               # Show details about the active pet
/pet gallery            # List all installed pets with thumbnails
/pet select <slug>      # Select and activate a pet
/pet remove <slug>      # Remove an installed pet
/pet disable            # Disable the pet display
/pet scale <n>          # Set display scale (0.1 - 3.0)
```

**Examples:**
```
/pet select niko
/pet scale 0.5
/pet disable
```

## Pet States

The pet animates through states based on agent activity:

| State | Trigger | Visual |
|-------|---------|--------|
| `idle` | Agent waiting | Resting animation |
| `wave` | Agent thinking | Waving animation |
| `run` | Tool executing | Running in place |
| `failed` | Error occurred | Sad/exhausted |
| `review` | Reviewing output | Reading animation |
| `jump` | Success/complete | Happy jump |
| `waiting` | Awaiting input | Patient waiting |

## Configuration

In `~/.slermes/config.yaml`:

```yaml
display:
  pet:
    enabled: true
    slug: "niko"           # Active pet slug
    render_mode: "auto"    # auto, kitty, iterm, sixel, unicode, off
    scale: 0.33            # Display scale
    unicode_cols: 24       # Unicode fallback columns
```

Or via CLI:
```
/config display.pet.enabled true
/config display.pet.slug niko
/config display.pet.scale 0.5
```

## Supported Terminal Modes

| Mode | Requires | Description |
|------|----------|-------------|
| `kitty` | Kitty/ghostty/wezterm | Full color, transparency |
| `iterm` | iTerm2 | Inline images |
| `sixel` | Sixel-capable terminal | Sixel graphics |
| `unicode` | Any terminal | Half-block unicode rendering |
| `auto` | — | Auto-detect from TERM env |
| `off` | — | Disable rendering |

Auto-detection:
- `KITTY_WINDOW_ID` or `TERM=*kitty*` → kitty mode
- `TERM_PROGRAM=vscode` → unicode mode
- `WEZTERM_PANE` → kitty mode
- `ghostty` → kitty mode
- No detection → unicode fallback

## Installing Pets

### From petdex.dev (default)
```
/pet select niko
```
Downloads and installs automatically.

### From local files
```bash
# Copy spritesheet to ~/.slermes/pets/<slug>/spritesheet.png
# Create ~/.slermes/pets/<slug>/pet.json with metadata
```

## Architecture

```
pet_commands.c         pet_store.c
     |                      |
     v                      v
pet_state.c  →  pet_render.c  →  Terminal
     |
     v
pet_constants.c   (scale, frame geometry, aliases)

pet_manifest.c ← petdex.dev (HTTP fetch, TTL cache)
```

## API Functions

See `include/pet.h` for the complete API (265 lines).

Key functions:
- `pet_init(cfg)` — Initialize from config
- `pet_select(slug)` — Set active pet
- `pet_info_json()` — Active pet JSON
- `pet_gallery_json()` — Installed pets JSON
- `pet_cells_json(cols)` — TUI frame cells
- `pet_install_pet(slug, force)` — Download and install
- `pet_fetch_manifest(out, max, force)` — Fetch petdex catalog
- `pet_thumbnail_png(slug, len)` — Get thumbnail bytes
