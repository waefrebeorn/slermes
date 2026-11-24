/*
 * cli_terminal_input.c — real C11 port of upstream Hermes
 * hermes_cli/cli_terminal_input.py.
 *
 * PoP: w17_clitermina_e2aba1_is_ghostty                          @ hermes_cli/cli_terminal_input.py:_is_ghostty_terminal
 * PoP: w17_clitermina_e2aba1_supports_extended_enter_keys       @ hermes_cli/cli_terminal_input.py:_terminal_supports_extended_enter_keys
 * PoP: w17_clitermina_e2aba1_multiline_shortcuts_enabled        @ hermes_cli/cli_terminal_input.py:_cli_multiline_shortcuts_enabled
 * PoP: w17_clitermina_e2aba1_is_backslash_line_continuation     @ hermes_cli/cli_terminal_input.py:_is_backslash_line_continuation
 * PoP: w17_clitermina_e2aba1_apply_backslash_line_continuation  @ hermes_cli/cli_terminal_input.py:_apply_backslash_line_continuation
 * PoP: w17_clitermina_e2aba1_should_auto_attach_clipboard_image @ hermes_cli/cli_terminal_input.py:_should_auto_attach_clipboard_image_on_paste
 * PoP: w17_clitermina_e2aba1_status_bar_visible                 @ hermes_cli/cli_terminal_input.py:_status_bar_visible_from_display_config
 * PoP: w17_clitermina_e2aba1_split_path_input                   @ hermes_cli/cli_terminal_input.py:_split_path_input
 *
 * Environment/config/terminal-width inputs are injected explicitly, matching
 * the oracle's fake objects. All string manipulation copies bytes out of
 * json_t nodes before building results; no interior pointer is ever freed.
 */
#include "noc_w17_clitermina_e2aba1.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *noc_ct_dupstr_len(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *noc_ct_dupstr(const char *s) {
    return noc_ct_dupstr_len(s, strlen(s));
}

/* Python truthiness for JSON-decoded values. */
static int noc_ct_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* (env.get(key) or "").strip() decoded from a JSON string value. */
static const char *noc_ct_env_get(const json_t *env, const char *key) {
    if (!json_is_object(env)) return "";
    const json_t *v = json_obj_get(env, key);
    if (!v || v->type != JSON_STRING || !v->str_val) return "";
    return v->str_val;
}

static const char *noc_ct_strip(const char *p, const char **out_end) {
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    if (out_end) *out_end = e;
    return p;
}

static int noc_ct_ieq(const char *p, const char *e, const char *want) {
    size_t n = (size_t)(e - p);
    return strlen(want) == n && strncasecmp(p, want, n) == 0;
}
static int noc_ct_eq(const char *p, const char *e, const char *want) {
    size_t n = (size_t)(e - p);
    return strlen(want) == n && memcmp(p, want, n) == 0;
}

int w17_clitermina_e2aba1_is_ghostty(const json_t *env) {
    const char *e1, *tp = noc_ct_strip(noc_ct_env_get(env, "TERM_PROGRAM"), &e1);
    if (noc_ct_eq(tp, e1, "ghostty")) return 1; /* case-sensitive, unlike TERM */
    const char *e2, *term = noc_ct_strip(noc_ct_env_get(env, "TERM"), &e2);
    return noc_ct_ieq(term, e2, "xterm-ghostty") ? 1 : 0;
}

int w17_clitermina_e2aba1_supports_extended_enter_keys(const json_t *env) {
    if (noc_ct_truthy(json_is_object(env) ? json_obj_get(env, "WT_SESSION") : NULL)) return 1;
    const char *e1, *tp = noc_ct_strip(noc_ct_env_get(env, "TERM_PROGRAM"), &e1);
    const char *e2, *term = noc_ct_strip(noc_ct_env_get(env, "TERM"), &e2);
    static const char *const allow[] = {"iTerm.app", "WezTerm", "ghostty", "vscode"};
    for (int i = 0; i < 4; i++) if (noc_ct_ieq(tp, e1, allow[i])) return 1;
    if (noc_ct_truthy(json_is_object(env) ? json_obj_get(env, "KITTY_WINDOW_ID") : NULL)) return 1;
    char low[128];
    size_t n = (size_t)(e2 - term);
    if (n >= sizeof low) n = sizeof low - 1;
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)term[i]);
    low[n] = '\0';
    if (strstr(low, "kitty")) return 1;
    if (!strcmp(low, "xterm-ghostty")) return 1;
    if (!strncmp(low, "tmux", 4)) return 1;
    /* term_program.lower() == "tmux" */
    char tpl[128];
    size_t tn = (size_t)(e1 - tp);
    if (tn >= sizeof tpl) tn = sizeof tpl - 1;
    for (size_t i = 0; i < tn; i++) tpl[i] = (char)tolower((unsigned char)tp[i]);
    tpl[tn] = '\0';
    return !strcmp(tpl, "tmux") ? 1 : 0;
}

