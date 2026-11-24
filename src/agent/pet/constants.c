/*
 * constants.c - C11 port of upstream agent/pet/constants.py
 * (gate w24_constants_b9be53).
 *
 * PoP: w24_constants_b9be53_clamp_scale @ agent/pet/constants.py:clamp_scale
 * PoP: w24_constants_b9be53_cols_for_scale @ agent/pet/constants.py:cols_for_scale
 * PoP: w24_constants_b9be53_resolve_cols @ agent/pet/constants.py:resolve_cols
 * PoP: w24_constants_b9be53_state_aliases_for @ agent/pet/constants.py:state_aliases_for
 * PoP: w24_constants_b9be53_state_rows_for_grid @ agent/pet/constants.py:state_rows_for_grid
 * PoP: w24_constants_b9be53_state_row_index @ agent/pet/constants.py:state_row_index
 * PoP: w24_constants_b9be53_constants @ agent/pet/constants.py:<geometry + taxonomy tables>
 *
 * Ported: the sprite frame geometry, the 8-row legacy / 9-row Codex row
 * taxonomies with their alias preference, and the three scale/column resolvers
 * that validate ``display.pet.scale``.  The spritesheet itself (its path, its
 * pixels, the atlas probe that decides row_count) is a seam: row_count arrives
 * as a value, and this file owns only what the values decide.  Python's
 * round() is round-half-to-even on the binary value, so pet_round() reproduces
 * that without libm; int() coercion is reproduced for numbers and integer
 * strings, and a value int() would refuse reads as zero rows exactly where the
 * upstream try/except does.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "noc_w24_constants_b9be53.h"

#define PET_FRAME_W            W24_CONSTANTS_B9BE53_FRAME_W
#define PET_FRAME_H            W24_CONSTANTS_B9BE53_FRAME_H
#define PET_FRAMES_PER_STATE   W24_CONSTANTS_B9BE53_FRAMES_PER_STATE
#define PET_LOOP_MS            W24_CONSTANTS_B9BE53_LOOP_MS
#define PET_DEFAULT_SCALE      W24_CONSTANTS_B9BE53_DEFAULT_SCALE
#define PET_MIN_SCALE          W24_CONSTANTS_B9BE53_MIN_SCALE
#define PET_MAX_SCALE          W24_CONSTANTS_B9BE53_MAX_SCALE
#define PET_BASE_UNICODE_COLS  W24_CONSTANTS_B9BE53_BASE_UNICODE_COLS
#define PET_UNICODE_MIN_COLS   W24_CONSTANTS_B9BE53_UNICODE_MIN_COLS

/* LEGACY_STATE_ROWS: the older 8-row, 9-column atlas, top -> bottom. */
static const char *const PET_LEGACY_ROWS[W24_CONSTANTS_B9BE53_LEGACY_ROWS] = {
    "idle", "wave", "run", "failed", "review", "jump", "extra1", "extra2",
};

/* CODEX_STATE_ROWS: the 1536x1872 atlas (8 cols x 9 rows), top -> bottom. */
static const char *const PET_CODEX_ROWS[W24_CONSTANTS_B9BE53_CODEX_ROWS] = {
    "idle", "running-right", "running-left", "waving", "jumping",
    "failed", "waiting", "running", "review",
};

/* STATE_ALIASES: canonical Hermes name -> accepted row names, in preference
 * order.  Only wave/jump/run carry a Codex alias (_CODEX_NAMES). */
static const struct {
    const char *name;
    const char *codex;
} PET_ALIASES[W24_CONSTANTS_B9BE53_ALIASES] = {
    {"idle", NULL}, {"wave", "waving"}, {"jump", "jumping"}, {"run", "running"},
    {"failed", NULL}, {"review", NULL}, {"waiting", NULL},
};

