/*
 * blueprint_cmd.c — /blueprint command resolution helpers (PoP port).
 *
 * Faithful, self-contained C port of hermes_cli/blueprint_cmd.py. The module
 * resolves a free-typed blueprint name to a catalog entry using the same
 * forgiving pipeline Python uses (exact -> prefix -> substring -> fuzzy),
 * and produces the catalog listing / candidate / no-match text plus the
 * agent-seed prompt. It consumes the catalog as JSON (the single baked
 * source-of-truth from port_blueprint_catalog_helpers.c via
 * blueprint_catalog_raw_json), so it owns no catalog data and no job engine.
 *
 * Fuzzy typo-tolerance reuses libdifflib (difflib_ratio), mirroring Python's
 * difflib.get_close_matches (cutoff 0.6, at most 3 close matches).
 *
 * PoP: hermes_cli/blueprint_cmd.py
 *   _parse_kv            -> blueprint_cmd_parse_kv
 *   match_blueprint      -> blueprint_cmd_match
 *   _fmt_catalog         -> blueprint_cmd_format_catalog
 *   _fmt_candidates      -> blueprint_cmd_format_candidates
 *   _fmt_no_match        -> blueprint_cmd_format_no_match
 *   build_blueprint_seed -> blueprint_cmd_build_seed
 *   (BlueprintCommandResult handling lives in the /blueprint command, which
 *    calls these helpers — that orchestration is not ported here as a unit.)
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "blueprint_cmd.h"
#include "blueprint_catalog_common.h"
#include "hermes_json.h"
#include "difflib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>

/* index-based string accessor for JSON arrays */
static const char *json_arr_str(json_t *arr, size_t idx) {
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    json_t *e = json_array_get(arr, idx);
    return (e && e->type == JSON_STRING) ? json_string_value(e) : NULL;
}

/* ── opaque catalog ─────────────────────────────────────────────────── */

struct blueprint_catalog {
    json_t *doc;          /* parsed array, owned */
    int n;                /* entry count */
};

/* ── helpers ────────────────────────────────────────────────────────── */

static const char *entry_key(json_t *e) {
    return json_get_str(e, "key", NULL);
}
static const char *entry_title(json_t *e) {
    return json_get_str(e, "title", NULL);
}
static const char *entry_desc(json_t *e) {
    return json_get_str(e, "description", NULL);
}

/* case-insensitive prefix test on whole title words */
static bool title_word_prefix(json_t *e, const char *q) {
    const char *title = entry_title(e);
    if (!title || !*title) return false;
    size_t ql = strlen(q);
    /* walk words */
    const char *p = title;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        const char *w = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t wl = (size_t)(p - w);
        if (wl >= ql && strncasecmp(w, q, ql) == 0) return true;
    }
    return false;
}

/* ── load / free ────────────────────────────────────────────────────── */

blueprint_catalog_t *blueprint_catalog_load_json(const char *catalog_json) {
    if (!catalog_json || !*catalog_json) return NULL;
    json_t *doc = json_parse(catalog_json, NULL);
    if (!doc || doc->type != JSON_ARRAY) { if (doc) json_free(doc); return NULL; }
    blueprint_catalog_t *cat = calloc(1, sizeof(*cat));
    if (!cat) { json_free(doc); return NULL; }
    cat->doc = doc;
    cat->n = (int)json_array_size(doc);
    return cat;
}

/* Convenience: load the baked shared catalog. */
static blueprint_catalog_t *catalog_load_baked(void) {
    return blueprint_catalog_load_json(blueprint_catalog_raw_json());
}

void blueprint_catalog_free(blueprint_catalog_t *cat) {
    if (!cat) return;
    if (cat->doc) json_free(cat->doc);
    free(cat);
}

int blueprint_catalog_count(const blueprint_catalog_t *cat) {
    return cat ? cat->n : 0;
}

int blueprint_catalog_at(const blueprint_catalog_t *cat, int i,
                          char **key, char **title, char **description) {
    if (!cat || i < 0 || i >= cat->n) return -1;
    json_t *e = json_array_get(cat->doc, (size_t)i);
    if (!e || e->type != JSON_OBJECT) return -1;
    if (key) *key = strdup(entry_key(e) ? entry_key(e) : "");
    if (title) *title = strdup(entry_title(e) ? entry_title(e) : "");
    if (description) *description = strdup(entry_desc(e) ? entry_desc(e) : "");
    return 0;
}

/* ── quote-aware key=value parsing (shlex-like) ─────────────────────── */

