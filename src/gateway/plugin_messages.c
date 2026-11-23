/*
 * plugin_messages.c — C home for upstream Hermes gateway/plugin_messages.py.
 *
 * PoP: w9_pluginmessa_825c6f_err @ gateway/plugin_messages.py:_err
 * PoP: w9_pluginmessa_825c6f_fit @ gateway/plugin_messages.py:_fit
 * PoP: w9_pluginmessa_825c6f_supports_edit @ gateway/plugin_messages.py:_supports_edit
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w9_pluginmessa_825c6f.h"

static long long dec_ll(const json_t *v, long long def) {
    if (!v || v->type != JSON_STRING || !v->str_val) return def;
    char *end = NULL;
    long long x = strtoll(v->str_val, &end, 10);
    return (end && *end == '\0') ? x : def;
}
/* Python len(str) counts Unicode code points, not UTF-8 bytes. */
static size_t cp_count(const char *s) {
    size_t n = 0;
    for (; s && *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}
/* Byte offset where the prefix of n code points ends. */
static size_t cp_prefix_len(const char *s, size_t n) {
    if (n == 0) return 0;
    size_t count = 0, i = 0;
    for (; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c & 0xC0) != 0x80) {
            if (count == n) return i;
            count++;
        }
    }
    return i;
}

json_t *w9_pluginmessa_825c6f_err(const struct json_t *in) {
    const char *code = json_get_str(in, "code", "");
    const char *detail = json_get_str(in, "detail", "");
    json_t *out = json_object();
    json_set(out, "ok", json_bool(false));
    json_set(out, "error", json_string(code));
    if (detail && detail[0]) json_set(out, "detail", json_string(detail));
    return out;
}

/* Mirrors: limit/measure from adapter (fallback 4096/len); if limit<=0 or
 * measure(text)<=limit return text; then repeatedly drop characters until
 * cut + ellipsis fits, and append the ellipsis. */
json_t *w9_pluginmessa_825c6f_fit(const struct json_t *in) {
    const char *text = json_get_str(in, "text", "");
    bool has_limit = json_has(in, "limit");
    long long limit = dec_ll(json_obj_get(in, "limit"), 4096);
    size_t n = cp_count(text);
    if (limit <= 0 || (long long)n <= limit) return json_string(text ? text : "");
    size_t cut = n;
    while (cut > 0 && (long long)(cut + 1) > limit) {
        long long remove = (long long)(cut + 1) - limit;
        if (remove < 1) remove = 1;
        if ((long long)cut < remove) cut = 0;
        else cut -= (size_t)remove;
    }
    (void)has_limit;
    size_t bytes = cp_prefix_len(text, cut);
    char *buf = (char *)malloc(bytes + 4);
    if (!buf) return json_string(text ? text : "");
    if (bytes) memcpy(buf, text, bytes);
    memcpy(buf + bytes, "\xe2\x80\xa6", 3); /* U+2026 */
    buf[bytes + 3] = '\0';
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

json_t *w9_pluginmessa_825c6f_supports_edit(const struct json_t *in) {
    const json_t *flag = json_obj_get(in, "flag");
    if (flag && flag->type == JSON_BOOL && !flag->bool_val) return json_bool(false);
    return json_bool(true);
}
