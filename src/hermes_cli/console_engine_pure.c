#define _GNU_SOURCE
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
#include <unistd.h>
#include "console_engine_model.h"
#include "web_server_chat_argv.h"
#include "difflib.h"

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
/* PoP: console_engine_strip_console_status_footer @ hermes_cli/console_engine.py:_strip_console_status_footer */
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

/* PoP: ce_format_sessions @ hermes_cli/console_engine.py:_format_sessions */
char *ce_format_sessions(const json_t *sessions) {
    /* Aligned table: ID(32) Source(12) Msgs(>5) Title/Preview. Empty input
     * renders the documented "No sessions found." line. */
    size_t n = (sessions && json_is_array(sessions)) ? json_array_size(sessions) : 0;
    if (n == 0) return strdup("No sessions found.");

    char *out = malloc(n * 256 + 128);
    if (!out) return NULL;
    int off = snprintf(out, 256,
        "%-32s %-12s %5s  %s\n%-82s",
        "ID", "Source", "Msgs", "Title / Preview",
        "----------------------------------------------------------------------------------");
    for (size_t i = 0; i < n; i++) {
        const json_t *s = json_array_get(sessions, i);
        if (!s || !json_is_object(s)) continue;
        const char *id = json_string_value(json_object_get(s, "id"));
        const char *source = json_string_value(json_object_get(s, "source"));
        double messages = 0;
        const json_t *mc = json_object_get(s, "message_count");
        if (mc && mc->type == JSON_NUMBER) messages = mc->num_val;
        const char *title = json_string_value(json_object_get(s, "title"));
        if (!title || !*title)
            title = json_string_value(json_object_get(s, "preview"));
        if (!title) title = "";
        /* truncate title to 60, newlines -> spaces */
        char tbuf[61];
        size_t k = 0;
        for (const char *q = title; *q && k < 60; q++, k++)
            tbuf[k] = (*q == '\n') ? ' ' : *q;
        tbuf[k] = '\0';
        char idb[33], srcb[13];
        snprintf(idb, sizeof(idb), "%.32s", id ? id : "");
        snprintf(srcb, sizeof(srcb), "%.12s",
                 (source && *source) ? source : "-");
        off += snprintf(out + off, 256,
                        "\n%-32s %-12s %5.0f  %s", idb, srcb, messages, tbuf);
    }
    return out;
}

/* PoP: ce_format_job @ hermes_cli/console_engine.py:_format_job */
char *ce_format_job(const json_t *job, const char *action) {
    /* "{action} job: {name} ({id}) [{state}]" with effective_job_state. */
    extern const char *cronjobs_effective_job_state(const json_t *job);
    if (!job || !json_is_object(job)) return strdup("");
    const char *id = json_string_value(json_object_get(job, "id"));
    if (!id || !*id) id = json_string_value(json_object_get(job, "job_id"));
    if (!id || !*id) id = "?";
    const char *name = json_string_value(json_object_get(job, "name"));
    if (!name || !*name) name = "(unnamed)";
    const char *state = cronjobs_effective_job_state(job);
    if (!state) state = "";
    size_t need = strlen(action) + strlen(name) + strlen(id) + strlen(state) + 32;
    char *out = malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "%s job: %s (%s) [%s]", action, name, id, state);
    return out;
}

/* ══════════════════════════════════════════════════════════════════════
 * REAL_GAP closure batch — faithful ports of the remaining
 * hermes_cli/console_engine.py surface (capture, parser-tree model,
 * summary extraction + memoization, dispatch orchestration, engine
 * methods). Python truth: /home/wubu/hermes-agent-dev/hermes_cli/console_engine.py
 * ══════════════════════════════════════════════════════════════════════ */

#include <stdarg.h>

/* forward declarations within this closure batch */
char *coe_clean_summary(const char *text);

/* PoP: _clean_summary @ hermes_cli/console_engine.py:_clean_summary */
char *coe_clean_summary(const char *text)
{
    if (!text || !*text) return strdup("");
    if (strcmp(text, "==SUPPRESS==") == 0) return strdup("");  /* argparse.SUPPRESS */
    /* " ".join(str(text).split()) — collapse whitespace runs to single spaces */
    size_t n = strlen(text);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0;
    int in_ws = 0;
    for (size_t k = 0; k < n; k++) {
        char ch = text[k];
        int ws = (ch==' '||ch=='\t'||ch=='\n'||ch=='\r'||ch=='\v'||ch=='\f');
        if (ws) { in_ws = 1; continue; }
        if (in_ws && o > 0) out[o++] = ' ';
        in_ws = 0;
        out[o++] = ch;
    }
    out[o] = '\0';
    if (!out[0] || strncmp(out, "Run `hermes ", 12) == 0) { free(out); return strdup(""); }
    return out;
}
char *ce_table_summary(const char *summary, int limit);
void *coe_lookup_symbol_two(const char *module, const char *func);
char *coe_rejection_for(const char **tokens, size_t ntok);

/* ── small utils ─────────────────────────────────────────────────────── */
static char *ce_json_escape(const char *s) {
    if (!s) s = "";
    size_t cap = strlen(s) * 6 + 3;
    char *out = malloc(cap);
    size_t o = 0;
    out[o++] = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20) { o += (size_t)snprintf(out + o, 8, "\\u%04x", c); }
        else out[o++] = (char)c;
    }
    out[o++] = '"';
    out[o] = '\0';
    return out;
}

void ce_rstrip_inplace(char *s) {
    if (!s) return;
    size_t l = strlen(s);
    while (l > 0 && isspace((unsigned char)s[l - 1])) s[--l] = '\0';
}

char *ce_strip_heap(const char *s) {
    char *d = strdup(s ? s : "");
    if (!d) return NULL;
    char *q = d;
    while (*q && isspace((unsigned char)*q)) q++;
    memmove(d, q, strlen(q) + 1);
    ce_rstrip_inplace(d);
    return d;
}

/* ── Namespace ───────────────────────────────────────────────────────── */
void ce_ns_init(ce_ns_t *ns) { ns->keys = NULL; ns->vals = NULL; ns->n = 0; ns->cap = 0; }

void ce_ns_set(ce_ns_t *ns, const char *key, const char *val) {
    for (size_t i = 0; i < ns->n; i++) {
        if (strcmp(ns->keys[i], key) == 0) {
            free(ns->vals[i]);
            ns->vals[i] = strdup(val ? val : "");
            return;
        }
    }
    if (ns->n == ns->cap) {
        ns->cap = ns->cap ? ns->cap * 2 : 8;
        ns->keys = realloc(ns->keys, sizeof(char *) * ns->cap);
        ns->vals = realloc(ns->vals, sizeof(char *) * ns->cap);
    }
    ns->keys[ns->n] = strdup(key);
    ns->vals[ns->n] = strdup(val ? val : "");
    ns->n++;
}

const char *ce_ns_get(const ce_ns_t *ns, const char *key) {
    if (!ns) return NULL;
    for (size_t i = 0; i < ns->n; i++)
        if (strcmp(ns->keys[i], key) == 0) return ns->vals[i];
    return NULL;
}

bool ce_ns_has(const ce_ns_t *ns, const char *key) {
    return ce_ns_get(ns, key) != NULL;
}

void ce_ns_free(ce_ns_t *ns) {
    for (size_t i = 0; i < ns->n; i++) { free(ns->keys[i]); free(ns->vals[i]); }
    free(ns->keys); free(ns->vals);
    ns->keys = ns->vals = NULL; ns->n = ns->cap = 0;
}

/* ── Symbol registry (importlib boundary: "module:function" -> C fn) ─── */
typedef struct ce_sym_ent { char *id; void *fn; } ce_sym_ent;
static ce_sym_ent *ce_syms = NULL;
static size_t ce_nsyms = 0, ce_symcap = 0;

void coe_register_symbol(const char *id, void *fn) {
    if (!id) return;
    for (size_t i = 0; i < ce_nsyms; i++) {
        if (strcmp(ce_syms[i].id, id) == 0) { ce_syms[i].fn = fn; return; }
    }
    if (ce_nsyms == ce_symcap) {
        ce_symcap = ce_symcap ? ce_symcap * 2 : 16;
        ce_syms = realloc(ce_syms, sizeof(ce_sym_ent) * ce_symcap);
    }
    ce_syms[ce_nsyms].id = strdup(id);
    ce_syms[ce_nsyms].fn = fn;
    ce_nsyms++;
}

