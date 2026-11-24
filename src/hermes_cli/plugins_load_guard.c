/*
 * plugins_load_guard.c — C11 port of upstream Hermes hermes_cli/plugins_load_guard.py.
 *
 * PoP: w17_pluginsloa_4e6576_guardable_names @ hermes_cli/plugins_load_guard.py:ignore_registrars_after_abandoned_load
 *
 * The decorator walks every class in cls.__mro__, collects names from vars(klass)
 * that start with "register_" or are exactly "subscribe"/"on_unload", then wraps
 * each callable in sorted order. This module ports that name-selection decision:
 * given the MRO's per-class attribute-name lists, return the sorted unique set of
 * names the guard wraps. The oracle builds real classes, applies the LIVE
 * decorator, and reads back the names carrying functools.wraps' __wrapped__.
 */
#include "noc_w17_pluginsloa_4e6576.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int wanted(const char *n) {
    if (!n || !*n) return 0;
    if (!strncmp(n, "register_", 9)) return 1;
    return !strcmp(n, "subscribe") || !strcmp(n, "on_unload");
}

static int in_list(const char **list, size_t count, const char *s) {
    for (size_t i = 0; i < count; i++)
        if (list[i] && !strcmp(list[i], s)) return 1;
    return 0;
}

static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

struct json_t *w17_pluginsloa_4e6576_guardable_names(const struct json_t *sets) {
    const char **names = NULL;
    size_t count = 0, cap = 0;

    if (sets && sets->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(sets); i++) {
            const json_t *grp = json_get(sets, i);
            if (!grp || grp->type != JSON_ARRAY) continue;
            for (size_t j = 0; j < json_len(grp); j++) {
                const json_t *n = json_get(grp, j);
                if (!n || n->type != JSON_STRING || !n->str_val) continue;
                if (!wanted(n->str_val) || in_list(names, count, n->str_val)) continue;
                if (count == cap) {
                    cap = cap ? cap * 2 : 16;
                    const char **tmp = (const char **)realloc(names, cap * sizeof(*names));
                    if (!tmp) break;
                    names = tmp;
                }
                names[count++] = dupstr(n->str_val);
            }
        }
    }

    if (count > 1) qsort(names, count, sizeof(*names), cmp_str);
    json_t *out = json_array();
    for (size_t i = 0; i < count; i++) {
        json_append(out, json_string(names[i]));
        free((void *)names[i]);
    }
    free(names);
    return out;
}
