/*
 * paths.c -- C11 port of hermes_agent/compat/paths.py.
 *
 * The mapping engine is real: dotted-prefix lookup, the slash-path shape
 * rules of `_map`, the bounded undo walk over a table inverse, and fnmatch
 * globbing for `current_globs`. The MOVES table is injected (the AST/file read
 * is the seam), so an empty table is the identity and a fixture drives every
 * branch.
 *
 * PoP: w22_paths_5ce33e_directory @ hermes_agent/compat/paths.py:_directory
 * PoP: w22_paths_5ce33e_current @ hermes_agent/compat/paths.py:current_path
 * PoP: w22_paths_5ce33e_original @ hermes_agent/compat/paths.py:original_path
 * PoP: w22_paths_5ce33e_module @ hermes_agent/compat/paths.py:current_module
 * PoP: w22_paths_5ce33e_original_module @ hermes_agent/compat/paths.py:original_module
 * PoP: w22_paths_5ce33e_current_globs @ hermes_agent/compat/paths.py:current_globs
 */
#include "hermes_json.h"
#include "noc_w22_paths_5ce33e.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static char *pm_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *pm_cat2(const char *a, const char *b) {
    size_t na = strlen(a), nb = strlen(b);
    char *out = (char *)malloc(na + nb + 1);
    if (!out) return NULL;
    memcpy(out, a, na);
    memcpy(out + na, b, nb + 1);
    return out;
}

static char *pm_cat3(const char *a, const char *b, const char *c) {
    size_t na = strlen(a), nb = strlen(b), nc = strlen(c);
    char *out = (char *)malloc(na + nb + nc + 1);
    if (!out) return NULL;
    memcpy(out, a, na);
    memcpy(out + na, b, nb);
    memcpy(out + na + nb, c, nc + 1);
    return out;
}

/* The table's value for an exact key, or NULL. Non-string values never match. */
static const char *pm_get(const json_t *table, const char *key) {
    json_t *value;
    if (!table || table->type != JSON_OBJECT) return NULL;
    value = json_obj_get(table, key);
    if (!value || value->type != JSON_STRING || !value->str_val) return NULL;
    return value->str_val;
}

/* current_name(name, table): the longest dotted prefix that is a key. */
static char *pm_current_name(const json_t *table, const char *name, int *found) {
    size_t n, size;
    char *prefix, *out;
    const char *value;
    if (!name) name = "";
    n = strlen(name);
    if (!table || table->type != JSON_OBJECT) { *found = 0; return NULL; }
    if (n == 0) {
        value = pm_get(table, "");
        if (!value) { *found = 0; return NULL; }
        *found = 1;
        return pm_dup(value);
    }
    prefix = (char *)malloc(n + 1);
    if (!prefix) { *found = 0; return NULL; }
    *found = 0;
    for (size = n; size > 0; size--) {
        if (size < n && name[size] != '.') continue;
        memcpy(prefix, name, size);
        prefix[size] = '\0';
        value = pm_get(table, prefix);
        if (value) {
            *found = 1;
            out = pm_cat2(value, name + size);
            free(prefix);
            return out;
        }
    }
    free(prefix);
    return NULL;
}

/* Is every "/"-separated segment non-empty and free of "."? */
static int pm_is_dotted_path(const char *path) {
    size_t i = 0, n = strlen(path);
    for (;;) {
        size_t start = i;
        while (i < n && path[i] != '/') i++;
        if (i == start) return 0;
        for (; start < i; start++) {
            if (path[start] == '.') return 0;
        }
        if (i >= n) return 1;
        i++;
    }
}

static char *pm_directory(const json_t *table, const char *path) {
    char *dotted, *mapped, *out;
    size_t i;
    int found = 0;
    if (!path) path = "";
    if (!table || table->type != JSON_OBJECT || table->c.count == 0) return pm_dup(path);
    if (!pm_is_dotted_path(path)) return pm_dup(path);
    dotted = pm_dup(path);
    if (!dotted) return NULL;
    for (i = 0; dotted[i]; i++) {
        if (dotted[i] == '/') dotted[i] = '.';
    }
    mapped = pm_current_name(table, dotted, &found);
    free(dotted);
    if (!found || !mapped) return pm_dup(path);
    for (i = 0; mapped[i]; i++) {
        if (mapped[i] == '.') mapped[i] = '/';
    }
    out = mapped;
    return out;
}