void *coe_lookup_symbol(const char *id) {
    if (!id) return NULL;
    for (size_t i = 0; i < ce_nsyms; i++)
        if (strcmp(ce_syms[i].id, id) == 0) return ce_syms[i].fn;
    return NULL;
}

/* ── Parser tree construction (mini-argparse) ────────────────────────── */
static ce_parser *ce_parser_new(const char *prog) {
    ce_parser *p = calloc(1, sizeof(ce_parser));
    p->prog = strdup(prog ? prog : "hermes");
    return p;
}

/* * _ArgumentParser(prog="hermes", add_help=False) plus one add_subparsers(
 * dest="_console_command") action.
 */
/* PoP: ce_parser_root @ hermes_cli/console_engine.py:_parser_root */
ce_parser *ce_parser_root(void) {
    ce_parser *parser = ce_parser_new("hermes");
    ce_action *sub = calloc(1, sizeof(ce_action));
    sub->kind = 2;
    sub->dest = strdup("_console_command");
    parser->actions = realloc(parser->actions, sizeof(ce_action *) * (parser->n_actions + 1));
    parser->actions[parser->n_actions++] = sub;
    return parser;
}

void ce_parser_free(ce_parser *p) {
    if (!p) return;
    for (size_t i = 0; i < p->n_actions; i++) {
        ce_action *a = p->actions[i];
        free(a->dest); free(a->metavar); free(a->help); free(a->type_name); free(a->deflt);
        for (size_t k = 0; k < a->n_subs; k++) {
            free(a->sub_names[k]);
            ce_parser_free(a->sub_parsers[k]);
        }
        free(a->sub_names); free(a->sub_parsers);
        for (size_t k = 0; k < a->n_choices; k++) {
            free(a->choice_actions[k].dest);
            free(a->choice_actions[k].metavar);
            free(a->choice_actions[k].help);
        }
        free(a->choice_actions);
        free(a);
    }
    free(p->actions);
    for (size_t i = 0; i < p->n_defs; i++) { free(p->def_keys[i]); free(p->def_vals[i]); }
    free(p->def_keys); free(p->def_vals);
    free(p->prog); free(p->description);
    free(p);
}

ce_action *ce_add_subparsers(ce_parser *p, const char *dest) {
    ce_action *a = calloc(1, sizeof(ce_action));
    a->kind = 2;
    a->dest = strdup(dest ? dest : "_console_command");
    p->actions = realloc(p->actions, sizeof(ce_action *) * (p->n_actions + 1));
    p->actions[p->n_actions++] = a;
    return a;
}

ce_parser *ce_add_parser(ce_action *subparsers, const char *name) {
    ce_parser *child = ce_parser_new(name);
    if (subparsers->n_subs == subparsers->sub_cap) {
        subparsers->sub_cap = subparsers->sub_cap ? subparsers->sub_cap * 2 : 4;
        subparsers->sub_names = realloc(subparsers->sub_names,
                                        sizeof(char *) * subparsers->sub_cap);
        subparsers->sub_parsers = realloc(subparsers->sub_parsers,
                                          sizeof(ce_parser *) * subparsers->sub_cap);
    }
    subparsers->sub_names[subparsers->n_subs] = strdup(name ? name : "");
    subparsers->sub_parsers[subparsers->n_subs] = child;
    subparsers->n_subs++;
    /* argparse also records a _choices_actions entry per added parser. */
    subparsers->choice_actions = realloc(subparsers->choice_actions,
                                         sizeof(ce_choice_item) * (subparsers->n_choices + 1));
    memset(&subparsers->choice_actions[subparsers->n_choices], 0, sizeof(ce_choice_item));
    subparsers->choice_actions[subparsers->n_choices].dest = strdup(name ? name : "");
    subparsers->n_choices++;
    return child;
}

void ce_add_choice_help(ce_action *subparsers, const char *name,
                        const char *metavar, const char *help) {
    for (size_t i = 0; i < subparsers->n_choices; i++) {
        if (strcmp(subparsers->choice_actions[i].dest, name) == 0) {
            free(subparsers->choice_actions[i].metavar);
            free(subparsers->choice_actions[i].help);
            subparsers->choice_actions[i].metavar = metavar ? strdup(metavar) : NULL;
            subparsers->choice_actions[i].help = help ? strdup(help) : NULL;
            return;
        }
    }
}

void ce_set_defaults(ce_parser *p, const char *key, const char *val) {
    p->def_keys = realloc(p->def_keys, sizeof(char *) * (p->n_defs + 1));
    p->def_vals = realloc(p->def_vals, sizeof(char *) * (p->n_defs + 1));
    p->def_keys[p->n_defs] = strdup(key);
    p->def_vals[p->n_defs] = strdup(val ? val : "");
    p->n_defs++;
}

ce_action *ce_add_argument(ce_parser *p, const char *flag_or_name, bool nargs_plus,
                           bool store_true, const char *type_name,
                           const char *deflt, const char *help) {
    ce_action *a = calloc(1, sizeof(ce_action));
    a->store_true = store_true;
    a->nargs_plus = nargs_plus;
    a->type_name = type_name ? strdup(type_name) : NULL;
    a->help = help ? strdup(help) : NULL;
    if (deflt) { a->deflt = strdup(deflt); a->has_default = true; }
    if (flag_or_name && flag_or_name[0] == '-') {
        a->kind = 1;
        /* argparse dest derivation: "--session-id" -> "session_id" */
        char *d = strdup(flag_or_name + 2);
        for (char *q = d; *q; q++) if (*q == '-') *q = '_';
        a->dest = d;
    } else {
        a->kind = 0;
        a->dest = strdup(flag_or_name ? flag_or_name : "");
    }
    p->actions = realloc(p->actions, sizeof(ce_action *) * (p->n_actions + 1));
    p->actions[p->n_actions++] = a;
    return a;
}

/* * The isinstance(action, _SubParsersAction) filter over parser._actions;
 * returns a malloc'd NULL-terminated array of matching actions.
 */
/* PoP: ce_subparser_actions @ hermes_cli/console_engine.py:_subparser_actions */
ce_action **ce_subparser_actions(const ce_parser *parser) {
    size_t n = 0;
    for (size_t i = 0; i < parser->n_actions; i++)
        if (parser->actions[i]->kind == 2) n++;
    ce_action **out = malloc(sizeof(ce_action *) * (n + 1));
    size_t k = 0;
    for (size_t i = 0; i < parser->n_actions; i++)
        if (parser->actions[i]->kind == 2) out[k++] = parser->actions[i];
    out[n] = NULL;
    return out;
}

/* * Match dest == name or metavar == name; suppressed/absent help -> "".
 */
/* PoP: ce_choice_help @ hermes_cli/console_engine.py:_choice_help */
char *ce_choice_help(const ce_action *action, const char *name) {
    if (!action || !name) return strdup("");
    for (size_t i = 0; i < action->n_choices; i++) {
        const ce_choice_item *c = &action->choice_actions[i];
        const char *dest = c->dest, *mv = c->metavar;
        if ((dest && strcmp(dest, name) == 0) || (mv && strcmp(mv, name) == 0)) {
            if (c->help && strcmp(c->help, CE_SUPPRESS) != 0)
                return strdup(c->help);
            return strdup("");
        }
    }
    return strdup("");
}

/* ── parse_args over the mini-argparse tree ───────────────────────────── */
static void ce_apply_defaults_level(ce_parser *cur, ce_ns_t *ns) {
    for (size_t i = 0; i < cur->n_defs; i++)
        if (!ce_ns_has(ns, cur->def_keys[i]))
            ce_ns_set(ns, cur->def_keys[i], cur->def_vals[i]);
    for (size_t i = 0; i < cur->n_actions; i++) {
        ce_action *a = cur->actions[i];
        if (a->has_default && !ce_ns_has(ns, a->dest))
            ce_ns_set(ns, a->dest, a->deflt);
        if (a->kind == 1 && a->store_true && !ce_ns_has(ns, a->dest))
            ce_ns_set(ns, a->dest, "false");
    }
}

/* dest -> "--dest-with-dashes" */
static void ce_flag_of(const char *dest, char *buf, size_t sz) {
    snprintf(buf, sz, "--");
    size_t o = 2;
    for (const char *q = dest; *q && o + 1 < sz; q++)
        buf[o++] = (*q == '_') ? '-' : *q;
    buf[o] = '\0';
}

