/*
 * skills_index_delta.c — C home for upstream Hermes agent/skills_index_delta.py.
 *
 * Faithful C11 ports of the pure skills-index codecs: index-row name parsing,
 * <available_skills> block extraction (DOTALL regex behaviour hand-coded), the
 * note renderer, and its inverse parser.  Set-derived results are emitted sorted.
 *
 * PoP: w10_skillsinde_bc8d57_row_name @ agent/skills_index_delta.py:_row_name
 * PoP: w10_skillsinde_bc8d57_index_entries @ agent/skills_index_delta.py:index_entries
 * PoP: w10_skillsinde_bc8d57_render @ agent/skills_index_delta.py:_render
 * PoP: w10_skillsinde_bc8d57_parse @ agent/skills_index_delta.py:_parse
 */
#include "noc_w10_skillsinde_bc8d57.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOTE_PREFIX "[System: The skills available in this conversation changed after its system prompt was built."
#define ADDED_HEAD  "Now available (load with skill_view(name) when relevant, like the skills listed in the system prompt):"
#define REMOVED_HEAD "No longer available:"
#define ACCURATE_AGAIN " The skills listed in the system prompt are accurate again; earlier skill-change notes are superseded."
#define ROW "    - "
#define BLOCK_OPEN "<available_skills>"
#define BLOCK_CLOSE "</available_skills>"

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char *dupn(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}
static char *py_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && is_ws(s[a])) a++;
    while (b > a && is_ws(s[b - 1])) b--;
    return dupn(s + a, b - a);
}
static char *py_lstrip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0;
    while (a < n && is_ws(s[a])) a++;
    return dupn(s + a, n - a);
}
static char *py_rstrip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n > 0 && is_ws(s[n - 1])) n--;
    return dupn(s, n);
}
static char *removesuffix_bracket(const char *s) {
    size_t n = strlen(s);
    if (n && s[n - 1] == ']') return dupn(s, n - 1);
    return dupn(s, n);
}

static char *row_name(const char *line)
{
    char *st = py_strip(line);
    if (!st) return NULL;
    size_t n = strlen(st);
    const char *p = st + (n >= 2 ? 2 : n);
    const char *q = strstr(p, ": ");
    char *first = q ? dupn(p, (size_t)(q - p)) : dupn(p, strlen(p));
    char *r = py_strip(first);
    free(first);
    free(st);
    return r;
}

typedef struct { char **v; size_t n, cap; } StrSet;
static void set_add(StrSet *s, char *name) {
    if (!name) return;
    for (size_t i = 0; i < s->n; i++) if (!strcmp(s->v[i], name)) { free(name); return; }
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->v = (char **)realloc(s->v, s->cap * sizeof(char *));
    }
    s->v[s->n++] = name;
}
static void set_free(StrSet *s) { for (size_t i = 0; i < s->n; i++) free(s->v[i]); free(s->v); }
static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static void sort_set(StrSet *s) { if (s->n > 1) qsort(s->v, s->n, sizeof(char *), cmp_str); }
static json_t *set_json(const StrSet *s) {
    json_t *arr = json_array();
    for (size_t i = 0; i < s->n; i++) json_append(arr, json_string(s->v[i]));
    return arr;
}

typedef struct { char *buf; size_t len, cap; } Sb;
static void sb_init(Sb *b) { b->buf = NULL; b->len = b->cap = 0; }
static void sb_addn(Sb *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2;
        b->buf = (char *)realloc(b->buf, b->cap);
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}
static void sb_add(Sb *b, const char *s) { sb_addn(b, s, strlen(s)); }

/* PoP: w10_skillsinde_bc8d57_row_name @ agent/skills_index_delta.py:_row_name */
json_t *w10_skillsinde_bc8d57_row_name(json_t *a)
{
    const char *line = json_get_str(a, "line", "");
    char *name = row_name(line);
    json_t *out = json_string(name ? name : "");
    free(name);
    return out;
}

/* PoP: w10_skillsinde_bc8d57_index_entries @ agent/skills_index_delta.py:index_entries */
json_t *w10_skillsinde_bc8d57_index_entries(json_t *a)
{
    json_t *out = json_object();
    const char *prompt = json_get_str(a, "prompt", "");
    if (!prompt) return out;
    const char *open = strstr(prompt, BLOCK_OPEN);
    if (!open) return out;
    const char *body = open + strlen(BLOCK_OPEN);
    if (*body == '\n') body++;
    const char *close = strstr(body, BLOCK_CLOSE);
    if (!close) return out;

    const char *p = body;
    while (p < close) {
        const char *e = p;
        while (e < close && *e != '\n' && *e != '\r') e++;
        char *line = dupn(p, (size_t)(e - p));
        if (!line) break;

        char *st = py_strip(line);
        int is_row = st && !strncmp(st, "- ", 2);
        int handled = 0;
        if (is_row) {
            char *name = row_name(line);
            if (name && *name) {
                if (json_obj_get(out, name) == NULL) {
                    char *rv = py_rstrip(line);
                    json_set(out, name, json_string(rv ? rv : ""));
                    free(rv);
                }
                handled = 1;
            }
            free(name);
        }
        if (!handled) {
            const char *m = strstr(line, "[names only]:");
            if (m) {
                const char *q = m + strlen("[names only]:");
                while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r' || *q == '\f' || *q == '\v') q++;
                if (*q) {
                    char *rest = dupn(q, strlen(q));
                    char *tok = strtok(rest, ",");
                    while (tok) {
                        char *nm = py_strip(tok);
                        if (nm && *nm && json_obj_get(out, nm) == NULL)
                            json_set(out, nm, json_string(""));
                        free(nm);
                        tok = strtok(NULL, ",");
                    }
                    free(rest);
                }
            }
        }
        free(st);
        free(line);
        if (e >= close) break;
        if (*e == '\r' && e + 1 < close && e[1] == '\n') e += 2;
        else e++;
        p = e;
    }
    return out;
}

