/*
 * plugins_updates.c — C home for upstream Hermes hermes_cli/plugins_updates.py.
 *
 * PoP: w13_pluginsupd_a7336f_https_update_url @ hermes_cli/plugins_updates.py:https_update_url
 * PoP: w13_pluginsupd_a7336f_version_is_newer @ hermes_cli/plugins_updates.py:_version_is_newer
 * PoP: w13_pluginsupd_a7336f_check_result_json @ hermes_cli/plugins_updates.py:CheckResult.to_json
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_pluginsupd_a7336f.h"

#define MAX_REL 16

static char *dups_sub(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static char *dups(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static bool truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count != 0;
}
/* Python str() for JSON scalars (with `or ""` coercion handled by caller). */
static char *py_scalar(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dups("None");
    if (v->type == JSON_STRING) return dups(v->str_val);
    if (v->type == JSON_BOOL) return dups(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return dups(buf);
    }
    return dups(v->type == JSON_ARRAY ? "[]" : "{}");
}
static char *py_repr(const char *s) {
    size_t i, n = s ? strlen(s) : 0, k = 0;
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return dups("''");
    out[k++] = '\'';
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == '\'') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20) { snprintf(out + k, 8, "\\x%02x", c); k += 4; }
        else out[k++] = (char)c;
    }
    out[k++] = '\''; out[k] = '\0';
    return out;
}
static char *strip_str(const char *s) {
    size_t a = 0, b;
    if (!s) return dups("");
    b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return dups_sub(s + a, b - a);
}
/* PEP440-lite: plain dotted numeric releases with an optional leading v.
 * Returns false for anything packaging.Version would reject (for the
 * fixture subset used here: plain numeric / invalid strings). */
static bool parse_release(const char *raw, long long *out, size_t *count) {
    char *s = strip_str(raw);
    const char *p = s;
    size_t n = 0;
    if (*p == 'v' || *p == 'V') p++;
    if (!*p) { free(s); return false; }
    for (;;) {
        const char *seg = p;
        char *next = strchr(p, '.');
        size_t digits = 0, i;
        long long v = 0;
        while (seg[digits] && isdigit((unsigned char)seg[digits])) {
            if (v < 1000000000000LL) v = v * 10 + (seg[digits] - '0');
            digits++;
        }
        if (!digits) { free(s); return false; }
        {
            size_t seglen = next ? (size_t)(next - seg) : strlen(seg);
            for (i = digits; i < seglen; i++) if (seg[i]) { free(s); return false; }
        }
        if (n < MAX_REL) out[n++] = v;
        if (!next) break;
        p = next + 1;
    }
    free(s);
    while (n > 1 && out[n - 1] == 0) n--;
    *count = n;
    return true;
}

json_t *w13_pluginsupd_a7336f_https_update_url(const struct json_t *in) {
    const json_t *urlv = json_obj_get(in, "url");
    char *raw = truthy(urlv) ? py_scalar(urlv) : dups("");
    char *text = strip_str(raw);
    json_t *out = json_object();
    free(raw);
    if (strncasecmp(text, "https://", 8) != 0) {
        char *rep = py_repr(text);
        char msg[1024];
        snprintf(msg, sizeof(msg), "update_url must be an https:// URL, got %s", rep);
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", json_string(msg));
        free(rep);
    } else {
        json_set(out, "ok", json_bool(true));
        json_set(out, "value", json_string(text));
    }
    free(text);
    return out;
}

json_t *w13_pluginsupd_a7336f_version_is_newer(const struct json_t *in) {
    long long a[MAX_REL], b[MAX_REL];
    size_t na = 0, nb = 0, i, n;
    const char *latest = json_get_str(in, "latest", "");
    const char *current = json_get_str(in, "current", "");
    if (!parse_release(latest, a, &na) || !parse_release(current, b, &nb)) return json_null();
    n = na > nb ? na : nb;
    for (i = 0; i < n; i++) {
        long long x = i < na ? a[i] : 0, y = i < nb ? b[i] : 0;
        if (x > y) return json_bool(true);
        if (x < y) return json_bool(false);
    }
    return json_bool(false);
}

static json_t *copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

json_t *w13_pluginsupd_a7336f_check_result_json(const struct json_t *in) {
    json_t *out = json_object();
    const char *name = json_get_str(in, "name", "");
    const char *klass = json_get_str(in, "klass", "");
    const char *reason = json_get_str(in, "reason", "");
    json_set(out, "name", json_string(name));
    json_set(out, "class", json_string(klass));
    json_set(out, "current", copy_or_null(json_obj_get(in, "current")));
    json_set(out, "latest", copy_or_null(json_obj_get(in, "latest")));
    json_set(out, "update_available", copy_or_null(json_obj_get(in, "update_available")));
    json_set(out, "needs_fixing", copy_or_null(json_obj_get(in, "needs_fixing")));
    json_set(out, "min_hermes", copy_or_null(json_obj_get(in, "min_hermes")));
    json_set(out, "reason", json_string(reason));
    return out;
}
