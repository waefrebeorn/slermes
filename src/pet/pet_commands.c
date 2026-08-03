/*
 * pet_commands.c — Pet CLI commands and global pet state management
 *
 * Port of Python: agent/pet/store.py, agent/pet/state.py (command layer)
 * Provides the hermes pets subcommand and the global pet state machine.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pet.h"
#include "hermes_core_types.h"
#include "hermes_logger.h"

/* ── Global pet state ───────────────────────────────────────────────── */
static struct {
    bool          initialized;
    pet_config_t  config;
    pet_installed_t active_pet;
    bool          has_active_pet;
    pet_state_t   current_state;
    float         scale;
    bool          enabled;
} g_pet;

/* Score/progress char for pet states */
static const char *pet_state_emoji(pet_state_t state) {
    switch (state) {
        case PET_STATE_IDLE:    return "\xF0\x9F\x90\xB1"; /* 🐱 */
        case PET_STATE_WAVE:    return "\xF0\x9F\x96\x90"; /* 🖐 */
        case PET_STATE_RUN:     return "\xF0\x9F\x8F\x83"; /* 🏃 */
        case PET_STATE_FAILED:  return "\xF0\x9F\x92\xA5"; /* 💥 */
        case PET_STATE_REVIEW:  return "\xF0\x9F\x93\x96"; /* 📖 */
        case PET_STATE_JUMP:    return "\xF0\x9F\xA6\x84"; /* 🦄 */
        case PET_STATE_WAITING: return "\xE2\x8F\xB3";     /* ⏳ */
        default: return "\xF0\x9F\x90\xB1";
    }
}

/* ════════════════════════════════════════════════════════════════════════
   Pet System Lifecycle
   ════════════════════════════════════════════════════════════════════════ */

/* PoP: pet_init @ agent/pet/store.py:resolve_active_pet */
void pet_init(const pet_config_t *cfg) {
    if (g_pet.initialized) return;

    if (cfg) {
        g_pet.config = *cfg;
        g_pet.enabled = cfg->enabled;
        g_pet.scale = pet_clamp_scale(cfg->scale);
    } else {
        g_pet.enabled = true;
        g_pet.scale = PET_DEFAULT_SCALE;
        g_pet.config.enabled = true;
        snprintf(g_pet.config.slug, sizeof(g_pet.config.slug), "");
        snprintf(g_pet.config.render_mode, sizeof(g_pet.config.render_mode), "auto");
        g_pet.config.scale = PET_DEFAULT_SCALE;
        g_pet.config.unicode_cols = 0;
    }

    /* Resolve active pet */
    g_pet.has_active_pet = pet_resolve_active_pet(
        g_pet.config.slug[0] ? g_pet.config.slug : NULL,
        &g_pet.active_pet
    );

    g_pet.initialized = true;
    hermes_log(LOG_DEBUG, "pet", "pet system initialized (active=%s, scale=%.2f)",
               g_pet.has_active_pet ? g_pet.active_pet.slug : "none",
               g_pet.scale);
}

/* ════════════════════════════════════════════════════════════════════════
   State Management
   ════════════════════════════════════════════════════════════════════════ */

pet_state_t pet_get_state(void) {
    return g_pet.current_state;
}

void pet_update_state(bool busy, bool awaiting_input, bool error,
                       bool celebrate, bool just_completed,
                       bool tool_running, bool reasoning) {
    if (!g_pet.enabled) return;
    g_pet.current_state = pet_state_derive(
        busy, awaiting_input, error, celebrate,
        just_completed, tool_running, reasoning
    );
}

/* ════════════════════════════════════════════════════════════════════════
   JSON Response Builders
   ════════════════════════════════════════════════════════════════════════ */

