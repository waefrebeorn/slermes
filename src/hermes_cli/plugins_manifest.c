/*
 * plugins_manifest.c — real C11 port of the pure manifest-normalization helpers
 * of upstream Hermes hermes_cli/plugins_manifest.py (gate w15_pluginsman_9202a1).
 *
 * PoP: w15_pluginsman_9202a1_portable_slug @ hermes_cli/plugins_manifest.py:_portable_slug
 * PoP: w15_pluginsman_9202a1_portable_mcp_server_name @ hermes_cli/plugins_manifest.py:portable_mcp_server_name
 * PoP: w15_pluginsman_9202a1_display_author @ hermes_cli/plugins_manifest.py:_display_author
 * PoP: w15_pluginsman_9202a1_config_key_path @ hermes_cli/plugins_manifest.py:_config_key_path
 * PoP: w15_pluginsman_9202a1_manifest_config_keys @ hermes_cli/plugins_manifest.py:manifest_config_keys
 * PoP: w15_pluginsman_9202a1_detect_kind_from_source @ hermes_cli/plugins_manifest.py:_detect_kind_from_source
 * PoP: w15_pluginsman_9202a1_version_satisfies @ hermes_cli/plugins_manifest.py:version_satisfies
 */
#include "noc_w15_pluginsman_9202a1.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static char *dupnstr(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

typedef struct { char *p; size_t n, cap; } lbuf_t;
static int lb_reserve(lbuf_t *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return 1;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->n + extra + 1) cap *= 2;
    char *q = (char *)realloc(b->p, cap);
    if (!q) return 0;
    b->p = q; b->cap = cap;
    return 1;
}
static void lb_init(lbuf_t *b) {
    b->p = NULL; b->n = 0; b->cap = 0;
    if (lb_reserve(b, 1) && b->p) b->p[0] = '\0';
}
static void lb_adds(lbuf_t *b, const char *s) {
    size_t n = strlen(s);
    if (!lb_reserve(b, n)) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

char *w15_pluginsman_9202a1_portable_slug(const char *key) {
    const char *k = key ? key : "";
    size_t n = strlen(k);
    char *slug = (char *)malloc(n + 1);
    if (!slug) return dupstr("plugin");
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)k[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
            slug[i] = (char)c;
        else
            slug[i] = '-';
    }
    slug[n] = '\0';
    size_t a = 0, b = n;
    while (a < b && (slug[a] == '-' || slug[a] == '_')) a++;
    while (b > a && (slug[b - 1] == '-' || slug[b - 1] == '_')) b--;
    if (a == b) { free(slug); return dupstr("plugin"); }
    if (a > 0) memmove(slug, slug + a, b - a);
    slug[b - a] = '\0'; /* NUL written at the shortened length */
    return slug;
}

char *w15_pluginsman_9202a1_portable_mcp_server_name(const char *key,
                                                    const char *server) {
    (void)key; /* signature parity with the live loader; identity not part of the name */
    return w15_pluginsman_9202a1_portable_slug(server);
}

