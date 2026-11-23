/*
 * approval_env_split.c — C port of upstream Hermes tools/approval_env_split.py.
 *
 * PoP: w11_approvalen_62f1c4_split_env_string @ tools/approval_env_split.py:_split_env_string
 * PoP: w11_approvalen_62f1c4_env_split_payload @ tools/approval_env_split.py:_env_split_payload
 *
 * Dynamic ${NAME} expansion is deliberately not evaluated, matching Python.
 */
#include "noc_w11_approvalen_62f1c4.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARGS 128
#define MAX_WORD 4096

static int env_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* GNU env -S literal escape map; returns 1 and sets *out when char is mapped. */
static int env_escape(char c, char *out) {
    switch (c) {
        case 'f': *out = '\f'; return 1;
        case 'n': *out = '\n'; return 1;
        case 'r': *out = '\r'; return 1;
        case 't': *out = '\t'; return 1;
        case 'v': *out = '\v'; return 1;
        case '#': case '$': case '"': case '\'': case '\\': *out = c; return 1;
        default: return 0;
    }
}

struct words {
    char items[MAX_ARGS][MAX_WORD];
    int count;
};

static void words_add(struct words *w, const char *s, size_t n) {
    if (w->count >= MAX_ARGS) return;
    if (n >= MAX_WORD) n = MAX_WORD - 1;
    memcpy(w->items[w->count], s, n);
    w->items[w->count][n] = 0;
    w->count++;
}

static json_t *words_to_json(const struct words *w) {
    json_t *arr = json_array();
    for (int i = 0; i < w->count; i++) json_append(arr, json_string(w->items[i]));
    return arr;
}

struct json_t *w11_approvalen_62f1c4_split_env_string(const char *payload) {
    if (!payload) return json_null();
    struct words args = {{ {0} }, 0};
    char word[MAX_WORD];
    size_t wn = 0;
    int have_word = 0;
    char quote = 0;
    size_t len = strlen(payload), index = 0;
    while (index < len) {
        char c = payload[index++];
        if (c == '\\') {
            if (index == len) return json_null();
            char esc = payload[index];
            if (quote == '\'' && esc != '\'' && esc != '\\') {
                if (wn + 1 < MAX_WORD) word[wn++] = c;
                have_word = 1;
                continue;
            }
            index++;
            if (esc == 'c') {
                if (quote) return json_null();
                break;
            }
            if (esc == '_' && !quote) {
                if (have_word) words_add(&args, word, wn);
                wn = 0; have_word = 0;
                continue;
            }
            char mapped = 0;
            if (esc == '_') mapped = ' ';
            else if (!env_escape(esc, &mapped)) return json_null();
            if (wn + 1 < MAX_WORD) word[wn++] = mapped;
            have_word = 1;
            continue;
        }
        if ((c == '\'' || c == '"') && (!quote || c == quote)) {
            quote = quote ? 0 : c;
            have_word = 1;
            continue;
        }
        if (!quote && env_space(c)) {
            if (have_word) words_add(&args, word, wn);
            wn = 0; have_word = 0;
            continue;
        }
        if (!quote && c == '#' && !have_word) break;
        if (c == '$' && quote != '\'') return json_null();
        if (wn + 1 < MAX_WORD) word[wn++] = c;
        have_word = 1;
    }
    if (quote) return json_null();
    if (have_word) words_add(&args, word, wn);
    return words_to_json(&args);
}

/* shlex.quote: leave shell-safe words bare, otherwise single-quote. */
static void shlex_append(char *out, size_t cap, const char *s) {
    if (!*s) { strncat(out, "''", cap - strlen(out) - 1); return; }
    int safe = 1;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              strchr("_@%+=:,./-", (int)c))) { safe = 0; break; }
    }
    if (safe) { strncat(out, s, cap - strlen(out) - 1); return; }
    strncat(out, "'", cap - strlen(out) - 1);
    for (const char *p = s; *p; p++) {
        if (*p == '\'') strncat(out, "'\"'\"'", cap - strlen(out) - 1);
        else { char t[2] = {*p, 0}; strncat(out, t, cap - strlen(out) - 1); }
    }
    strncat(out, "'", cap - strlen(out) - 1);
}

static void join_args(char *out, size_t cap, char **items, int count) {
    out[0] = 0;
    for (int i = 0; i < count; i++) {
        if (i) strncat(out, " ", cap - strlen(out) - 1);
        shlex_append(out, cap, items[i]);
    }
}