int ce_parse_args(ce_parser *root, char **argv, size_t argc, ce_ns_t *ns, char **err) {
    ce_parser *cur = root;
    size_t i = 0;
    /* descend through subparsers while the next token names a choice */
    for (;;) {
        ce_action *sub = NULL;
        for (size_t k = 0; k < cur->n_actions; k++)
            if (cur->actions[k]->kind == 2) { sub = cur->actions[k]; break; }
        if (!sub || i >= argc) break;
        size_t m;
        for (m = 0; m < sub->n_subs; m++)
            if (strcmp(sub->sub_names[m], argv[i]) == 0) break;
        if (m == sub->n_subs) break;
        ce_ns_set(ns, sub->dest, argv[i]);
        cur = sub->sub_parsers[m];
        i++;
    }

    ce_action **positionals = malloc(sizeof(ce_action *) * (cur->n_actions + 1));
    size_t np = 0;
    for (size_t k = 0; k < cur->n_actions; k++)
        if (cur->actions[k]->kind == 0) positionals[np++] = cur->actions[k];
    size_t pi = 0;

    while (i < argc) {
        const char *tok = argv[i];
        if (tok[0] == '-' && tok[1] != '\0' && !isdigit((unsigned char)tok[1])) {
            char namebuf[128];
            const char *eq = strchr(tok, '=');
            size_t nl = eq ? (size_t)(eq - tok) : strlen(tok);
            if (nl >= sizeof(namebuf)) nl = sizeof(namebuf) - 1;
            memcpy(namebuf, tok, nl); namebuf[nl] = '\0';
            ce_action *match = NULL;
            for (size_t k = 0; k < cur->n_actions; k++) {
                ce_action *a = cur->actions[k];
                if (a->kind != 1) continue;
                char flag[128];
                ce_flag_of(a->dest, flag, sizeof(flag));
                if (strcmp(flag, namebuf) == 0) { match = a; break; }
            }
            if (!match) {
                if (asprintf(err, "%s: unrecognized arguments: %s", cur->prog, tok) < 0)
                    *err = NULL;
                free(positionals);
                return -1;
            }
            if (match->store_true) {
                ce_ns_set(ns, match->dest, "true");
                i++;
                continue;
            }
            const char *val;
            if (eq) { val = eq + 1; i++; }
            else if (i + 1 < argc) { val = argv[++i]; i++; }
            else {
                if (asprintf(err, "%s: argument %s: expected one argument",
                             cur->prog, namebuf) < 0) *err = NULL;
                free(positionals);
                return -1;
            }
            if (match->type_name && strcmp(match->type_name, "int") == 0) {
                char *end = NULL;
                (void)strtol(val, &end, 10);
                if (!end || *end != '\0') {
                    if (asprintf(err, "%s: argument %s: invalid int value: '%s'",
                                 cur->prog, match->dest, val) < 0) *err = NULL;
                    free(positionals);
                    return -1;
                }
            }
            ce_ns_set(ns, match->dest, val);
            continue;
        }
        /* positional */
        if (pi >= np) {
            if (asprintf(err, "%s: unrecognized arguments: %s", cur->prog, tok) < 0)
                *err = NULL;
            free(positionals);
            return -1;
        }
        ce_action *a = positionals[pi];
        if (a->nargs_plus) {
            /* consume all remaining tokens into one space-joined value */
            size_t need = 1;
            for (size_t k = i; k < argc; k++) need += strlen(argv[k]) + 1;
            char *joined = malloc(need);
            joined[0] = '\0';
            for (; i < argc; i++) {
                if (joined[0]) strcat(joined, " ");
                strcat(joined, argv[i]);
            }
            ce_ns_set(ns, a->dest, joined);
            free(joined);
            pi++;
            continue;
        }
        ce_ns_set(ns, a->dest, tok);
        i++;
        pi++;
    }
    /* required positional missing (no default)? */
    for (; pi < np; pi++) {
        ce_action *a = positionals[pi];
        if (!a->has_default) {
            if (asprintf(err, "%s: the following arguments are required: %s",
                         cur->prog, a->dest) < 0) *err = NULL;
            free(positionals);
            return -1;
        }
    }
    free(positionals);

    /* apply defaults down the chain of chosen subparsers */
    ce_parser *lvl = root;
    ce_apply_defaults_level(lvl, ns);
    for (;;) {
        ce_action *sub = NULL;
        for (size_t k = 0; k < lvl->n_actions; k++)
            if (lvl->actions[k]->kind == 2) { sub = lvl->actions[k]; break; }
        if (!sub) break;
        const char *chosen = ce_ns_get(ns, sub->dest);
        if (!chosen) break;
        ce_parser *next = NULL;
        for (size_t m = 0; m < sub->n_subs; m++)
            if (strcmp(sub->sub_names[m], chosen) == 0) { next = sub->sub_parsers[m]; break; }
        if (!next) break;
        lvl = next;
        ce_apply_defaults_level(lvl, ns);
    }
    return 0;
}

/* ── summaries walk ───────────────────────────────────────────────────── */
typedef struct ce_sum_ctx {
    char ***paths;   /* each entry: depth-length array of path segments */
    size_t *depths;
    char **sums;
    size_t n, cap;
} ce_sum_ctx;

static void ce_sum_add(ce_sum_ctx *ctx, char **path, size_t depth, const char *summary) {
    /* dict.setdefault(child_path, summary): first writer wins */
    for (size_t i = 0; i < ctx->n; i++) {
        if (ctx->depths[i] != depth) continue;
        bool same = true;
        for (size_t k = 0; k < depth && same; k++)
            if (strcmp(ctx->paths[i][k], path[k]) != 0) same = false;
        if (same) return;
    }
    if (ctx->n == ctx->cap) {
        ctx->cap = ctx->cap ? ctx->cap * 2 : 16;
        ctx->paths = realloc(ctx->paths, sizeof(char **) * ctx->cap);
        ctx->depths = realloc(ctx->depths, sizeof(size_t) * ctx->cap);
        ctx->sums = realloc(ctx->sums, sizeof(char *) * ctx->cap);
    }
    char **copy = malloc(sizeof(char *) * depth);
    for (size_t k = 0; k < depth; k++) copy[k] = strdup(path[k]);
    ctx->paths[ctx->n] = copy;
    ctx->depths[ctx->n] = depth;
    ctx->sums[ctx->n] = strdup(summary);
    ctx->n++;
}

static void ce_walk_parser(ce_parser *cur, char **path, size_t depth, ce_sum_ctx *ctx) {
    ce_action **subs = ce_subparser_actions(cur);
    for (size_t s = 0; subs[s]; s++) {
        ce_action *action = subs[s];
        for (size_t m = 0; m < action->n_subs; m++) {
            const char *name = action->sub_names[m];
            ce_parser *child = action->sub_parsers[m];
            char *ch = ce_choice_help(action, name);
            char *cs = coe_clean_summary(ch);
            char *cd = coe_clean_summary(child->description);
            free(ch);
            const char *summary = (cs && *cs) ? cs : cd;
            char **child_path = malloc(sizeof(char *) * (depth + 1));
            memcpy(child_path, path, sizeof(char *) * depth);
            child_path[depth] = (char *)name;
            if (summary && *summary)
                ce_sum_add(ctx, child_path, depth + 1, summary);
            free(cs);
            free(cd);
            ce_walk_parser(child, child_path, depth + 1, ctx);
            free(child_path);
        }
    }
    free(subs);
}

static char *ce_sums_to_json(const ce_sum_ctx *ctx) {
    size_t cap = 64;
    char *out = malloc(cap);
    size_t o = 0;
    out[o++] = '{'; out[o] = '\0';
    for (size_t i = 0; i < ctx->n; i++) {
        size_t klen = 1;
        for (size_t k = 0; k < ctx->depths[i]; k++) klen += strlen(ctx->paths[i][k]) + 1;
        char *key = malloc(klen + 1);
        key[0] = '\0';
        for (size_t k = 0; k < ctx->depths[i]; k++) {
            if (key[0]) strcat(key, " ");
            strcat(key, ctx->paths[i][k]);
        }
        char *ek = ce_json_escape(key);
        char *ev = ce_json_escape(ctx->sums[i]);
        size_t need = strlen(out) + strlen(ek) + strlen(ev) + 4;
        if (need > cap) { cap = need * 2; out = realloc(out, cap); }
        if (i) out[o++] = ',';
        o += (size_t)snprintf(out + o, cap - o, "%s:%s", ek, ev);
        free(ek); free(ev); free(key);
    }
    if (o + 2 > cap) { cap = o + 4; out = realloc(out, cap); }
    out[o++] = '}';
    out[o] = '\0';
    return out;
}

/* * Depth-first walk of the parser tree collecting path->summary; first writer
 * wins per path (dict.setdefault). Returns malloc'd JSON object string keyed
 * by the space-joined path tuple.
 */
