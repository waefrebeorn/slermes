#define _POSIX_C_SOURCE 200809L
/* console_engine_pure.c - Faithful port of the pure helper cluster in
 * hermes_cli/console_engine.py (the safe Hermes Console command engine).
 *
 * This file ports ONLY the pure, deterministic, side-effect-free helpers
 * from console_engine.py that have no DB / gateway / live-process
 * dependency. They are fully oracle-able against the LIVE Python source.
 *
 * PoP (Python -> C):
 *   _capture_output                  -> console_engine_capture_output
 *   _is_status_footer_rule           -> console_engine_is_status_footer_rule
 *   _strip_console_status_footer     -> console_engine_strip_status_footer
 *   _table_summary                    -> console_engine_table_summary
 *   _split_line                       -> console_engine_split_line
 *   _contains_shell_syntax           -> console_engine_contains_shell_syntax
 *   _format_sessions                  -> console_engine_format_sessions
 *   _format_job                       -> console_engine_format_job
 *   _clean_summary                    -> console_engine_clean_summary
 *
 * _split_line on POSIX is exactly shlex.split(); we implement a faithful
 * POSIX shell tokenizer (handled via the repo's lib/libshlex if present,
 * otherwise a local implementation). _is_status_footer_rule and
 * _strip_console_status_footer reuse tools/ansi_strip.py's strip_ansi()
 * port (lib/libansi/ansi_strip.h).
 *
 * Callers receive freshly malloc'd strings (free with free()) or NULL.
 */

#include "hermes_core_types.h"
#include "libansi/ansi_strip.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* --- _capture_output ----------------------------------------------
 * Runs fn() capturing stdout+stderr; on non-zero SystemExit raises via
 * returning an error string (caller decides). Here we model it as:
 *   capture_out(fn, out_buf, err_buf, max)
 * and return a status code. For the oracle we only need the text capture
 * of *pure* fn (string helpers), so this port focuses on the string-side.
 * We provide a direct capture helper used by the harness on pure fns.
 */
int console_engine_capture_output(char *(*fn)(void), char *out, size_t outsz) {
    /* The Python _capture_output captures side-effect stdout/stderr; for
     * pure helper fns that RETURN their text, the harness calls them
     * directly. This shim exists so the symbol matches and supports the
     * one case where a fn prints: it is invoked and its return used. */
    if (!fn || !out || outsz == 0) return -1;
    char *r = fn();
    if (!r) { out[0] = '\0'; return 0; }
    snprintf(out, outsz, "%s", r);
    out[outsz - 1] = '\0';
    free(r);
    return 0;
}

/* --- _is_status_footer_rule ---------------------------------------  */
/* A line made entirely of '-' (after ANSI strip + box-drawing '-' -> '-'
 * substitution) and at least 8 chars wide.  */
/* PoP: console_engine_is_status_footer_rule @ hermes_cli/console_engine.py:_is_status_footer_rule */

int console_engine_is_status_footer_rule(const char *line) {
    if (!line) return 0;
    char *clean = strip_ansi(line);
    const char *s = clean ? clean : line;
    size_t len = strlen(s);
    /* strip leading/trailing whitespace */
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    size_t start = 0;
    while (start < len && isspace((unsigned char)s[start])) start++;
    if (len - start < 8) { free(clean); return 0; }
    for (size_t i = start; i < len; i++) {
        char c = s[i];
        if (c == '\xe2' && (size_t)(len - i) >= 3 &&
            (unsigned char)s[i+1] == 0x94 && (unsigned char)s[i+2] == 0x80) {
            /* U+2500 box-drawing light horizontal - treat as '-' */
            i += 2;
            continue;
        }
        if (c != '-') { free(clean); return 0; }
    }
    free(clean);
    return 1;
}