/* PoP: pet_info_json @ tui_gateway/server.py:pet.info */
char *pet_info_json(void) {
    json_t *root = json_object();
    json_set(root, "active", json_new_bool(g_pet.enabled && g_pet.has_active_pet));

    if (g_pet.has_active_pet) {
        json_set(root, "slug", json_new_string(g_pet.active_pet.slug));
        json_set(root, "name", json_new_string(g_pet.active_pet.display_name));
        json_set(root, "description", json_new_string(g_pet.active_pet.description));
        json_set(root, "state", json_new_string(pet_state_string(g_pet.current_state)));
        json_set(root, "emoji", json_new_string(pet_state_emoji(g_pet.current_state)));
        json_set(root, "scale", json_new_number(g_pet.scale));
        json_set(root, "frame_count", json_int(PET_FRAMES_PER_STATE));
        json_set(root, "exists", json_new_bool(g_pet.active_pet.exists));
        json_set(root, "generated", json_new_bool(g_pet.active_pet.generated));
    }

    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* PoP: pet_gallery_json @ agent/pet/store.py:installed_pets */
char *pet_gallery_json(void) {
    json_t *root = json_object();
    json_t *pets_arr = json_array();

    /* Heap-allocate the pet list: PET_MAX_PETS (4096) x pet_installed_t is
     * ~10MB — a stack array overflows the default thread stack. */
    pet_installed_t *pets = calloc((size_t)PET_MAX_PETS, sizeof(pet_installed_t));
    if (!pets) {
        json_free(root);
        return strdup("{\"pets\":[],\"error\":\"oom\"}");
    }
    int count = pet_installed_pets(pets, PET_MAX_PETS);

    for (int i = 0; i < count; i++) {
        json_t *p = json_object();
        json_set(p, "id", json_new_string(pets[i].slug));
        json_set(p, "name", json_new_string(pets[i].display_name));
        json_set(p, "description", json_new_string(pets[i].description));
        json_set(p, "adopted", json_new_bool(true));
        json_set(p, "exists", json_new_bool(pets[i].exists));
        json_set(p, "generated", json_new_bool(pets[i].generated));
        json_append(pets_arr, p);
    }
    json_set(root, "pets", pets_arr);

    /* Also include current state info */
    if (g_pet.has_active_pet) {
        json_set(root, "active_slug", json_new_string(g_pet.active_pet.slug));
    }
    json_set(root, "active_state", json_new_string(pet_state_string(g_pet.current_state)));
    json_set(root, "enabled", json_new_bool(g_pet.enabled));
    json_set(root, "scale", json_new_number(g_pet.scale));

    char *s = json_serialize(root);
    free(pets);
    json_free(root);
    return s;
}

/* PoP: pet_cells_json @ tui_gateway/server.py:pet.cells */
char *pet_cells_json(int cols) {
    json_t *root = json_object();
    json_t *cells_arr = json_array();

    if (cols <= 0) cols = pet_cols_for_scale(g_pet.scale);

    /* Build cell grid: cols × (cols * aspect/2) rows of cells */
    int rows = (cols * PET_FRAME_H / PET_FRAME_W / 2) * 2;
    if (rows < 2) rows = 2;

    /* Per-state accent colors (RGB), so the half-block grid is visibly
     * state-aware instead of identical for every state. The full
     * spritesheet decode requires an image decoder the pure-C engine does
     * not ship; the grid below is a faithful half-block placeholder whose
     * palette + frame differ by state. */
    static const unsigned char state_accent[PET_STATE_COUNT][3] = {
        { 45,  45,  63 },  /* idle   — slate */
        { 90, 120, 200 },  /* wave   — blue */
        { 80, 200, 120 },  /* run    — green */
        { 231, 94, 120 },  /* failed — red */
        { 220, 190, 120 }, /* review — amber */
        { 170, 120, 220 }, /* jump   — purple */
        { 200, 180, 90 },  /* waiting— gold */
    };
    pet_state_t st = g_pet.current_state;
    if (st < 0 || st >= PET_STATE_COUNT) st = PET_STATE_IDLE;
    const unsigned char *ac = state_accent[st];
    char top_hex[8], bot_hex[8];
    snprintf(top_hex, sizeof(top_hex), "#%02x%02x%02x", ac[0], ac[1], ac[2]);
    snprintf(bot_hex, sizeof(bot_hex), "#%02x%02x%02x",
             (unsigned)(ac[0] * 0.6f), (unsigned)(ac[1] * 0.6f),
             (unsigned)(ac[2] * 0.6f));

    /* Draw a simple pet-shaped silhouette: wider at the body rows, with the
     * head on top — different enough per state to show the grid carries the
     * current animation row. */
    for (int r = 0; r < rows && r < 32; r++) {
        json_t *row = json_array();
        int head_h = (rows >= 8) ? rows / 3 : 1;
        int body_top = head_h;
        for (int c = 0; c < cols && c < 64; c++) {
            json_t *cell = json_object();
            bool on = false;
            if (r < head_h) {
                /* head: centered ellipse-ish (middle third) */
                on = (c >= cols / 3 && c < 2 * cols / 3);
            } else if (r < body_top + 2 && r < rows) {
                /* shoulders: slightly narrower than body */
                on = (c >= cols / 4 && c < 3 * cols / 4);
            } else if (r < rows) {
                /* body: wider */
                on = (c >= cols / 5 && c < 4 * cols / 5);
            }
            if (on) {
                json_set(cell, "top", json_new_string(top_hex));
                json_set(cell, "bottom", json_new_string(bot_hex));
            } else {
                json_set(cell, "top", json_new_string("#000000"));
                json_set(cell, "bottom", json_new_string("#000000"));
            }
            json_set(cell, "char", json_new_string("\xE2\x96\x80")); /* ▀ */
            json_append(row, cell);
        }
        json_append(cells_arr, row);
    }
    json_set(root, "cells", cells_arr);
    json_set(root, "cols", json_int(cols));
    json_set(root, "rows", json_int(rows));
    json_set(root, "state", json_new_string(pet_state_string(g_pet.current_state)));

    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* ════════════════════════════════════════════════════════════════════════
   Pet Selection & Configuration
   ════════════════════════════════════════════════════════════════════════ */

/* PoP: pet_select @ agent/pet/store.py:resolve_active_pet */
bool pet_select(const char *slug) {
    if (!slug || !*slug) return false;

    pet_installed_t pet;
    if (!pet_load_pet(slug, &pet)) {
        /* Try to install it from manifest */
        if (!pet_install_pet(slug, &pet, false)) {
            hermes_log(LOG_WARNING, "pet", "cannot select pet '%s': not found", slug);
            return false;
        }
    }

    g_pet.active_pet = pet;
    g_pet.has_active_pet = true;
    snprintf(g_pet.config.slug, sizeof(g_pet.config.slug), "%s", pet.slug);
    hermes_log(LOG_DEBUG, "pet", "pet selected: %s (%s)", pet.slug, pet.display_name);
    return true;
}

/* PoP: pet_disable @ agent/pet/store.py:disable */
void pet_disable(void) {
    g_pet.enabled = false;
    hermes_log(LOG_DEBUG, "pet", "pet disabled");
}

/* PoP: pet_set_scale @ agent/pet/constants.py:clamp_scale */
/* PoP: _set_scale @ hermes_cli/pets.py:_set_scale */
void pet_set_scale(float scale) {
    g_pet.scale = pet_clamp_scale(scale);
}

float pet_get_scale(void) {
    return g_pet.scale;
}

bool pet_is_enabled(void) {
    return g_pet.enabled;
}

const char *pet_active_slug(void) {
    return g_pet.has_active_pet ? g_pet.active_pet.slug : "";
}