/* last segment after the final "/" (the whole string when there is none) */
static const char *pm_last_segment(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int pm_ends_with(const char *s, const char *suffix) {
    size_t ns = strlen(s), nf = strlen(suffix);
    return ns >= nf && !strcmp(s + ns - nf, suffix);
}

static char *pm_map(const json_t *table, const char *path) {
    const char *last, *slash;
    size_t n, head_len;
    char *head, *dir, *out, *base, *trimmed;

    if (!path) path = "";
    if (!table || table->type != JSON_OBJECT || table->c.count == 0) return pm_dup(path);
    n = strlen(path);
    if (n && path[n - 1] == '/') {
        /* _directory(path[:-1], table) + "/" */
        trimmed = (char *)malloc(n);
        if (!trimmed) return NULL;
        memcpy(trimmed, path, n - 1);
        trimmed[n - 1] = '\0';
        dir = pm_directory(table, trimmed);
        free(trimmed);
        if (!dir) return NULL;
        out = pm_cat2(dir, "/");
        free(dir);
        return out;
    }
    last = pm_last_segment(path);
    slash = last == path ? NULL : last - 1;
    head_len = slash ? (size_t)(slash - path) : 0;
    head = (char *)malloc(head_len + 1);
    if (!head) return NULL;
    if (head_len) memcpy(head, path, head_len);
    head[head_len] = '\0';

    if (!strcmp(last, "__init__.py")) {
        if (head_len == 0) { free(head); return pm_dup(path); }
        dir = pm_directory(table, head);
        free(head);
        if (!dir) return NULL;
        out = pm_cat2(dir, "/__init__.py");
        free(dir);
        return out;
    }
    if (pm_ends_with(last, ".py")) {
        size_t stem = strlen(last) - 3;
        char *stem_text = (char *)malloc(stem + 1);
        if (!stem_text) { free(head); return NULL; }
        memcpy(stem_text, last, stem);
        stem_text[stem] = '\0';
        if (head_len) base = pm_cat3(head, "/", stem_text);
        else base = pm_dup(stem_text);
        free(stem_text);
        free(head);
        if (!base) return NULL;
        dir = pm_directory(table, base);
        free(base);
        if (!dir) return NULL;
        out = pm_cat2(dir, ".py");
        free(dir);
        return out;
    }
    if (strchr(last, '.')) {
        if (head_len == 0) { free(head); return pm_dup(path); }
        dir = pm_directory(table, head);
        free(head);
        if (!dir) return NULL;
        out = pm_cat3(dir, "/", last);
        free(dir);
        return out;
    }
    free(head);
    return pm_directory(table, path);
}

/* PathMoves.inverse: first old name per target; keys outside hermes_agent/
 * first, then those inside it (the sort key is `key.startswith(...)`). */
static json_t *pm_inverse(const json_t *table) {
    json_t *inv = json_object();
    size_t i;
    int pass;
    if (!table || table->type != JSON_OBJECT) return inv;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < table->c.count; i++) {
            const char *key = table->c.keys[i];
            const json_t *value = table->c.items[i];
            int inside;
            if (!key || !value || value->type != JSON_STRING || !value->str_val) continue;
            inside = !strncmp(key, "hermes_agent.", 13);
            if (inside != pass) continue;
            if (json_obj_get(inv, value->str_val) != NULL) continue;
            json_set(inv, value->str_val, json_string(key));
        }
    }
    return inv;
}

static char *pm_undo(const json_t *table, const char *name, char *(*step)(const json_t *, const char *)) {
    char *current = pm_dup(name);
    int i;
    if (!current) return NULL;
    for (i = 0; i < 8; i++) {
        char *next = step(table, current);
        if (!next) { free(current); return NULL; }
        if (!strcmp(next, current)) { free(next); break; }
        free(current);
        current = next;
    }
    return current;
}