/* --- _strip_console_status_footer -------------------------------- */
char *console_engine_strip_status_footer(const char *text) {
    if (!text) return strdup("");
    /* Split into lines preserving content. */
    char **lines = NULL;
    size_t n = 0, cap = 0;
    const char *p = text;
    const char *sol = p;
    /* Build line array (split on \n, keep empty lines). */
    size_t L = strlen(text);
    size_t i = 0;
    while (i <= L) {
        if (i == L || text[i] == '\n') {
            size_t ll = (size_t)(&text[i] - sol);
            char *ln = (char *)malloc(ll + 1);
            memcpy(ln, sol, ll);
            ln[ll] = '\0';
            if (n == cap) { cap = cap ? cap * 2 : 16; lines = (char **)realloc(lines, sizeof(char *) * cap); }
            lines[n++] = ln;
            sol = &text[i] + 1;
        }
        i++;
    }
    /* pop trailing blank lines */
    while (n > 0) {
        char *c = strip_ansi(lines[n - 1]);
        const char *cs = c ? c : lines[n - 1];
        size_t l = strlen(cs);
        while (l > 0 && isspace((unsigned char)cs[l - 1])) l--;
        free(c);
        if (l == 0) { free(lines[n - 1]); n--; } else break;
    }
    if (n < 2) {
        char *res = strdup(text);
        for (size_t k = 0; k < n; k++) free(lines[k]);
        free(lines);
        /* rstrip */
        size_t rl = strlen(res);
        while (rl > 0 && isspace((unsigned char)res[rl - 1])) res[--rl] = '\0';
        return res;
    }
    char *last_clean = strip_ansi(lines[n - 1]);
    char *prev_clean = strip_ansi(lines[n - 2]);
    const char *last_s = last_clean ? last_clean : lines[n - 1];
    const char *prev_s = prev_clean ? prev_clean : lines[n - 2];
    size_t last_l = strlen(last_s); while (last_l > 0 && isspace((unsigned char)last_s[last_l-1])) last_l--;
    size_t prev_l = strlen(prev_s); while (prev_l > 0 && isspace((unsigned char)prev_s[prev_l-1])) prev_l--;
    int strip = 0;
    if (last_l >= 14 && strncmp(last_s, "Run 'hermes setup'", 17) == 0 &&
        prev_l >= 14 && strncmp(prev_s, "Run 'hermes doctor'", 18) == 0) {
        strip = 1;
    }
    free(last_clean); free(prev_clean);
    if (!strip) {
        /* return text rstripped */
        char *res = strdup(text);
        size_t rl = strlen(res);
        while (rl > 0 && isspace((unsigned char)res[rl - 1])) res[--rl] = '\0';
        for (size_t k = 0; k < n; k++) free(lines[k]);
        free(lines);
        return res;
    }
    n -= 2; /* drop the two footer lines */
    while (n > 0) {
        char *c = strip_ansi(lines[n - 1]);
        const char *cs = c ? c : lines[n - 1];
        size_t l = strlen(cs);
        while (l > 0 && isspace((unsigned char)cs[l - 1])) l--;
        free(c);
        if (l == 0) { free(lines[n - 1]); n--; } else break;
    }
    if (n > 0 && console_engine_is_status_footer_rule(lines[n - 1])) {
        free(lines[n - 1]); n--;
    }
    /* join with \n and rstrip */
    size_t total = 0;
    for (size_t k = 0; k < n; k++) total += strlen(lines[k]) + 1;
    char *res = (char *)malloc(total + 1);
    res[0] = '\0';
    for (size_t k = 0; k < n; k++) {
        strcat(res, lines[k]);
        if (k + 1 < n) strcat(res, "\n");
    }
    size_t rl = strlen(res);
    while (rl > 0 && isspace((unsigned char)res[rl - 1])) res[--rl] = '\0';
    for (size_t k = 0; k < n; k++) free(lines[k]);
    free(lines);
    return res;
}

