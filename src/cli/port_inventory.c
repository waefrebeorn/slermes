/*
 * port_inventory.c
 *
 * Pure, portable helpers ported from hermes_cli/inventory.py — the shared
 * substrate behind the dashboard /api/model/options endpoint, the TUI
 * model.options / model.save_key RPC handlers, and the interactive picker.
 *
 * Only the dependency-free core lives here. The rest of that module is runtime
 * by nature: load_picker_context() reads the config, build_models_payload()
 * probes custom endpoints over HTTP, _apply_usage() reads credential pools,
 * _apply_limits() reads the rate-limit state. Those are deliberately NOT
 * ported - a C body would have to invent the I/O the Python side delegates.
 *
 * Functions:
 *   inv_slug               <- inventory.py:_slug
 *   inv_without_slug       <- inventory.py:_without_slug
 *   inv_row                <- inventory.py:_row
 *   inv_iso_from_epoch     <- inventory.py:_iso_from_epoch
 *   inv_with_overrides     <- inventory.py:ConfigContext.with_overrides
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

/* ---------------------------------------------------------- py repr ------- */

/* Render a JSON value the way Python's str()/repr() renders the equivalent
 * object, because _slug() applies str() to whatever it finds. Python uses
 * single quotes, ", " between items, ": " after keys, and True/False/None.
 * Only the shapes a slug field can realistically hold are handled. */
static void sb_put(char **buf, size_t *len, size_t *cap, const char *s, size_t n)
{
    if (*len + n + 1 > *cap) {
        *cap = (*len + n + 1) * 2;
        *buf = (char *)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
}

static void pyrepr_rec(char **buf, size_t *len, size_t *cap, const json_t *v)
{
    if (!v) { sb_put(buf, len, cap, "None", 4); return; }
    char tmp[64];
    switch (v->type) {
    case JSON_NULL:   sb_put(buf, len, cap, "None", 4); break;
    case JSON_BOOL:   sb_put(buf, len, cap, v->bool_val ? "True" : "False",
                              v->bool_val ? 4 : 5); break;
    case JSON_NUMBER:
        snprintf(tmp, sizeof tmp, "%.17g", v->num_val);
        sb_put(buf, len, cap, tmp, strlen(tmp));
        break;
    case JSON_STRING: {
        sb_put(buf, len, cap, "'", 1);
        for (const char *p = v->str_val ? v->str_val : ""; *p; p++) {
            if (*p == '\'' || *p == '\\') {
                sb_put(buf, len, cap, "\\", 1);
                sb_put(buf, len, cap, p, 1);
            } else if (*p == '\n') {
                sb_put(buf, len, cap, "\\n", 2);
            } else {
                sb_put(buf, len, cap, p, 1);
            }
        }
        sb_put(buf, len, cap, "'", 1);
        break;
    }
    case JSON_ARRAY:
        sb_put(buf, len, cap, "[", 1);
        for (size_t i = 0; i < json_len(v); i++) {
            if (i) sb_put(buf, len, cap, ", ", 2);
            pyrepr_rec(buf, len, cap, json_get(v, i));
        }
        sb_put(buf, len, cap, "]", 1);
        break;
    case JSON_OBJECT:
        sb_put(buf, len, cap, "{", 1);
        for (size_t i = 0; i < json_object_size(v); i++) {
            if (i) sb_put(buf, len, cap, ", ", 2);
            const char *k = json_object_get_key_at(v, i);
            /* render the key directly rather than via a temp json_string */
            sb_put(buf, len, cap, "'", 1);
            for (const char *p = k ? k : ""; *p; p++) {
                if (*p == '\'' || *p == '\\') {
                    sb_put(buf, len, cap, "\\", 1);
                    sb_put(buf, len, cap, p, 1);
                } else {
                    sb_put(buf, len, cap, p, 1);
                }
            }
            sb_put(buf, len, cap, "'", 1);
            sb_put(buf, len, cap, ": ", 2);
            pyrepr_rec(buf, len, cap, json_object_get_at(v, i));
        }
        sb_put(buf, len, cap, "}", 1);
        break;
    }
}

/* Public: a freshly allocated Python-repr string, or NULL on OOM. */
char *json_pyrepr(const json_t *v)
{
    size_t cap = 64, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    pyrepr_rec(&buf, &len, &cap, v);
    return buf;
}

/* ===========================================================================
 *  _slug  (inventory.py:_slug)
 *
 *      str(row.get("slug") or "").strip().lower()
 *
 *  A missing key, a JSON null, and a non-string scalar all normalise the same
 *  way: Python's `or ""` turns None into "", and str() stringifies a number.
 *  Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _slug @ hermes_cli/inventory.py:_slug */
char *inv_slug(const json_t *row)
{
    const char *raw = "";
    if (row && row->type == JSON_OBJECT) {
        json_t *v = json_obj_get(row, "slug");
        if (v) {
            if (v->type == JSON_STRING && v->str_val)
                raw = v->str_val;
            else if (v->type == JSON_NUMBER)
                raw = NULL;   /* handled below: str(number) */
            else if (v->type == JSON_BOOL)
                raw = NULL;   /* handled below: str(bool) */
            else if (v->type == JSON_OBJECT || v->type == JSON_ARRAY)
                raw = NULL;   /* handled below: str() is the repr */
            else
                raw = "";     /* JSON_NULL */
        }
    }

    char numbuf[80];
    if (raw == NULL) {
        /* str() of a JSON scalar. Python's str() of a dict/list is its repr,
         * so an object slug stringifies to e.g. "{'a': 1}" - not "". */
        json_t *v = (row && row->type == JSON_OBJECT) ? json_obj_get(row, "slug") : NULL;
        if (v && v->type == JSON_NUMBER)
            snprintf(numbuf, sizeof numbuf, "%.17g", v->num_val);
        else if (v && v->type == JSON_BOOL)
            snprintf(numbuf, sizeof numbuf, "%s", v->bool_val ? "True" : "False");
        else if (v && (v->type == JSON_OBJECT || v->type == JSON_ARRAY)) {
            char *d = json_pyrepr(v);
            snprintf(numbuf, sizeof numbuf, "%s", d ? d : "");
            free(d);
        } else
            numbuf[0] = '\0';
        raw = numbuf;
    }

    /* strip() then lower(), in place on a copy */
    size_t len = strlen(raw);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    size_t start = 0;
    while (start < len && isspace((unsigned char)raw[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)raw[end - 1])) end--;

    size_t n = 0;
    for (size_t i = start; i < end; i++)
        out[n++] = (char)tolower((unsigned char)raw[i]);
    out[n] = '\0';
    return out;
}

/* ===========================================================================
 *  _without_slug  (inventory.py:_without_slug)
 *
 *      [r for r in rows if _slug(r) != slug]
 *
 *  Returns a NEW array. json_append() takes OWNERSHIP of what it is given, so
 *  each kept row must be COPIED - appending the borrowed row would leave the
 *  source array and the result sharing elements and the caller's two
 *  json_free() calls would double-free. Caller frees the array (and the copies
 *  it owns); the source rows are untouched.
 * =========================================================================== */

/* PoP: _without_slug @ hermes_cli/inventory.py:_without_slug */
json_t *inv_without_slug(const json_t *rows, const char *slug)
{
    json_t *out = json_array();
    if (!out || !rows || rows->type != JSON_ARRAY) return out;

    size_t n = json_len(rows);
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_get(rows, i);
        if (!row) continue;
        char *s = inv_slug(row);
        if (!s) continue;
        int keep = strcmp(s, slug ? slug : "") != 0;
        free(s);
        if (keep) json_append(out, json_copy(row));   /* copy: append owns it */
    }
    return out;
}

