#ifndef SLERMES_PET_H
#define SLERMES_PET_H

#include <stdbool.h>
#include <stddef.h>
#include "json.h"

/* ── Pet Constants ──────────────────────────────────────────────────── */
#define PET_FRAME_W             192
#define PET_FRAME_H             208
#define PET_FRAMES_PER_STATE    6
#define PET_LOOP_MS             1100
#define PET_DEFAULT_SCALE       0.33f
#define PET_MIN_SCALE           0.1f
#define PET_MAX_SCALE           3.0f
#define PET_BASE_UNICODE_COLS   24
#define PET_UNICODE_MIN_COLS    16
#define PET_MANIFEST_URL        "https://petdex.dev/api/manifest"
#define PET_DOWNLOAD_TIMEOUT    60.0
#define PET_MANIFEST_TTL        300.0
#define PET_MAX_SLUG            128
#define PET_MAX_NAME            256
#define PET_MAX_DESC            512
#define PET_MAX_URL             1024
#define PET_MAX_PETS            4096
#define PET_THUMB_W             96
#define PET_THUMB_H             104

/* ── Pet State Enum ─────────────────────────────────────────────────── */
typedef enum {
    PET_STATE_IDLE = 0,
    PET_STATE_WAVE,
    PET_STATE_RUN,
    PET_STATE_FAILED,
    PET_STATE_REVIEW,
    PET_STATE_JUMP,
    PET_STATE_WAITING,
    PET_STATE_COUNT
} pet_state_t;

/* ── Row name aliases (max 4 aliases per state) ─────────────────────── */
typedef struct {
    pet_state_t state;
    const char *aliases[4];
    int alias_count;
} pet_state_alias_t;

/* ── Manifest Entry ─────────────────────────────────────────────────── */
typedef struct {
    char slug[PET_MAX_SLUG];
    char display_name[PET_MAX_NAME];
    char kind[64];
    char submitted_by[PET_MAX_NAME];
    char spritesheet_url[PET_MAX_URL];
    char pet_json_url[PET_MAX_URL];
    char zip_url[PET_MAX_URL];
} pet_manifest_entry_t;

/* ── Manifest cache ─────────────────────────────────────────────────── */
typedef struct {
    pet_manifest_entry_t *entries;
    int count;
    double fetched_at; /* monotonic time */
} pet_manifest_cache_t;

/* ── Installed Pet ──────────────────────────────────────────────────── */
typedef struct {
    char slug[PET_MAX_SLUG];
    char display_name[PET_MAX_NAME];
    char description[PET_MAX_DESC];
    char directory[PET_MAX_URL];
    char spritesheet_path[PET_MAX_URL];
    bool exists;
    bool generated;
} pet_installed_t;

/* ── Render Modes ───────────────────────────────────────────────────── */
typedef enum {
    PET_MODE_AUTO = 0,
    PET_MODE_KITTY,
    PET_MODE_ITERM,
    PET_MODE_SIXEL,
    PET_MODE_UNICODE,
    PET_MODE_OFF,
    PET_MODE_COUNT
} pet_render_mode_t;

/* ── Cell (half-block pixel pair) ───────────────────────────────────── */
typedef struct {
    unsigned char tr, tg, tb, ta;
    unsigned char br, bg, bb, ba;
} pet_cell_t;

/* ── Pet Configuration (from config.yaml display.pet.*) ─────────────── */
typedef struct {
    bool  enabled;
    char  slug[PET_MAX_SLUG];
    char  render_mode[16];    /* auto/kitty/iterm/sixel/unicode/off */
    float scale;
    int   unicode_cols;
} pet_config_t;

/* ════════════════════════════════════════════════════════════════════════
   Pet Constants API
   ════════════════════════════════════════════════════════════════════════ */

/* Clamp scale to [MIN_SCALE, MAX_SCALE] */
float pet_clamp_scale(float scale);

/* Terminal columns for a given scale */
int pet_cols_for_scale(float scale);

/* Resolve terminal width: explicit unicode_cols override, else from scale */
int pet_resolve_cols(float scale, int unicode_cols);

/* Get the row taxonomy for a spritesheet (codex=9 rows, legacy=8 rows) */
const char **pet_state_rows_for_grid(int row_count);

/* Get the spritesheet row index for a PetState (clamped, returns 0 on miss) */
int pet_state_row_index(pet_state_t state, int row_count);

/* Convert state enum to string */
const char *pet_state_string(pet_state_t state);

/* Convert string to state enum */
pet_state_t pet_state_from_string(const char *s);

/* Get alias names for a state (NULL-terminated array) */
const char **pet_state_aliases(pet_state_t state);

/* ════════════════════════════════════════════════════════════════════════
   Pet State Machine API
   ════════════════════════════════════════════════════════════════════════ */

/* Check if all todos are done (>=1 todo, every one completed/cancelled) */
bool pet_todos_all_done(const char **statuses, int count);

/* Derive pet animation state from agent activity signals */
pet_state_t pet_state_derive(
    bool busy,
    bool awaiting_input,
    bool error,
    bool celebrate,
    bool just_completed,
    bool tool_running,
    bool reasoning
);

/* ════════════════════════════════════════════════════════════════════════
   Pet Manifest API
   ════════════════════════════════════════════════════════════════════════ */

/* Fetch the petdex manifest (cached in-process for TTL seconds) */
int pet_fetch_manifest(pet_manifest_entry_t *out, int max_count, bool force);

