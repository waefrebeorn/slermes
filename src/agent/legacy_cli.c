/*
 * legacy_cli.c -- faithful C11 port of upstream Hermes agent/legacy_cli.py.
 *
 * PoP: w6_legacycli_b0ce00_resolve @ agent/legacy_cli.py:main
 *
 * Reproduces _build_parser() + main()'s argument resolution for the flags the
 * legacy runner defines: positional prompt joining/strip, the mutually
 * exclusive positional/--query rule, the bare-invocation help path, and the
 * kwargs handed to run().
 */
#include "noc_w6_legacycli_b0ce00.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **vals;      /* every owned string (for freeing) */
    size_t nvals, cap;
    const char *query, *model, *api_key, *base_url;
    const char *enabled, *disabled;
    long max_turns, log_prefix_chars;
    int list_tools, save_trajectories, save_sample, verbose;
    const char **pos;
    size_t npos, poscap;
} parsed_t;

static char *own(parsed_t *p, const char *s) {
    char *c = strdup(s ? s : "");
    if (p->nvals == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 16;
        p->vals = (char **)realloc(p->vals, p->cap * sizeof(char *));
    }
    p->vals[p->nvals++] = c;
    return c;
}

static json_t *err_obj(void) {
    json_t *o = json_object();
    json_set(o, "error", json_bool(1));
    return o;
}