/* PoP: w10_skillsinde_bc8d57_render @ agent/skills_index_delta.py:_render */
json_t *w10_skillsinde_bc8d57_render(json_t *a)
{
    json_t *added = a ? json_obj_get(a, "added") : NULL;
    json_t *removed = a ? json_obj_get(a, "removed") : NULL;
    size_t na = json_is_object(added) ? json_object_size(added) : 0;
    size_t nr = json_is_array(removed) ? json_len(removed) : 0;
    Sb b; sb_init(&b);
    sb_add(&b, NOTE_PREFIX);
    if (!na && !nr) {
        sb_add(&b, ACCURATE_AGAIN);
        sb_add(&b, "]");
        json_t *out = json_string(b.buf ? b.buf : "");
        free(b.buf);
        return out;
    }
    if (na) {
        StrSet keys = {0};
        for (size_t i = 0; i < json_object_size(added); i++) {
            const char *k = json_object_get_key_at(added, i);
            if (k) set_add(&keys, dupn(k, strlen(k)));
        }
        sort_set(&keys);
        sb_add(&b, "\n");
        sb_add(&b, ADDED_HEAD);
        for (size_t i = 0; i < keys.n; i++) {
            json_t *rowv = json_obj_get(added, keys.v[i]);
            const char *rs = json_is_string(rowv) ? json_string_value(rowv) : "";
            char *l = py_lstrip(rs);
            char *body;
            if (l && !*l) {
                size_t nn = strlen(keys.v[i]) + 3;
                body = (char *)malloc(nn);
                snprintf(body, nn, "- %s", keys.v[i]);
            } else {
                body = l ? dupn(l, strlen(l)) : dupn("", 0);
            }
            sb_add(&b, "\n");
            sb_add(&b, ROW);
            sb_add(&b, body + (strlen(body) >= 2 ? 2 : strlen(body)));
            free(body);
            free(l);
        }
        set_free(&keys);
    }
    if (nr) {
        StrSet names = {0};
        for (size_t i = 0; i < json_len(removed); i++) {
            json_t *v = json_get(removed, i);
            if (json_is_string(v)) set_add(&names, dupn(json_string_value(v), strlen(json_string_value(v))));
        }
        sort_set(&names);
        sb_add(&b, "\n");
        sb_add(&b, REMOVED_HEAD);
        sb_add(&b, " ");
        for (size_t i = 0; i < names.n; i++) {
            if (i) { sb_add(&b, ","); sb_add(&b, " "); }
            sb_add(&b, names.v[i]);
        }
        set_free(&names);
    }
    sb_add(&b, "]");
    json_t *out = json_string(b.buf ? b.buf : "");
    free(b.buf);
    return out;
}

/* PoP: w10_skillsinde_bc8d57_parse @ agent/skills_index_delta.py:_parse */
json_t *w10_skillsinde_bc8d57_parse(json_t *a)
{
    const char *note = json_get_str(a, "note", "");
    StrSet added = {0}, removed = {0};
    json_t *out = json_object();
    if (!note) note = "";

    char *copy = dupn(note, strlen(note));
    size_t nlines = 0;
    for (char *q = copy; ; q++) { nlines++; char *nl = strchr(q, '\n'); if (!nl) break; q = nl; }
    char **lines = (char **)calloc(nlines, sizeof(char *));
    size_t li = 0;
    for (char *q = copy; li < nlines; ) {
        char *nl = strchr(q, '\n');
        if (nl) { *nl = '\0'; lines[li++] = q; q = nl + 1; }
        else { lines[li++] = q; break; }
    }

    if (nlines >= 2 && !strcmp(lines[1], ADDED_HEAD)) {
        for (size_t i = 2; i < nlines; i++) {
            if (strncmp(lines[i], ROW, strlen(ROW))) break;
            char *t = removesuffix_bracket(lines[i]);
            char *nm = row_name(t);
            if (nm && *nm) set_add(&added, nm); else free(nm);
            free(t);
        }
    }
    for (size_t i = 1; i < nlines; i++) {
        if (!strncmp(lines[i], REMOVED_HEAD, strlen(REMOVED_HEAD))) {
            char *t = removesuffix_bracket(lines[i] + strlen(REMOVED_HEAD));
            char *tok = strtok(t, ",");
            while (tok) {
                char *nm = py_strip(tok);
                if (nm && *nm) set_add(&removed, nm); else free(nm);
                tok = strtok(NULL, ",");
            }
            free(t);
            break;
        }
    }
    sort_set(&added);
    sort_set(&removed);
    json_set(out, "added", set_json(&added));
    json_set(out, "removed", set_json(&removed));
    set_free(&added);
    set_free(&removed);
    free(lines);
    free(copy);
    return out;
}
