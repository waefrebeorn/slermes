/*
 * debug_summary.c — C home for upstream Hermes hermes_cli/debug_summary.py.
 *
 * PoP: w10_debugsumma_bc82f8_config_shape @ hermes_cli/debug_summary.py:_config_shape
 * PoP: w10_debugsumma_bc82f8_toolset_names @ hermes_cli/debug_summary.py:_toolset_names
 *
 * Faithful C11 port of the two pure allowlist/formatting helpers. The report
 * assemblers that read logs, git and platform state are out of scope.
 */
#include "noc_w10_debugsumma_bc82f8.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W10_MAX_SHAPE_DEPTH 2

struct w10_kv { const char *key; json_t *val; };

static int w10_cmp_kv(const void *a, const void *b) {
    const char *ka = ((const struct w10_kv *)a)->key;
    const char *kb = ((const struct w10_kv *)b)->key;
    return strcmp(ka ? ka : "", kb ? kb : "");
}

static void w10_shape_rec(const struct json_t *raw, const struct json_t *schema,
                          int depth, json_t *out) {
    if (!raw || raw->type != JSON_OBJECT) return;
    size_t n = json_object_size(raw);
    struct w10_kv *kv = (struct w10_kv *)calloc(n ? n : 1, sizeof(*kv));
    if (!kv) return;
    size_t m = 0;
    for (size_t i = 0; i < n; ++i) {
        kv[m].key = json_object_get_key_at(raw, i);
        kv[m].val = json_object_get_at(raw, i);
        if (kv[m].key) m++;
    }
    qsort(kv, m, sizeof(*kv), w10_cmp_kv);

    char indent[64];
    size_t ip = 0;
    for (int d = 0; d <= depth && ip + 2 < sizeof(indent) - 1; ++d) {
        indent[ip++] = ' '; indent[ip++] = ' ';
    }
    indent[ip] = 0;

    int unknown = 0;
    char line[4096];
    for (size_t i = 0; i < m; ++i) {
        const char *key = kv[i].key;
        json_t *def = schema ? json_obj_get(schema, key) : NULL;
        if (!def) { unknown++; continue; }
        json_t *val = kv[i].val;
        if (val && val->type == JSON_OBJECT && def->type == JSON_OBJECT &&
            json_object_size(def) > 0 && depth < W10_MAX_SHAPE_DEPTH) {
            snprintf(line, sizeof(line), "%s%s:", indent, key);
            json_append(out, json_string(line));
            w10_shape_rec(val, def, depth + 1, out);
        } else if (val && val->type == JSON_OBJECT) {
            snprintf(line, sizeof(line), "%s%s: {%d entries}", indent, key,
                     (int)json_object_size(val));
            json_append(out, json_string(line));
        } else if (val && val->type == JSON_ARRAY) {
            snprintf(line, sizeof(line), "%s%s: [%d items]", indent, key, (int)json_len(val));
            json_append(out, json_string(line));
        } else {
            snprintf(line, sizeof(line), "%s%s", indent, key);
            json_append(out, json_string(line));
        }
    }
    if (unknown > 0) {
        snprintf(line, sizeof(line), "%s(+%d other keys)", indent, unknown);
        json_append(out, json_string(line));
    }
    free(kv);
}

struct json_t *w10_debugsumma_bc82f8_config_shape(const struct json_t *raw,
                                                  const struct json_t *schema) {
    json_t *out = json_array();
    w10_shape_rec(raw, schema, 0, out);
    return out;
}

static int w10_in_toolsets(const char *name, const struct json_t *toolsets) {
    if (!toolsets || toolsets->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(toolsets); ++i) {
        json_t *v = json_get(toolsets, i);
        if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, name) == 0) return 1;
    }
    return 0;
}

struct json_t *w10_debugsumma_bc82f8_toolset_names(const struct json_t *raw_config,
                                                   const struct json_t *toolsets) {
    json_t *configured = raw_config ? json_obj_get(raw_config, "toolsets") : NULL;
    if (!configured || configured->type != JSON_ARRAY) return json_string("(default)");
    size_t n = json_len(configured);
    const char **known = (const char **)calloc(n ? n : 1, sizeof(*known));
    if (!known) return json_string("none");
    size_t kc = 0;
    int matched = 0;
    for (size_t i = 0; i < n; ++i) {
        json_t *v = json_get(configured, i);
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        if (!w10_in_toolsets(v->str_val, toolsets)) continue;
        matched++;
        int dup = 0;
        for (size_t j = 0; j < kc; ++j) if (strcmp(known[j], v->str_val) == 0) { dup = 1; break; }
        if (!dup) known[kc++] = v->str_val;
    }
    /* strcmp sort for the sorted(set(...)) ordering (ASCII keys). */
    for (size_t i = 1; i < kc; ++i) {
        const char *tmp = known[i];
        size_t j = i;
        while (j > 0 && strcmp(known[j - 1], tmp) > 0) { known[j] = known[j - 1]; j--; }
        known[j] = tmp;
    }
    int custom = (int)n - matched;
    if (kc == 0 && custom == 0) { free(known); return json_string("none"); }
    char out[8192];
    size_t p = 0;
    out[0] = 0;
    for (size_t i = 0; i < kc; ++i) {
        if (p >= sizeof(out)) break;
        if (p) { int w = snprintf(out + p, sizeof(out) - p, ", "); if (w > 0) p += (size_t)w; }
        if (p >= sizeof(out)) break;
        int w = snprintf(out + p, sizeof(out) - p, "%s", known[i]);
        if (w > 0) p += (size_t)w;
    }
    if (custom > 0 && p < sizeof(out)) {
        if (p) { int w = snprintf(out + p, sizeof(out) - p, ", "); if (w > 0) p += (size_t)w; }
        if (p < sizeof(out)) snprintf(out + p, sizeof(out) - p, "+%d custom", custom);
    }
    free(known);
    return json_string(out);
}