static int py_truthy(json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}
static void append_py_str(lbuf_t *b, json_t *v) {
    if (!v) return;
    switch (v->type) {
        case JSON_STRING: lb_adds(b, v->str_val ? v->str_val : ""); break;
        case JSON_BOOL: lb_adds(b, v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER: {
            char num[64];
            double d = v->num_val;
            if (d == (double)(long long)d) snprintf(num, sizeof(num), "%lld", (long long)d);
            else snprintf(num, sizeof(num), "%.15g", d);
            lb_adds(b, num);
            break;
        }
        case JSON_NULL: lb_adds(b, "None"); break;
        default: break;
    }
}

char *w15_pluginsman_9202a1_display_author(json_t *value) {
    lbuf_t b;
    lb_init(&b);
    if (!value || value->type == JSON_NULL) return b.p ? b.p : dupstr("");
    if (value->type == JSON_OBJECT) {
        static const char *const fields[] = {"name", "email", "url", NULL};
        int first = 1;
        for (int i = 0; fields[i]; i++) {
            json_t *f = json_obj_get(value, fields[i]);
            if (!py_truthy(f)) continue;
            if (!first) lb_adds(&b, ", ");
            first = 0;
            append_py_str(&b, f);
        }
    } else {
        append_py_str(&b, value);
    }
    return b.p ? b.p : dupstr("");
}

char *w15_pluginsman_9202a1_config_key_path(const char *item) {
    if (!item) return NULL;
    const char *a = item, *z = item + strlen(item);
    while (a < z && isspace((unsigned char)*a)) a++;
    while (z > a && isspace((unsigned char)*(z - 1))) z--;
    if (z - a < 1) return NULL;
    int dots = 0;
    for (const char *p = a; p < z; p++) {
        if (*p == '.') {
            dots++;
            if (p == a || p + 1 == z || p[-1] == '.' || p[1] == '.') return NULL;
        }
    }
    if (dots < 1) return NULL;
    return dupnstr(a, (size_t)(z - a));
}

struct json_t *w15_pluginsman_9202a1_manifest_config_keys(struct json_t *data) {
    json_t *out = json_array();
    if (!data || data->type != JSON_OBJECT) return out;
    json_t *ck = json_obj_get(data, "config_keys");
    if (!ck || ck->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(ck); i++) {
        json_t *item = json_get(ck, i);
        if (!item || item->type != JSON_STRING || !item->str_val) continue;
        char *path = w15_pluginsman_9202a1_config_key_path(item->str_val);
        if (path) { json_append(out, json_string(path)); free(path); }
    }
    return out;
}

const char *w15_pluginsman_9202a1_detect_kind_from_source(const char *source) {
    if (!source) return NULL;
    static const char *const markers[] = {
        "register_memory_provider", "MemoryProvider", "register_cron_scheduler",
        "CronScheduler", "register_computer_use_provider", "ComputerUseProvider", NULL
    };
    for (int i = 0; markers[i]; i++)
        if (strstr(source, markers[i])) return "exclusive";
    if (strstr(source, "register_provider") && strstr(source, "ProviderProfile"))
        return "model-provider";
    return NULL;
}

typedef struct { int v[3]; } vtriple_t;

/* _version_tuple: strip, lstrip("v"), cut at first '-'/'+', split(".", 3). */
static int version_tuple(const char *s, vtriple_t *out) {
    const char *a = s ? s : "", *z = a + strlen(a);
    while (a < z && isspace((unsigned char)*a)) a++;
    while (z > a && isspace((unsigned char)*(z - 1))) z--;
    while (a < z && *a == 'v') a++;
    const char *cut = a;
    while (cut < z && *cut != '-' && *cut != '+') cut++;
    vtriple_t t = {{0, 0, 0}};
    const char *p = a;
    for (int slot = 0; slot < 3; slot++) {
        const char *start = p;
        while (p < cut && isspace((unsigned char)*p)) p++;
        if (p >= cut) { t.v[slot] = 0; continue; } /* absent segment pads with zero */
        const char *dstart = p;
        while (p < cut && *p >= '0' && *p <= '9') p++;
        if (p == dstart) return 0; /* no leading digits */
        t.v[slot] = (int)strtol(dstart, NULL, 10);
        (void)start;
        if (p < cut) {
            if (*p != '.') {
                /* non-dot before cut: stop consuming; remaining slots stay 0 */
                p = cut;
            } else p++;
        }
    }
    out->v[0] = t.v[0]; out->v[1] = t.v[1]; out->v[2] = t.v[2];
    return 1;
}
static int cmp3(const vtriple_t *a, const vtriple_t *b) {
    for (int i = 0; i < 3; i++) {
        if (a->v[i] < b->v[i]) return -1;
        if (a->v[i] > b->v[i]) return 1;
    }
    return 0;
}

int w15_pluginsman_9202a1_version_satisfies(const char *spec, const char *current) {
    vtriple_t cur;
    if (!version_tuple(current, &cur)) return 1; /* permissive on unparseable current */
    const char *p = spec ? spec : "";
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len >= 256) len = 255;
        char clause[256];
        memcpy(clause, p, len);
        clause[len] = '\0';
        char *a = clause, *z = clause + len;
        while (a < z && isspace((unsigned char)*a)) a++;
        while (z > a && isspace((unsigned char)*(z - 1))) z--;
        *z = '\0';
        if (a < z) {
            const char *op = ">=";
            char *target = a;
            if (a + 1 < z && ((a[0] == '>' && (a[1] == '=')) ||
                              (a[0] == '<' && a[1] == '=') ||
                              (a[0] == '=' && a[1] == '=') ||
                              (a[0] == '!' && a[1] == '='))) {
                if (a[0] == '>') op = (a[1] == '=') ? ">=" : ">";
                else if (a[0] == '<') op = (a[1] == '=') ? "<=" : "<";
                else if (a[0] == '=') op = "==";
                else op = "!=";
                target = a + 2;
            } else if (a[0] == '>' || a[0] == '<') {
                op = (a[0] == '>') ? ">" : "<";
                target = a + 1;
            }
            while (*target && isspace((unsigned char)*target)) target++;
            vtriple_t tgt;
            if (version_tuple(target, &tgt)) {
                int c = cmp3(&cur, &tgt);
                int ok = (!strcmp(op, ">=") && c >= 0) || (!strcmp(op, "<=") && c <= 0) ||
                         (!strcmp(op, "==") && c == 0) || (!strcmp(op, "!=") && c != 0) ||
                         (!strcmp(op, ">") && c > 0) || (!strcmp(op, "<") && c < 0);
                if (!ok) return 0;
            }
        }
        if (!comma) break;
        p = comma + 1;
    }
    return 1;
}

