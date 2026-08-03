/*
 * yaml.c — Standalone YAML config parser for C.
 * Parses key:value, nested indent, lists, comments. Zero external deps.
 * Never calls exit(). Returns NULL on OOM.
 * MIT License — WuBu Hermes Project
 */

#include "yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Internal helpers — graceful OOM
 * ================================================================ */

static bool oom_flag = false;

static void *xmalloc(size_t sz) {
    if (oom_flag) return NULL;
    void *p = malloc(sz ? sz : 1);
    if (!p) { oom_flag = true; return NULL; }
    return p;
}

static void *xrealloc(void *p, size_t sz) {
    if (oom_flag) return NULL;
    void *r = realloc(p, sz ? sz : 1);
    if (!r) { oom_flag = true; return NULL; }
    return r;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)xmalloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

/* ================================================================
 *  Internal tree
 * ================================================================ */

typedef enum { YVAL_STRING, YVAL_LIST, YVAL_MAP } yaml_val_type_t;

typedef struct yaml_entry {
    char *key;
    yaml_val_type_t type;
    char *str_val;
    struct yaml_entry **items;
    size_t item_count;
    struct yaml_entry **children;
    size_t child_count;
} yaml_entry_t;

struct yaml_doc {
    yaml_entry_t *root;
};

/* ================================================================
 *  Lexer
 * ================================================================ */

typedef struct {
    const char *text;
    size_t len;
    size_t pos;
} yaml_lexer_t;

typedef struct {
    char   *line;
    char   *raw;
    int     indent;
    size_t  lineno;
} yaml_line_t;

static void lex_init(yaml_lexer_t *lx, const char *text) {
    lx->text = text;
    lx->len = strlen(text);
    lx->pos = 0;
}

static bool lex_next(yaml_lexer_t *lx, yaml_line_t *line) {
    memset(line, 0, sizeof(*line));
    if (lx->pos >= lx->len) return false;

    const char *start = lx->text + lx->pos;
    const char *end = start;
    while (lx->pos < lx->len && lx->text[lx->pos] != '\n') {
        lx->pos++;
        end = lx->text + lx->pos;
    }
    if (lx->pos < lx->len) lx->pos++;

    size_t raw_len = (size_t)(end - start);
    line->raw = (char *)xmalloc(raw_len + 1);
    if (!line->raw) return false;
    memcpy(line->raw, start, raw_len);
    line->raw[raw_len] = '\0';

    line->indent = 0;
    while (line->raw[line->indent] == ' ') line->indent++;

    const char *p = line->raw + line->indent;
    const char *comment = NULL;
    /* Scan for a comment '#', but NOT inside quoted strings — YAML values
     * like `background: "#0b0e1a"` must keep their hex colors. Per YAML,
     * '#' starts a comment at line start or after whitespace. */
    {
        char quote = '\0';
        for (const char *q = p; *q; q++) {
            if (quote) {
                if (*q == quote) quote = '\0';
                continue;
            }
            if (*q == '"' || *q == '\'') { quote = *q; continue; }
            if (*q == '#' && (q == p || *(q-1) == ' ' || *(q-1) == '\t')) {
                comment = q;
                break;
            }
        }
    }
    size_t content_end = comment ? (size_t)(comment - line->raw) : raw_len;
    while (content_end > 0 && (line->raw[content_end-1] == ' ' || line->raw[content_end-1] == '\t'))
        content_end--;

    size_t clen = content_end - (size_t)line->indent;
    line->line = (char *)xmalloc(clen + 1);
    if (!line->line) return false;
    memcpy(line->line, p, clen);
    line->line[clen] = '\0';
    return true;
}

static void line_free(yaml_line_t *line) {
    free(line->raw);
    free(line->line);
}

/* ================================================================
 *  Parser — same logic as original hermes_yaml
 * ================================================================ */

static yaml_entry_t *new_entry(const char *key) {
    yaml_entry_t *e = (yaml_entry_t *)xmalloc(sizeof(yaml_entry_t));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->key = key ? xstrdup(key) : NULL;
    return e;
}

