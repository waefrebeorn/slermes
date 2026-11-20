<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T17:36:51Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Petdex Mascot System

Animated pixel-art sprites that reflect the agent's activity. The subsystem is
implemented in C11 and ports the Python `agent/pet/` package (PoP references are in
the source files).

## Where the code lives

- `src/pet/` — 8 C files:
  - `pet_commands.c` — command/API surface (`pet_info_json`, `pet_gallery_json`,
    select/remove/disable/scale, config defaults)
  - `pet_store.c` — installed-pet store under `~/.slermes/pets/<slug>/`
    (profile-scoped), `pet.json` metadata, spritesheet resolution
  - `pet_state.c` — agent activity → animation state (`pet_state_derive`)
  - `pet_render.c` — terminal capability detection, frame geometry, kitty/unicode
    encoding; decodes spritesheets with the from-scratch PNG decoder (`pngdec`)
  - `pet_manifest.c` — fetches `https://petdex.dev/api/manifest`, in-process cache
    with a 300 s TTL (`PET_MANIFEST_TTL`)
  - `pet_constants.c` — frame geometry, state-row taxonomies (legacy 8-row and
    Codex/petdex 9-row atlases), scale clamping
  - `atlas.c` — deterministic spritesheet assembly (port of
    `agent/pet/generate/atlas.py`), pure RGBA pixel-buffer ops
  - `pet_prompts.c` — hatch prompt construction (`pet_prompts_build_base`)
- Headers: `include/pet.h`, `include/pet_atlas.h`, `include/pet_prompts.h`,
  `include/pet_ui.h`.
- Integration: `cmd_pet` in `src/hermes_cli/cli_cmd_display.c` (the `/pet` slash
  handler), `src/hermes_cli/pet_input.c` + `pets.c` (config-driven CLI side),
  `src/pet_ui.c` (SDL GUI renderer).

## Commands

`/pet` is registered in the live `COMMANDS[]` registry (Display category). Verified
subcommands in `cmd_pet`:

```
/pet                    # same as /pet info
/pet info               # enabled state, active slug, state, scale, JSON dump
/pet gallery            # installed pets as JSON (no terminal thumbnails)
/pet select <slug>      # activate an installed pet
/pet remove <slug>      # remove an installed pet
/pet disable            # disable the pet display
/pet enable             # print-only: says to re-init with config; does not re-enable
/pet scale <n>          # set scale, clamped to 0.1–3.0 (PET_MIN_SCALE/PET_MAX_SCALE)
/pet hatch [concept]    # generate a new pet: build prompt → image_generate tool →
                        # install into the pet store → select it
```

`/hatch` (alias `/generate-pet`, Tools & Skills category) routes to the same hatch
pipeline via `ccm_handle_hatch_command`.

## Pet states

Seven states (`pet_state_t` in `include/pet.h`). `pet_state_derive`
(`src/agent/pet/pet_state.c`) picks exactly one, in this priority order:

| Priority | Condition | State |
|----------|-----------|-------|
| 1 | error | `failed` |
| 2 | celebrate | `jump` |
| 3 | just_completed | `wave` |
| 4 | awaiting_input | `waiting` |
| 5 | tool_running | `run` |
| 6 | reasoning | `review` |
| 7 | busy | `run` |
| — | none of the above | `idle` |

(Older docs mapped `wave` to "thinking" and `jump` to generic success; the table above
is what the code actually does.)

## Configuration

Config keys under `display.pet` in `~/.slermes/config.yaml` (read by
`src/hermes_cli/pet_input.c`, written by `src/hermes_cli/pets.c`):

```yaml
display:
  pet:
    enabled: true
    slug: "niko"
    render_mode: "auto"
    scale: 0.33
    unicode_cols: 24
```

The GUI integration (`src/pet_ui.c`) initializes with `enabled=true`, `scale=0.33`,
`unicode_cols=24`; `pet_commands.c` defaults `render_mode` to `auto`. Via CLI:

```
/config display.pet.enabled true
/config display.pet.slug niko
/config display.pet.scale 0.5
```

## Terminal render modes

Modes (`g_render_mode_names` in `pet_render.c`): `auto`, `kitty`, `iterm`, `sixel`,
`unicode`, `off`. A configured mode name wins; `auto` (or an unknown name) detects
from the environment, and non-TTY output resolves to `off` (`pet_resolve_mode`).

Detection order in `pet_detect_terminal_graphics` (TERM/TERM_PROGRAM lowercased):

1. `TERM_PROGRAM=vscode` → `unicode` (xterm.js can't do kitty/iterm protocols)
2. `KITTY_WINDOW_ID` set, or `TERM` contains `kitty`/`ghostty` → `kitty`
3. `TERM_PROGRAM=ghostty` or `wezterm`, or `WEZTERM_PANE` set → `kitty`
4. `TERM_PROGRAM=iterm.app` or `ITERM_SESSION_ID` set → `iterm`
5. `TERM_PROGRAM=mintty`, or `TERM` contains `foot`/`mlterm`/`sixel` → `sixel`
6. otherwise → `unicode` (half-block fallback)

## Rendering surfaces

- **Terminal cell path:** `pet_render.c` encodes frames for kitty/unicode terminals;
  `pet_cells_json(cols)` produces the cell data. The TUI backend exposes it through
  the JSON-RPC pet methods (`pet_info`, `pet_cells`, `pet_gallery`, `pet_select`,
  `pet_remove`, `pet_disable`, `pet_scale`, `pet_thumb` in
  `src/hermes_cli/tui_json_rpc.c`).
- **SDL GUI path:** `src/pet_ui.c` draws a state-colored vector mascot (ears, head,
  body, tail, state-dependent eyes/mouth). Its source notes that full PNG spritesheet
  decode is not used in that GUI renderer — the terminal path is the one that decodes
  real spritesheets.

## Architecture

```
cmd_pet (cli_cmd_display.c) / pet_* JSON-RPC methods
        │
        v
pet_commands.c ──> pet_store.c (~/.slermes/pets/<slug>/)
        │               ^
        v               │
pet_state.c        pet_manifest.c (petdex.dev/api/manifest, 300 s TTL cache)
        │
        v
pet_render.c (mode detect + PNG decode + kitty/unicode encode) ──> terminal cells
pet_ui.c (SDL GUI vector renderer) ──> desktop window

hatch pipeline: pet_prompts.c → image_generate tool → atlas.c → pet_store.c
```

## API

See `include/pet.h` for the full API. Key functions (all present in the header):
`pet_init(cfg)`, `pet_select(slug)`, `pet_info_json()`, `pet_gallery_json()`,
`pet_cells_json(cols)`, plus `pet_set_scale`/`pet_get_scale`, `pet_disable`,
`pet_get_state`, `pet_state_derive`.

Behavioral parity with Python `agent/pet/` is proven per-port by the oracle suites;
this page describes the C code as inspected, not a parity guarantee.