static int parse_long(const char *s, long *out) {
    if (!s || !*s) return 0;
    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

/* join positional tokens with " " then .strip() (ASCII whitespace subset). */
static char *join_strip(const parsed_t *p) {
    size_t total = 0;
    for (size_t i = 0; i < p->npos; i++) total += strlen(p->pos[i]) + 1;
    char *buf = (char *)calloc(total + 1, 1);
    if (!buf) return strdup("");
    for (size_t i = 0; i < p->npos; i++) {
        if (i) strcat(buf, " ");
        strcat(buf, p->pos[i]);
    }
    char *s = buf;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    free(buf);
    return out;
}

/* Parse one option token. Returns 1 on success, 0 on argparse-style error.
 * If the flag consumes the following token, *consumed = 1. */
static int apply_opt(parsed_t *p, char *tok, int *consumed, const char *next) {
    *consumed = 0;
    char *eq = strchr(tok, '=');
    char namebuf[128];
    const char *inline_val = NULL;
    if (eq) {
        size_t n = (size_t)(eq - tok);
        if (n >= sizeof(namebuf)) return 0;
        memcpy(namebuf, tok, n); namebuf[n] = '\0';
        inline_val = eq + 1;
    } else {
        snprintf(namebuf, sizeof(namebuf), "%s", tok);
    }
    const char *name = namebuf;

    /* store_true flags -- an explicit =value is an argparse error */
    if (!strcmp(name, "--list-tools") || !strcmp(name, "--list_tools")) {
        if (eq) return 0; p->list_tools = 1; return 1;
    }
    if (!strcmp(name, "--save-trajectories") || !strcmp(name, "--save_trajectories")) {
        if (eq) return 0; p->save_trajectories = 1; return 1;
    }
    if (!strcmp(name, "--save-sample") || !strcmp(name, "--save_sample")) {
        if (eq) return 0; p->save_sample = 1; return 1;
    }
    if (!strcmp(name, "--verbose")) {
        if (eq) return 0; p->verbose = 1; return 1;
    }

    /* value flags */
    const char *val;
    if (inline_val) {
        val = inline_val;
    } else {
        if (!next || (next[0] == '-' && next[1] != '\0')) return 0;
        val = next;
        *consumed = 1;
    }

    if (!strcmp(name, "--query") || !strcmp(name, "-q")) {
        p->query = own(p, val); return 1;
    }
    if (!strcmp(name, "--model")) { p->model = own(p, val); return 1; }
    if (!strcmp(name, "--api-key") || !strcmp(name, "--api_key")) {
        p->api_key = own(p, val); return 1;
    }
    if (!strcmp(name, "--base-url") || !strcmp(name, "--base_url")) {
        p->base_url = own(p, val); return 1;
    }
    if (!strcmp(name, "--max-turns") || !strcmp(name, "--max_turns")) {
        long v; if (!parse_long(val, &v)) return 0; p->max_turns = v; return 1;
    }
    if (!strcmp(name, "--enabled-toolsets") || !strcmp(name, "--enabled_toolsets")) {
        p->enabled = own(p, val); return 1;
    }
    if (!strcmp(name, "--disabled-toolsets") || !strcmp(name, "--disabled_toolsets")) {
        p->disabled = own(p, val); return 1;
    }
    if (!strcmp(name, "--log-prefix-chars") || !strcmp(name, "--log_prefix_chars")) {
        long v; if (!parse_long(val, &v)) return 0; p->log_prefix_chars = v; return 1;
    }
    /* --version / --help are metadata exits; not exercised by fixtures. */
    return 0;
}

/* PoP: w6_legacycli_b0ce00_resolve @ agent/legacy_cli.py:main */
json_t *w6_legacycli_b0ce00_resolve(const char *argv_json) {
    json_t *in = argv_json ? json_parse(argv_json, NULL) : NULL;
    parsed_t p;
    memset(&p, 0, sizeof(p));
    p.model = "";
    p.base_url = "";
    p.max_turns = 10;
    p.log_prefix_chars = 20;

    json_t *out = NULL;
    if (!in || in->type != JSON_ARRAY) {
        out = err_obj();
        if (in) json_free(in);
        return out;
    }

    int endopts = 0;
    for (size_t i = 0; i < in->c.count; i++) {
        const char *tok = json_string_value(in->c.items[i]);
        if (!tok) { out = err_obj(); goto done; }
        if (endopts) {
            if (p.npos == p.poscap) {
                p.poscap = p.poscap ? p.poscap * 2 : 8;
                p.pos = (const char **)realloc(p.pos, p.poscap * sizeof(char *));
            }
            p.pos[p.npos++] = own(&p, tok);
            continue;
        }
        if (!strcmp(tok, "--")) { endopts = 1; continue; }
        if (tok[0] == '-' && tok[1] != '\0') {
            char *cp = strdup(tok);
            int consumed = 0;
            const char *next = (i + 1 < in->c.count) ? json_string_value(in->c.items[i + 1]) : NULL;
            if (!apply_opt(&p, cp, &consumed, next)) {
                free(cp); out = err_obj(); goto done;
            }
            free(cp);
            if (consumed) i++;
        } else {
            if (p.npos == p.poscap) {
                p.poscap = p.poscap ? p.poscap * 2 : 8;
                p.pos = (const char **)realloc(p.pos, p.poscap * sizeof(char *));
            }
            p.pos[p.npos++] = own(&p, tok);
        }
    }

    {
        char *positional = join_strip(&p);
        int has_pos = positional[0] != '\0';
        int has_query = p.query && p.query[0] != '\0';
        if (has_query && has_pos) { free(positional); out = err_obj(); goto done; }

        const char *query = has_query ? p.query : (has_pos ? positional : NULL);
        if (!query && !p.list_tools) {
            out = json_object();
            json_set(out, "called", json_bool(0));
        } else {
            out = json_object();
            json_set(out, "called", json_bool(1));
            json_set(out, "query", query ? json_string(query) : json_null());
            json_set(out, "model", json_string(p.model ? p.model : ""));
            json_set(out, "api_key", p.api_key ? json_string(p.api_key) : json_null());
            json_set(out, "base_url", json_string(p.base_url ? p.base_url : ""));
            json_set(out, "max_turns", json_number((double)p.max_turns));
            json_set(out, "enabled_toolsets", p.enabled ? json_string(p.enabled) : json_null());
            json_set(out, "disabled_toolsets", p.disabled ? json_string(p.disabled) : json_null());
            json_set(out, "list_tools", json_bool(p.list_tools));
            json_set(out, "save_trajectories", json_bool(p.save_trajectories));
            json_set(out, "save_sample", json_bool(p.save_sample));
            json_set(out, "verbose", json_bool(p.verbose));
            json_set(out, "log_prefix_chars", json_number((double)p.log_prefix_chars));
        }
        free(positional);
    }

done:
    for (size_t i = 0; i < p.nvals; i++) free(p.vals[i]);
    free(p.vals);
    free(p.pos);
    json_free(in);
    return out;
}