static void free_entry(yaml_entry_t *e) {
    if (!e) return;
    free(e->key);
    free(e->str_val);
    for (size_t i = 0; i < e->item_count; i++) free_entry(e->items[i]);
    free(e->items);
    for (size_t i = 0; i < e->child_count; i++) free_entry(e->children[i]);
    free(e->children);
    free(e);
}

static yaml_entry_t *parse_map(yaml_line_t *lines, size_t count,
                                int parent_indent, size_t *consumed, char **err);

static yaml_entry_t *parse_value(const char *val_str) {
    yaml_entry_t *e = new_entry(NULL);
    if (!e) return NULL;
    e->type = YVAL_STRING;
    e->str_val = xstrdup(val_str ? val_str : "");
    return e;
}

static void parse_inline(yaml_entry_t *e, const char *val) {
    while (*val == ' ') val++;
    if (*val == '\0') {
        e->type = YVAL_STRING;
        e->str_val = xstrdup("");
        return;
    }
    if (val[0] == '-' && (val[1] == ' ' || val[1] == '\0')) {
        e->type = YVAL_LIST;
        val += 1;
        while (*val == ' ') val++;
        yaml_entry_t *item = parse_value(val);
        e->items = (yaml_entry_t **)xmalloc(sizeof(yaml_entry_t *));
        if (e->items) { e->items[0] = item; e->item_count = 1; }
        return;
    }
    e->type = YVAL_STRING;
    /* Strip matching surrounding quotes (YAML flow scalars).
     * Double-quoted: process \n \t \" \\ escapes. Single-quoted: literal,
     * with '' -> '. Unquoted: stored verbatim (line already right-trimmed). */
    size_t vlen = strlen(val);
    if (vlen >= 2 && val[0] == '"' && val[vlen-1] == '"') {
        char *buf = (char *)xmalloc(vlen);  /* <= vlen-1 chars + NUL */
        if (!buf) { e->str_val = xstrdup(val); return; }
        size_t bi = 0;
        for (size_t k = 1; k < vlen - 1; k++) {
            if (val[k] == '\\' && k + 1 < vlen - 1) {
                char c = val[++k];
                switch (c) {
                    case 'n':  buf[bi++] = '\n'; break;
                    case 't':  buf[bi++] = '\t'; break;
                    case 'r':  buf[bi++] = '\r'; break;
                    case '"':  buf[bi++] = '"';  break;
                    case '\\': buf[bi++] = '\\'; break;
                    case '0':  buf[bi++] = '\0'; break;
                    default:   buf[bi++] = c;    break;
                }
            } else {
                buf[bi++] = val[k];
            }
        }
        buf[bi] = '\0';
        e->str_val = buf;
        return;
    }
    if (vlen >= 2 && val[0] == '\'' && val[vlen-1] == '\'') {
        char *buf = (char *)xmalloc(vlen);
        if (!buf) { e->str_val = xstrdup(val); return; }
        size_t bi = 0;
        for (size_t k = 1; k < vlen - 1; k++) {
            if (val[k] == '\'' && k + 1 < vlen - 1 && val[k+1] == '\'') {
                buf[bi++] = '\'';
                k++;
            } else {
                buf[bi++] = val[k];
            }
        }
        buf[bi] = '\0';
        e->str_val = buf;
        return;
    }
    e->str_val = xstrdup(val);
}

