/*
 * finder.c -- C11 port of the name algebra of upstream Hermes
 *   hermes_agent/compat/finder.py.
 *
 * PoP: w22_finder_213faa_current_name @ hermes_agent/compat/finder.py:current_name
 * PoP: w22_finder_213faa_child_names @ hermes_agent/compat/finder.py:_child_names
 * PoP: w22_finder_213faa_strict_error @ hermes_agent/compat/finder.py:Finder.find_spec
 *
 * The rest of the module (_canonical, _Alias, _AliasSpec, and Finder.find_spec's
 * PathFinder probes plus its importlib.util.find_spec call) is the live import
 * system: those stay upstream. Pure C11, ASCII, no VLA.
 */
#include "hermes_json.h"
#include "noc_w22_finder_213faa.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *fn213faa_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *fn213faa_dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Object key lookup that keeps dict semantics (no default): the value stored
 * under `key`, or NULL when the mapping does not carry it. */
static const json_t *fn213faa_dict_get(const json_t *moves, const char *key, size_t key_len) {
    size_t i;
    if (!moves || moves->type != JSON_OBJECT) return NULL;
    for (i = 0; i < moves->c.count; i++) {
        const char *k = moves->c.keys ? moves->c.keys[i] : NULL;
        if (!k) continue;
        if (strlen(k) == key_len && memcmp(k, key, key_len) == 0) return moves->c.items[i];
    }
    return NULL;
}

/* Segment offsets of `name` split on '.', Python's str.split('.') shape (an
 * empty string yields one empty segment). Caller frees the answer. */
static size_t *fn213faa_segments(const char *name, size_t *count_out) {
    size_t n = strlen(name), i, k = 1;
    size_t *starts;
    for (i = 0; i < n; i++) {
        if (name[i] == '.') k++;
    }
    starts = (size_t *)malloc(k * sizeof(size_t));
    if (!starts) return NULL;
    starts[0] = 0;
    k = 1;
    for (i = 0; i < n; i++) {
        if (name[i] == '.') starts[k++] = i + 1;
    }
    *count_out = k;
    return starts;
}

char *w22_finder_213faa_current_name(const char *name, const json_t *moves) {
    size_t *starts, segs = 0, size;
    const char *self = name ? name : "";
    if (!moves || moves->type != JSON_OBJECT) return NULL;
    starts = fn213faa_segments(self, &segs);
    if (!starts) return NULL;
    for (size = segs; size > 0; size--) {
        /* parts[:size] joined: offset 0 up to the start of segment `size`. */
        size_t plen = (size == segs) ? strlen(self) : starts[size] - 1;
        const json_t *v = fn213faa_dict_get(moves, self, plen);
        if (v) {
            char *tail, *out;
            size_t tlen;
            if (v->type != JSON_STRING) { free(starts); return NULL; }
            tlen = strlen(self) - plen;
            tail = fn213faa_dup_n(self + plen, tlen);
            if (!tail) { free(starts); return NULL; }
            out = (char *)malloc(strlen(v->str_val ? v->str_val : "") + tlen + 1);
            if (!out) { free(tail); free(starts); return NULL; }
            strcpy(out, v->str_val ? v->str_val : "");
            strcat(out, tail);
            free(tail);
            free(starts);
            return out;
        }
    }
    free(starts);
    return NULL;
}

/* The derived child name of `old`, or NULL when its old parent did not move. */
static char *fn213faa_derived_name(const char *old, const json_t *moves) {
    const char *dot = strrchr(old, '.');
    char *parent, *moved_parent, *out;
    const char *child;
    size_t plen, clen;
    if (dot) {
        plen = (size_t)(dot - old);
        child = dot + 1;
    } else {
        plen = 0;
        child = old;
    }
    if (plen == 0) return NULL;
    parent = fn213faa_dup_n(old, plen);
    if (!parent) return NULL;
    moved_parent = w22_finder_213faa_current_name(parent, moves);
    free(parent);
    if (!moved_parent) return NULL;
    clen = strlen(child);
    out = (char *)malloc(strlen(moved_parent) + clen + 2);
    if (!out) { free(moved_parent); return NULL; }
    strcpy(out, moved_parent);
    strcat(out, ".");
    strcat(out, child);
    free(moved_parent);
    return out;
}

json_t *w22_finder_213faa_child_names(const json_t *moves) {
    json_t *out = json_object();
    size_t i;
    if (!out) return NULL;
    if (!moves || moves->type != JSON_OBJECT) return out;
    for (i = 0; i < moves->c.count; i++) {
        const char *old = moves->c.keys ? moves->c.keys[i] : NULL;
        const json_t *v = moves->c.items ? moves->c.items[i] : NULL;
        char *name;
        if (!old || !v) continue;
        name = fn213faa_derived_name(old, moves);
        if (!name) continue;
        /* `name != new` is False only for an equal string; any other value type
         * is unequal to a str, exactly like Python's !=. */
        if (!(v->type == JSON_STRING && strcmp(name, v->str_val ? v->str_val : "") == 0) &&
            fn213faa_dict_get(moves, name, strlen(name)) == NULL) {
            json_t *cv = json_copy(v);
            json_set(out, name, cv ? cv : json_null());
        }
        free(name);
    }
    return out;
}

char *w22_finder_213faa_strict_error(const char *name, const json_t *moves) {
    const char *self = name ? name : "";
    char *new = w22_finder_213faa_current_name(self, moves);
    char *out;
    size_t n;
    if (!new) {
        json_t *derived = w22_finder_213faa_child_names(moves);
        const json_t *v = derived ? fn213faa_dict_get(derived, self, strlen(self)) : NULL;
        if (v && v->type == JSON_STRING) new = fn213faa_dup(v->str_val ? v->str_val : "");
        json_free(derived);
    }
    if (!new) return NULL;
    n = strlen(self) + strlen(new) + strlen(" has moved: import ") + 1;
    out = (char *)malloc(n);
    if (!out) { free(new); return NULL; }
    snprintf(out, n, "%s has moved: import %s", self, new);
    free(new);
    return out;
}
