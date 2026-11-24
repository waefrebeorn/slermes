/*
 * skills_guard_credentials.c -- real C11 port of the skill-only credential
 * rules of upstream Hermes tools/skills_guard_credentials.py.
 *
 * PoP: w21_skillsguar_8a9cfb_pattern_source @ tools/skills_guard_credentials.py:_closed
 * PoP: w21_skillsguar_8a9cfb_dotenv_match @ tools/skills_guard_credentials.py:SKILL_ONLY_PATTERNS
 * PoP: w21_skillsguar_8a9cfb_env_read_names @ tools/skills_guard_credentials.py:_ENV_READ_NAME_RE
 * PoP: w21_skillsguar_8a9cfb_raise_reads @ tools/skills_guard_credentials.py:raise_provider_key_reads
 *
 * The two patterns are reproduced as matchers with the regex's own semantics:
 * the dotenv span is the `_open_prefix(4)` language (any position inside the
 * call's parentheses, at most 4 levels deep, no crossing a newline), and the
 * env-read name is the first argument of getenv / environ.get / environ[.
 * `model_provider_key_names` is the config seam and is passed in.
 */
#include "hermes_json.h"
#include "noc_w21_skillsguar_8a9cfb.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python's `\s` (str patterns, ASCII text). */
static int sg_space(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static int sg_word(int ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') || ch == '_';
}

/* --- regex sources: _closed / _open_prefix -------------------------------- */

typedef struct { char *buf; size_t len, cap; } sg_buf;

static void sg_put(sg_buf *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = (b->cap ? b->cap : 128);
        while (cap < b->len + n + 1) cap *= 2;
        {
            char *nb = (char *)realloc(b->buf, cap);
            if (!nb) return;
            b->buf = nb;
            b->cap = cap;
        }
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void sg_closed_into(sg_buf *b, int depth) {
    if (depth <= 1) {
        sg_put(b, "\\([^()\\n]*\\)");
        return;
    }
    sg_put(b, "\\((?:[^()\\n]|");
    sg_closed_into(b, depth - 1);
    sg_put(b, ")*\\)");
}

static void sg_open_prefix_into(sg_buf *b, int depth) {
    if (depth <= 1) {
        sg_put(b, "[^()\\n]*");
        return;
    }
    sg_put(b, "(?:[^()\\n]|");
    sg_closed_into(b, depth - 1);
    sg_put(b, ")*(?:\\(");
    sg_open_prefix_into(b, depth - 1);
    sg_put(b, ")?");
}

json_t *w21_skillsguar_8a9cfb_pattern_source(int depth) {
    json_t *out = json_object();
    sg_buf closed = {NULL, 0, 0}, prefix = {NULL, 0, 0};
    if (depth < 1) depth = 1;
    if (depth > 8) depth = 8;
    sg_closed_into(&closed, depth);
    sg_open_prefix_into(&prefix, depth);
    json_set(out, "closed", json_string(closed.buf ? closed.buf : ""));
    json_set(out, "open_prefix", json_string(prefix.buf ? prefix.buf : ""));
    free(closed.buf);
    free(prefix.buf);
    return out;
}

/* --- SKILL_ONLY_PATTERNS[0]: the Hermes .env load ------------------------- */

/* (?:HERMES_HOME|get_hermes_home|[~/\\"']\.hermes(?:[/\\"']|$)) */
static int sg_name_alt(const char *p) {
    if (!strncmp(p, "HERMES_HOME", 11)) return 1;
    if (!strncmp(p, "get_hermes_home", 15)) return 1;
    if (*p && strchr("~/\\\"'", *p) && !strncmp(p + 1, ".hermes", 7)) {
        const char *q = p + 8;
        if (!*q) return 1;                                  /* $ */
        if (!strcmp(q, "\n")) return 1;                     /* $ before a final newline */
        if (strchr("/\\\"'", *q)) return 1;
    }
    return 0;
}

/* The `_open_prefix(4)` span: positions inside the call, at most 4 deep. */
static int sg_scan_span(const char *p) {
    int depth = 1;
    for (;;) {
        if (sg_name_alt(p)) return 1;
        if (*p == '\0' || *p == '\n') return 0;              /* [^()\n] cannot cross */
        if (*p == ')') {
            depth--;
            if (depth == 0) return 0;                        /* the call's group closed */
        } else if (*p == '(') {
            depth++;
            if (depth > 4) return 0;                         /* beyond _open_prefix(4) */
        }
        p++;
    }
}

int w21_skillsguar_8a9cfb_dotenv_match(const char *view) {
    const char *p;
    if (!view) return 0;
    for (p = view; *p; p++) {
        const char *q;
        int boundary = (p == view) || !sg_word((unsigned char)p[-1]);
        if (!boundary) continue;                             /* \b before the call name */
        if (!strncmp(p, "load_dotenv", 11)) q = p + 11;
        else if (!strncmp(p, "dotenv_values", 13)) q = p + 13;
        else continue;
        while (*q && sg_space((unsigned char)*q)) q++;        /* \s* */
        if (*q != '(') continue;
        if (sg_scan_span(q + 1)) return 1;
    }
    return 0;
}

/* --- _ENV_READ_NAME_RE ---------------------------------------------------- */

/*
 * (?:getenv|environ\s*\.\s*get|environ\s*\[)\s*\(?\s*["']([A-Za-z_]\w*)["']
 *
 * The group is the maximal \w run that starts with [A-Za-z_]: a shorter prefix
 * could only be followed by a quote if the run itself contained one, which \w
 * never does.
 */
static const char *sg_try_read(const char *p, const char **name, size_t *nlen) {
    const char *q = NULL;
    const char *start;
    if (!strncmp(p, "getenv", 6)) {
        q = p + 6;
    } else if (!strncmp(p, "environ", 7)) {
        const char *r = p + 7;
        while (*r && sg_space((unsigned char)*r)) r++;
        if (*r == '.') {
            r++;
            while (*r && sg_space((unsigned char)*r)) r++;
            if (!strncmp(r, "get", 3)) q = r + 3;
        } else if (*r == '[') {
            q = r + 1;
        }
    }
    if (!q) return NULL;
    while (*q && sg_space((unsigned char)*q)) q++;
    if (*q == '(') {
        q++;
        while (*q && sg_space((unsigned char)*q)) q++;
    }
    if (*q != '"' && *q != '\'') return NULL;
    q++;
    if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') || *q == '_')) return NULL;
    start = q;
    q++;
    while (sg_word((unsigned char)*q)) q++;
    if (*q != '"' && *q != '\'') return NULL;
    *name = start;
    *nlen = (size_t)(q - start);
    return q + 1;
}

json_t *w21_skillsguar_8a9cfb_env_read_names(const char *line) {
    json_t *arr = json_array();
    const char *p = line;
    if (!line) return arr;
    while (*p) {
        const char *name = NULL, *end;
        size_t nlen = 0;
        end = sg_try_read(p, &name, &nlen);
        if (end) {
            char *copy = (char *)malloc(nlen + 1);
            if (copy) {
                memcpy(copy, name, nlen);
                copy[nlen] = '\0';
                json_append(arr, json_string(copy));
                free(copy);
            }
            p = end;
        } else {
            p++;
        }
    }
    return arr;
}

/* --- raise_provider_key_reads -------------------------------------------- */

static int sg_env_read_id(const char *pid) {
    return pid && (!strcmp(pid, "python_getenv_secret") ||
                   !strcmp(pid, "python_environ_get_secret"));
}

static int sg_provider(const json_t *providers, const char *name, size_t nlen) {
    size_t i;
    if (!providers || providers->type != JSON_ARRAY) return 0;
    for (i = 0; i < providers->c.count; i++) {
        const json_t *one = providers->c.items[i];
        size_t k;
        if (!one || one->type != JSON_STRING || !one->str_val) continue;
        if (strlen(one->str_val) != nlen) continue;
        for (k = 0; k < nlen; k++) {
            if ((unsigned char)one->str_val[k] != (unsigned char)toupper((unsigned char)name[k])) break;
        }
        if (k == nlen) return 1;
    }
    return 0;
}

/* `in_text[f.line - 1][m.start()]` -- absent mask means "the whole line counts". */
static int sg_in_text_at(const json_t *in_text, size_t row, size_t col) {
    const json_t *mask;
    if (!in_text || in_text->type == JSON_NULL) return 0;
    if (in_text->type != JSON_ARRAY || row >= in_text->c.count) return 0;
    mask = in_text->c.items[row];
    if (!mask || mask->type != JSON_STRING || !mask->str_val) return 0;
    if (col >= strlen(mask->str_val)) return 0;
    return mask->str_val[col] == '1';
}

json_t *w21_skillsguar_8a9cfb_raise_reads(const json_t *findings, const json_t *lines,
                                          const json_t *providers, const json_t *in_text) {
    json_t *out = json_array();
    size_t i;

    if (!findings || findings->type != JSON_ARRAY) return out;
    for (i = 0; i < findings->c.count; i++) {
        const json_t *f = findings->c.items[i];
        const char *pid;
        size_t row;
        const json_t *line_node;
        const char *line;
        char *named = NULL;
        size_t named_len = 0;

        if (!f || f->type != JSON_OBJECT) continue;
        pid = json_get_str(f, "pattern_id", "");
        if (!sg_env_read_id(pid)) { json_append(out, json_copy((json_t *)f)); continue; }

        row = (size_t)(json_get_num(f, "line", 0) - 1);
        line_node = (lines && lines->type == JSON_ARRAY && row < lines->c.count)
                        ? lines->c.items[row] : NULL;
        line = (line_node && line_node->type == JSON_STRING) ? line_node->str_val : "";

        {
            const char *p = line;
            while (*p) {
                const char *name = NULL, *end;
                size_t nlen = 0;
                end = sg_try_read(p, &name, &nlen);
                if (!end) { p++; continue; }
                if (sg_provider(providers, name, nlen) &&
                    !sg_in_text_at(in_text, row, (size_t)(p - line))) {
                    named = (char *)malloc(nlen + 1);
                    if (named) {
                        memcpy(named, name, nlen);
                        named[nlen] = '\0';
                        named_len = nlen;
                    }
                    break;
                }
                p = end;
            }
        }
        if (named) {
            json_t *copy = json_object();
            char *desc;
            size_t n = strlen(named) + 160;
            json_set(copy, "pattern_id", json_string("provider_key_env_read"));
            json_set(copy, "line", json_number(json_get_num(f, "line", 0)));
            json_set(copy, "severity", json_string("high"));
            desc = (char *)malloc(n);
            if (desc) {
                snprintf(desc, n,
                         "reads Hermes model-provider key %s; skill scripts never receive it, "
                         "and the skill must not fetch it another way", named);
                json_set(copy, "description", json_string(desc));
                free(desc);
            }
            json_append(out, copy);
            free(named);
            (void)named_len;
        } else {
            json_append(out, json_copy((json_t *)f));
        }
    }
    return out;
}