static yaml_entry_t *parse_map(yaml_line_t *lines, size_t count,
                                int parent_indent, size_t *consumed, char **err)
{
    yaml_entry_t *map = new_entry(NULL);
    if (!map) return NULL;
    map->type = YVAL_MAP;
    *consumed = 0;

    size_t i = 0;
    bool saw_key = false;   /* saw a key: entry at this level */
    bool saw_seq = false;   /* saw a "- " item at this level (pure list) */
    while (i < count) {
        yaml_line_t *ln = &lines[i];
        if (ln->indent <= parent_indent) break;
        if (ln->line[0] == '\0') { i++; continue; }

        /* Document markers: PyYAML's yaml.parse accepts multi-document
         * streams ("---" / "..." separators). Skip them. */
        if (strcmp(ln->line, "---") == 0 || strcmp(ln->line, "...") == 0) {
            i++;
            continue;
        }

        char *colon = strchr(ln->line, ':');
        if (!colon) {
            if (ln->line[0] == '-' && (ln->line[1] == ' ' || ln->line[1] == '\0')) {
                /* A "- " sequence item at this level. PyYAML forbids mixing
                 * block-sequence items and block-mapping keys at the same
                 * indentation ("while parsing a block collection"). */
                if (saw_key) {
                    if (err) *err = xstrdup("ParserError: while parsing a block collection");
                    free_entry(map);
                    return NULL;
                }
                saw_seq = true;
                i++;
                continue;
            }
            if (err) *err = xstrdup("expected ':' in YAML line");
            free_entry(map);
            return NULL;
        }
        /* PyYAML forbids mixing block-sequence items and block-mapping keys
         * at the same indentation, in either order. */
        if (saw_seq) {
            if (err) *err = xstrdup("ParserError: while parsing a block collection");
            free_entry(map);
            return NULL;
        }
        saw_key = true;

        /* PyYAML rejects an empty mapping key (`: value`) with a
         * ParserError while parsing a block mapping. */
        {
            size_t klen = (size_t)(colon - ln->line);
            size_t k = 0;
            while (k < klen && ln->line[k] == ' ') k++;
            if (k == klen) {
                if (err) *err = xstrdup("ParserError: while parsing a block mapping");
                free_entry(map);
                return NULL;
            }
        }

        size_t key_len = (size_t)(colon - ln->line);
        char *key = (char *)xmalloc(key_len + 1);
        if (!key) { free_entry(map); return NULL; }
        memcpy(key, ln->line, key_len);
        key[key_len] = '\0';
        while (key_len > 0 && key[key_len-1] == ' ') key[--key_len] = '\0';

        yaml_entry_t *child = new_entry(key);
        free(key);
        if (!child) { free_entry(map); return NULL; }

        const char *val = colon + 1;
        /* PyYAML: only a BARE key (`a:` with nothing after the colon) may
         * open a nested block; any inline value token (even `""`) makes a
         * deeper-indented key line an error ("while parsing a block mapping"
         * / "mapping values are not allowed here"). */
        const char *val_scan = val;
        while (*val_scan == ' ') val_scan++;
        bool has_inline_value = (*val_scan != '\0');

        /* PyYAML rejects a plain (unquoted) scalar containing ": " —
         * "mapping values are not allowed here". Quoted/flow values keep
         * colons fine (e.g. `a: "b: c"`, `a: http://x`). A colon is only
         * illegal when followed by whitespace or EOL outside quotes/flow. */
        if (has_inline_value && *val_scan != '"' && *val_scan != '\'' &&
            *val_scan != '[' && *val_scan != '{') {
            bool flow = false;
            for (const char *q = val_scan; *q; q++) {
                if (*q == '"' || *q == '\'') { /* skip quoted span */
                    char quote = *q;
                    q++;
                    while (*q && *q != quote) q++;
                    if (!*q) break;
                    continue;
                }
                if (*q == '[' || *q == '{') flow = true;
                else if (*q == ']' || *q == '}') flow = false;
                else if (*q == ':' && (q[1] == ' ' || q[1] == '\0') && !flow) {
                    if (err) *err = xstrdup("ScannerError: mapping values are not allowed here");
                    free_entry(map);
                    return NULL;
                }
            }
        }
        parse_inline(child, val);

        if (child->type == YVAL_STRING) {
            size_t j = i + 1;
            if (j < count && lines[j].indent > ln->indent && lines[j].line[0] == '-') {
                child->type = YVAL_LIST;
                free(child->str_val);
                child->str_val = NULL;
                child->items = NULL;
                child->item_count = 0;

                const char *orig_val = colon + 1;
                while (*orig_val == ' ') orig_val++;
                if (*orig_val) {
                    child->items = (yaml_entry_t **)xmalloc(sizeof(yaml_entry_t *));
                    if (child->items) {
                        child->items[0] = parse_value(orig_val);
                        child->item_count = 1;
                    }
                }

                while (j < count && lines[j].indent > ln->indent) {
                    if (lines[j].line[0] == '-' && (lines[j].line[1] == ' ' || lines[j].line[1] == '\0')) {
                        int dash_indent = lines[j].indent;
                        const char *item_text = lines[j].line + 1;
                        while (*item_text == ' ') item_text++;
                        /* Column where item_text begins within the physical line
                         * (used as the synthetic indent for a mapping item). */
                        int item_col = dash_indent + (int)(item_text - (lines[j].line + 1)) + 1;

                        /* Does the item start a mapping ("key: value")? A colon
                         * followed by space or EOL marks a YAML key (so "http://x"
                         * is NOT treated as a key). */
                        bool item_is_map = false;
                        for (const char *q = item_text; *q; q++) {
                            if (*q == ':' && (q[1] == ' ' || q[1] == '\0')) { item_is_map = true; break; }
                        }
                        /* Continuation lines: deeper-indented than the dash line
                         * and before the next dash at this indent. */
                        size_t cont_start = j + 1, cont_end = cont_start;
                        while (cont_end < count && lines[cont_end].indent > dash_indent)
                            cont_end++;
                        if (cont_end > cont_start) item_is_map = true;

                        yaml_entry_t *item = NULL;
                        if (item_is_map && *item_text) {
                            /* Build a temp line array: synthetic first line for the
                             * inline "key: value" at item_col, then the real
                             * continuation lines. parse_map borrows .line pointers
                             * and never frees them, so shallow copies are safe. */
                            size_t nlines = 1 + (cont_end - cont_start);
                            yaml_line_t *tmp = (yaml_line_t *)xmalloc(nlines * sizeof(yaml_line_t));
                            if (tmp) {
                                tmp[0] = lines[j];
                                tmp[0].line = (char *)item_text;  /* borrowed */
                                tmp[0].indent = item_col;
                                for (size_t t = cont_start; t < cont_end; t++)
                                    tmp[1 + (t - cont_start)] = lines[t];
                                size_t used = 0;
                                item = parse_map(tmp, nlines, item_col - 1, &used, err);
                                free(tmp);
                            }
                        }
                        if (!item) item = parse_value(item_text);

                        child->items = (yaml_entry_t **)xrealloc(child->items,
                            (child->item_count + 1) * sizeof(yaml_entry_t *));
                        if (child->items)
                            child->items[child->item_count++] = item;
                        else if (item)
                            free_entry(item);
                        /* Skip past the continuation lines we just consumed. */
                        j = cont_end;
                        continue;
                    }
                    j++;
                }
                i = j - 1;
            } else {
                size_t k = i + 1;
                while (k < count && lines[k].line[0] == '\0') k++;
                if (k < count && lines[k].indent > ln->indent) {
                    /* PyYAML rejects an inline value followed by a
                     * deeper-indented key (`a: 1\n  b: 2`): "mapping values
                     * are not allowed here" / "while parsing a block mapping".
                     * Only a BARE key (no inline value) may open a nested
                     * block. Exceptions: unbalanced flow containers
                     * (`a: [1,\n  2]`) and block scalar indicators
                     * (`a: |\n  text`, `a: >\n  folded`) legitimately
                     * continue on deeper lines. */
                    bool flow_open = false;
                    bool block_ind = false;
                    const char *vs = val;
                    while (*vs == ' ') vs++;
                    if (*vs == '[' || *vs == '{') {
                        int depth = 0;
                        for (const char *q = vs; *q; q++) {
                            if (*q == '[' || *q == '{') depth++;
                            else if (*q == ']' || *q == '}') depth--;
                        }
                        flow_open = (depth > 0);
                    } else if (*vs == '|' || *vs == '>') {
                        /* block scalar indicator: |, |-, |+, >, >-, >+, |2, ... */
                        const char *q = vs + 1;
                        while (*q == '-' || *q == '+' || (*q >= '1' && *q <= '9')) q++;
                        while (*q == ' ') q++;
                        block_ind = (*q == '\0');
                    }
                    if (has_inline_value && !flow_open && !block_ind) {
                        if (err) *err = xstrdup("ScannerError: mapping values are not allowed here");
                        free_entry(map);
                        return NULL;
                    }
                    if (flow_open || block_ind) {
                        /* Consume the deeper-indented continuation lines as
                         * part of this value: flow content (`a: [1,\n  2]`)
                         * or literal/folded block scalar lines
                         * (`a: |\n  text`). PyYAML accepts both. */
                        while (k < count && lines[k].indent > ln->indent) k++;
                        i = k - 1;
                    } else {
                    free(child->str_val);
                    child->str_val = NULL;
                    child->type = YVAL_MAP;
                    size_t child_consumed = 0;
                    yaml_entry_t *submap = parse_map(&lines[k], count - k,
                                                       ln->indent, &child_consumed, err);
                    if (submap) {
                        child->children = submap->children;
                        child->child_count = submap->child_count;
                        free(submap->key);
                        free(submap);
                        i = k + child_consumed - 1;
                    }
                    }
                }
            }
        } else if (child->type == YVAL_MAP) {
            size_t child_consumed = 0;
            yaml_entry_t *submap = parse_map(&lines[i+1], count - i - 1,
                                               ln->indent, &child_consumed, err);
            if (submap) {
                child->children = submap->children;
                child->child_count = submap->child_count;
                free(submap->key);
                free(submap);
                i += child_consumed;
            }
        }

        map->children = (yaml_entry_t **)xrealloc(map->children,
            (map->child_count + 1) * sizeof(yaml_entry_t *));
        if (map->children)
            map->children[map->child_count++] = child;
        i++;
    }

    *consumed = i;
    return map;
}