/* env's valued short options (from tools.approval_wrappers._short_bundle). */
static int env_valued_short(char c) {
    return c == 'u' || c == 'C' || c == 'a' || c == 'S';
}

static int long_option_with_arg(const char *opt) {
    static const char *const opts[] = {"-a", "--argv0", "-C", "--chdir", "-S", "--split-string", "-u", "--unset", NULL};
    for (int i = 0; opts[i]; i++) if (!strcmp(opts[i], opt)) return 1;
    return 0;
}

static json_t *env_split_payload_rec(char **tok, int n, int depth) {
    int index = 1;
    while (index < n) {
        const char *token = tok[index];
        if (!strcmp(token, "--") || token[0] != '-') return json_null();
        const char *eq = strchr(token, '=');
        char option[512];
        size_t olen = eq ? (size_t)(eq - token) : strlen(token);
        if (olen >= sizeof(option)) olen = sizeof(option) - 1;
        memcpy(option, token, olen); option[olen] = 0;
        int has_eq = eq != NULL;
        const char *value = has_eq ? eq + 1 : "";
        int is_split = !strcmp(option, "--split-string");
        int attached_mode = 0;
        const char *attached = NULL;
        if (is_split) {
            attached_mode = 1;
            attached = has_eq ? value : NULL;
        } else if (token[1] == '-') {
            if (!has_eq && long_option_with_arg(option)) index += 2;
            else index += 1;
            continue;
        } else {
            char owner = 0;
            const char *rest = "";
            for (size_t i = 1; token[i]; i++) {
                if (env_valued_short(token[i])) {
                    owner = token[i];
                    rest = token + i + 1;
                    break;
                }
            }
            if (owner != 'S') {
                index += (owner && !*rest) ? 2 : 1;
                continue;
            }
            attached_mode = 1;
            attached = *rest ? rest : NULL;
        }
        if (attached_mode && attached == NULL) index += 1;
        const char *payload = attached ? attached : (index < n ? tok[index] : "");
        json_t *args = w11_approvalen_62f1c4_split_env_string(payload);
        if (!args || args->type != JSON_ARRAY) { json_free(args); return json_null(); }
        /* Build rebuilt = args + tokens[index+1:] */
        char *rebuilt[MAX_ARGS + 64];
        char *owned[MAX_ARGS + 64];
        int rc = 0, owned_count = 0;
        for (size_t i = 0; i < json_len(args) && rc < MAX_ARGS; i++) {
            json_t *e = json_get(args, i);
            owned[rc] = e && e->type == JSON_STRING ? strdup(e->str_val ? e->str_val : "") : strdup("");
            rebuilt[rc] = owned[rc];
            rc++; owned_count++;
        }
        json_free(args);
        for (int i = index + 1; i < n && rc < MAX_ARGS + 64; i++) rebuilt[rc++] = tok[i];
        if (rc > 0 && rebuilt[0][0] == '-' && depth < 8) {
            char *nested_tok[MAX_ARGS + 65];
            nested_tok[0] = "env";
            for (int i = 0; i < rc; i++) nested_tok[i + 1] = rebuilt[i];
            json_t *nested = env_split_payload_rec(nested_tok, rc + 1, depth + 1);
            if (nested && nested->type == JSON_STRING) {
                for (int i = 0; i < owned_count; i++) free(owned[i]);
                return nested;
            }
            json_free(nested);
        }
        char joined[16384];
        join_args(joined, sizeof(joined), rebuilt, rc);
        for (int i = 0; i < owned_count; i++) free(owned[i]);
        return json_string(joined);
    }
    return json_null();
}

struct json_t *w11_approvalen_62f1c4_env_split_payload(const struct json_t *tokens) {
    if (!tokens || tokens->type != JSON_ARRAY || json_len(tokens) == 0) return json_null();
    char *owned[MAX_ARGS + 64];
    int n = 0;
    for (size_t i = 0; i < json_len(tokens) && n < MAX_ARGS + 64; i++) {
        json_t *e = json_get(tokens, i);
        owned[n++] = e && e->type == JSON_STRING ? strdup(e->str_val ? e->str_val : "") : strdup("");
    }
    json_t *out = env_split_payload_rec(owned, n, 0);
    for (int i = 0; i < n; i++) free(owned[i]);
    return out;
}
