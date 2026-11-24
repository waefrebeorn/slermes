/*
 * terminal_tool_macos_open.c -- real C11 port of the macOS ``open`` frontmost
 * raise-ladder of upstream Hermes tools/terminal_tool_macos_open.py.
 *
 * PoP: w20_terminalto_4615fc_transform @ tools/terminal_tool_macos_open.py:_transform_macos_open_command
 * PoP: w20_terminalto_4615fc_ladder @ tools/terminal_tool_macos_open.py:_build_macos_open_raise_ladder
 * PoP: w20_terminalto_4615fc_osascript_app @ tools/terminal_tool_macos_open.py:_osascript_app
 * PoP: w20_terminalto_4615fc_quote @ tools/terminal_tool_macos_open.py:shlex.quote
 * PoP: w20_terminalto_4615fc_split @ tools/terminal_tool_macos_open.py:shlex.split
 *
 * The transform is pure and deterministic: shlex.split the command, decide
 * conservatively whether it is a file-opening ``open``, and append a verified
 * raise-ladder.  Nothing here runs a subprocess -- the ladder is text the shell
 * executes later -- so the whole module is portable and testable off macOS.
 */
#include "noc_w20_terminalto_4615fc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TO_TOKEN_CAP 64
#define TO_APP_SPLICE "\"'\"$_H_APP\"'\""

/* --- string builder ------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} to_sb;

static char *to_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static void to_sb_init(to_sb *sb) {
    sb->cap = 128;
    sb->len = 0;
    sb->buf = (char *)malloc(sb->cap);
    if (sb->buf) sb->buf[0] = '\0';
}

static void to_sb_add(to_sb *sb, const char *s) {
    size_t n;
    if (!sb->buf || !s) return;
    n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        size_t cap = sb->cap;
        char *nb;
        while (sb->len + n + 1 > cap) cap *= 2;
        nb = (char *)realloc(sb->buf, cap);
        if (!nb) return;
        sb->buf = nb;
        sb->cap = cap;
    }
    memcpy(sb->buf + sb->len, s, n + 1);
    sb->len += n;
}

static char *to_sb_take(to_sb *sb) {
    char *out = sb->buf;
    sb->buf = NULL;
    sb->len = sb->cap = 0;
    return out;
}

/* --- shlex.split (posix, comments=False) --------------------------------- */

static int to_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int to_split(const char *s, char **tok, int maxtok, int *count) {
    char buf[8192];
    size_t i = 0;
    int n = 0;

    *count = 0;
    if (!s) return 1;
    while (s[i]) {
        size_t bl = 0;
        int have = 0, done = 0;
        while (s[i] && to_is_ws(s[i])) i++;
        if (!s[i]) break;
        while (s[i] && !done) {
            char c = s[i];
            if (c == '\'') {
                size_t j = i + 1;
                while (s[j] && s[j] != '\'') {
                    if (bl + 1 < sizeof(buf)) buf[bl++] = s[j];
                    j++;
                }
                if (!s[j]) return 0;            /* ValueError: No closing quotation */
                i = j + 1;
                have = 1;
            } else if (c == '"') {
                size_t j = i + 1;
                while (s[j] && s[j] != '"') {
                    /* POSIX: inside double quotes only \", \\, \$ and \` escape. */
                    if (s[j] == '\\' && s[j + 1] &&
                        (s[j + 1] == '"' || s[j + 1] == '\\' || s[j + 1] == '$' ||
                         s[j + 1] == '`')) {
                        j++;
                    }
                    if (bl + 1 < sizeof(buf)) buf[bl++] = s[j];
                    j++;
                }
                if (!s[j]) return 0;            /* ValueError: No closing quotation */
                i = j + 1;
                have = 1;
            } else if (c == '\\') {
                if (!s[i + 1]) return 0;        /* ValueError: No escaped character */
                if (bl + 1 < sizeof(buf)) buf[bl++] = s[i + 1];
                i += 2;
                have = 1;
            } else if (to_is_ws(c)) {
                done = 1;
            } else {
                if (bl + 1 < sizeof(buf)) buf[bl++] = c;
                i++;
                have = 1;
            }
        }
        if (have) {
            if (n >= maxtok) return 0;
            buf[bl] = '\0';
            tok[n] = to_dup(buf);
            if (!tok[n]) return 0;
            n++;
        }
    }
    *count = n;
    return 1;
}