/* ================================================================
 *  Public API
 * ================================================================ */

yaml_doc_t *yaml_parse(const char *input, char **error_msg) {
    oom_flag = false;
    if (!input) {
        if (error_msg) *error_msg = xstrdup("NULL input");
        return NULL;
    }

    yaml_lexer_t lx;
    lex_init(&lx, input);

    size_t cap = 64, count = 0;
    yaml_line_t *lines = (yaml_line_t *)xmalloc(cap * sizeof(yaml_line_t));
    if (!lines) return NULL;
    yaml_line_t ln;
    while (lex_next(&lx, &ln)) {
        if (count >= cap) {
            cap *= 2;
            yaml_line_t *nb = (yaml_line_t *)xrealloc(lines, cap * sizeof(yaml_line_t));
            if (!nb) { for (size_t i = 0; i < count; i++) line_free(&lines[i]); free(lines); return NULL; }
            lines = nb;
        }
        lines[count++] = ln;
    }

    yaml_doc_t *doc = (yaml_doc_t *)xmalloc(sizeof(yaml_doc_t));
    if (!doc) { for (size_t i = 0; i < count; i++) line_free(&lines[i]); free(lines); return NULL; }
    size_t consumed = 0;
    doc->root = parse_map(lines, count, -1, &consumed, error_msg);

    for (size_t i = 0; i < count; i++) line_free(&lines[i]);
    free(lines);

    if (!doc->root) { free(doc); return NULL; }
    return doc;
}

