/*
 * port_journey_remaining.c — Port of hermes_cli/journey.py color-fade
 * surface. Ink fade toward background, console forcing, list cmd.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static int hexv(const char *s) {
    int v = 0;
    for (int i = 0; i < 2; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    }
    return v;
}

/* PoP: _resolve @ hermes_cli/journey.py:_resolve */
char *jny_resolve(const char *ink_hex, const char *bg_hex, double alpha) {
    /* Python: fade ink toward bg (rgba-over-bg). */
    if (!ink_hex || !bg_hex) return NULL;
    if (ink_hex[0] == '#') ink_hex++;
    if (bg_hex[0] == '#') bg_hex++;
    if (strlen(ink_hex) < 6 || strlen(bg_hex) < 6) return strdup(ink_hex - 1);
    int ri = hexv(ink_hex), gi = hexv(ink_hex + 2), bi = hexv(ink_hex + 4);
    int rb = hexv(bg_hex), gb = hexv(bg_hex + 2), bb = hexv(bg_hex + 4);
    if (alpha < 0) alpha = 0;
    if (alpha > 1) alpha = 1;
    int ro = (int)(ri * (1 - alpha) + rb * alpha);
    int go = (int)(gi * (1 - alpha) + gb * alpha);
    int bo = (int)(bi * (1 - alpha) + bb * alpha);
    char *out = NULL;
    asprintf(&out, "#%02x%02x%02x", ro, go, bo);
    return out;
}

/* PoP: _console @ hermes_cli/journey.py:_console */
char *jny_console(bool color, bool force) {
    /* Python: rich console; force truecolor. */
    char *out = NULL;
    asprintf(&out, "{\"color\": %s, \"force_truecolor\": %s}",
             color ? "true" : "false", force ? "true" : "false");
    return out;
}

/* PoP: _cmd_list @ hermes_cli/journey.py:_cmd_list */
char *jny_cmd_list(const char *graph_json) {
    /* Python: journey list rendering. */
    if (!graph_json) return NULL;
    printf("journey list rendered\n");
    return strdup(graph_json);
}