static char *dup_range(const char *a, const char *b) {
    size_t n = (size_t)(b - a);
    char *s = malloc(n + 1);
    memcpy(s, a, n);
    s[n] = '\0';
    return s;
}

/* Tokenize like shlex: whitespace separates tokens, but "..." / '...' keep
 * their content (including spaces) as one token. A quote may appear mid-token
 * (e.g. criteria="from my boss") and the rest of the quoted span stays part of
 * that same token. Returns a malloc'd array of malloc'd tokens; *out_n set. */
static char **tokenize(const char *s, int *out_n) {
    char **toks = NULL;
    int cap = 0, n = 0;
    const char *p = s ? s : "";
    char *buf = NULL;
    size_t blen = 0, bcap = 0;
    bool in_tok = false;
    char quote = 0;
    while (*p) {
        char c = *p;
        if (!in_tok) {
            if (isspace((unsigned char)c)) { p++; continue; }
            in_tok = true; blen = 0; quote = 0;
        }
        if (quote) {
            if (c == quote) { quote = 0; p++; continue; }
            goto emit_char;
        }
        if (c == '"' || c == '\'') { quote = c; p++; continue; }
        if (isspace((unsigned char)c)) {
            /* end of token */
            if (blen + 1 > bcap) { bcap = blen + 16; buf = realloc(buf, bcap); }
            buf[blen] = '\0';
            if (n >= cap) { cap = cap ? cap * 2 : 8; toks = realloc(toks, sizeof(char*) * (size_t)cap); }
            toks[n++] = buf; buf = NULL; bcap = 0; blen = 0; in_tok = false;
            p++; continue;
        }
emit_char:
        if (blen + 1 > bcap) { bcap = blen + 16; buf = realloc(buf, bcap); }
        buf[blen++] = c;
        p++;
    }
    if (in_tok && blen > 0) {
        if (blen + 1 > bcap) { bcap = blen + 16; buf = realloc(buf, bcap); }
        buf[blen] = '\0';
        if (n >= cap) { cap = cap ? cap * 2 : 8; toks = realloc(toks, sizeof(char*) * (size_t)cap); }
        toks[n++] = buf;
    }
    *out_n = n;
    return toks;
}

int blueprint_cmd_parse_kv(const char *args,
                           char ***out_keys, char ***out_vals, int *out_n,
                           char ***out_leftovers, int *out_leftover_n) {
    int ntok = 0;
    char **toks = tokenize(args, &ntok);
    char **keys = NULL, **vals = NULL;
    char **left = NULL;
    int nkv = 0, nleft = 0, kvcap = 0, lcap = 0;
    for (int i = 0; i < ntok; i++) {
        char *t = toks[i];
        char *eq = strchr(t, '=');
        if (eq && eq != t) {
            *eq = '\0';
            char *k = strdup(t);
            char *v = strdup(eq + 1);
            if (nkv >= kvcap) { kvcap = kvcap ? kvcap * 2 : 8; keys = realloc(keys, sizeof(char*) * (size_t)kvcap); vals = realloc(vals, sizeof(char*) * (size_t)kvcap); }
            keys[nkv] = k; vals[nkv] = v; nkv++;
        } else {
            if (nleft >= lcap) { lcap = lcap ? lcap * 2 : 8; left = realloc(left, sizeof(char*) * (size_t)lcap); }
            left[nleft++] = strdup(t);
        }
        free(t);
    }
    free(toks);
    if (out_keys) *out_keys = keys; else { for (int i=0;i<nkv;i++) free(keys[i]); free(keys); }
    if (out_vals) *out_vals = vals; else { for (int i=0;i<nkv;i++) free(vals[i]); free(vals); }
    if (out_n) *out_n = nkv;
    if (out_leftovers) *out_leftovers = left; else { for (int i=0;i<nleft;i++) free(left[i]); free(left); }
    if (out_leftover_n) *out_leftover_n = nleft;
    return 0;
}

/* ── resolution (mirrors match_blueprint) ────────────────────────────── */

static json_t *entry_by_key(blueprint_catalog_t *cat, const char *k) {
    for (int i = 0; i < cat->n; i++) {
        json_t *e = json_array_get(cat->doc, (size_t)i);
        const char *ek = entry_key(e);
        if (ek && strcmp(ek, k) == 0) return e;
    }
    return NULL;
}