int w17_clitermina_e2aba1_multiline_shortcuts_enabled(const json_t *config) {
    const json_t *display = json_is_object(config) ? json_obj_get(config, "display") : NULL;
    if (!json_is_object(display)) return 1;
    const json_t *v = json_obj_get(display, "cli_multiline_shortcuts");
    if (!v) return 1;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) {
        const char *end, *p = noc_ct_strip(v->str_val ? v->str_val : "", &end);
        static const char *const falsy[] = {"0", "false", "no", "off", "disabled"};
        for (int i = 0; i < 5; i++) if (noc_ct_ieq(p, end, falsy[i])) return 0;
        return 1;
    }
    return 1; /* not isinstance(value, str) -> not False -> True */
}

/* Locate the trailing backslash+[ \t]* match before an optional final '\n'.
 * Returns the backslash index, or SIZE_MAX when absent. */
static size_t noc_ct_backslash_pos(const char *text) {
    size_t len = strlen(text);
    size_t end = len;
    if (end > 0 && text[end - 1] == '\n') end--;
    size_t i = end;
    while (i > 0 && (text[i - 1] == ' ' || text[i - 1] == '\t')) i--;
    if (i > 0 && text[i - 1] == '\\') return i - 1;
    return (size_t)-1;
}

int w17_clitermina_e2aba1_is_backslash_line_continuation(const char *text) {
    return noc_ct_backslash_pos(text ? text : "") != (size_t)-1 ? 1 : 0;
}

json_t *w17_clitermina_e2aba1_apply_backslash_line_continuation(const char *text) {
    const char *s = text ? text : "";
    size_t bs = noc_ct_backslash_pos(s);
    if (bs == (size_t)-1) {
        /* No match: re.sub returns the original text, then + "\n". */
        size_t n = strlen(s);
        char *buf = (char *)malloc(n + 2);
        if (!buf) return json_string("");
        memcpy(buf, s, n); buf[n] = '\n'; buf[n + 1] = '\0';
        json_t *out = json_string(buf); free(buf); return out;
    }
    size_t len = strlen(s);
    size_t end = len;
    if (end > 0 && s[end - 1] == '\n') end--;
    size_t tail = len - end;
    /* re.sub removes the match, then the helper appends a literal "\n". */
    size_t n = bs + tail + 1;
    char *buf = (char *)malloc(n + 1);
    if (!buf) return json_string("");
    memcpy(buf, s, bs);
    memcpy(buf + bs, s + end, tail);
    buf[bs + tail] = '\n';
    buf[n] = '\0';
    json_t *out = json_string(buf); free(buf); return out;
}

int w17_clitermina_e2aba1_should_auto_attach_clipboard_image(const char *pasted_text) {
    const char *end;
    const char *p = noc_ct_strip(pasted_text ? pasted_text : "", &end);
    return p == end ? 1 : 0;
}

int w17_clitermina_e2aba1_status_bar_visible(const json_t *config) {
    const json_t *dc = json_is_object(config) ? config : NULL;
    const json_t *v = dc ? json_obj_get(dc, "statusbar") : NULL;
    if (!v) v = dc ? json_obj_get(dc, "tui_statusbar") : NULL;
    if (!v) return 1; /* default "top" */
    if (v->type == JSON_STRING) {
        const char *end, *p = noc_ct_strip(v->str_val ? v->str_val : "", &end);
        static const char *const falsy[] = {"0", "false", "hidden", "no", "off"};
        for (int i = 0; i < 5; i++) if (noc_ct_ieq(p, end, falsy[i])) return 0;
        return 1;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 1; /* is not False */
}

json_t *w17_clitermina_e2aba1_split_path_input(const char *raw) {
    const char *end;
    const char *s = noc_ct_strip(raw ? raw : "", &end);
    size_t n = (size_t)(end - s);
    char *path = NULL, *rest = NULL;
    if (n == 0) {
        path = noc_ct_dupstr(""); rest = noc_ct_dupstr("");
    } else if (s[0] == '"' || s[0] == '\'') {
        char q = s[0];
        size_t pos = 1;
        int closed = 0;
        while (pos < n) {
            char ch = s[pos];
            if (ch == '\\' && pos + 1 < n) { pos += 2; continue; }
            if (ch == q) { closed = 1; break; }
            pos++;
        }
        size_t plen = (closed ? pos : n) - 1;
        path = (char *)malloc(plen + 1);
        if (path) { memcpy(path, s + 1, plen); path[plen] = '\0'; }
        const char *rstart = s + (closed ? pos + 1 : n);
        const char *rend;
        const char *rp = noc_ct_strip(rstart, &rend);
        rest = noc_ct_dupstr_len(rp, (size_t)(rend - rp));
    } else {
        size_t pos = 0;
        while (pos < n) {
            char ch = s[pos];
            if (ch == '\\' && pos + 1 < n && s[pos + 1] == ' ') pos += 2;
            else if (ch == ' ') break;
            else pos++;
        }
        path = (char *)malloc(pos + 1);
        if (path) {
            size_t j = 0;
            for (size_t i = 0; i < pos; i++) {
                if (s[i] == '\\' && i + 1 < pos && s[i + 1] == ' ') { path[j++] = ' '; i++; }
                else path[j++] = s[i];
            }
            path[j] = '\0';
        }
        const char *rend;
        const char *rp = noc_ct_strip(s + pos, &rend);
        rest = noc_ct_dupstr_len(rp, (size_t)(rend - rp));
    }
    json_t *out = json_array();
    json_append(out, json_string(path ? path : ""));
    json_append(out, json_string(rest ? rest : ""));
    free(path); free(rest);
    return out;
}