yaml_doc_t *yaml_parse_file(const char *path, char **error_msg) {
    oom_flag = false;
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (error_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "cannot open '%s'", path);
            *error_msg = xstrdup(buf);
        }
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = (char *)xmalloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    (void)n;
    fclose(f);
    buf[sz] = '\0';
    yaml_doc_t *doc = yaml_parse(buf, error_msg);
    free(buf);
    return doc;
}

/* ================================================================
 *  Path navigation
 * ================================================================ */

static yaml_entry_t *navigate(const yaml_doc_t *doc, const char *path) {
    if (!doc || !doc->root || !path) return NULL;
    yaml_entry_t *e = doc->root;

    char *path_copy = xstrdup(path);
    if (!path_copy) return NULL;
    char *saveptr;
    char *tok = strtok_r(path_copy, ".", &saveptr);

    while (tok && e) {
        if (e->type != YVAL_MAP) { e = NULL; break; }
        yaml_entry_t *found = NULL;
        for (size_t i = 0; i < e->child_count; i++) {
            if (strcmp(e->children[i]->key, tok) == 0) { found = e->children[i]; break; }
        }
        e = found;
        tok = strtok_r(NULL, ".", &saveptr);
    }

    free(path_copy);
    return e;
}

const char *yaml_get_string(const yaml_doc_t *doc, const char *path) {
    yaml_entry_t *e = navigate(doc, path);
    if (!e || e->type != YVAL_STRING) return NULL;
    return e->str_val;
}