/* PoP: ce_summaries_from_parser @ hermes_cli/console_engine.py:_summaries_from_parser */
char *ce_summaries_from_parser(ce_parser *parser) {
    ce_sum_ctx ctx = {0};
    ce_walk_parser(parser, NULL, 0, &ctx);
    char *json = ce_sums_to_json(&ctx);
    for (size_t i = 0; i < ctx.n; i++) {
        for (size_t k = 0; k < ctx.depths[i]; k++) free(ctx.paths[i][k]);
        free(ctx.paths[i]);
        free(ctx.sums[i]);
    }
    free(ctx.paths); free(ctx.depths); free(ctx.sums);
    return json;
}

/* PoP: ce_noop_console_command @ hermes_cli/console_engine.py:_noop_console_command */
int ce_noop_console_command(ce_ns_t *args, char **err) {
    (void)args; (void)err;
    return 0; /* Python returns None */
}

/* ── lru_cache(maxsize=None) for the summary extractors ──────────────── */
typedef struct ce_cache_ent { char *key; char *json; } ce_cache_ent;
static ce_cache_ent *ce_cache = NULL;
static size_t ce_ncache = 0, ce_cachecap = 0;

static const char *ce_cache_get(const char *key) {
    for (size_t i = 0; i < ce_ncache; i++)
        if (strcmp(ce_cache[i].key, key) == 0) return ce_cache[i].json;
    return NULL;
}

static void ce_cache_put(const char *key, const char *json) {
    if (ce_ncache == ce_cachecap) {
        ce_cachecap = ce_cachecap ? ce_cachecap * 2 : 8;
        ce_cache = realloc(ce_cache, sizeof(ce_cache_ent) * ce_cachecap);
    }
    ce_cache[ce_ncache].key = strdup(key);
    ce_cache[ce_ncache].json = strdup(json);
    ce_ncache++;
}

static char *ce_cache_key3(const char *a, const char *b, const char *c) {
    size_t need = strlen(a) + strlen(b) + strlen(c) + 4;
    char *k = malloc(need);
    snprintf(k, need, "%s\x1f%s\x1f%s", a, b, c);
    return k;
}

typedef int (*coemain_fn)(ce_ns_t *ns, char **exit_msg);

/* * Build a throwaway parser tree via module.builder(subparsers,
 * main_handler=_noop_console_command), then walk it for summaries. The
 * importlib.import_module step maps onto the symbol registry; an unknown
 * module/builder takes Python's `except Exception: return {}` path.
 * Memoized by args (lru_cache maxsize=None).
 */
/* PoP: ce_extracted_summaries @ hermes_cli/console_engine.py:_extracted_summaries */
char *ce_extracted_summaries(const char *module_name, const char *builder_name,
                             const char *main_handler_name) {
    char *key = ce_cache_key3(module_name, builder_name, main_handler_name);
    const char *hit = ce_cache_get(key);
    if (hit) { char *r = strdup(hit); free(key); return r; }
    char *json = strdup("{}");
    ce_parser *root = ce_parser_root();
    ce_action *sub = NULL;
    for (size_t k = 0; k < root->n_actions; k++)
        if (root->actions[k]->kind == 2) { sub = root->actions[k]; break; }
    void *fn = coe_lookup_symbol_two(module_name, builder_name);
    if (sub && fn) {
        ((void (*)(ce_action *, const char *))fn)(sub, main_handler_name);
        free(json);
        json = ce_summaries_from_parser(root);
    }
    ce_parser_free(root);
    ce_cache_put(key, json);
    free(key);
    return json;
}

/* * top = subparsers.add_parser(root); register(top); walk. Memoized.
 */
/* PoP: ce_registered_summaries @ hermes_cli/console_engine.py:_registered_summaries */
char *ce_registered_summaries(const char *root_name, const char *module_name,
                              const char *register_name) {
    char *key = ce_cache_key3(root_name, module_name, register_name);
    const char *hit = ce_cache_get(key);
    if (hit) { char *r = strdup(hit); free(key); return r; }
    char *json = strdup("{}");
    ce_parser *root = ce_parser_root();
    ce_action *sub = NULL;
    for (size_t k = 0; k < root->n_actions; k++)
        if (root->actions[k]->kind == 2) { sub = root->actions[k]; break; }
    void *fn = coe_lookup_symbol_two(module_name, register_name);
    if (sub && fn) {
        ce_parser *top = ce_add_parser(sub, root_name);
        ((void (*)(ce_parser *))fn)(top);
        free(json);
        json = ce_summaries_from_parser(root);
    }
    ce_parser_free(root);
    ce_cache_put(key, json);
    free(key);
    return json;
}

/* * getattr(module, builder_name)(subparsers); walk. Memoized.
 */
/* PoP: ce_builder_summaries @ hermes_cli/console_engine.py:_builder_summaries */
char *ce_builder_summaries(const char *module_name, const char *builder_name) {
    char *key = ce_cache_key3(module_name, builder_name, "");
    const char *hit = ce_cache_get(key);
    if (hit) { char *r = strdup(hit); free(key); return r; }
    char *json = strdup("{}");
    ce_parser *root = ce_parser_root();
    ce_action *sub = NULL;
    for (size_t k = 0; k < root->n_actions; k++)
        if (root->actions[k]->kind == 2) { sub = root->actions[k]; break; }
    void *fn = coe_lookup_symbol_two(module_name, builder_name);
    if (sub && fn) {
        ((void (*)(ce_action *))fn)(sub);
        free(json);
        json = ce_summaries_from_parser(root);
    }
    ce_parser_free(root);
    ce_cache_put(key, json);
    free(key);
    return json;
}

/* * Same shape as _builder_summaries with an add_* function. Memoized.
 */
/* PoP: ce_adder_summaries @ hermes_cli/console_engine.py:_adder_summaries */
char *ce_adder_summaries(const char *module_name, const char *add_name) {
    return ce_builder_summaries(module_name, add_name);
}

/* ── fd-level stdout/stderr capture (contextlib.redirect_*) ──────────── */
/* fd-level analog of contextlib.redirect_stdout+redirect_stderr. */
/* PoP: coe_capture_output @ hermes_cli/console_engine.py:_capture_output */
char *coe_capture_output(int (*fn)(void *ctx, char **exit_msg), void *ctx, char **err) {
    *err = NULL;
    fflush(stdout); fflush(stderr);
    FILE *tmp = tmpfile();
    if (!tmp) return NULL;
    int saved_out = dup(STDOUT_FILENO);
    int saved_err = dup(STDERR_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    dup2(fileno(tmp), STDERR_FILENO);
    char *emsg = NULL;
    int code = fn(ctx, &emsg);
    fflush(stdout); fflush(stderr);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_err, STDERR_FILENO);
    close(saved_out); close(saved_err);
    /* read captured text back */
    rewind(tmp);
    size_t cap = 4096, len = 0;
    char *text = malloc(cap);
    size_t r;
    while ((r = fread(text + len, 1, cap - len - 1, tmp)) > 0) {
        len += r;
        text[len] = '\0';
        if (cap - len < 2) { cap *= 2; text = realloc(text, cap); }
    }
    fclose(tmp);

    /* Python: SystemExit(str) sets code=1 + message; SystemExit(int) keeps code. */
    if (code != 0 || emsg) {
        char *msg = emsg ? ce_strip_heap(emsg) : NULL;
        free(emsg);
        if (!msg || !*msg) {
            char *tstrip = ce_strip_heap(text);
            if (tstrip && *tstrip) msg = tstrip;
            else {
                free(tstrip);
                if (asprintf(&msg, "Command exited with status %d", code > 0 ? code : 1) < 0)
                    msg = NULL;
            }
        } else {
            /* message wins even when empty text exists */
        }
        free(text);
        *err = msg ? msg : strdup("Command exited with status 1");
        return NULL;
    }
    ce_rstrip_inplace(text);
    return text;
}

/* "module:function" registry lookup (importlib.import_module + getattr). */
void *coe_lookup_symbol_two(const char *module, const char *func) {
    if (!module || !func) return NULL;
    size_t need = strlen(module) + strlen(func) + 2;
    char *id = malloc(need);
    snprintf(id, need, "%s:%s", module, func);
    void *fn = coe_lookup_symbol(id);
    free(id);
    return fn;
}

/* * Windows-safe splitter: delegates to the _subprocess_compat port
 * (split_command_line); ValueError becomes ConsoleCommandError. Returns a
 * malloc'd JSON array of tokens; on failure returns NULL with *err set to
 * "Could not parse command: <exc>".
 */
