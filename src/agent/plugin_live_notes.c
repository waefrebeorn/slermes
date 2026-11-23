/*
 * plugin_live_notes.c - real C11 port of upstream Hermes agent/plugin_live_notes.py.
 *
 * PoP: w9_pluginlive_da5eef_append @ agent/plugin_live_notes.py:_append
 * PoP: w9_pluginlive_da5eef_queue  @ agent/plugin_live_notes.py:queue_plugin_notice
 * PoP: w9_pluginlive_da5eef_pop    @ agent/plugin_live_notes.py:pop_plugin_notices
 *
 * The queue is an insertion-ordered dict of plugin_key -> note.  The oracle
 * seeds the agent's queue and injects an identity redactor seam, so the ported
 * logic is the module's own ordering / replacement / join / append rules.
 */
#include "hermes_json.h"
#include "noc_w9_pluginlive_da5eef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *concat3(const char *a, const char *b, const char *c) {
    size_t la = a ? strlen(a) : 0, lb = b ? strlen(b) : 0, lc = c ? strlen(c) : 0;
    char *out = (char *)malloc(la + lb + lc + 1);
    if (!out) return NULL;
    if (la) memcpy(out, a, la);
    if (lb) memcpy(out + la, b, lb);
    if (lc) memcpy(out + la + lb, c, lc);
    out[la + lb + lc] = '\0';
    return out;
}

struct json_t *w9_pluginlive_da5eef_append(const struct json_t *content, const char *notes)
{
    const char *n = notes ? notes : "";
    if (content && content->type == JSON_ARRAY) {
        struct json_t *arr = json_array();
        for (size_t i = 0; i < content->c.count; i++)
            json_append(arr, json_copy(content->c.items[i]));
        struct json_t *row = json_object();
        json_set(row, "type", json_string("text"));
        json_set(row, "text", json_string(n));
        json_append(arr, row);
        return arr;
    }
    /* else: f"{content or ''}\n\n{notes}" */
    const char *base = (content && content->type == JSON_STRING) ? content->str_val : "";
    char *s = concat3(base, "\n\n", n);
    struct json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

struct json_t *w9_pluginlive_da5eef_queue(const struct json_t *notes,
                                           const char *plugin_key, const char *note)
{
    struct json_t *out = json_object();
    if (notes && notes->type == JSON_OBJECT) {
        for (size_t i = 0; i < notes->c.count; i++) {
            const char *k = notes->c.keys[i];
            if (k && plugin_key && !strcmp(k, plugin_key)) continue;
            json_set(out, k ? k : "", json_copy(notes->c.items[i]));
        }
    }
    json_set(out, plugin_key ? plugin_key : "", json_string(note ? note : ""));
    return out;
}

struct json_t *w9_pluginlive_da5eef_pop(const struct json_t *notes)
{
    /* "\n\n".join(notes.values()) in insertion order; redactor seam is identity. */
    size_t total = 1, n = (notes && notes->type == JSON_OBJECT) ? notes->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = notes->c.items[i];
        total += (v && v->type == JSON_STRING && v->str_val) ? strlen(v->str_val) : 0;
        if (i + 1 < n) total += 2;
    }
    char *buf = (char *)malloc(total + 1);
    if (!buf) return json_string("");
    size_t off = 0;
    buf[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (i) { buf[off++] = '\n'; buf[off++] = '\n'; }
        json_t *v = notes->c.items[i];
        const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
        size_t l = strlen(s);
        memcpy(buf + off, s, l);
        off += l;
    }
    buf[off] = '\0';
    struct json_t *out = json_string(buf);
    free(buf);
    return out;
}