bool yaml_get_bool(const yaml_doc_t *doc, const char *path, bool def) {
    const char *s = yaml_get_string(doc, path);
    if (!s) return def;
    if (strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "on") == 0) return true;
    if (strcmp(s, "false") == 0 || strcmp(s, "no") == 0 || strcmp(s, "off") == 0) return false;
    return def;
}

int yaml_get_int(const yaml_doc_t *doc, const char *path, int def) {
    const char *s = yaml_get_string(doc, path);
    if (!s) return def;
    return (int)strtol(s, NULL, 10);
}

size_t yaml_list_count(const yaml_doc_t *doc, const char *path) {
    yaml_entry_t *e = navigate(doc, path);
    if (!e || e->type != YVAL_LIST) return 0;
    return e->item_count;
}

const char *yaml_list_get(const yaml_doc_t *doc, const char *path, size_t index) {
    yaml_entry_t *e = navigate(doc, path);
    if (!e || e->type != YVAL_LIST || index >= e->item_count) return NULL;
    if (e->items[index]->type != YVAL_STRING) return NULL;
    return e->items[index]->str_val;
}

void yaml_iterate(const yaml_doc_t *doc,
                  void (*fn)(const char *key, const char *value, void *user),
                  void *user)
{
    if (!doc || !doc->root || !fn) return;
    if (doc->root->type != YVAL_MAP) return;
    for (size_t i = 0; i < doc->root->child_count; i++) {
        yaml_entry_t *child = doc->root->children[i];
        if (child->type == YVAL_STRING)
            fn(child->key, child->str_val, user);
    }
}

char **yaml_map_keys(const yaml_doc_t *doc, const char *path, size_t *count) {
    if (count) *count = 0;
    if (!doc || !path) return NULL;
    yaml_entry_t *e = navigate(doc, path);
    if (!e || e->type != YVAL_MAP) return NULL;

    size_t n = e->child_count;
    char **keys = (char **)malloc(n * sizeof(char *));
    if (!keys) return NULL;

    size_t out = 0;
    for (size_t i = 0; i < n; i++) {
        if (e->children[i]->key) {
            keys[out] = strdup(e->children[i]->key);
            if (keys[out]) out++;
        }
    }

    if (out == 0) { free(keys); return NULL; }
    if (count) *count = out;
    return keys;
}

/* ─── YAML-to-JSON serialization ────────────────────── */

/* JSON-escape a string: returns malloc'd string with escaped quotes/backslash/control chars */
static char *json_escape_str(const char *s) {
    if (!s) return strdup("");
    size_t cap = strlen(s) * 2 + 3;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t pos = 0;
    out[pos++] = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (pos + 6 >= cap) {
            cap = cap * 2 + 16;
            char *tmp = (char *)realloc(out, cap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
        }
        switch (c) {
            case '"':  out[pos++] = '\\'; out[pos++] = '"'; break;
            case '\\': out[pos++] = '\\'; out[pos++] = '\\'; break;
            case '\n': out[pos++] = '\\'; out[pos++] = 'n'; break;
            case '\t': out[pos++] = '\\'; out[pos++] = 't'; break;
            case '\r': out[pos++] = '\\'; out[pos++] = 'r'; break;
            default:
                if (c < 0x20) {
                    snprintf(&out[pos], cap - pos, "\\u%04x", c);
                    pos += strlen(&out[pos]);
                } else {
                    out[pos++] = c;
                }
                break;
        }
    }
    out[pos++] = '"';
    out[pos] = '\0';
    return out;
}