/* Python truthiness of a JSON value. */
static int pet_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len((json_t *)v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python's numeric view of a value: int, float or bool (None excluded). */
static int pet_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python round(float): nearest, ties to even, on the exact binary value. */
static double pet_round(double x) {
    double t = (double)(long long)x;
    double diff = x - t;
    if (diff > 0.5) return t + 1.0;
    if (diff < -0.5) return t - 1.0;
    if (diff == 0.5) return ((long long)t % 2 == 0) ? t : t + 1.0;
    if (diff == -0.5) return ((long long)t % 2 == 0) ? t : t - 1.0;
    return t;
}

/* int(x) for the shapes int() accepts; 0 when it would raise TypeError or
 * ValueError (callers that wrap it in try/except read that as zero rows). */
static int pet_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != d) return 0;                       /* nan */
        if (d > 9.0e18 || d < -9.0e18) return 0;
        *out = (long long)d;                        /* truncates toward zero */
        return 1;
    }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        char *end = NULL;
        long long n;
        while (*s == ' ' || *s == '\t' || *s == '\n') s++;
        n = strtoll(s, &end, 10);
        if (end == s) return 0;                     /* no digits: ValueError */
        while (*end == ' ' || *end == '\t' || *end == '\n') end++;
        if (*end != '\0') return 0;                 /* "9.5", "abc": ValueError */
        *out = n;
        return 1;
    }
    return 0;                                       /* None/list/dict: TypeError */
}

/* str(value) for the scalars str() gives a plain rendering for.  None/True/False
 * and integral floats print the way Python does; composite values are the seam
 * (NULL, and the caller keeps the canonical name). */
static char *pet_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    default:          return NULL;
    }
}

/* The index of *name* in rows, or -1 (list.index raises: callers guard). */
static int pet_row_index(const char *const *rows, int n, const char *name) {
    int i;
    if (!name) return -1;
    for (i = 0; i < n; i++)
        if (strcmp(rows[i], name) == 0) return i;
    return -1;
}

json_t *w24_constants_b9be53_clamp_scale(const json_t *scale) {
    double s;
    if (!pet_number(scale, &s)) return json_null();
    if (s > PET_MAX_SCALE) s = PET_MAX_SCALE;
    if (s < PET_MIN_SCALE) s = PET_MIN_SCALE;
    return json_number(s);
}

json_t *w24_constants_b9be53_cols_for_scale(const json_t *scale) {
    double s, cols;
    /* ``scale or DEFAULT_SCALE``: every Python-falsy value falls back before any
     * arithmetic, so None/""/0/false/[]/{} never reach the TypeError seam. */
    if (pet_truthy(scale)) {
        if (!pet_number(scale, &s)) return json_null();  /* TypeError seam */
    } else {
        s = PET_DEFAULT_SCALE;
    }
    cols = pet_round((double)PET_BASE_UNICODE_COLS * s);
    if (cols < (double)PET_UNICODE_MIN_COLS) cols = (double)PET_UNICODE_MIN_COLS;
    return json_number(cols);
}

json_t *w24_constants_b9be53_resolve_cols(const json_t *scale, const json_t *unicode_cols) {
    long long n;
    if (pet_truthy(unicode_cols)) {
        if (!pet_int(unicode_cols, &n)) return json_null();
        if (n > 0) return json_number((double)n);
    }
    return w24_constants_b9be53_cols_for_scale(scale);
}

json_t *w24_constants_b9be53_state_aliases_for(const json_t *state) {
    char *name = pet_str(state);
    json_t *out = json_array();
    int i;

    if (!out) { free(name); return json_null(); }
    if (!name) return out;                           /* composite: no plain str() */
    for (i = 0; i < W24_CONSTANTS_B9BE53_ALIASES; i++) {
        if (strcmp(PET_ALIASES[i].name, name) == 0) {
            json_append(out, json_string(PET_ALIASES[i].name));
            if (PET_ALIASES[i].codex) json_append(out, json_string(PET_ALIASES[i].codex));
            free(name);
            return out;
        }
    }
    json_append(out, json_string(name));             /* STATE_ALIASES.get(v) or (v,) */
    free(name);
    return out;
}