int blueprint_cmd_match(const blueprint_catalog_t *cat, const char *query,
                        char **out_matched_key,
                        char ***out_candidates, int *out_ncand) {
    if (out_matched_key) *out_matched_key = NULL;
    if (out_candidates) *out_candidates = NULL;
    if (out_ncand) *out_ncand = 0;
    if (!cat) return 0;

    char qb[512];
    size_t ql = strlen(query ? query : "");
    if (ql >= sizeof(qb)) ql = sizeof(qb) - 1;
    memcpy(qb, query ? query : "", ql);
    qb[ql] = '\0';
    for (size_t i = 0; i < ql; i++) qb[i] = (char)tolower((unsigned char)qb[i]);
    if (!*qb) return 0;

    /* 1) exact key */
    for (int i = 0; i < cat->n; i++) {
        json_t *e = json_array_get(cat->doc, (size_t)i);
        const char *ek = entry_key(e);
        if (ek && strcasecmp(ek, qb) == 0) {
            if (out_matched_key) *out_matched_key = strdup(ek);
            return 1;
        }
    }

    /* 2) prefix on key or title word */
    int *prefix = calloc((size_t)cat->n, sizeof(int));
    int np = 0;
    for (int i = 0; i < cat->n; i++) {
        json_t *e = json_array_get(cat->doc, (size_t)i);
        const char *ek = entry_key(e);
        if ((ek && strncasecmp(ek, qb, ql) == 0) || title_word_prefix(e, qb))
            prefix[np++] = i;
    }
    if (np == 1) {
        json_t *e = json_array_get(cat->doc, (size_t)prefix[0]);
        free(prefix);
        if (out_matched_key) *out_matched_key = strdup(entry_key(e));
        return 1;
    }
    if (np > 1) {
        char **cands = malloc(sizeof(char*) * (size_t)np);
        for (int i = 0; i < np; i++)
            cands[i] = strdup(entry_key(json_array_get(cat->doc, (size_t)prefix[i])));
        free(prefix);
        if (out_candidates) *out_candidates = cands;
        if (out_ncand) *out_ncand = np;
        return 0;
    }
    free(prefix);

    /* 3) substring anywhere in key/title/description */
    int *sub = calloc((size_t)cat->n, sizeof(int));
    int ns = 0;
    for (int i = 0; i < cat->n; i++) {
        json_t *e = json_array_get(cat->doc, (size_t)i);
        const char *ek = entry_key(e), *et = entry_title(e), *ed = entry_desc(e);
        bool hit = (ek && strcasestr(ek, qb)) || (et && strcasestr(et, qb)) || (ed && strcasestr(ed, qb));
        if (hit) sub[ns++] = i;
    }
    if (ns == 1) {
        json_t *e = json_array_get(cat->doc, (size_t)sub[0]);
        free(sub);
        if (out_matched_key) *out_matched_key = strdup(entry_key(e));
        return 1;
    }
    if (ns > 1) {
        char **cands = malloc(sizeof(char*) * (size_t)ns);
        for (int i = 0; i < ns; i++)
            cands[i] = strdup(entry_key(json_array_get(cat->doc, (size_t)sub[i])));
        free(sub);
        if (out_candidates) *out_candidates = cands;
        if (out_ncand) *out_ncand = ns;
        return 0;
    }
    free(sub);

    /* 4) fuzzy on keys (difflib-based, cutoff 0.6, at most 3) */
    char **close = NULL;
    int nc = 0;
    for (int i = 0; i < cat->n; i++) {
        json_t *e = json_array_get(cat->doc, (size_t)i);
        const char *ek = entry_key(e);
        if (!ek) continue;
        double r = difflib_ratio(qb, ek);
        if (r >= 0.6) {
            close = realloc(close, sizeof(char*) * (size_t)(nc + 1));
            close[nc++] = strdup(ek);
        }
    }
    /* keep at most 3, best-first (simple: stable order is fine) */
    if (nc > 3) {
        for (int i = 3; i < nc; i++) free(close[i]);
        nc = 3;
    }
    if (nc == 1) {
        if (out_matched_key) *out_matched_key = strdup(close[0]);
        for (int i = 0; i < nc; i++) free(close[i]);
        free(close);
        return 1;
    }
    if (nc > 1) {
        if (out_candidates) *out_candidates = close; else { for (int i=0;i<nc;i++) free(close[i]); free(close); }
        if (out_ncand) *out_ncand = nc;
        return 0;
    }
    if (close) { for (int i=0;i<nc;i++) free(close[i]); free(close); }
    return 0;
}

/* ── formatters ─────────────────────────────────────────────────────── */