/* PoP: ce_split_line @ hermes_cli/console_engine.py:_split_line */
char *ce_split_line(const char *line, char **err) {
    extern char *subprocess_compat_split_command_line(const char *line, int *err);
    int perr = 0;
    char *tokens = subprocess_compat_split_command_line(line, &perr);
    if (perr || !tokens) {
        if (asprintf(err, "Could not parse command: %s",
                     perr ? "No closing quotation" : "empty command") < 0) *err = NULL;
        free(tokens);
        return NULL;
    }
    return tokens;
}

/* * namespace.func(args) or ConsoleCommandError when no handler is attached.
 * The handler runs inside coe_capture_output at the call sites; here the
 * SystemExit protocol is: return 0 ok, >0 exit code, -1 with *msg set for
 * sys.exit("message").
 */
/* PoP: ce_invoke_namespace @ hermes_cli/console_engine.py:_invoke_namespace */
char *ce_invoke_namespace(ce_ns_t *ns, char **err) {
    const char *fid = ce_ns_get(ns, "func");
    coemain_fn fn = fid ? (coemain_fn)coe_lookup_symbol(fid) : NULL;
    if (!fn) {
        *err = strdup("No handler is available for that console command.");
        return NULL;
    }
    char *emsg = NULL;
    int rc = fn(ns, &emsg);
    if (rc != 0 || emsg) {
        if (emsg && *emsg) *err = emsg;
        else {
            free(emsg);
            if (asprintf(err, "Command exited with status %d", rc > 0 ? rc : 1) < 0) *err = NULL;
        }
        return NULL;
    }
    return NULL; /* output arrives via the captured streams */
}

/* PoP: ce_set_attrs @ hermes_cli/console_engine.py:_set_attrs */
ce_ns_t *ce_set_attrs(ce_ns_t *args, const char *name, const char *value) {
    ce_ns_set(args, name, value ? value : "");
    return args;
}

/* ── dispatch core shared by the four subcommand flavors ────────────── */
typedef struct ce_dispatch_spec {
    int kind; /* 1 extracted, 2 registered, 3 builder, 4 adder */
    const char *root;
    const char *const *fixed;
    size_t n_fixed;
    const char *module_name;
    const char *name1; /* builder_name / register_name / add_name */
    const char *name2; /* main_handler_name / handler_name / NULL */
    ce_ns_update_fn namespace_update;
} ce_dispatch_spec;

static char *ce_argv_join_root(const ce_dispatch_spec *spec,
                               char **args, size_t nargs,
                               char ***out_argv, size_t *out_argc) {
    /* argv = [root, *fixed, *args] */
    size_t argc = 1 + spec->n_fixed + nargs;
    char **argv = malloc(sizeof(char *) * argc);
    size_t k = 0;
    argv[k++] = strdup(spec->root ? spec->root : "");
    for (size_t i = 0; i < spec->n_fixed; i++) argv[k++] = strdup(spec->fixed[i]);
    for (size_t i = 0; i < nargs; i++) argv[k++] = strdup(args && args[i] ? args[i] : "");
    *out_argv = argv;
    *out_argc = argc;
    return NULL;
}

static int ce_run_capture_trampoline(void *ctx, char **exit_msg) {
    ce_ns_t *ns = ctx;
    const char *fid = ce_ns_get(ns, "func");
    coemain_fn fn = fid ? (coemain_fn)coe_lookup_symbol(fid) : NULL;
    if (!fn) return 0; /* invoke_namespace reports the error afterwards */
    return fn(ns, exit_msg);
}

static char *ce_dispatch_common(const ce_dispatch_spec *spec,
                                char **args, size_t nargs, char **err) {
    ce_parser *parser = ce_parser_root();
    ce_action *subparsers = NULL;
    bool built = false;
    for (size_t k = 0; k < parser->n_actions; k++)
        if (parser->actions[k]->kind == 2) { subparsers = parser->actions[k]; break; }

    void *modfn = coe_lookup_symbol_two(spec->module_name, spec->name1);

    if (subparsers && modfn) {
        if (spec->kind == 1) {
            /* builder(subparsers, main_handler=getattr(main_module, name)) */
            ((void (*)(ce_action *, const char *))modfn)(subparsers, spec->name2);
            built = true;
        } else if (spec->kind == 2) {
            ce_parser *top = ce_add_parser(subparsers, spec->root);
            ((void (*)(ce_parser *))modfn)(top);
            if (spec->name2)
                ce_set_defaults(top, "func", spec->name2);
            built = true;
        } else if (spec->kind == 3) {
            ce_parser *top = ((ce_parser * (*)(ce_action *))modfn)(subparsers);
            if (top) ce_set_defaults(top, "func", spec->name2);
            built = true;
        } else { /* kind == 4: adder */
            ((void (*)(ce_action *))modfn)(subparsers);
            built = true;
        }
    }
    if (!built) {
        /* importlib/getattr failure escapes execute()'s handler in Python;
         * surface it as the equivalent console error rather than crashing. */
        if (asprintf(err, "Could not parse command: module '%s' has no attribute '%s'",
                     spec->module_name, spec->name1) < 0) *err = NULL;
        ce_parser_free(parser);
        return NULL;
    }

    char **argv = NULL;
    size_t argc = 0;
    ce_argv_join_root(spec, args, nargs, &argv, &argc);
    ce_ns_t ns;
    ce_ns_init(&ns);
    char *perr = NULL;
    if (ce_parse_args(parser, argv, argc, &ns, &perr) != 0) {
        *err = perr; /* "<prog>: <argparse message>" -> ConsoleCommandError */
        for (size_t i = 0; i < argc; i++) free(argv[i]);
        free(argv);
        ce_ns_free(&ns);
        ce_parser_free(parser);
        return NULL;
    }
    for (size_t i = 0; i < argc; i++) free(argv[i]);
    free(argv);

    if (spec->namespace_update) {
        if (spec->namespace_update(&ns, err) != 0) {
            ce_ns_free(&ns);
            ce_parser_free(parser);
            return NULL;
        }
    }

    /* _capture_output(lambda: _invoke_namespace(namespace)) */
    const char *fid = ce_ns_get(&ns, "func");
    coemain_fn fn = fid ? (coemain_fn)coe_lookup_symbol(fid) : NULL;
    char *result = NULL;
    if (!fn) {
        *err = strdup("No handler is available for that console command.");
    } else {
        result = coe_capture_output(ce_run_capture_trampoline, &ns, err);
    }
    ce_ns_free(&ns);
    ce_parser_free(parser);
    return result;
}

/* PoP: ce_dispatch_extracted_subcommand @ hermes_cli/console_engine.py:_dispatch_extracted_subcommand */
char *ce_dispatch_extracted_subcommand(const char *root,
                                       const char *const *fixed, size_t n_fixed,
                                       const char *args[], size_t nargs,
                                       const char *module_name,
                                       const char *builder_name,
                                       const char *main_handler_name,
                                       ce_ns_update_fn namespace_update,
                                       char **err) {
    ce_dispatch_spec s = {1, root, fixed, n_fixed, module_name, builder_name,
                          main_handler_name, namespace_update};
    return ce_dispatch_common(&s, (char **)args, nargs, err);
}

/* PoP: ce_dispatch_registered_subcommand @ hermes_cli/console_engine.py:_dispatch_registered_subcommand */
char *ce_dispatch_registered_subcommand(const char *root,
                                        const char *const *fixed, size_t n_fixed,
                                        const char *args[], size_t nargs,
                                        const char *module_name,
                                        const char *register_name,
                                        const char *handler_name,
                                        ce_ns_update_fn namespace_update,
                                        char **err) {
    ce_dispatch_spec s = {2, root, fixed, n_fixed, module_name, register_name,
                          handler_name, namespace_update};
    return ce_dispatch_common(&s, (char **)args, nargs, err);
}

/* PoP: ce_dispatch_builder_subcommand @ hermes_cli/console_engine.py:_dispatch_builder_subcommand */
char *ce_dispatch_builder_subcommand(const char *root,
                                     const char *const *fixed, size_t n_fixed,
                                     const char *args[], size_t nargs,
                                     const char *module_name,
                                     const char *builder_name,
                                     const char *main_handler_name,
                                     ce_ns_update_fn namespace_update,
                                     char **err) {
    ce_dispatch_spec s = {3, root, fixed, n_fixed, module_name, builder_name,
                          main_handler_name, namespace_update};
    return ce_dispatch_common(&s, (char **)args, nargs, err);
}

