/*
 * process_registry_wsl.c — faithful C11 port of tools/process_registry_wsl.py.
 * Gate: procwsl.
 * PoP: procwsl_is_wsl_launcher_command @ tools/process_registry_wsl.py:_is_wsl_launcher_command
 *
 * Two candidates are checked: the first token of shlex.split(command, posix=True)
 * (skipped on ValueError), and the first whitespace-delimited word of the raw
 * stripped command (POSIX shlex eats Windows backslashes, so the naive token
 * preserves path separators). For each: basename(first.replace("\\","/"))
 * .strip("'\"").lower() in {"wsl", "wsl.exe"}.
 */
#include "noc_procwsl.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static int shlex_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r'; /* shlex.whitespace */
}

/* First token of shlex.split(s, posix=True); 0 ok (token in out), -1 ValueError. */
static int shlex_first(const char *s, char *out, size_t cap) {
    const char *p = s;
    size_t olen = 0;
    int state = 0; /* 0, '\'' or '"' */
    while (*p && shlex_ws(*p)) p++;
    if (!*p) { out[0] = '\0'; return 0; } /* shlex.split -> [] -> (" or [""])[0] == "" */
    while (*p) {
        char c = *p;
        if (state == 0) {
            if (shlex_ws(c)) break;
            if (c == '\'' || c == '"') { state = (unsigned char)c; p++; continue; }
            if (c == '\\') {
                p++;
                if (!*p) return -1; /* No escaped character */
                if (olen + 1 < cap) out[olen++] = *p;
                p++;
                continue;
            }
            if (olen + 1 < cap) out[olen++] = c;
            p++;
        } else if (state == '\'') {
            if (c == '\'') { state = 0; p++; }
            else { if (olen + 1 < cap) out[olen++] = c; p++; }
        } else { /* state == '"' */
            if (c == '"') { state = 0; p++; }
            else if (c == '\\') {
                p++;
                if (!*p) return -1; /* No escaped character */
                char nx = *p;
                /* only " and \ are escaped inside double quotes; others keep the backslash */
                if (nx == '"' || nx == '\\') {
                    if (olen + 1 < cap) out[olen++] = nx;
                } else {
                    if (olen + 2 < cap) { out[olen++] = '\\'; out[olen++] = nx; }
                }
                p++;
            } else { if (olen + 1 < cap) out[olen++] = c; p++; }
        }
    }
    if (state != 0) return -1; /* No closing quotation */
    out[olen] = '\0';
    return 0;
}

/* First whitespace-delimited word of command.strip(). */
static void naive_first(const char *s, char *out, size_t cap) {
    const char *p = s;
    size_t olen = 0;
    while (*p && is_space(*p)) p++;
    while (*p && !is_space(*p)) { if (olen + 1 < cap) out[olen++] = *p; p++; }
    out[olen] = '\0';
}

/* base = basename(first.replace("\\","/")).strip("'\"").lower() */
static int is_wsl_base(const char *first) {
    char norm[1024];
    size_t n = 0;
    for (const char *p = first; *p && n + 1 < sizeof(norm); p++)
        norm[n++] = (*p == '\\') ? '/' : *p;
    norm[n] = '\0';

    const char *base = norm;
    for (const char *p = norm; *p; p++) if (*p == '/') base = p + 1;

    size_t len = strlen(base);
    size_t a = 0, b = len;
    while (a < b && (base[a] == '\'' || base[a] == '"')) a++;
    while (b > a && (base[b - 1] == '\'' || base[b - 1] == '"')) b--;
    if (b - a == 3) {
        char w[4] = { base[a], base[a + 1], base[a + 2], 0 };
        w[0] = (char)tolower((unsigned char)w[0]);
        w[1] = (char)tolower((unsigned char)w[1]);
        w[2] = (char)tolower((unsigned char)w[2]);
        return !strcmp(w, "wsl");
    }
    if (b - a == 7) {
        char w[8];
        memcpy(w, base + a, 7); w[7] = 0;
        for (int i = 0; i < 7; i++) w[i] = (char)tolower((unsigned char)w[i]);
        return !strcmp(w, "wsl.exe");
    }
    return 0;
}

int procwsl_is_wsl_launcher_command(const char *command) {
    if (!command) return 0;                 /* not isinstance(command, str) */
    const char *p = command;
    int has_nonws = 0;
    for (; *p; p++) if (!is_space(*p)) { has_nonws = 1; break; }
    if (!has_nonws) return 0;               /* not command.strip() */

    char cand[1024];
    if (shlex_first(command, cand, sizeof(cand)) == 0 && is_wsl_base(cand)) return 1;
    naive_first(command, cand, sizeof(cand));
    if (is_wsl_base(cand)) return 1;
    return 0;
}