char *blueprint_cmd_format_catalog(const blueprint_catalog_t *cat) {
    if (!cat || cat->n == 0) return strdup("Automation Blueprints are unavailable in this build.");
    size_t cap = 1024, len = 0;
    char *buf = malloc(cap);
    const char *hdr = "Automation Blueprints — `/blueprint <name>` and I'll ask you what I need:\n";
    len = strlen(hdr);
    memcpy(buf, hdr, len);
    for (int i = 0; i < cat->n; i++) {
        json_t *e = json_array_get(cat->doc, (size_t)i);
        const char *k = entry_key(e), *t = entry_title(e), *d = entry_desc(e);
        char line1[1024], line2[1024];
        snprintf(line1, sizeof(line1), "  • %s — %s\n", k ? k : "?", t ? t : "?");
        snprintf(line2, sizeof(line2), "    %s\n", d ? d : "");
        size_t add = strlen(line1) + strlen(line2);
        if (len + add + 256 >= cap) { cap = len + add + 1024; buf = realloc(buf, cap); }
        memcpy(buf + len, line1, strlen(line1)); len += strlen(line1);
        memcpy(buf + len, line2, strlen(line2)); len += strlen(line2);
    }
    const char *tip = "\nTip: `/blueprint <name>` walks you through it. Power users can "
                      "pass values inline, e.g. `/blueprint morning-brief time=08:00`.\n";
    size_t tl = strlen(tip);
    if (len + tl + 1 >= cap) { cap = len + tl + 8; buf = realloc(buf, cap); }
    memcpy(buf + len, tip, tl); len += tl;
    buf[len] = '\0';
    return buf;
}

char *blueprint_cmd_format_candidates(const blueprint_catalog_t *cat, const char *query) {
    char qb[512];
    size_t ql = strlen(query ? query : "");
    if (ql >= sizeof(qb)) ql = sizeof(qb) - 1;
    memcpy(qb, query ? query : "", ql); qb[ql] = '\0';
    char *buf = NULL;
    size_t cap = 1024, len = 0;
    char head[512];
    snprintf(head, sizeof(head), "'%s' matches several blueprints — which one?\n", qb);
    len = strlen(head); buf = malloc(cap); memcpy(buf, head, len);
    for (int i = 0; i < cat->n; i++) {
        json_t *e = json_array_get(cat->doc, (size_t)i);
        const char *k = entry_key(e), *t = entry_title(e);
        /* only list the entries whose key matches the query as a prefix */
        if (!k || strncasecmp(k, qb, ql) != 0) continue;
        char line[1024];
        snprintf(line, sizeof(line), "  • %s — %s\n", k, t ? t : "?");
        size_t add = strlen(line);
        if (len + add + 256 >= cap) { cap = len + add + 512; buf = realloc(buf, cap); }
        memcpy(buf + len, line, add); len += add;
    }
    const char *foot = "\nRun `/blueprint <name>` with one of the names above.\n";
    size_t fl = strlen(foot);
    if (len + fl + 1 >= cap) { cap = len + fl + 8; buf = realloc(buf, cap); }
    memcpy(buf + len, foot, fl); len += fl; buf[len] = '\0';
    return buf;
}

char *blueprint_cmd_format_no_match(const blueprint_catalog_t *cat, const char *query) {
    char qb[512];
    size_t ql = strlen(query ? query : "");
    if (ql >= sizeof(qb)) ql = sizeof(qb) - 1;
    memcpy(qb, query ? query : "", ql); qb[ql] = '\0';
    /* gather up to 3 close keys (cutoff 0.4) */
    char near[3][128]; int nn = 0;
    for (int i = 0; i < cat->n && nn < 3; i++) {
        json_t *e = json_array_get(cat->doc, (size_t)i);
        const char *k = entry_key(e);
        if (k && difflib_ratio(qb, k) >= 0.4) {
            strncpy(near[nn], k, sizeof(near[nn]) - 1); near[nn][sizeof(near[nn])-1] = '\0'; nn++;
        }
    }
    char *buf;
    if (nn) {
        char suggest[512];
        int off = 0;
        for (int i = 0; i < nn; i++) off += snprintf(suggest + off, sizeof(suggest) - (size_t)off, "%s%s", i ? ", " : "", near[i]);
        size_t need = strlen(qb) + strlen(suggest) + 128;
        buf = malloc(need);
        snprintf(buf, need, "No automation blueprint matches '%s'. Did you mean: %s? Run /blueprint to see the catalog.", qb, suggest);
    } else {
        size_t need = strlen(qb) + 96;
        buf = malloc(need);
        snprintf(buf, need, "No automation blueprint matches '%s'. Run /blueprint to see the catalog.", qb);
    }
    return buf;
}