/* PoP: ce_dispatch_adder_subcommand @ hermes_cli/console_engine.py:_dispatch_adder_subcommand */
char *ce_dispatch_adder_subcommand(const char *root,
                                   const char *const *fixed, size_t n_fixed,
                                   const char *args[], size_t nargs,
                                   const char *module_name,
                                   const char *add_name,
                                   ce_ns_update_fn namespace_update,
                                   char **err) {
    ce_dispatch_spec s = {4, root, fixed, n_fixed, module_name, add_name,
                          NULL, namespace_update};
    return ce_dispatch_common(&s, (char **)args, nargs, err);
}

/* ── handler closures ────────────────────────────────────────────────── */
static ce_hcmd *ce_hcmd_new(int kind, const char *root,
                            const char *const *fixed, size_t n_fixed,
                            const char *module, const char *name1, const char *name2) {
    ce_hcmd *h = calloc(1, sizeof(ce_hcmd));
    h->kind = kind;
    h->root = strdup(root);
    h->n_fixed = n_fixed;
    h->fixed = malloc(sizeof(char *) * (n_fixed ? n_fixed : 1));
    for (size_t i = 0; i < n_fixed; i++) h->fixed[i] = strdup(fixed[i]);
    h->module = strdup(module);
    h->name1 = strdup(name1);
    h->name2 = name2 ? strdup(name2) : NULL;
    return h;
}

void ce_hcmd_free(ce_hcmd *h) {
    if (!h) return;
    for (size_t i = 0; i < h->n_fixed; i++) free(h->fixed[i]);
    free(h->fixed); free(h->root); free(h->module); free(h->name1); free(h->name2);
    free(h);
}

/* Invoke a stored closure through its matching dispatch flavor. */
char *ce_hcmd_invoke(ce_hcmd *h, char **args, size_t nargs, ce_ns_update_fn upd,
                     char **err) {
    switch (h->kind) {
    case 1:
        return ce_dispatch_extracted_subcommand(h->root, (const char *const *)h->fixed,
                                                h->n_fixed, (const char **)args, nargs,
                                                h->module, h->name1, h->name2, upd, err);
    case 2:
        return ce_dispatch_registered_subcommand(h->root, (const char *const *)h->fixed,
                                                 h->n_fixed, (const char **)args, nargs,
                                                 h->module, h->name1, h->name2, upd, err);
    case 3:
        return ce_dispatch_builder_subcommand(h->root, (const char *const *)h->fixed,
                                              h->n_fixed, (const char **)args, nargs,
                                              h->module, h->name1, h->name2, upd, err);
    default:
        return ce_dispatch_adder_subcommand(h->root, (const char *const *)h->fixed,
                                            h->n_fixed, (const char **)args, nargs,
                                            h->module, h->name1, upd, err);
    }
}

/* PoP: ce_extracted_handler @ hermes_cli/console_engine.py:_extracted_handler */
ce_hcmd *ce_extracted_handler(const char *root, const char *const *fixed, size_t n_fixed,
                              const char *module_name, const char *builder_name,
                              const char *main_handler_name) {
    return ce_hcmd_new(1, root, fixed, n_fixed, module_name, builder_name, main_handler_name);
}

/* PoP: ce_registered_handler @ hermes_cli/console_engine.py:_registered_handler */
ce_hcmd *ce_registered_handler(const char *root, const char *const *fixed, size_t n_fixed,
                               const char *module_name, const char *register_name,
                               const char *handler_name) {
    return ce_hcmd_new(2, root, fixed, n_fixed, module_name, register_name, handler_name);
}

/* PoP: ce_builder_handler @ hermes_cli/console_engine.py:_builder_handler */
ce_hcmd *ce_builder_handler(const char *root, const char *const *fixed, size_t n_fixed,
                            const char *module_name, const char *builder_name,
                            const char *main_handler_name) {
    return ce_hcmd_new(3, root, fixed, n_fixed, module_name, builder_name, main_handler_name);
}

/* PoP: ce_adder_handler @ hermes_cli/console_engine.py:_adder_handler */
ce_hcmd *ce_adder_handler(const char *root, const char *const *fixed, size_t n_fixed,
                          const char *module_name, const char *add_name) {
    return ce_hcmd_new(4, root, fixed, n_fixed, module_name, add_name, NULL);
}

/* ── engine plumbing shared with console_engine_ports.c ─────────────── */
void coe_engine_init(coe_engine *eng, long output_limit) {
    eng->output_limit = output_limit > 0 ? output_limit : 20000;
    eng->history = NULL; eng->n_hist = 0; eng->hist_cap = 0;
    eng->cmds = NULL; eng->n_cmds = 0; eng->cmds_cap = 0;
}

void coe_history_append(coe_engine *eng, const char *line) {
    if (eng->n_hist == eng->hist_cap) {
        eng->hist_cap = eng->hist_cap ? eng->hist_cap * 2 : 16;
        eng->history = realloc(eng->history, sizeof(char *) * eng->hist_cap);
    }
    eng->history[eng->n_hist++] = strdup(line);
}

/* HermesConsoleEngine.register() storage layer used by the family registrar. */
void coe_engine_register(coe_engine *eng, const char *key, const char *usage,
                         const char *summary, void *handler, int kind,
                         bool mutating, const char *confirmation) {
    for (size_t i = 0; i < eng->n_cmds; i++) {
        if (strcmp(eng->cmds[i].key, key) == 0) {
            free(eng->cmds[i].usage); free(eng->cmds[i].summary);
            free(eng->cmds[i].confirmation);
            eng->cmds[i].usage = strdup(usage);
            eng->cmds[i].summary = strdup(summary);
            eng->cmds[i].handler = handler;
            eng->cmds[i].kind = kind;
            eng->cmds[i].mutating = mutating;
            eng->cmds[i].confirmation = confirmation ? strdup(confirmation) : strdup("");
            return;
        }
    }
    if (eng->n_cmds == eng->cmds_cap) {
        eng->cmds_cap = eng->cmds_cap ? eng->cmds_cap * 2 : 32;
        eng->cmds = realloc(eng->cmds, sizeof(coe_cmd) * eng->cmds_cap);
    }
    coe_cmd *c = &eng->cmds[eng->n_cmds++];
    c->key = strdup(key);
    c->usage = strdup(usage);
    c->summary = strdup(summary);
    c->handler = handler;
    c->kind = kind;
    c->mutating = mutating;
    c->confirmation = strdup(confirmation ? confirmation : "");
}

static coe_cmd *coe_find_cmd(coe_engine *eng, const char *key) {
    for (size_t i = 0; i < eng->n_cmds; i++)
        if (strcmp(eng->cmds[i].key, key) == 0) return &eng->cmds[i];
    return NULL;
}

/* difflib.get_close_matches(probe, available, n=3, cutoff=0.45) */
char **coe_close_matches(const char *probe, char **available, size_t navail,
                                size_t n, double cutoff, size_t *out_n) {
    typedef struct { char *s; double r; } cand_t;
    cand_t *cands = malloc(sizeof(cand_t) * (navail ? navail : 1));
    size_t nc = 0;
    for (size_t i = 0; i < navail; i++) {
        double r = difflib_ratio(probe, available[i]);
        if (r >= cutoff) { cands[nc].s = available[i]; cands[nc].r = r; nc++; }
    }
    /* sort by ratio desc (stable enough for suggestion output) */
    for (size_t i = 0; i + 1 < nc; i++)
        for (size_t j = 0; j + 1 < nc - i; j++)
            if (cands[j].r < cands[j + 1].r) {
                cand_t t = cands[j]; cands[j] = cands[j + 1]; cands[j + 1] = t;
            }
    if (nc > n) nc = n;
    char **out = malloc(sizeof(char *) * (nc ? nc : 1));
    for (size_t i = 0; i < nc; i++) out[i] = strdup(cands[i].s);
    *out_n = nc;
    free(cands);
    return out;
}