/* ``int(row_count or 0)`` inside try/except (TypeError, ValueError) -> 0. */
static int pet_rows_of(const json_t *row_count) {
    long long n = 0;
    if (!pet_truthy(row_count)) return 0;
    if (!pet_int(row_count, &n)) return 0;
    if (n < 0) n = 0;
    return n > 1000 ? 1000 : (int)n;
}

json_t *w24_constants_b9be53_state_rows_for_grid(const json_t *row_count) {
    int rows = pet_rows_of(row_count);
    json_t *out = json_array();
    int i, n;

    if (rows >= W24_CONSTANTS_B9BE53_CODEX_ROWS) {
        for (i = 0, n = W24_CONSTANTS_B9BE53_CODEX_ROWS; i < n; i++)
            json_append(out, json_string(PET_CODEX_ROWS[i]));
    } else {
        for (i = 0, n = W24_CONSTANTS_B9BE53_LEGACY_ROWS; i < n; i++)
            json_append(out, json_string(PET_LEGACY_ROWS[i]));
    }
    return out;
}

json_t *w24_constants_b9be53_state_row_index(const json_t *state, const json_t *row_count) {
    json_t *aliases = w24_constants_b9be53_state_aliases_for(state);
    int rows = pet_rows_of(row_count);
    int codex = rows >= W24_CONSTANTS_B9BE53_CODEX_ROWS;
    const char *const *table = codex ? PET_CODEX_ROWS : PET_LEGACY_ROWS;
    int table_n = codex ? W24_CONSTANTS_B9BE53_CODEX_ROWS : W24_CONSTANTS_B9BE53_LEGACY_ROWS;
    size_t i, n = json_len(aliases);
    int found = -1;

    for (i = 0; i < n && found < 0; i++) {
        const json_t *a = json_get(aliases, i);
        if (!a || a->type != JSON_STRING) continue;
        found = pet_row_index(table, table_n, a->str_val);   /* ``name in rows`` */
    }
    json_free(aliases);
    return json_number(found < 0 ? 0.0 : (double)found);     /* 0 = idle row */
}

json_t *w24_constants_b9be53_constants(void) {
    json_t *o = json_object();
    json_t *aliases = json_object();
    json_t *legacy = json_array();
    json_t *codex = json_array();
    json_t *state_rows = json_array();
    int i;

    json_set(o, "FRAME_W", json_number(PET_FRAME_W));
    json_set(o, "FRAME_H", json_number(PET_FRAME_H));
    json_set(o, "FRAMES_PER_STATE", json_number(PET_FRAMES_PER_STATE));
    json_set(o, "LOOP_MS", json_number(PET_LOOP_MS));
    json_set(o, "DEFAULT_SCALE", json_number(PET_DEFAULT_SCALE));
    json_set(o, "MIN_SCALE", json_number(PET_MIN_SCALE));
    json_set(o, "MAX_SCALE", json_number(PET_MAX_SCALE));
    json_set(o, "BASE_UNICODE_COLS", json_number(PET_BASE_UNICODE_COLS));
    json_set(o, "UNICODE_MIN_COLS", json_number(PET_UNICODE_MIN_COLS));

    for (i = 0; i < W24_CONSTANTS_B9BE53_LEGACY_ROWS; i++)
        json_append(legacy, json_string(PET_LEGACY_ROWS[i]));
    for (i = 0; i < W24_CONSTANTS_B9BE53_CODEX_ROWS; i++) {
        json_append(codex, json_string(PET_CODEX_ROWS[i]));
        json_append(state_rows, json_string(PET_CODEX_ROWS[i]));
    }
    json_set(o, "LEGACY_STATE_ROWS", legacy);
    json_set(o, "CODEX_STATE_ROWS", codex);
    json_set(o, "STATE_ROWS", state_rows);

    for (i = 0; i < W24_CONSTANTS_B9BE53_ALIASES; i++) {
        json_t *list = json_array();
        json_append(list, json_string(PET_ALIASES[i].name));
        if (PET_ALIASES[i].codex) json_append(list, json_string(PET_ALIASES[i].codex));
        json_set(aliases, PET_ALIASES[i].name, list);
    }
    json_set(o, "STATE_ALIASES", aliases);
    return o;
}