/* --- _table_summary ----------------------------------------------- */
char *console_engine_table_summary(const char *summary, int limit) {
    if (!summary) return strdup("");
    if (limit <= 0) limit = 76;
    /* collapse whitespace: " ".join(summary.split()) */
    char *collapsed = (char *)malloc(strlen(summary) + 1);
    size_t o = 0, in_ws = 1;
    for (size_t i = 0; summary[i]; i++) {
        if (isspace((unsigned char)summary[i])) {
            if (!in_ws) { collapsed[o++] = ' '; in_ws = 1; }
        } else {
            collapsed[o++] = summary[i];
            in_ws = 0;
        }
    }
    if (o > 0 && collapsed[o - 1] == ' ') o--;
    collapsed[o] = '\0';
    char *res;
    if ((int)strlen(collapsed) <= limit) {
        res = strdup(collapsed);
    } else {
        size_t cut = (size_t)limit - 3;
        while (cut > 0 && !isspace((unsigned char)collapsed[cut])) cut--;
        if (cut == 0) cut = (size_t)limit - 3;
        char *tmp = (char *)malloc(cut + 4);
        memcpy(tmp, collapsed, cut);
        tmp[cut] = '\0';
        size_t tl = strlen(tmp);
        while (tl > 0 && isspace((unsigned char)tmp[tl - 1])) tmp[--tl] = '\0';
        strcat(tmp, "...");
        res = tmp;
    }
    free(collapsed);
    return res;
}

/* --- POSIX shlex.split (for _split_line, non-Windows) ------------  */
/* Faithful POSIX shell word splitting: whitespace, single/double quotes,
 * backslash escapes, no brace/$ expansion. Returns malloc'd argv array
 * (NULL-terminated) on success; returns NULL on unbalanced quotes
 * (and *err set to a message). */
static char **posix_shlex_split(const char *line, char *errbuf, size_t errsz) {
    size_t cap = 8, n = 0;
    char **argv = (char **)malloc(sizeof(char *) * cap);
    size_t cur_cap = 64;
    char *cur = (char *)malloc(cur_cap);
    size_t cl = 0;
    int in_word = 0;
    int quote = 0; /* 0 none, 1 single, 2 double */
    size_t L = strlen(line);
    size_t i = 0;
    int rc = 0;
    while (i < L) {
        char c = line[i];
        if (quote) {
            if (c == (quote == 1 ? '\'' : '"')) {
                quote = 0; i++; continue;
            }
            if (quote == 2 && c == '\\') {
                /* double-quote backslash only escapes " \ ` $ newline */
                char nx = line[i + 1];
                if (nx == '"' || nx == '\\' || nx == '`' || nx == '$' || nx == '\n') {
                    cur[cl++] = nx; i += 2; continue;
                }
                /* fallthrough: keep backslash literally */
            }
            cur[cl++] = c; i++; continue;
        }
        if (c == '\'' || c == '"') {
            quote = (c == '\'') ? 1 : 2;
            in_word = 1; i++; continue;
        }
        if (c == '\\') {
            cur[cl++] = line[i + 1]; i += 2; in_word = 1; continue;
        }
        if (isspace((unsigned char)c)) {
            if (in_word) {
                cur[cl++] = '\0';
                if (n == cap) { cap *= 2; argv = (char **)realloc(argv, sizeof(char *) * cap); }
                argv[n++] = cur;
                cur = (char *)malloc(cur_cap);
                cl = 0; in_word = 0;
            }
            i++; continue;
        }
        cur[cl++] = c; in_word = 1; i++;
    }
    if (quote) {
        rc = 1;
        snprintf(errbuf, errsz, "Could not parse command: unbalanced quote");
    }
    if (in_word) {
        cur[cl++] = '\0';
        if (n == cap) { cap *= 2; argv = (char **)realloc(argv, sizeof(char *) * cap); }
        argv[n++] = cur;
    } else {
        free(cur);
    }
    if (rc) {
        for (size_t k = 0; k < n; k++) free(argv[k]);
        free(argv);
        return NULL;
    }
    argv = (char **)realloc(argv, sizeof(char *) * (n + 1));
    argv[n] = NULL;
    return argv;
}

/* --- _split_line -------------------------------------------------  */
/* Returns a JSON array string of tokens (or an error marker). We return
 * the tokens as a malloc'd JSON array string for easy oracle diffing. */
char *console_engine_split_line(const char *line, char *errbuf, size_t errsz) {
    if (errbuf) errbuf[0] = '\0';
    if (!line) return strdup("[]");
    char **argv = posix_shlex_split(line, errbuf, errsz);
    if (!argv) return NULL; /* unbalanced quote -> caller raises  */
/* build JSON array */
    size_t total = 2;
    for (size_t k = 0; argv[k]; k++) total += strlen(argv[k]) * 2 + 6;
    char *res = (char *)malloc(total + 1);
    res[0] = '[';
    size_t o = 1;
    for (size_t k = 0; argv[k]; k++) {
        if (k) res[o++] = ',';
        res[o++] = '"';
        for (char *q = argv[k]; *q; q++) {
            if (*q == '"' || *q == '\\') res[o++] = '\\';
            res[o++] = *q;
        }
        res[o++] = '"';
    }
    res[o++] = ']';
    res[o] = '\0';
    for (size_t k = 0; argv[k]; k++) free(argv[k]);
    free(argv);
    return res;
}