/* ── agent seed ─────────────────────────────────────────────────────── */

char *blueprint_cmd_build_seed(const blueprint_catalog_t *cat, const char *key) {
    if (!cat) return NULL;
    json_t *bp = entry_by_key((blueprint_catalog_t *)cat, key);
    if (!bp) return NULL;
    const char *title = entry_title(bp);
    const char *desc = entry_desc(bp);
    const char *sched_tmpl = json_get_str(bp, "schedule_template", "");
    const char *prompt_tmpl = json_get_str(bp, "prompt_template", "");

    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);

    char l0[2048];
    snprintf(l0, sizeof(l0),
             "Set up the '%s' automation for me (automation blueprint '%s'). %s\n"
             "\n"
             "Ask me for each of these, one at a time, offering the default in "
             "brackets if I don't have a preference:\n",
             title ? title : key, key, desc ? desc : "");
    len = strlen(l0); memcpy(buf, l0, len);

    json_t *slots = json_object_get(bp, "slots");
    if (slots && slots->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(slots); i++) {
            json_t *s = json_array_get(slots, i);
            if (!s || s->type != JSON_OBJECT) continue;
            const char *slabel = json_get_str(s, "label", "");
            const char *sname = json_get_str(s, "name", "");
            const char *sdef = json_get_str(s, "default", NULL);
            const char *shelp = json_get_str(s, "help", NULL);
            bool optional = json_get_bool(s, "optional", false);

            size_t need = len + 1024;
            if (need >= cap) { cap = need + 1024; buf = realloc(buf, cap); }
            int n = snprintf(buf + len, cap - len, "- %s (%s)", slabel, sname);
            len += (size_t)n;

            json_t *opts = json_object_get(s, "options");
            if (opts && opts->type == JSON_ARRAY && json_array_size(opts) > 0) {
                size_t base = len;
                for (size_t j = 0; j < json_array_size(opts); j++) {
                    const char *o = json_arr_str(opts, j);
                    if (!o) continue;
                    char piece[256];
                    snprintf(piece, sizeof(piece), "%s%s", j ? ", " : " — one of: ", o);
                    if (len + strlen(piece) + 256 >= cap) { cap = len + strlen(piece) + 512; buf = realloc(buf, cap); }
                    memcpy(buf + len, piece, strlen(piece)); len += strlen(piece);
                }
                (void)base;
            }
            if (sdef && *sdef) {
                char piece[256];
                snprintf(piece, sizeof(piece), " [default: %s]", sdef);
                if (len + strlen(piece) + 256 >= cap) { cap = len + strlen(piece) + 512; buf = realloc(buf, cap); }
                memcpy(buf + len, piece, strlen(piece)); len += strlen(piece);
            }
            if (optional) {
                const char *piece = " (optional)";
                if (len + strlen(piece) + 256 >= cap) { cap = len + strlen(piece) + 512; buf = realloc(buf, cap); }
                memcpy(buf + len, piece, strlen(piece)); len += strlen(piece);
            }
            if (shelp && *shelp) {
                char piece[256];
                snprintf(piece, sizeof(piece), " — %s", shelp);
                if (len + strlen(piece) + 256 >= cap) { cap = len + strlen(piece) + 512; buf = realloc(buf, cap); }
                memcpy(buf + len, piece, strlen(piece)); len += strlen(piece);
            }
            if (len + 2 >= cap) { cap = len + 8; buf = realloc(buf, cap); }
            buf[len++] = '\n';
        }
    }

    char tail[2048];
    snprintf(tail, sizeof(tail),
             "\n"
             "Once you have my answers, create the job by calling the cronjob tool "
             "with action='create'. Build the schedule as a cron expression from "
             "this template: `%s` (fill {minute}/{hour} from the chosen time, {dow} "
             "from the weekday choice using {'everyday': '*', 'weekdays': '1-5', "
             "'weekends': '0,6'}, {interval_min} from any interval). Use this exact "
             "prompt for the job (substituting my answers into any {slot} "
             "placeholders): \"%s\". Confirm the schedule and what it will do before "
             "you create it.\n",
             sched_tmpl ? sched_tmpl : "", prompt_tmpl ? prompt_tmpl : "");
    if (len + strlen(tail) + 1 >= cap) { cap = len + strlen(tail) + 8; buf = realloc(buf, cap); }
    memcpy(buf + len, tail, strlen(tail)); len += strlen(tail);
    buf[len] = '\0';
    return buf;
}