/* PoP: coe_register_command_family @ hermes_cli/console_engine.py:_register_command_family */
void coe_register_command_family(coe_engine *eng, const char *root,
                                 const char *const *paths, size_t n_paths,
                                 ce_hcmd *(*handler_factory)(const char *const *fixed,
                                                             size_t n_fixed),
                                 const char *const *mutating, size_t n_mutating,
                                 const char *summary, const char *summaries_json,
                                 const char *confirmation) {
    /* summaries dict keyed by space-joined full path; parsed on demand. */
    for (size_t p = 0; p < n_paths; p++) {
        /* child_path comes in space-joined ("boards list") */
        const char *child = paths[p];
        size_t need = strlen(root) + strlen(child) + 2;
        char *full_path = malloc(need);
        snprintf(full_path, need, "%s%s%s", root, *child ? " " : "", child);
        char *usage = strdup(full_path); /* " ".join(full_path) */

        bool mut = false;
        for (size_t m = 0; m < n_mutating; m++)
            if (strcmp(mutating[m], child) == 0) { mut = true; break; }

        char *command_summary = NULL;
        if (summary && *summary) {
            command_summary = strdup(summary);
        } else {
            /* summaries.get(full_path) from the JSON map */
            command_summary = NULL;
            if (summaries_json && *summaries_json) {
                json_t *obj = json_parse(summaries_json, NULL);
                if (obj && json_is_object(obj)) {
                    const json_t *v = json_object_get(obj, full_path);
                    if (v && json_is_string(v) && v->str_val && *v->str_val)
                        command_summary = strdup(v->str_val);
                    json_free(obj);
                }
            }
            if (!command_summary) {
                if (asprintf(&command_summary, "Run `hermes %s`.", usage) < 0)
                    command_summary = strdup("");
            }
        }
        char *confirm = NULL;
        if (confirmation && *confirmation) confirm = strdup(confirmation);
        else if (asprintf(&confirm, "Run `hermes %s`?", usage) < 0) confirm = strdup("");

        /* fixed tuple = tuple(child_path): split back into segments */
        char **fixed = NULL;
        size_t n_fixed = 0;
        char *tmpc = strdup(child), *sp = tmpc, *seg;
        while ((seg = strtok_r(sp, " ", &sp))) {
            fixed = realloc(fixed, sizeof(char *) * (n_fixed + 1));
            fixed[n_fixed++] = strdup(seg);
            sp = NULL;
        }
        free(tmpc);

        ce_hcmd *h = handler_factory((const char *const *)fixed, n_fixed);
        for (size_t i = 0; i < n_fixed; i++) free(fixed[i]);
        free(fixed);

        coe_engine_register(eng, full_path, usage, command_summary, h, 0, mut, confirm);
        free(full_path); free(usage); free(command_summary); free(confirm);
    }
}

/* ── ConsoleResult JSON model ───────────────────────────────────────── */
char *coe_result_json(const char *status, const char *output,
                      const char *command, const char *confirmation_message) {
    char *eo = ce_json_escape(output ? output : "");
    char *ec = ce_json_escape(command ? command : "");
    char *em = ce_json_escape(confirmation_message ? confirmation_message : "");
    char *out = NULL;
    if (asprintf(&out, "{\"status\": \"%s\", \"output\": %s, \"command\": %s, "
                      "\"confirmation_message\": %s}",
                 status ? status : "ok", eo, ec, em) < 0) out = NULL;
    free(eo); free(ec); free(em);
    return out;
}

/* PoP: coe_help_text @ hermes_cli/console_engine.py:help_text */
char *coe_help_text(coe_engine *eng, const char *subject, char **err) {
    if (subject && *subject) {
        /* tokens = subject.split(); command,_args = self._resolve_command(tokens) */
        char *tmp = strdup(subject);
        char **toks = NULL;
        size_t ntok = 0;
        char *sp2 = tmp, *seg;
        while ((seg = strtok_r(sp2, " \t", &sp2))) {
            toks = realloc(toks, sizeof(char *) * (ntok + 1));
            toks[ntok++] = seg;
            sp2 = NULL;
        }
        free(tmp);
        coe_cmd *command = NULL;
        char *rej = (ntok > 0) ? coe_rejection_for((const char **)toks, ntok)
                               : strdup("");
        if (rej && *rej) {
            if (asprintf(err, "%s", rej) < 0) *err = NULL;
            free(rej); free(toks);
            return NULL;
        }
        free(rej);
        size_t maxk = ntok < 3 ? ntok : 3;
        for (size_t sz = maxk; sz >= 1 && !command; sz--) {
            size_t klen = 1;
            for (size_t k = 0; k < sz; k++) klen += strlen(toks[k]) + 1;
            char key[256];
            size_t o = 0;
            key[0] = '\0';
            for (size_t k = 0; k < sz; k++) {
                if (o) key[o++] = ' ';
                o += (size_t)snprintf(key + o, sizeof(key) - o, "%s", toks[k]);
            }
            command = coe_find_cmd(eng, key);
        }
        if (!command) {
            /* suggestions via difflib.get_close_matches(probe, available, 3, 0.45) */
            char **available = malloc(sizeof(char *) * (eng->n_cmds ? eng->n_cmds : 1));
            for (size_t i = 0; i < eng->n_cmds; i++) available[i] = eng->cmds[i].key;
            const char *probe = ntok > 1 ? NULL : (ntok ? toks[0] : "");
            char probebuf[512];
            if (ntok > 1) {
                probebuf[0] = '\0';
                for (size_t k = 0; k < 2 && k < ntok; k++) {
                    if (probebuf[0]) strcat(probebuf, " ");
                    strcat(probebuf, toks[k]);
                }
                probe = probebuf;
            }
            size_t nsug = 0;
            char **sug = coe_close_matches(probe, available, eng->n_cmds, 3, 0.45, &nsug);
            char *sugjoin = strdup("");
            for (size_t i = 0; i < nsug; i++) {
                char *old = sugjoin;
                if (asprintf(&sugjoin, "%s%s%s", old, i ? ", " : "", sug[i]) < 0) { sugjoin = old; break; }
                if (sugjoin != old) free(old);
            }
            if (nsug == 0) { free(sugjoin); sugjoin = NULL; }
            if (sugjoin)
                asprintf(err, "Unsupported Hermes Console command: %s. Did you mean: %s?",
                         probe, sugjoin);
            else
                asprintf(err, "Unsupported Hermes Console command: %s.", probe);
            for (size_t i = 0; i < nsug; i++) free(sug[i]);
            free(sug); free(sugjoin); free(available); free(toks);
            return NULL;
        }
        free(toks);
        char *out = NULL;
        if (asprintf(&out, "%s\n%s", command->usage, command->summary) < 0) out = NULL;
        return out;
    }

    /* full listing sorted by usage */
    coe_engine *e = eng;
    size_t *order = malloc(sizeof(size_t) * (e->n_cmds ? e->n_cmds : 1));
    for (size_t i = 0; i < e->n_cmds; i++) order[i] = i;
    for (size_t i = 0; i + 1 < e->n_cmds; i++)
        for (size_t j = 0; j + 1 < e->n_cmds - i; j++)
            if (strcmp(e->cmds[order[j]].usage, e->cmds[order[j + 1]].usage) > 0) {
                size_t t = order[j]; order[j] = order[j + 1]; order[j + 1] = t;
            }
    size_t cap = 256;
    char *out = malloc(cap);
    size_t o = 0;
    o += (size_t)snprintf(out + o, cap - o,
                          "Hermes Console\n\nSupported commands:\n");
    for (size_t i = 0; i < e->n_cmds; i++) {
        const coe_cmd *c = &e->cmds[order[i]];
        char *ts = ce_table_summary(c->summary, 76);
        char linebuf[1024];
        snprintf(linebuf, sizeof(linebuf), "%s %-*s %s\n",
                 c->mutating ? "*" : " ", 32, c->usage, ts);
        free(ts);
        while (o + strlen(linebuf) + 1 > cap) { cap *= 2; out = realloc(out, cap); }
        o += (size_t)snprintf(out + o, cap - o, "%s", linebuf);
    }
    free(order);
    const char *tail = "\n* requires confirmation\n"
                       "Built-ins: help, help <command>, history, clear, exit, quit\n";
    while (o + strlen(tail) + 1 > cap) { cap *= 2; out = realloc(out, cap); }
    strcat(out, tail);
    return out;
}

/* * Returns a ConsoleResult JSON string, or NULL when the head token is not a
 * builtin (Python returns None). Caller frees.
 */