/* Recursively serialize a YAML entry to a JSON string (malloc'd). */
static char *yaml_entry_to_json(const yaml_entry_t *e) {
    if (!e) return strdup("null");

    switch (e->type) {
    case YVAL_STRING: {
        /* YAML scalar coercion (mirrors PyYAML's implicit resolver):
         * true/false/null map to JSON literals; canonical int/float forms
         * become JSON numbers; everything else stays a string. Without this
         * every `enabled: true` reached consumers as the STRING "true" and
         * every port/timeout as a string number. */
        const char *s = e->str_val ? e->str_val : "";
        if (strcmp(s, "true") == 0 || strcmp(s, "True") == 0)
            return strdup("true");
        if (strcmp(s, "false") == 0 || strcmp(s, "False") == 0)
            return strdup("false");
        if (strcmp(s, "null") == 0 || strcmp(s, "~") == 0 || s[0] == '\0')
            return strdup("null");
        /* number? canonical decimal int or float only (no leading zeros
         * except "0", optional sign, at most one dot, no trailing junk). */
        {
            const char *p = s;
            if (*p == '-' || *p == '+') p++;
            int digits = 0, dots = 0;
            const char *q = p;
            while (*q) {
                if (*q >= '0' && *q <= '9') digits++;
                else if (*q == '.' && dots == 0) dots++;
                else { digits = 0; break; }
                q++;
            }
            /* reject "007"-style (YAML treats as string in JSON contexts is
             * debatable, but Python int("007") == 7 — PyYAML resolves it as
             * int. Keep octal-looking values numeric-faithful to PyYAML. */
            if (digits > 0) {
                /* strip a leading '+' (JSON forbids it) */
                if (s[0] == '+') {
                    char *out = strdup(s + 1);
                    return out;
                }
                return strdup(s);
            }
        }
        return json_escape_str(s);
    }

    case YVAL_LIST: {
        size_t cap = 256;
        char *out = (char *)malloc(cap);
        if (!out) return NULL;
        size_t pos = 0;
        out[pos++] = '[';
        for (size_t i = 0; i < e->item_count; i++) {
            if (i > 0) out[pos++] = ',';
            char *item_str = yaml_entry_to_json(e->items[i]);
            if (item_str) {
                size_t slen = strlen(item_str);
                if (pos + slen + 4 >= cap) {
                    cap = cap + slen + 32;
                    char *tmp = (char *)realloc(out, cap);
                    if (!tmp) { free(out); free(item_str); return NULL; }
                    out = tmp;
                }
                memcpy(out + pos, item_str, slen);
                pos += slen;
                free(item_str);
            }
        }
        out[pos++] = ']';
        out[pos] = '\0';
        return out;
    }

    case YVAL_MAP: {
        size_t cap = 256;
        char *out = (char *)malloc(cap);
        if (!out) return NULL;
        size_t pos = 0;
        out[pos++] = '{';
        for (size_t i = 0; i < e->child_count; i++) {
            if (i > 0) out[pos++] = ',';
            char *key_str = json_escape_str(e->children[i]->key);
            char *val_str = yaml_entry_to_json(e->children[i]);
            size_t klen = key_str ? strlen(key_str) : 0;
            size_t vlen = val_str ? strlen(val_str) : 0;
            if (pos + klen + vlen + 4 >= cap) {
                cap = cap + klen + vlen + 32;
                char *tmp = (char *)realloc(out, cap);
                if (!tmp) { free(out); free(key_str); free(val_str); return NULL; }
                out = tmp;
            }
            if (key_str) { memcpy(out + pos, key_str, klen); pos += klen; }
            out[pos++] = ':';
            if (val_str) { memcpy(out + pos, val_str, vlen); pos += vlen; }
            free(key_str);
            free(val_str);
        }
        out[pos++] = '}';
        out[pos] = '\0';
        return out;
    }
    }
    return strdup("null");
}