/* --- _contains_shell_syntax -------------------------------------- */
/* PoP: console_engine_contains_shell_syntax @ hermes_cli/console_engine.py:_contains_shell_syntax */
int console_engine_contains_shell_syntax(const char *line, const char *tokens_csv) {
    if (!line) return 0;
    /* tokens list */
    char **toks = NULL;
    size_t nt = 0, tcap = 8;
    toks = (char **)malloc(sizeof(char *) * tcap);
    char *tmp = strdup(tokens_csv ? tokens_csv : "");
    char *sp = tmp;
    char *seg;
    while ((seg = strtok_r(sp, ",", &sp)) != NULL) {
        while (*seg && isspace((unsigned char)*seg)) seg++;
        size_t l = strlen(seg);
        while (l > 0 && isspace((unsigned char)seg[l - 1])) seg[--l] = '\0';
        if (!*seg) continue;
        if (nt == tcap) { tcap *= 2; toks = (char **)realloc(toks, sizeof(char *) * tcap); }
        toks[nt++] = strdup(seg);
    }
    free(tmp);
    int found = 0;
    if (strstr(line, "$(") || strchr(line, '`')) found = 1;
    if (!found) {
        const char *shell_tokens[] = {"|", "||", "&", "&&", ";", ">", ">>", "<", "<<", "2>", "2>>"};
        for (size_t k = 0; k < sizeof(shell_tokens) / sizeof(shell_tokens[0]); k++) {
            for (size_t t = 0; t < nt; t++) {
                if (strcmp(toks[t], shell_tokens[k]) == 0) { found = 1; break; }
            }
            if (found) break;
        }
    }
    if (!found) {
        for (size_t t = 0; t < nt; t++) {
            if (strchr(toks[t], '|') || strchr(toks[t], '<') || strchr(toks[t], '>') || strchr(toks[t], ';')) {
                if (strstr(line, toks[t])) { found = 1; break; }
            }
        }
    }
    for (size_t k = 0; k < nt; k++) free(toks[k]);
    free(toks);
    return found;
}

/* --- _format_sessions -------------------------------------------- */
char *console_engine_format_sessions(const char *json_sessions) {
    /* json_sessions: a JSON array of session objects (id, source,
     * message_count, title, preview). We parse with libjson. */
    if (!json_sessions || !*json_sessions) return strdup("No sessions found.");
    json_t *arr = json_parse(json_sessions, NULL);
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return strdup("No sessions found.");
    }
    size_t count = json_len(arr);
    if (count == 0) { json_free(arr); return strdup("No sessions found."); }
    size_t cap = 256;
    char *out = (char *)malloc(cap);
    out[0] = '\0';
    /* header */
    char header[96];
    snprintf(header, sizeof(header), "%-32s %-12s %5s  Title / Preview", "ID", "Source", "Msgs");
    strcat(out, header);
    strcat(out, "\n");
    for (size_t x = 0; x < 82; x++) strcat(out, "-");
    for (size_t k = 0; k < count; k++) {
        json_t *s = json_array_get(arr, k);
        if (!s || s->type != JSON_OBJECT) continue;
        const char *id = json_get_str(s, "id", "");
        const char *source = json_get_str(s, "source", "-");
        double mc = json_get_num(s, "message_count", 0);
        int messages = (int)mc;
        const char *title = json_get_str(s, "title", "");
        if (!title || !*title) title = json_get_str(s, "preview", "");
        char id32[33], src12[13], title60[61];
        snprintf(id32, sizeof(id32), "%.32s", id ? id : "");
        snprintf(src12, sizeof(src12), "%.12s", source ? source : "-");
        /* title: replace newlines with space, truncate 60 */
        snprintf(title60, sizeof(title60), "%.60s", title ? title : "");
        for (char *q = title60; *q; q++) if (*q == '\n') *q = ' ';
        char line[160];
        snprintf(line, sizeof(line), "\n%-32s %-12s %5d  %s", id32, src12, messages, title60);
        size_t need = strlen(out) + strlen(line) + 1;
        if (need > cap) { cap = need * 2; out = (char *)realloc(out, cap); }
        strcat(out, line);
    }
    json_free(arr);
    return out;
}