/* PoP: coe_execute_builtin @ hermes_cli/console_engine.py:_execute_builtin */
char *coe_execute_builtin(coe_engine *eng, char **tokens, size_t ntok) {
    if (ntok == 0) return NULL;
    const char *head = tokens[0];
    if (strcmp(head, "help") == 0) {
        /* subject = " ".join(tokens[1:]).strip() or None */
        char *subject = strdup("");
        for (size_t i = 1; i < ntok; i++) {
            char *old = subject;
            if (asprintf(&subject, "%s%s%s", old, i > 1 ? " " : "", tokens[i]) < 0)
                subject = old;
            else free(old);
        }
        char *subj_strip = ce_strip_heap(subject);
        free(subject);
        char *err = NULL;
        char *ht = coe_help_text(eng, (subj_strip && *subj_strip) ? subj_strip : NULL, &err);
        free(subj_strip);
        char *res;
        if (ht) res = coe_result_json("ok", ht, "", "");
        else res = coe_result_json("error", err ? err : "", "", "");
        free(ht); free(err);
        return res;
    }
    if (strcmp(head, "history") == 0) {
        if (eng->n_hist == 0)
            return coe_result_json("ok", "No history yet.", "", "");
        size_t cap = 128, len = 0;
        char *out = malloc(cap);
        out[0] = '\0';
        for (size_t i = 0; i < eng->n_hist; i++) {
            char row[1200];
            snprintf(row, sizeof(row), "%zu: %s", i + 1, eng->history[i]);
            while (len + strlen(row) + 2 > cap) { cap *= 2; out = realloc(out, cap); }
            if (i) strcat(out, "\n");
            strcat(out, row);
            len = strlen(out);
        }
        char *res = coe_result_json("ok", out, "", "");
        free(out);
        return res;
    }
    if (strcmp(head, "clear") == 0)
        return coe_result_json("clear", "\033[2J\033[H", "", "");
    if (strcmp(head, "exit") == 0 || strcmp(head, "quit") == 0)
        return coe_result_json("exit", "", "", "");
    return NULL;
}

/* PoP: coe_rejection_for @ hermes_cli/console_engine.py:_rejection_for */
char *coe_rejection_for(const char **tokens, size_t ntok) {
    if (ntok == 0) return strdup("");
    const char *first = tokens[0];
    static const char *blocked_top[] = {
        "acp", "chat", "claw", "completion", "dashboard", "desktop", "fallback",
        "gateway", "gui", "login", "logout", "model", "moa", "oneshot",
        "proxy", "serve", "setup", "uninstall", "update", "whatsapp",
        "whatsapp-cloud",
    };
    struct { const char *a, *b, *msg; } blocked_pairs[] = {
        {"config", "edit", "`config edit` opens an editor and is not available in Hermes Console."},
        {"mcp", "serve", "`mcp serve` starts a server and is not available in Hermes Console."},
        {"profile", "alias", "`profile alias` creates shell wrappers and is not available in Hermes Console."},
        {"skills", "config", "`skills config` is interactive and is not available in Hermes Console."},
        {"skills", "publish", "`skills publish` is not available in Hermes Console."},
        {"portal", "login", "`portal login` is interactive and is not available in Hermes Console."},
        {"portal", "open", "`portal open` opens a browser and is not available in Hermes Console."},
        {"kanban", "tail", "`kanban tail` streams output and is not available in Hermes Console."},
        {"kanban", "watch", "`kanban watch` streams output and is not available in Hermes Console."},
        {"kanban", "daemon", "`kanban daemon` starts a service and is not available in Hermes Console."},
        {"kanban", "dispatcher", "`kanban dispatcher` starts a worker and is not available in Hermes Console."},
        {"kanban", "swarm", "`kanban swarm` starts agent work and is not available in Hermes Console."},
        {"kanban", "decompose", "`kanban decompose` starts agent work and is not available in Hermes Console."},
        {"kanban", "specify", "`kanban specify` starts agent work and is not available in Hermes Console."},
        {"kanban", "gc", "`kanban gc` is not available in Hermes Console."},
    };
    if (first[0] == '-') {
        char *out = NULL;
        if (asprintf(&out, "%s is not available in Hermes Console.", first) < 0) out = NULL;
        return out;
    }
    for (size_t i = 0; i < sizeof(blocked_top) / sizeof(blocked_top[0]); i++) {
        if (strcmp(first, blocked_top[i]) == 0) {
            char *out = NULL;
            if (asprintf(&out, "`hermes %s` is not available in Hermes Console.", first) < 0)
                out = NULL;
            return out;
        }
    }
    if (ntok >= 2) {
        for (size_t i = 0; i < sizeof(blocked_pairs) / sizeof(blocked_pairs[0]); i++) {
            if (strcmp(tokens[0], blocked_pairs[i].a) == 0 &&
                strcmp(tokens[1], blocked_pairs[i].b) == 0)
                return strdup(blocked_pairs[i].msg);
        }
        if ((strcmp(tokens[0], "sessions") == 0) &&
            (strcmp(tokens[1], "delete") == 0 || strcmp(tokens[1], "prune") == 0))
            return strdup("`sessions delete` and `sessions prune` are not available "
                          "in Hermes Console.");
    }
    return strdup("");
}

/* PoP: coe_help_result @ hermes_cli/console_engine.py:_help_result */
char *coe_help_result(coe_engine *eng) {
    char *ht = coe_help_text(eng, NULL, NULL);
    char *res = coe_result_json("ok", ht ? ht : "", "", "");
    free(ht);
    return res;
}

/* PoP: coe_cap_output @ hermes_cli/console_engine.py:_cap_output */
char *coe_cap_output(coe_engine *eng, const char *output) {
    if (!output) return strdup("");
    size_t len = strlen(output);
    if ((long)len <= eng->output_limit) return strdup(output);
    long omitted = (long)len - eng->output_limit;
    char *out = malloc((size_t)eng->output_limit + 128);
    memcpy(out, output, (size_t)eng->output_limit);
    out[eng->output_limit] = '\0';
    char tail[96];
    snprintf(tail, sizeof(tail), "\n... output truncated (%ld bytes omitted)", omitted);
    strcat(out, tail);
    return out;
}

/* * Returns NULL when clean, or a malloc'd "Usage: <usage>" error message.
 */
/* PoP: coe_expect_no_args @ hermes_cli/console_engine.py:_expect_no_args */
char *coe_expect_no_args(char **args, size_t nargs, const char *usage) {
    if (nargs > 0) {
        char *out = NULL;
        if (asprintf(&out, "Usage: %s", usage) < 0) out = NULL;
        return out;
    }
    return NULL;
}

/* * Skip nested prompts after the console-level confirmation has happened.
 * Returns 0, or -1 with *err set for the api-key rejection.
 */
/* PoP: coe_apply_confirmed_defaults @ hermes_cli/console_engine.py:_apply_confirmed_defaults */
int coe_apply_confirmed_defaults(ce_ns_t *args, char **err) {
    if (ce_ns_has(args, "yes"))
        ce_ns_set(args, "yes", "true");
    const char *ccmd = ce_ns_get(args, "_console_command");
    if (ccmd && strcmp(ccmd, "import") == 0)
        ce_ns_set(args, "force", "true");
    const char *cp = ce_ns_get(args, "checkpoints_command");
    if (cp && (strcmp(cp, "prune") == 0 || strcmp(cp, "clear") == 0 ||
               strcmp(cp, "clear-legacy") == 0))
        ce_ns_set(args, "force", "true");
    const char *pa = ce_ns_get(args, "plugins_action");
    if (pa && strcmp(pa, "install") == 0) {
        const char *en = ce_ns_get(args, "enable");
        const char *nen = ce_ns_get(args, "no_enable");
        bool enb = en && (strcmp(en, "true") == 0 || strcmp(en, "True") == 0 ||
                          strcmp(en, "1") == 0);
        bool nenb = nen && (strcmp(nen, "true") == 0 || strcmp(nen, "True") == 0 ||
                            strcmp(nen, "1") == 0);
        if (!enb && !nenb)
            ce_ns_set(args, "no_enable", "true");
    }
    const char *aa = ce_ns_get(args, "auth_action");
    if (aa && strcmp(aa, "add") == 0) {
        const char *at = ce_ns_get(args, "auth_type");
        const char *ak = ce_ns_get(args, "api_key");
        if (at && (strcmp(at, "api-key") == 0 || strcmp(at, "api_key") == 0) &&
            !(ak && *ak)) {
            *err = strdup("auth add --type api-key requires --api-key in Hermes Console.");
            return -1;
        }
    }
    if (ce_ns_has(args, "import_name")) {
        /* profile import has no prompt flag; leave it alone. */
        return 0;
    }
    const char *sa = ce_ns_get(args, "skills_action");
    if (sa && (strcmp(sa, "install") == 0 || strcmp(sa, "reset") == 0 ||
               strcmp(sa, "opt-out") == 0 || strcmp(sa, "repair-official") == 0))
        ce_ns_set(args, "yes", "true");
    const char *mc = ce_ns_get(args, "memory_command");
    if (mc && strcmp(mc, "reset") == 0)
        ce_ns_set(args, "yes", "true");
    return 0;
}