static char *pm_undo_module(const json_t *table, const char *name) {
    char *current = pm_dup(name);
    int i;
    if (!current) return NULL;
    for (i = 0; i < 8; i++) {
        int found = 0;
        char *mapped = pm_current_name(table, current, &found);
        char *next = found ? mapped : pm_dup(current);
        if (!next) { free(current); return NULL; }
        if (!strcmp(next, current)) { free(next); break; }
        free(current);
        current = next;
    }
    return current;
}

/* --- fnmatch ------------------------------------------------------------- */

static char pm_fold(char c) {
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c == '/') c = '\\';
    return c;
}

static int pm_match_here(const char *name, const char *pattern);

static int pm_match_class(const char *name, const char *pattern, const char **after) {
    /* pattern points just past '['. Returns 1/0 for a match and sets *after. */
    const char *p = pattern;
    int negate = 0, matched = 0;
    char c = pm_fold(*name);
    if (*p == '!') { negate = 1; p++; }
    if (*p == ']') {
        if (c == ']') matched = 1;
        p++;
    }
    while (*p && *p != ']') {
        char lo, hi;
        if (p[1] == '-' && p[2] && p[2] != ']') {
            lo = pm_fold(p[0]);
            hi = pm_fold(p[2]);
            if (c >= lo && c <= hi) matched = 1;
            p += 3;
        } else {
            if (c == pm_fold(*p)) matched = 1;
            p++;
        }
    }
    if (*p == ']') p++;
    *after = p;
    return negate ? !matched : matched;
}

static int pm_match_here(const char *name, const char *pattern) {
    while (*pattern) {
        if (*pattern == '*') {
            const char *p = pattern;
            while (*p == '*') p++;
            if (!*p) return 1;
            for (;;) {
                if (pm_match_here(name, p)) return 1;
                if (!*name) return 0;
                name++;
            }
        }
        if (!*name) return 0;
        if (*pattern == '?') { name++; pattern++; continue; }
        if (*pattern == '[') {
            const char *after = NULL;
            if (pm_match_class(name, pattern + 1, &after)) {
                name++;
                pattern = after;
                continue;
            }
            return 0;
        }
        if (pm_fold(*name) != pm_fold(*pattern)) return 0;
        name++;
        pattern++;
    }
    return *name == '\0';
}

static int pm_fnmatch(const char *name, const char *pattern) {
    return pm_match_here(name, pattern);
}

/* --- current_globs ------------------------------------------------------- */

typedef struct { char **items; size_t count, cap; } pm_list;

static int pm_list_add(pm_list *list, const char *value) {
    size_t i;
    char *copy;
    for (i = 0; i < list->count; i++) {
        if (!strcmp(list->items[i], value)) return 1;
    }
    if (list->count == list->cap) {
        size_t ncap = list->cap ? list->cap * 2 : 8;
        char **grown = (char **)realloc(list->items, ncap * sizeof(char *));
        if (!grown) return 0;
        list->items = grown;
        list->cap = ncap;
    }
    copy = pm_dup(value);
    if (!copy) return 0;
    list->items[list->count++] = copy;
    return 1;
}

