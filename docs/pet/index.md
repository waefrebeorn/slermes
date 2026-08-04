# Petdex Mascot System

Animated sprites that display alongside the agent's status in the TUI. Built entirely in C11.

## Overview

The pet system provides a visual indicator of the agent's state through animated pixel-art sprites. Pets are downloaded from petdex.dev or installed from local files.

**Files:** `src/pet/` (6 C files, 1 header)

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
    slug: "niko"
    render_mode: "auto"
    scale: 0.33
    unicode_cols: 24
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

## Architecture

```
pet_commands.c  →  pet_store.c  →  pet_state.c  →  pet_render.c  →  Terminal
     |
     v
pet_manifest.c  ←  petdex.dev (HTTP fetch, TTL cache)
pet_constants.c  (scale, frame geometry, aliases)
```

## API Functions

See `include/pet.h` for the complete API. Key functions: `pet_init()`, `pet_select(slug)`, `pet_info_json()`, `pet_gallery_json()`, `pet_cells_json(cols)`.