char *yaml_to_json_string(const yaml_doc_t *doc, const char *path) {
    if (!doc || !path) return NULL;
    yaml_entry_t *e = navigate(doc, path);
    if (!e) return NULL;
    return yaml_entry_to_json(e);
}

/* ─── Free memory ──────────────────────────────────── */

void yaml_free(yaml_doc_t *doc) {
    if (!doc) return;
    if (doc->root) free_entry(doc->root);
    free(doc);
}

/* ─── Multi-document support ─────────────────────────── */

yaml_doc_t **yaml_parse_multi(const char *input, size_t *count, char **error_msg) {
    if (count) *count = 0;
    if (!input) {
        if (error_msg) *error_msg = strdup("NULL input");
        return NULL;
    }

    /* Count documents by splitting on --- */
    size_t cap = 8, n = 0;
    yaml_doc_t **docs = (yaml_doc_t **)malloc(cap * sizeof(yaml_doc_t *));
    if (!docs) { if (error_msg) *error_msg = strdup("OOM"); return NULL; }

    const char *p = input;
    const char *doc_start = input;

    /* Skip leading whitespace before first doc */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    doc_start = p;

    while (*p) {
        /* Look for \n---\n or \n---$ or ^--- (document separator) */
        if ((p == input || *(p-1) == '\n') && p[0] == '-' && p[1] == '-' && p[2] == '-'
            && (p[3] == '\n' || p[3] == '\r' || p[3] == '\0'
                || p[3] == ' ' || p[3] == '\t')) {
            /* Parse document from doc_start to p */
            size_t doc_len = (size_t)(p - doc_start);
            /* Trim trailing whitespace */
            while (doc_len > 0 && (doc_start[doc_len-1] == ' ' ||
                   doc_start[doc_len-1] == '\t' || doc_start[doc_len-1] == '\n' ||
                   doc_start[doc_len-1] == '\r'))
                doc_len--;

            if (doc_len > 0) {
                char *doc_text = (char *)malloc(doc_len + 1);
                if (doc_text) {
                    memcpy(doc_text, doc_start, doc_len);
                    doc_text[doc_len] = '\0';
                    yaml_doc_t *doc = yaml_parse(doc_text, NULL);
                    free(doc_text);
                    if (doc) {
                        if (n >= cap) {
                            cap *= 2;
                            yaml_doc_t **new_docs = (yaml_doc_t **)realloc(docs, cap * sizeof(yaml_doc_t *));
                            if (!new_docs) { free(docs); return NULL; }
                            docs = new_docs;
                        }
                        docs[n++] = doc;
                    }
                }
            }

            /* Skip past --- and trailing newline */
            p += 3;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\n') p++;
            else if (*p == '\r') { p++; if (*p == '\n') p++; }
            doc_start = p;
            continue;
        }
        p++;
    }

    /* Parse final document (after last --- or only doc) */
    const char *end = p;
    while (end > doc_start && (*(end-1) == ' ' || *(end-1) == '\t' ||
           *(end-1) == '\n' || *(end-1) == '\r'))
        end--;
    size_t final_len = (size_t)(end - doc_start);

    if (final_len > 0) {
        char *doc_text = (char *)malloc(final_len + 1);
        if (doc_text) {
            memcpy(doc_text, doc_start, final_len);
            doc_text[final_len] = '\0';
            yaml_doc_t *doc = yaml_parse(doc_text, NULL);
            free(doc_text);
            if (doc) {
                if (n >= cap) {
                    cap++;
                    yaml_doc_t **new_docs = (yaml_doc_t **)realloc(docs, cap * sizeof(yaml_doc_t *));
                    if (!new_docs) { free(docs); return NULL; }
                    docs = new_docs;
                }
                docs[n++] = doc;
            }
        }
    }

    if (n == 0) {
        free(docs);
        if (error_msg) *error_msg = strdup("No valid YAML documents found");
        return NULL;
    }

    if (count) *count = n;
    return docs;
}
