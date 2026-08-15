/*
 * port_session_listing.c — pure helpers ported from
 * hermes_cli/session_listing.py. Self-contained; implements a minimal
 * shlex-style tokenizer for parse_session_listing_args; reuses libjson.
 *
 *   - parse_session_listing_args   -> session_listing_parse_args (+ _parse_flags)
 *   - format_gateway_session_listing -> session_listing_format_gateway
 */

#include "session_listing_helpers.h"
#include "libjson/json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* PoP: parse_session_listing_args @ hermes_cli/session_listing.py:parse_session_listing_args */
/* Minimal shlex.split: splits on whitespace, honors ' and " quoting, drops
 * unquoted quotes. Returns malloc'd argv (NULL-terminated) + count. Caller
 * frees with session_listing_free_argv. */
/* PoP: parse_args @ hermes_cli/codex_runtime_switch.py:parse_args */
char **session_listing_parse_args(const char *raw, int *out_count)
{
    *out_count = 0;
    char **argv = calloc(8, sizeof(char *));
    int cap = 8, n = 0;
    if (!raw || !*raw) return argv;

    const char *p = raw;
    char buf[2048];
    size_t blen = 0;
    enum { ST_WS, ST_WORD, ST_SQ, ST_DQ } st = ST_WS;

    for (; *p; p++) {
        char c = *p;
        switch (st) {
        case ST_WS:
            if (c == '\'') { st = ST_SQ; }
            else if (c == '"') { st = ST_DQ; }
            else if (!isspace((unsigned char)c)) { buf[blen++] = c; st = ST_WORD; }
            break;
        case ST_WORD:
            if (c == '\'') { st = ST_SQ; }
            else if (c == '"') { st = ST_DQ; }
            else if (isspace((unsigned char)c)) {
                if (n >= cap) { cap *= 2; argv = realloc(argv, (size_t)cap * sizeof(char *)); }
                argv[n] = malloc(blen + 1);
                memcpy(argv[n], buf, blen);
                argv[n][blen] = '\0';
                n++;
                blen = 0;
                st = ST_WS;
            } else { buf[blen++] = c; }
            break;
        case ST_SQ:
            if (c == '\'') { st = ST_WORD; }
            else { buf[blen++] = c; }
            break;
        case ST_DQ:
            if (c == '"') { st = ST_WORD; }
            else { buf[blen++] = c; }
            break;
        }
    }
    if (blen > 0) {
        if (n >= cap) { cap *= 2; argv = realloc(argv, (size_t)cap * sizeof(char *)); }
        argv[n] = malloc(blen + 1);
        memcpy(argv[n], buf, blen);
        argv[n][blen] = '\0';
        n++;
    }
    *out_count = n;
    return argv;
}

void session_listing_free_argv(char **argv, int count)
{
    if (!argv) return;
    for (int i = 0; i < count; i++) free(argv[i]);
    free(argv);
}

/* PoP: parse_session_listing_args flags.
 * Outputs: *include_all, *include_unnamed, target (malloc-free caller buffer). */