json_t *w20_terminalto_4615fc_split(const char *command) {
    char *tok[TO_TOKEN_CAP];
    int n = 0, i;
    json_t *out = json_object();
    if (!to_split(command, tok, TO_TOKEN_CAP, &n)) {
        for (i = 0; i < n; i++) free(tok[i]);
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string("ValueError"));
        return out;
    }
    json_set(out, "ok", json_bool(1));
    {
        json_t *arr = json_array();
        for (i = 0; i < n; i++) {
            json_append(arr, json_string(tok[i]));
            free(tok[i]);
        }
        json_set(out, "tokens", arr);
    }
    return out;
}

/* --- shlex.quote --------------------------------------------------------- */

static int to_safe_char(unsigned char c) {
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= '0' && c <= '9') return 1;
    switch (c) {
    case '_': case '@': case '%': case '+': case '=': case ':':
    case ',': case '.': case '/': case '-':
        return 1;
    }
    return 0;
}

char *w20_terminalto_4615fc_quote(const char *s) {
    to_sb sb;
    size_t i;
    if (!s) s = "";
    if (!s[0]) return to_dup("''");
    for (i = 0; s[i]; i++) {
        if (!to_safe_char((unsigned char)s[i])) break;
    }
    if (!s[i]) return to_dup(s);
    to_sb_init(&sb);
    to_sb_add(&sb, "'");
    for (i = 0; s[i]; i++) {
        if (s[i] == '\'') to_sb_add(&sb, "'\"'\"'");
        else {
            char one[2];
            one[0] = s[i];
            one[1] = '\0';
            to_sb_add(&sb, one);
        }
    }
    to_sb_add(&sb, "'");
    return to_sb_take(&sb);
}

/* --- _osascript_app ------------------------------------------------------ */

char *w20_terminalto_4615fc_osascript_app(const char *script) {
    to_sb sb;
    to_sb_init(&sb);
    to_sb_add(&sb, "osascript -e '");
    to_sb_add(&sb, script ? script : "");
    to_sb_add(&sb, "' 2>/dev/null");
    return to_sb_take(&sb);
}

/* --- _MACOS_OPEN_NO_TRANSFORM_FLAGS ------------------------------------- */

static const char *const TO_NO_TRANSFORM_FLAGS[] = {
    "-D", "-R", "-W", "-b", "-e", "-f", "-g", "-h", "-n", "-t", NULL
};

json_t *w20_terminalto_4615fc_no_transform_flags(void) {
    json_t *arr = json_array();
    int i;
    for (i = 0; TO_NO_TRANSFORM_FLAGS[i]; i++) {
        json_append(arr, json_string(TO_NO_TRANSFORM_FLAGS[i]));
    }
    return arr;
}

static int to_is_no_transform_flag(const char *flag) {
    int i;
    for (i = 0; TO_NO_TRANSFORM_FLAGS[i]; i++) {
        if (!strcmp(flag, TO_NO_TRANSFORM_FLAGS[i])) return 1;
    }
    return 0;
}

/* --- _build_macos_open_raise_ladder ------------------------------------- */

static char *to_app_assign(const char *app) {
    to_sb sb;
    to_sb_init(&sb);
    if (app) {
        char *q = w20_terminalto_4615fc_quote(app);
        to_sb_add(&sb, "_H_APP=");
        to_sb_add(&sb, q ? q : "");
        free(q);
    } else {
        /* Bare `open <file>`: resolve the app at runtime from the frontmost process. */
        to_sb_add(&sb,
                  "_H_APP=\"$(osascript -e 'tell application \"System Events\" to get "
                  "name of first application process whose frontmost is true' "
                  "2>/dev/null)\"");
    }
    return to_sb_take(&sb);
}

