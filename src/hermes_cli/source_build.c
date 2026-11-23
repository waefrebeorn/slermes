/*
 * source_build.c — C port of upstream Hermes hermes_cli/source_build.py.
 *
 * PoP: w9_sourcebuild_63397b_npmrc_script_settings @ hermes_cli/source_build.py:_npmrc_script_settings
 *
 * Faithful port of the npmrc -> install-script settings filter: only plain
 * [A-Za-z0-9][A-Za-z0-9_-]* keys that are not otp/key/cert (case-insensitively)
 * pass through, preserving insertion order.
 */
#include "noc_w9_sourcebuild_63397b.h"
#include "hermes_json.h"
#include <string.h>

static int alnum_ascii(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

/* _NPMRC_SETTING.fullmatch(key): first char alnum, rest alnum/_/-. */
static int plain_key(const char *k) {
    if (!k || !k[0] || !alnum_ascii((unsigned char)k[0])) return 0;
    for (const char *p = k + 1; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!alnum_ascii(c) && c != '_' && c != '-') return 0;
    }
    return 1;
}

static unsigned char lower_ascii(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c - 'A' + 'a') : c;
}

static int eq_ignore_case(const char *a, const char *b) {
    while (*a && *b) {
        if (lower_ascii((unsigned char)*a) != lower_ascii((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static int is_secret(const char *k) {
    return eq_ignore_case(k, "otp") || eq_ignore_case(k, "key") || eq_ignore_case(k, "cert");
}

struct json_t *w9_sourcebuild_63397b_npmrc_script_settings(const json_t *settings) {
    json_t *out = json_array();
    if (!settings || settings->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(settings); i++) {
        const char *key = json_object_get_key_at(settings, i);
        json_t *value = json_object_get_at(settings, i);
        if (!key || !plain_key(key) || is_secret(key)) continue;
        json_t *pair = json_array();
        json_append(pair, json_string(key));
        json_append(pair, json_copy(value));
        json_append(out, pair);
    }
    return out;
}
