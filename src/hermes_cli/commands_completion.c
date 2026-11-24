/*
 * commands_completion.c — C home for upstream hermes_cli/commands_completion.py.
 *
 * PoP: w18_commandsco_af81e3_split_args @ hermes_cli/commands_completion.py:_split_args
 * PoP: w18_commandsco_af81e3_extract_path_word @ hermes_cli/commands_completion.py:_extract_path_word
 * PoP: w18_commandsco_af81e3_score_path @ hermes_cli/commands_completion.py:_score_path
 * PoP: w18_commandsco_af81e3_normalize_skill_token @ hermes_cli/commands_completion.py:SlashCommandCompleter._normalize_skill_token
 * PoP: w18_commandsco_af81e3_completion_text @ hermes_cli/commands_completion.py:SlashCommandCompleter._completion_text
 * PoP: w18_commandsco_af81e3_extract_context_word @ hermes_cli/commands_completion.py:SlashCommandCompleter._extract_context_word
 *
 * Self-contained, libjson-only.  Windows ntpath.basename splits on both '/'
 * and '\\', so `_score_path` does the same.
 */
#include "noc_w18_commandsco_af81e3.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *substrdup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *ascii_lower(const char *s) {
    char *p = dupstr(s ? s : "");
    if (p) for (char *q = p; *q; ++q) *q = (char)tolower((unsigned char)*q);
    return p;
}
static void free_strv(char **v) {
    if (!v) return;
    for (size_t i = 0; v[i]; ++i) free(v[i]);
    free(v);
}

/* Python str.split() over ASCII whitespace; NULL-terminated token array. */
static char **split_ws(const char *s) {
    size_t cap = 8, n = 0;
    char **out = (char **)calloc(cap, sizeof(char *));
    if (!out) return NULL;
    const char *p = s ? s : "";
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (n + 2 > cap) {
            size_t nc = cap * 2;
            char **np = (char **)realloc(out, nc * sizeof(char *));
            if (!np) { free_strv(out); return NULL; }
            out = np; cap = nc;
        }
        out[n++] = substrdup(start, (size_t)(p - start));
    }
    out[n] = NULL;
    return out;
}

struct json_t *w18_commandsco_af81e3_split_args(const char *sub_text) {
    char **parts = split_ws(sub_text);
    size_t n = 0;
    if (parts) while (parts[n]) n++;
    int trailing_space = sub_text && strchr(sub_text, '\0') &&
                         sub_text[strlen(sub_text) - 1] == ' ';
    struct json_t *completed = json_array();
    const char *partial = "";
    if (trailing_space || n == 0) {
        for (size_t i = 0; i < n; ++i) json_append(completed, json_string(parts[i]));
    } else {
        for (size_t i = 0; i + 1 < n; ++i) json_append(completed, json_string(parts[i]));
        partial = parts[n - 1];
    }
    struct json_t *out = json_array();
    json_append(out, completed);
    json_append(out, json_string(partial));
    free_strv(parts);
    return out;
}

char *w18_commandsco_af81e3_extract_path_word(const char *text) {
    const char *word = text ? text : "";
    const char *sp = strrchr(word, ' ');
    if (sp) word = sp + 1;
    if (*word && !strstr(word, "://") && strchr(word, '/')) return dupstr(word);
    return NULL;
}

static const char *py_basename(const char *p) {
    const char *last = p;
    for (const char *q = p; *q; ++q)
        if (*q == '/' || *q == '\\') last = q + 1;
    return last;
}

int w18_commandsco_af81e3_score_path(const char *filepath, const char *query) {
    if (!query || !*query) return 1;
    char *lower_file = ascii_lower(py_basename(filepath ? filepath : ""));
    char *lower_q = ascii_lower(query);
    char *lower_path = ascii_lower(filepath ? filepath : "");
    int score = 0;
    if (!strcmp(lower_file, lower_q)) score = 100;
    else if (strncmp(lower_file, lower_q, strlen(lower_q)) == 0) score = 80;
    else if (strstr(lower_file, lower_q)) score = 60;
    else if (strstr(lower_path, lower_q)) score = 40;
    if (score) { free(lower_file); free(lower_q); free(lower_path); return score; }

    size_t qi = 0, boundary_hits = 0, qlen = strlen(lower_q);
    char prev = '_';
    for (const char *c = lower_file; *c; ++c) {
        if (qi < qlen && *c == lower_q[qi]) {
            if (prev == '_' || prev == '-' || prev == '.' || prev == '/') boundary_hits++;
            qi++;
        }
        prev = *c;
    }
    free(lower_file); free(lower_q); free(lower_path);
    if (qi < qlen) return 0;
    return ((double)boundary_hits >= (double)qlen * 0.5) ? 35 : 25;
}

char *w18_commandsco_af81e3_normalize_skill_token(const char *token) {
    const char *p = token ? token : "";
    while (*p == '/') p++;
    size_t n = strlen(p);
    char *buf = (char *)malloc(n + 2);
    if (!buf) return NULL;
    buf[0] = '/';
    for (size_t i = 0; i < n; ++i) {
        char c = p[i];
        if (c == '_') c = '-';
        buf[i + 1] = (char)tolower((unsigned char)c);
    }
    buf[n + 1] = '\0';
    return buf;
}

char *w18_commandsco_af81e3_completion_text(const char *cmd_name, const char *word) {
    const char *c = cmd_name ? cmd_name : "";
    const char *w = word ? word : "";
    int picker = !strcmp(c, "model") || !strcmp(c, "skin") || !strcmp(c, "personality");
    int exact = !strcmp(c, w) && !picker;
    size_t n = strlen(c);
    char *buf = (char *)malloc(n + 2);
    if (!buf) return NULL;
    memcpy(buf, c, n);
    if (exact) { buf[n] = ' '; buf[n + 1] = '\0'; }
    else buf[n] = '\0';
    return buf;
}

char *w18_commandsco_af81e3_extract_context_word(const char *text) {
    const char *word = text ? text : "";
    const char *sp = strrchr(word, ' ');
    if (sp) word = sp + 1;
    if (*word == '@') return dupstr(word);
    return NULL;
}