char *w20_terminalto_4615fc_ladder(const char *app, const char *file) {
    char *app_assign, *file_q, *activate, *frontmost, *unminimise, *asn, *setfront, *reopen;
    char *frontmost_name, *report;
    to_sb rs, sb;
    char *out;

    app_assign = to_app_assign(app);
    file_q = w20_terminalto_4615fc_quote(file);

    activate = w20_terminalto_4615fc_osascript_app("tell application " TO_APP_SPLICE " to activate");
    frontmost = w20_terminalto_4615fc_osascript_app(
        "tell application \"System Events\" to get frontmost of application process "
        TO_APP_SPLICE);
    unminimise = w20_terminalto_4615fc_osascript_app(
        "tell application \"System Events\" to set visible of every process to true");
    asn = to_dup("lsappinfo find \"$_H_APP\" 2>/dev/null | awk '{print $1}'");
    setfront = to_dup("[ -n \"$_H_ASN\" ] && lsappinfo setfront \"$_H_ASN\" 2>/dev/null");

    to_sb_init(&sb);
    to_sb_add(&sb, "open -a \"$_H_APP\" ");
    to_sb_add(&sb, file_q ? file_q : "''");
    to_sb_add(&sb, " 2>/dev/null");
    reopen = to_sb_take(&sb);

    frontmost_name = to_dup(
        "osascript -e 'tell application \"System Events\" to get name of "
        "first application process whose frontmost is true' 2>/dev/null");

    /* Report from the FINAL observed frontmost state, never from "a rung returned 0". */
    to_sb_init(&rs);
    to_sb_add(&rs, "if ");
    to_sb_add(&rs, frontmost);
    to_sb_add(&rs, "; then echo open: ");
    to_sb_add(&rs, file_q ? file_q : "''");
    to_sb_add(&rs, " is now frontmost; else _H_FOCUS=$(");
    to_sb_add(&rs, frontmost_name);
    to_sb_add(&rs, "); echo open: ");
    to_sb_add(&rs, file_q ? file_q : "''");
    to_sb_add(&rs, " opened but another application holds focus; ");
    to_sb_add(&rs, "echo \"frontmost: $_H_FOCUS\"; fi");
    report = to_sb_take(&rs);

    to_sb_init(&sb);
    to_sb_add(&sb, app_assign);
    to_sb_add(&sb, "; if ! ");
    to_sb_add(&sb, frontmost);
    to_sb_add(&sb, "; then ");
    to_sb_add(&sb, activate);
    to_sb_add(&sb, "; sleep 0.2; if ! ");
    to_sb_add(&sb, frontmost);
    to_sb_add(&sb, "; then _H_ASN=$(");
    to_sb_add(&sb, asn);
    to_sb_add(&sb, "); ");
    to_sb_add(&sb, setfront);
    to_sb_add(&sb, "; sleep 0.2; if ! ");
    to_sb_add(&sb, frontmost);
    to_sb_add(&sb, "; then ");
    to_sb_add(&sb, reopen);
    to_sb_add(&sb, "; sleep 0.2; ");
    to_sb_add(&sb, unminimise);
    to_sb_add(&sb, "; ");
    to_sb_add(&sb, activate);
    to_sb_add(&sb, "; fi; fi; fi; sleep 0.8; if ! ");
    to_sb_add(&sb, frontmost);
    to_sb_add(&sb, "; then ");
    to_sb_add(&sb, activate);
    to_sb_add(&sb, "; sleep 0.4; fi; ");
    to_sb_add(&sb, report);
    out = to_sb_take(&sb);

    free(app_assign);
    free(file_q);
    free(activate);
    free(frontmost);
    free(unminimise);
    free(asn);
    free(setfront);
    free(reopen);
    free(frontmost_name);
    free(report);
    return out;
}

/* --- _transform_macos_open_command -------------------------------------- */

char *w20_terminalto_4615fc_transform(const char *command, const char *system) {
    char *tok[TO_TOKEN_CAP];
    int n = 0, i, argc;
    const char *app = NULL, *file = NULL;
    char *ladder, *out;
    to_sb sb;

    if (strcmp(system ? system : "", "Darwin")) return to_dup(command);
    if (!command) return NULL;
    if (!to_split(command, tok, TO_TOKEN_CAP, &n)) return to_dup(command); /* shlex ValueError */
    if (n == 0 || strcmp(tok[0], "open")) {
        for (i = 0; i < n; i++) free(tok[i]);
        return to_dup(command);
    }
    argc = n - 1;
    if (argc == 0) {
        for (i = 0; i < n; i++) free(tok[i]);
        return to_dup(command);
    }

    if (tok[1][0] == '-') {
        if (to_is_no_transform_flag(tok[1])) {
            for (i = 0; i < n; i++) free(tok[i]);
            return to_dup(command);
        }
        if (strcmp(tok[1], "-a")) {
            for (i = 0; i < n; i++) free(tok[i]);
            return to_dup(command);
        }
        /* open -a <App> <file> */
        if (argc < 3 || n != 4) {
            for (i = 0; i < n; i++) free(tok[i]);
            return to_dup(command);
        }
        app = tok[2];
        file = tok[3];
        if (file[0] == '-') {
            for (i = 0; i < n; i++) free(tok[i]);
            return to_dup(command);
        }
    } else {
        /* bare open <file> */
        file = tok[1];
        app = NULL;
        if (n != 2) {
            for (i = 0; i < n; i++) free(tok[i]);
            return to_dup(command);
        }
    }

    ladder = w20_terminalto_4615fc_ladder(app, file);
    to_sb_init(&sb);
    to_sb_add(&sb, command);
    to_sb_add(&sb, "; ");
    to_sb_add(&sb, ladder ? ladder : "");
    out = to_sb_take(&sb);
    free(ladder);
    for (i = 0; i < n; i++) free(tok[i]);
    return out;
}