void session_listing_parse_flags(const char *raw_args,
                                int *include_all, int *include_unnamed,
                                char *target, size_t target_cap)
{
    int n = 0;
    char **argv = session_listing_parse_args(raw_args, &n);
    *include_all = 0; *include_unnamed = 0;
    if (target && target_cap) target[0] = '\0';
    size_t tlen = 0;

    for (int i = 0; i < n; i++) {
        size_t L = strlen(argv[i]);
        char *lower = malloc(L + 1);
        for (size_t k = 0; k <= L; k++) lower[k] = (char)tolower((unsigned char)argv[i][k]);
        int is_list = strcmp(lower, "list") == 0 || strcmp(lower, "ls") == 0 ||
                      strcmp(lower, "browse") == 0;
        int is_all = strcmp(lower, "all") == 0 || strcmp(lower, "--all") == 0;
        int is_full = strcmp(lower, "full") == 0 || strcmp(lower, "--full") == 0;
        free(lower);
        if (is_list || is_all || is_full) {
            if (is_all) *include_all = 1;
            if (is_full) *include_unnamed = 1;
            continue;
        }
        if (target && target_cap) {
            if (tlen) { target[tlen++] = ' '; target[tlen] = '\0'; }
            for (size_t k = 0; k < L && tlen < target_cap - 1; k++)
                target[tlen++] = argv[i][k];
            target[tlen] = '\0';
        }
    }
    while (tlen > 0 && target[tlen - 1] == ' ') target[--tlen] = '\0';
    /* strip leading whitespace too (mirrors Python str.strip() on joined target) */
    size_t lead = 0;
    while (target[lead] == ' ') lead++;
    if (lead) {
        memmove(target, target + lead, tlen - lead + 1);
        tlen -= lead;
    }
    session_listing_free_argv(argv, n);
}

/* PoP: format_gateway_session_listing @ hermes_cli/session_listing.py:format_gateway_session_listing */
/* Renders rows (JSON array of {id,title,preview,source}) to a Markdown-ish
 * string. Returns malloc'd string; caller frees. Empty -> the "no sessions"
 * message. */
char *session_listing_format_gateway(const char *rows_json,
                                     int include_source,
                                     const char *title)
{
    char *err = NULL;
    json_t *rows = json_parse(rows_json ? rows_json : "[]", &err);
    if (err) { free(err); return strdup(""); }
    if (!rows || rows->type != JSON_ARRAY) { if (rows) json_free(rows); return strdup(""); }

    if (rows->c.count == 0) {
        char *msg = strdup(
            "No sessions found.\n"
            "Use `/title My Session` to name this chat, or `/sessions full` "
            "to include unnamed sessions.");
        json_free(rows);
        return msg;
    }

    size_t cap = 2048, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';

#define APP(s) do { \
        size_t _L = strlen(s); \
        while (len + _L + 1 > cap) { cap *= 2; out = realloc(out, cap); } \
        memcpy(out + len, (s), _L + 1); \
        len += _L; \
    } while (0)

    char hdr[256];
    snprintf(hdr, sizeof hdr, "\xf0\x9f\x93\x8b **%s**\n", title ? title : "Sessions");
    APP(hdr);

    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        if (!row || row->type != JSON_OBJECT) continue;
        const json_t *id = json_obj_get(row, "id");
        const json_t *tt = json_obj_get(row, "title");
        const json_t *pv = json_obj_get(row, "preview");
        const json_t *sc = json_obj_get(row, "source");
        const char *sid = (id && id->type == JSON_STRING) ? id->str_val : "";
        const char *stit = (tt && tt->type == JSON_STRING && tt->str_val[0] != '\0')
                              ? tt->str_val : "\xe2\x80\x94";
        const char *spv = (pv && pv->type == JSON_STRING) ? pv->str_val : "";
        const char *ssc = (sc && sc->type == JSON_STRING) ? sc->str_val : "";

        char line[2048];
        int off = snprintf(line, sizeof line, "%zu. **%s**", i + 1, stit);
        if (include_source && *ssc)
            off += snprintf(line + off, sizeof line - (size_t)off, " `%s`", ssc);
        off += snprintf(line + off, sizeof line - (size_t)off, " — `%s`", sid);
        if (*spv) {
            char pv40[41];
            strncpy(pv40, spv, 40); pv40[40] = '\0';
            off += snprintf(line + off, sizeof line - (size_t)off, " — _%s_", pv40);
        }
        off += snprintf(line + off, sizeof line - (size_t)off, "\n");
        APP(line);
    }
    APP("\n");
    APP("Resume: `/resume <session id>` or `/resume <number>` from `/resume`.\n");
    APP("More: `/sessions all`, `/sessions full`, `/sessions all full`.");

#undef APP
    json_free(rows);
    return out;
}
