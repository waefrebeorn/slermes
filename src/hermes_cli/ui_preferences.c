/*
 * ui_preferences.c — C home for upstream Hermes hermes_cli/ui_preferences.py.
 * PoP: w12_uipreferen_26932e_sanitize @ hermes_cli/ui_preferences.py:_sanitize
 * PoP: w12_uipreferen_26932e_merge @ hermes_cli/ui_preferences.py:_merge
 * PoP: w12_uipreferen_26932e_validate @ hermes_cli/ui_preferences.py:_validate
 *
 * The three pure store helpers; the on-disk lock/revision machinery is not
 * part of these seams.
 */
#include "hermes_json.h"
#include "noc_w12_uipreferen_26932e.h"

#include <stdio.h>
#include <string.h>

#define UP_MAX_KEY_CHARS   256
#define UP_MAX_VALUE_CHARS (256 * 1024)

static const char *const UP_PREFIXES[] = {
    "hermes.desktop.", "hermes-desktop-", "hermes-onboarding-"
};

struct json_t *w12_uipreferen_26932e_sanitize(const struct json_t *values)
{
    json_t *out = json_object();
    if (!values || values->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < values->c.count; i++) {
        const char *k = values->c.keys[i];
        json_t *v = values->c.items[i];
        if (k && v && v->type == JSON_STRING)
            json_set(out, k, json_copy(v));
    }
    return out;
}

struct json_t *w12_uipreferen_26932e_merge(const struct json_t *current,
                                           const struct json_t *changes,
                                           int seed)
{
    json_t *out = (current && current->type == JSON_OBJECT)
                    ? json_copy(current) : json_object();
    if (!changes || changes->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < changes->c.count; i++) {
        const char *k = changes->c.keys[i];
        json_t *v = changes->c.items[i];
        if (!k) continue;
        if (seed) {
            if (v && v->type != JSON_NULL && !json_obj_get(out, k))
                json_set(out, k, json_copy(v));
        } else if (!v || v->type == JSON_NULL) {
            json_obj_del(out, k);
        } else {
            json_set(out, k, json_copy(v));
        }
    }
    return out;
}

/* Python repr() of a str, the {key!r} in the PreferencesError message. */
static void up_py_repr(const char *s, char *out, size_t cap)
{
    size_t o = 0;
    if (!cap) return;
    if (o < cap - 1) out[o++] = '\'';
    for (; s && *s && o + 6 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\\' || c == '\'') {
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c == '\n') {
            out[o++] = '\\'; out[o++] = 'n';
        } else if (c == '\r') {
            out[o++] = '\\'; out[o++] = 'r';
        } else if (c == '\t') {
            out[o++] = '\\'; out[o++] = 't';
        } else if (c < 0x20 || c == 0x7f) {
            int w = snprintf(out + o, cap - o, "\\x%02x", c);
            if (w > 0) o += (size_t)w;
        } else {
            out[o++] = (char)c;
        }
    }
    if (o < cap - 1) out[o++] = '\'';
    out[o] = '\0';  /* NUL at the final length */
}

static int up_has_prefix(const char *k)
{
    for (size_t i = 0; i < sizeof(UP_PREFIXES) / sizeof(UP_PREFIXES[0]); i++)
        if (!strncmp(k, UP_PREFIXES[i], strlen(UP_PREFIXES[i]))) return 1;
    return 0;
}

int w12_uipreferen_26932e_validate(const struct json_t *changes,
                                   char *errbuf, size_t cap)
{
    if (!errbuf || !cap) return 0;
    errbuf[0] = '\0';
    if (!changes || changes->type != JSON_OBJECT) return 1;
    for (size_t i = 0; i < changes->c.count; i++) {
        const char *k = changes->c.keys[i];
        json_t *v = changes->c.items[i];
        if (!k) continue;
        if (!up_has_prefix(k) || strlen(k) > UP_MAX_KEY_CHARS) {
            char r[768];
            up_py_repr(k, r, sizeof(r));
            snprintf(errbuf, cap, "unsupported preference key: %s", r);
            return 0;
        }
        if (v && v->type != JSON_NULL) {
            if (v->type != JSON_STRING ||
                (v->str_val && strlen(v->str_val) > UP_MAX_VALUE_CHARS)) {
                snprintf(errbuf, cap, "unsupported value for %s", k);
                return 0;
            }
        }
    }
    return 1;
}
