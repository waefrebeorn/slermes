/*
 * pet_constants.c — Pet sprite geometry + animation-state taxonomy
 *
 * Port of Python: agent/pet/constants.py
 * Frame geometry, PetState enum, row mapping, scale calculation.
 */
#include <string.h>
#include <math.h>
#include "pet.h"

/* ── State row taxonomies ───────────────────────────────────────────── */

/* Legacy Hermes/petdex 8-row format (top to bottom) */
static const char *g_legacy_state_rows[8] = {
    "idle",       /* PET_STATE_IDLE */
    "wave",       /* PET_STATE_WAVE */
    "run",        /* PET_STATE_RUN */
    "failed",     /* PET_STATE_FAILED */
    "review",     /* PET_STATE_REVIEW */
    "jump",       /* PET_STATE_JUMP */
    "extra1",
    "extra2"
};

/* Current Codex/petdex 9-row format (1536x1872 atlases) */
static const char *g_codex_state_rows[9] = {
    "idle",          /* PET_STATE_IDLE */
    "running-right", /* alt RUN alias */
    "running-left",  /* alt RUN alias */
    "waving",        /* alt WAVE alias */
    "jumping",       /* alt JUMP alias */
    "failed",        /* PET_STATE_FAILED */
    "waiting",       /* PET_STATE_WAITING */
    "running",       /* alt RUN alias */
    "review"         /* PET_STATE_REVIEW */
};

/* ── State names (canonical) ────────────────────────────────────────── */
static const char *g_state_names[PET_STATE_COUNT] = {
    "idle",
    "wave",
    "run",
    "failed",
    "review",
    "jump",
    "waiting"
};

/* ── State aliases ──────────────────────────────────────────────────── */
static const pet_state_alias_t g_state_aliases[PET_STATE_COUNT] = {
    {PET_STATE_IDLE,   {"idle"},                      1},
    {PET_STATE_WAVE,   {"wave", "waving"},            2},
    {PET_STATE_RUN,    {"run", "running", "running-right", "running-left"}, 4},
    {PET_STATE_FAILED, {"failed"},                    1},
    {PET_STATE_REVIEW, {"review"},                    1},
    {PET_STATE_JUMP,   {"jump", "jumping"},           2},
    {PET_STATE_WAITING,{"waiting"},                   1}
};

/* ════════════════════════════════════════════════════════════════════════
   Pet Constants API
   ════════════════════════════════════════════════════════════════════════ */

/* PoP: pet_clamp_scale @ agent/pet/constants.py:clamp_scale */
float pet_clamp_scale(float scale) {
    if (scale < PET_MIN_SCALE) return PET_MIN_SCALE;
    if (scale > PET_MAX_SCALE) return PET_MAX_SCALE;
    return scale;
}

/* PoP: pet_cols_for_scale @ agent/pet/constants.py:cols_for_scale */
int pet_cols_for_scale(float scale) {
    int cols = (int)(PET_BASE_UNICODE_COLS * (scale > 0.001f ? scale : PET_DEFAULT_SCALE) + 0.5f);
    if (cols < PET_UNICODE_MIN_COLS) cols = PET_UNICODE_MIN_COLS;
    return cols;
}

/* PoP: pet_resolve_cols @ agent/pet/constants.py:resolve_cols */
int pet_resolve_cols(float scale, int unicode_cols) {
    if (unicode_cols > 0) return unicode_cols;
    return pet_cols_for_scale(scale);
}

/* PoP: pet_state_rows_for_grid @ agent/pet/constants.py:state_rows_for_grid */
const char **pet_state_rows_for_grid(int row_count) {
    if (row_count >= 9) {
        static const char *codex_rows[9];
        for (int i = 0; i < 9; i++) codex_rows[i] = g_codex_state_rows[i];
        return (const char **)codex_rows;
    }
    static const char *legacy_rows[8];
    for (int i = 0; i < 8; i++) legacy_rows[i] = g_legacy_state_rows[i];
    return (const char **)legacy_rows;
}

/* PoP: pet_state_row_index @ agent/pet/constants.py:state_row_index */
int pet_state_row_index(pet_state_t state, int row_count) {
    if (state < 0 || state >= PET_STATE_COUNT) return 0;

    const char **rows = (row_count >= 9)
        ? (const char **)g_codex_state_rows
        : (const char **)g_legacy_state_rows;
    int nrows = (row_count >= 9) ? 9 : 8;

    const pet_state_alias_t *al = &g_state_aliases[state];
    for (int a = 0; a < al->alias_count; a++) {
        for (int r = 0; r < nrows; r++) {
            if (strcmp(rows[r], al->aliases[a]) == 0)
                return r;
        }
    }
    return 0;
}

/* PoP: pet_state_string @ agent/pet/constants.py:state_aliases_for */
const char *pet_state_string(pet_state_t state) {
    if (state >= 0 && state < PET_STATE_COUNT)
        return g_state_names[state];
    return "idle";
}

/* PoP: pet_state_from_string @ agent/pet/constants.py:state_aliases_for */
pet_state_t pet_state_from_string(const char *s) {
    if (!s || !*s) return PET_STATE_IDLE;
    for (int i = 0; i < PET_STATE_COUNT; i++) {
        const pet_state_alias_t *al = &g_state_aliases[i];
        for (int a = 0; a < al->alias_count; a++) {
            if (strcmp(s, al->aliases[a]) == 0)
                return (pet_state_t)i;
        }
    }
    return PET_STATE_IDLE;
}

/* PoP: pet_state_aliases @ agent/pet/constants.py:state_aliases_for */
const char **pet_state_aliases(pet_state_t state) {
    if (state < 0 || state >= PET_STATE_COUNT) return NULL;
    static const char *alias_list[PET_STATE_COUNT][5];
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < PET_STATE_COUNT; i++) {
            for (int a = 0; a < g_state_aliases[i].alias_count; a++)
                alias_list[i][a] = g_state_aliases[i].aliases[a];
            alias_list[i][g_state_aliases[i].alias_count] = NULL;
        }
        initialized = true;
    }
    return alias_list[state];
}
