/*
 * adapter_inbound_media.c -- C11 port of upstream Hermes
 *   gateway/relay/adapter_inbound_media.py
 *
 * PoP: w21_adapterinb_940828_localize @ gateway/relay/adapter_inbound_media.py:localize_inbound_media
 * PoP: w21_adapterinb_940828_pad_flags @ gateway/platforms/base_media_flags.py:pad_inline_flags
 *
 * The connector media client (download_with_name / is_relay_media_url) is the
 * network seam: the C side receives its per-URL answers as injected state and
 * reproduces the filter exactly -- including pad_inline_flags, the zip over the
 * four parallel lists, the relay-URL drop with no client, and the legacy
 * unflagged normalization to [].
 */
#include "hermes_json.h"
#include "noc_w21_adapterinb_940828.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADAPTERINB_COLUMNS 4
#define ADAPTERINB_MAX_ROWS 512

typedef struct {
    json_t *cells[ADAPTERINB_COLUMNS];
} adapterinb_row;

static json_t *adapterinb_array(const json_t *obj, const char *key) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (v && v->type == JSON_ARRAY) return v;
    return NULL;
}

static size_t adapterinb_count(const json_t *arr) {
    return (arr && arr->type == JSON_ARRAY) ? arr->c.count : 0;
}

/* Python truthiness of a decoded value; absent reads as None. */
static int adapterinb_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static const char *adapterinb_string_at(const json_t *arr, size_t i) {
    json_t *v = (arr && arr->type == JSON_ARRAY && i < arr->c.count) ? arr->c.items[i] : NULL;
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return NULL;
}

/* --- pad_inline_flags ----------------------------------------------------- */

json_t *w21_adapterinb_940828_pad_flags(const json_t *state) {
    json_t *urls = adapterinb_array(state, "media_urls");
    json_t *flags = adapterinb_array(state, "media_text_inlined");
    json_t *out = json_array();
    size_t have = adapterinb_count(flags);
    size_t want = adapterinb_count(urls);
    size_t i;

    for (i = 0; i < have; i++) {
        json_t *v = flags->c.items[i];
        json_append(out, v ? json_copy(v) : json_null());
    }
    for (i = have; i < want; i++) json_append(out, json_null());
    return out;
}

/* --- localize_inbound_media ---------------------------------------------- */

/* The client's answer for one URL: 0 = not found (None), 1 = (path, name),
 * 2 = the download raised. */
static int adapterinb_download(const json_t *downloads, const char *url,
                               const char *name, char **path_out, char **name_out) {
    json_t *got = downloads ? json_obj_get(downloads, url) : NULL;
    json_t *first;
    const char *p, *n;

    *path_out = NULL;
    *name_out = NULL;
    if (got && got->type == JSON_STRING && got->str_val &&
        !strcmp(got->str_val, "__raise__")) {
        return 2;
    }
    if (!got || got->type != JSON_ARRAY || got->c.count < 2) return 0;
    first = got->c.items[0];
    p = (first && first->type == JSON_STRING && first->str_val) ? first->str_val : "";
    n = adapterinb_string_at(got, 1);
    *path_out = (char *)malloc(strlen(p) + 1);
    if (*path_out) memcpy(*path_out, p, strlen(p) + 1);
    if (n) {
        *name_out = (char *)malloc(strlen(n) + 1);
        if (*name_out) memcpy(*name_out, n, strlen(n) + 1);
    }
    return 1;
}

static int adapterinb_is_relay(const json_t *relay_urls, const char *url) {
    size_t i, n = adapterinb_count(relay_urls);
    for (i = 0; i < n; i++) {
        const char *cand = adapterinb_string_at(relay_urls, i);
        if (cand && !strcmp(cand, url)) return 1;
    }
    return 0;
}

static json_t *adapterinb_snapshot(const json_t *state, int raised) {
    static const char *const KEYS[ADAPTERINB_COLUMNS] = {
        "media_urls", "media_types", "media_filenames", "media_text_inlined"};
    json_t *out = json_object();
    int i;
    for (i = 0; i < ADAPTERINB_COLUMNS; i++) {
        json_t *v = adapterinb_array(state, KEYS[i]);
        json_set(out, KEYS[i], v ? json_copy(v) : json_null());
    }
    json_set(out, "raised", json_bool(raised));
    return out;
}