/* ===========================================================================
 *  _row  (inventory.py:_row)
 *
 *      {"slug": slug, "name": name, "is_current": is_current,
 *       "is_user_defined": False, **extra}
 *
 *  The two fixed keys are always present; is_user_defined is hard-coded False
 *  because the caller overrides it for user-defined rows. Returns a NEW object.
 * =========================================================================== */

/* PoP: _row @ hermes_cli/inventory.py:_row */
json_t *inv_row(const char *slug, const char *name, int is_current)
{
    json_t *row = json_object();
    if (!row) return NULL;
    json_set(row, "slug", json_string(slug ? slug : ""));
    json_set(row, "name", json_string(name ? name : ""));
    json_set(row, "is_current", json_bool(is_current ? 1 : 0));
    json_set(row, "is_user_defined", json_bool(0));
    return row;
}

/* ===========================================================================
 *  _iso_from_epoch  (inventory.py:_iso_from_epoch)
 *
 *      datetime.fromtimestamp(float(epoch), UTC).isoformat()
 *
 *  Python renders microseconds only when they are non-zero, and always includes
 *  the "+00:00" UTC offset. Reproduced here so the string is byte-identical.
 *  Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _iso_from_epoch @ hermes_cli/inventory.py:_iso_from_epoch */
char *inv_iso_from_epoch(double epoch)
{
    time_t secs = (time_t)epoch;
    long usec = (long)((epoch - (double)secs) * 1000000.0);
    /* carry for a negative epoch whose fractional part rounds the wrong way */
    if (usec < 0) { secs -= 1; usec += 1000000; }

    struct tm tmv;
    if (!gmtime_r(&secs, &tmv)) return NULL;

    char buf[64];
    char *out;
    if (usec == 0) {
        /* datetime.isoformat() omits .000000 entirely */
        snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    } else {
        snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d.%06ld+00:00",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec, usec);
    }
    out = (char *)malloc(strlen(buf) + 1);
    if (!out) return NULL;
    strcpy(out, buf);
    return out;
}

/* ===========================================================================
 *  ConfigContext.with_overrides  (inventory.py:ConfigContext.with_overrides)
 *
 *      overrides = (("current_provider", p), ("current_model", m),
 *                   ("current_base_url", u))
 *      kw = {k: v for k, v in overrides if v}      # TRUTHY only
 *      return replace(self, **kw) if kw else self
 *
 *  Truthiness is the whole point: the TUI overlays live agent attributes that
 *  are empty strings before an agent is spawned, and an empty string must NOT
 *  clobber the disk-config value. Returns a NEW object; NULL means "no
 *  overrides", which the Python side expresses by returning self.
 * =========================================================================== */

/* PoP: with_overrides @ hermes_cli/inventory.py:ConfigContext.with_overrides */
json_t *inv_with_overrides(const json_t *ctx,
                           const char *current_provider,
                           const char *current_model,
                           const char *current_base_url)
{
    char has_p = current_provider && current_provider[0];
    char has_m = current_model && current_model[0];
    char has_u = current_base_url && current_base_url[0];
    if (!has_p && !has_m && !has_u) return NULL;   /* caller keeps ctx */

    json_t *out = json_copy(ctx);
    if (!out) return NULL;
    if (has_p) json_set(out, "current_provider", json_string(current_provider));
    if (has_m) json_set(out, "current_model", json_string(current_model));
    if (has_u) json_set(out, "current_base_url", json_string(current_base_url));
    return out;
}