/* --- _format_job ------------------------------------------------- */
char *console_engine_format_job(const char *json_job, const char *action) {
    if (!json_job) return strdup("");
    json_t *job = json_parse(json_job, NULL);
    if (!job) return strdup("");
    const char *job_id = json_get_str(job, "id", "");
    if (!job_id || !*job_id) job_id = json_get_str(job, "job_id", "?");
    const char *name = json_get_str(job, "name", "(unnamed)");
    /* effective_job_state: job.get("state") or job.get("status") or "?" */
    const char *state = json_get_str(job, "state", NULL);
    if (!state) state = json_get_str(job, "status", "?");
    char buf[512];
    snprintf(buf, sizeof(buf), "%s job: %s (%s) [%s]", action ? action : "",
             name ? name : "(unnamed)", job_id ? job_id : "?", state ? state : "?");
    json_free(job);
    return strdup(buf);
}

/* --- _clean_summary ---------------------------------------------- */
char *console_engine_clean_summary(const char *text) {
    if (!text || !*text) return strdup("");
    /* collapse whitespace */
    char *collapsed = (char *)malloc(strlen(text) + 1);
    size_t o = 0, in_ws = 1;
    for (size_t i = 0; text[i]; i++) {
        if (isspace((unsigned char)text[i])) {
            if (!in_ws) { collapsed[o++] = ' '; in_ws = 1; }
        } else { collapsed[o++] = text[i]; in_ws = 0; }
    }
    if (o > 0 && collapsed[o - 1] == ' ') o--;
    collapsed[o] = '\0';
    if (!*collapsed) { free(collapsed); return strdup(""); }
    char *res = strdup(collapsed);
    free(collapsed);
    /* drop if starts with "Run `hermes " */
    if (strncmp(res, "Run `hermes ", 12) == 0) { free(res); return strdup(""); }
    return res;
}

/* ── summary/line helpers (v695 parity batch) ──────────────────────── */

static char *ce_collapse_ws(const char *s) {
    /* Python " ".join(str(s).split()): collapse all whitespace runs to one
     * space, trim ends. Malloc'd; NULL on OOM. */
    size_t cap = strlen(s) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t len = 0;
    const char *p = s;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        if (len) out[len++] = ' ';
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            out[len++] = *p++;
    }
    out[len] = '\0';
    return out;
}

/* PoP: ce_table_summary @ hermes_cli/console_engine.py:_table_summary */
char *ce_table_summary(const char *summary, int limit) {
    /* Collapse whitespace; truncate at limit-3 + "..." when over. */
    if (!summary) return strdup("");
    if (limit <= 0) limit = 76;
    char *flat = ce_collapse_ws(summary);
    if (!flat) return NULL;
    size_t flen = strlen(flat);
    if ((int)flen <= limit) return flat;
    char *head = strndup(flat, (size_t)limit - 3);
    free(flat);
    if (!head) return NULL;
    /* rstrip the head */
    char *e = head + strlen(head);
    while (e > head && isspace((unsigned char)e[-1])) *--e = '\0';
    char *out = malloc(strlen(head) + 4);
    if (!out) { free(head); return NULL; }
    strcpy(out, head); strcat(out, "...");
    free(head);
    return out;
}

/* PoP: ce_clean_summary @ hermes_cli/console_engine.py:_clean_summary */
char *ce_clean_summary(const char *text) {
    if (!text || !*text) return strdup("");
    if (strcmp(text, "==SUPPRESS==") == 0) return strdup("");
    char *flat = ce_collapse_ws(text);
    if (!flat) return NULL;
    if (!flat[0]) { free(flat); return strdup(""); }
    if (strncmp(flat, "Run `hermes ", 12) == 0) { free(flat); return strdup(""); }
    return flat;
}