/* Find a manifest entry by slug */
bool pet_find_entry(const char *slug, pet_manifest_entry_t *out);

/* Prefetch manifest in background (daemon thread, best-effort) */
void pet_prefetch_manifest(void);

/* Clear manifest cache */
void pet_clear_manifest_cache(void);

/* ════════════════════════════════════════════════════════════════════════
   Pet Store API
   ════════════════════════════════════════════════════════════════════════ */

/* Get pets directory path (~/.slermes/pets/) */
const char *pet_pets_dir(void);

/* Normalize slug to a safe path segment */
const char *pet_safe_slug(const char *slug);

/* Load an installed pet by slug (returns true if found) */
bool pet_load_pet(const char *slug, pet_installed_t *out);

/* List all installed pets */
int pet_installed_pets(pet_installed_t *out, int max_count);

/* Resolve active pet: configured slug first, else first installed */
bool pet_resolve_active_pet(const char *configured_slug, pet_installed_t *out);

/* Install a locally generated pet (spritesheet + pet.json). */
bool pet_install(const char *slug, const char *display_name,
                 const char *description, const char *spritesheet_src);

/* Install a pet from the manifest */
bool pet_install_pet(const char *slug, pet_installed_t *out, bool force);

/* Remove an installed pet */
bool pet_remove_pet(const char *slug);

/* Rename a pet's display name (also updates slug/dir if free) */
const char *pet_rename_pet(const char *slug, const char *display_name);

/* Slugify a display name into a filesystem-safe slug */
const char *pet_slugify(const char *name);

/* Generate a unique slug that doesn't collide with installed pets */
const char *pet_unique_slug(const char *name);

/* Generate thumbnail PNG bytes for a pet (cached on disk) */
unsigned char *pet_thumbnail_png(const char *slug, int *out_len);

/* Get thumbs directory path */
const char *pet_thumbs_dir(void);

/* Check if a pet was AI-generated (reads pet.json generated field) */
bool pet_is_generated(const char *slug);

/* Export pet spritesheet bytes (returns malloc'd buffer, caller frees) */
unsigned char *pet_export_pet(const char *slug, int *out_len);

/* Check if a URL is a petdex host */
bool pet_is_petdex_host(const char *url);

/* Download JSON from a URL */
json_t *pet_download_json(const char *url);

/* Copy spritesheet file to destination */
bool pet_write_spritesheet(const char *source_path, const char *dest_path);

/* Register a local pet by copying its spritesheet and creating metadata */
bool pet_register_local_pet(const char *slug, const char *display_name,
                            const char *spritesheet_path, const char *description);

/* ════════════════════════════════════════════════════════════════════════
   Pet Render API
   ════════════════════════════════════════════════════════════════════════ */

/* Detect terminal graphics capability from env vars */
pet_render_mode_t pet_detect_terminal_graphics(void);

/* Resolve effective render mode from config setting + environment */
pet_render_mode_t pet_resolve_mode(const char *configured, bool is_tty);

/* Get frame count for a state row (padding-trimmed) */
int pet_state_frame_count(const char *spritesheet_path, pet_state_t state);

/* ── Decoded sprite frames (from-scratch PNG decoder) ───────────────── */

/* One decoded RGBA frame cropped from a spritesheet state row. */
typedef struct {
    int           width;      /* PET_FRAME_W */
    int           height;     /* PET_FRAME_H */
    unsigned char *rgba;      /* w*h*4, non-premultiplied; caller frees */
} pet_frame_t;

/* Decode the real (padding-trimmed) frames for one state row of a
 * spritesheet. Returns malloc'd array (caller frees with free()); *out_count
 * gets the frame count. Returns NULL on decode failure. */
pet_frame_t *pet_sprite_frames(const char *spritesheet_path, pet_state_t state,
                               int *out_count);

/* Get the active pet's spritesheet path (static buffer, empty if none). */
const char *pet_active_spritesheet(void);

/* Build a kitty image ID from slug */
int pet_kitty_image_id(const char *slug);

/* Get kitty color hex string (static buffer) */
const char *pet_kitty_color_hex(int image_id);

/* Check if renderer is available (mode != off AND spritesheet exists) */
bool pet_renderer_available(pet_render_mode_t mode, const char *spritesheet_path);

/* ════════════════════════════════════════════════════════════════════════
   Pet Commands API
   ════════════════════════════════════════════════════════════════════════ */

/* Initialize pet system (load config, resolve active pet) */
void pet_init(const pet_config_t *cfg);

/* Get current pet state (from last derive call) */
pet_state_t pet_get_state(void);

/* Update pet state from agent signals */
void pet_update_state(bool busy, bool awaiting_input, bool error,
                       bool celebrate, bool just_completed,
                       bool tool_running, bool reasoning);

/* Get active pet info as JSON string (caller frees) */
char *pet_info_json(void);

/* Get pet gallery as JSON string (caller frees) */
char *pet_gallery_json(void);

/* Get pet cells for TUI as JSON string (caller frees) */
char *pet_cells_json(int cols);

/* Select a pet by slug */
bool pet_select(const char *slug);

/* Disable pet */
void pet_disable(void);

/* Set pet scale */
void pet_set_scale(float scale);

/* Get current pet scale */
float pet_get_scale(void);

/* Check if pet is enabled */
bool pet_is_enabled(void);

/* Get current pet's slug (active slug, empty if none) */
const char *pet_active_slug(void);

#endif /* SLERMES_PET_H */