json_t *w21_adapterinb_940828_localize(const json_t *state) {
    json_t *urls = adapterinb_array(state, "media_urls");
    json_t *types = adapterinb_array(state, "media_types");
    json_t *names = adapterinb_array(state, "media_filenames");
    json_t *flags_src = adapterinb_array(state, "media_text_inlined");
    json_t *downloads = json_obj_get(state, "downloads");
    json_t *relay_urls = adapterinb_array(state, "relay_urls");
    json_t *rows[ADAPTERINB_MAX_ROWS];
    size_t nrows = 0;
    size_t total = adapterinb_count(urls);
    size_t i;
    int has_client = json_get_bool(state, "has_client", 0);
    int client_raises = json_get_bool(state, "client_raises", 0);
    int legacy_unflagged = !adapterinb_truthy(json_obj_get(state, "media_text_inlined"));
    json_t *out;

    if (total == 0) return adapterinb_snapshot(state, 0);
    if (client_raises) return adapterinb_snapshot(state, 1);

    /* types/names: list(... or []) + [""] * len(urls); flags: pad_inline_flags. */
    for (i = 0; i < total; i++) {
        const char *url = adapterinb_string_at(urls, i);
        const char *mime = adapterinb_string_at(types, i);
        const char *name = adapterinb_string_at(names, i);
        json_t *inlined = (flags_src && i < flags_src->c.count) ? flags_src->c.items[i] : NULL;
        json_t *row;

        if (!url || !url[0]) continue;              /* not a non-empty str */
        if (!has_client) {
            /* No origin to classify against: drop anything that might be a re-host. */
            if (strstr(url, "/relay/media/")) continue;
            row = json_array();
            json_append(row, json_string(url));
            json_append(row, json_string(mime ? mime : ""));
            json_append(row, json_string(name ? name : ""));
            json_append(row, inlined ? json_copy(inlined) : json_null());
        } else {
            char *path = NULL, *got_name = NULL;
            int got = adapterinb_download(downloads, url, name, &path, &got_name);
            if (got == 2) {
                /* the await raised: the whole localization is abandoned */
                size_t k;
                for (k = 0; k < nrows; k++) json_free(rows[k]);
                free(path);
                free(got_name);
                return adapterinb_snapshot(state, 1);
            }
            if (got == 1) {
                /* The path is a random temp name; the original name rides beside it. */
                row = json_array();
                json_append(row, json_string(path ? path : ""));
                json_append(row, json_string(mime ? mime : ""));
                json_append(row, json_string(got_name ? got_name : ""));
                json_append(row, inlined ? json_copy(inlined) : json_null());
            } else if (!adapterinb_is_relay(relay_urls, url)) {
                row = json_array();
                json_append(row, json_string(url));
                json_append(row, json_string(mime ? mime : ""));
                json_append(row, json_string(name ? name : ""));
                json_append(row, inlined ? json_copy(inlined) : json_null());
            } else {
                free(path);
                free(got_name);
                continue;
            }
            free(path);
            free(got_name);
        }
        if (nrows < ADAPTERINB_MAX_ROWS) rows[nrows++] = row;
        else json_free(row);
    }

    out = json_object();
    {
        static const char *const KEYS[ADAPTERINB_COLUMNS] = {
            "media_urls", "media_types", "media_filenames", "media_text_inlined"};
        int col;
        for (col = 0; col < ADAPTERINB_COLUMNS; col++) {
            json_t *column = json_array();
            size_t r;
            for (r = 0; r < nrows; r++) json_append(column, json_copy(rows[r]->c.items[col]));
            json_set(out, KEYS[col], column);
        }
    }
    for (i = 0; i < nrows; i++) json_free(rows[i]);
    /* [] and [None, ...] mean the same, but the admission payload stores the
     * list verbatim: keep [] for a delivery committed before per-file flags. */
    if (legacy_unflagged) {
        json_set(out, "media_text_inlined", json_array());
    }
    json_set(out, "raised", json_bool(0));
    return out;
}