static void pm_list_free(pm_list *list) {
    size_t i;
    for (i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = list->cap = 0;
}

static int pm_has_glob(const char *text) {
    return text && strpbrk(text, "*?[") != NULL;
}

static int pm_cmp_strings(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w22_paths_5ce33e_current_globs(const json_t *table, const char *pattern) {
    json_t *out = json_array();
    pm_list found;
    char *slash, *head, *tail, *mapped, *joined;
    size_t i;

    if (!pattern) pattern = "";
    found.items = NULL;
    found.count = found.cap = 0;

    if (!table || table->type != JSON_OBJECT || table->c.count == 0 || !pm_has_glob(pattern)) {
        mapped = pm_map(table, pattern);
        json_append(out, json_string(mapped ? mapped : ""));
        free(mapped);
        return out;
    }

    if (!pm_list_add(&found, pattern)) { pm_list_free(&found); return out; }

    slash = strrchr(pattern, '/');
    if (slash) {
        head = (char *)malloc((size_t)(slash - pattern) + 1);
        if (!head) { pm_list_free(&found); return out; }
        memcpy(head, pattern, (size_t)(slash - pattern));
        head[slash - pattern] = '\0';
        tail = pm_dup(slash + 1);
    } else {
        head = pm_dup("");
        tail = pm_dup(pattern);
    }
    if (head && tail && head[0] && !pm_has_glob(head)) {
        char *mapped_head = pm_map(table, head);
        if (mapped_head) {
            joined = pm_cat3(mapped_head, "/", tail);
            if (joined) {
                pm_list_add(&found, joined);
                free(joined);
            }
            free(mapped_head);
        }
    }
    free(head);
    free(tail);

    for (i = 0; i < table->c.count; i++) {
        const char *old = table->c.keys[i];
        char *candidate, *mapped_candidate, *new_head;
        const char *cand_last, *new_last;
        size_t k;
        if (!old) continue;
        /* old.replace(".", "/") + ".py" */
        candidate = pm_dup(old);
        if (!candidate) continue;
        for (k = 0; candidate[k]; k++) {
            if (candidate[k] == '.') candidate[k] = '/';
        }
        candidate = (char *)realloc(candidate, strlen(candidate) + 4);
        if (!candidate) continue;
        strcat(candidate, ".py");
        if (!pm_fnmatch(candidate, pattern)) { free(candidate); continue; }
        mapped_candidate = pm_map(table, candidate);
        if (!mapped_candidate) { free(candidate); continue; }
        cand_last = pm_last_segment(candidate);
        new_last = pm_last_segment(mapped_candidate);
        if (!strcmp(cand_last, new_last)) {
            size_t hlen = (size_t)(new_last - mapped_candidate);
            if (hlen > 0) {
                new_head = (char *)malloc(hlen + 1);
                if (new_head) {
                    memcpy(new_head, mapped_candidate, hlen - 1);
                    new_head[hlen - 1] = '\0';
                    joined = pm_cat3(new_head, "/", slash ? slash + 1 : pattern);
                    if (joined) {
                        pm_list_add(&found, joined);
                        free(joined);
                    }
                    free(new_head);
                }
            } else {
                joined = pm_cat3("", "/", slash ? slash + 1 : pattern);
                if (joined) {
                    pm_list_add(&found, joined);
                    free(joined);
                }
            }
        } else {
            pm_list_add(&found, mapped_candidate);
        }
        free(mapped_candidate);
        free(candidate);
    }

    /* the input pattern first, then the rest sorted */
    if (found.count > 1) {
        qsort(found.items + 1, found.count - 1, sizeof(char *), pm_cmp_strings);
    }
    for (i = 0; i < found.count; i++) {
        json_append(out, json_string(found.items[i]));
    }
    pm_list_free(&found);
    return out;
}

/* --- public wrappers ----------------------------------------------------- */

json_t *w22_paths_5ce33e_directory(const json_t *table, const char *path) {
    char *got = pm_directory(table, path);
    json_t *out = json_string(got ? got : "");
    free(got);
    return out;
}

json_t *w22_paths_5ce33e_current(const json_t *table, const char *path) {
    char *got = pm_map(table, path);
    json_t *out = json_string(got ? got : "");
    free(got);
    return out;
}

json_t *w22_paths_5ce33e_original(const json_t *table, const char *path) {
    json_t *inv = pm_inverse(table);
    char *got = pm_undo(inv, path, pm_map);
    json_t *out = json_string(got ? got : "");
    free(got);
    json_free(inv);
    return out;
}

json_t *w22_paths_5ce33e_module(const json_t *table, const char *name) {
    int found = 0;
    char *mapped = pm_current_name(table, name, &found);
    char *got = found && mapped ? mapped : pm_dup(name);
    json_t *out = json_string(got ? got : "");
    if (mapped && !found) free(mapped);
    free(got);
    return out;
}

json_t *w22_paths_5ce33e_original_module(const json_t *table, const char *name) {
    json_t *inv = pm_inverse(table);
    char *got = pm_undo_module(inv, name);
    json_t *out = json_string(got ? got : "");
    free(got);
    json_free(inv);
    return out;
}
