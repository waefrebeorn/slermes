/*
 * cli_terminal_mixin.c - C home for upstream Hermes hermes_cli/cli_terminal_mixin.py.
 *
 * PoP: w17_clitermina_bf2d1a_is_eio @ hermes_cli/cli_terminal_mixin.py:_is_eio
 * PoP: w17_clitermina_bf2d1a_tmux_passthrough @ hermes_cli/cli_terminal_mixin.py:_tmux_passthrough
 * PoP: w17_clitermina_bf2d1a_rebuilds_scrollback @ hermes_cli/cli_terminal_mixin.py:CLITerminalMixin._redraw_rebuilds_scrollback
 *
 * Environment-dependent values (CLI_CONFIG) are injected from the fixture; the
 * escape-string and threshold logic mirrors the live module exactly.
 */
#include "noc_w17_clitermina_bf2d1a.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Python errno.EIO == 5 on every platform Hermes runs on. */
json_t *w17_clitermina_bf2d1a_is_eio(int err) {
    return json_bool(err == 5 ? true : false);
}

/* _tmux_passthrough: "\x1bPtmux;" + seq.replace("\x1b", "\x1b\x1b") + "\x1b\\" */
json_t *w17_clitermina_bf2d1a_tmux_passthrough(const char *seq) {
    const char *s = seq ? seq : "";
    size_t n = strlen(s), esc = 0, cap, i, p = 0;
    char *out;
    json_t *res;
    for (i = 0; i < n; ++i) if ((unsigned char)s[i] == 0x1b) esc++;
    cap = 7 + n + esc + 2 + 1;
    out = (char *)malloc(cap);
    if (!out) return json_string("");
    out[p++] = 0x1b; memcpy(out + p, "Ptmux;", 6); p += 6;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) out[p++] = 0x1b;
        out[p++] = (char)c;
    }
    out[p++] = 0x1b; out[p++] = '\\';
    out[p] = '\0';
    res = json_string(out);
    free(out);
    return res;
}

static int in_truthy_set(const char *s) {
    static const char *const yes[] = {"1", "true", "yes", "on", "always", NULL};
    int i;
    for (i = 0; yes[i]; ++i) if (!strcmp(s, yes[i])) return 1;
    return 0;
}

/* Python str.strip().lower() over ASCII (the accepted tokens are all ASCII). */
static char *strip_lower(const char *s) {
    const char *a = s ? s : "";
    const char *b;
    char *out;
    size_t i, n;
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; ++i) out[i] = (char)tolower((unsigned char)a[i]);
    out[n] = '\0';
    return out;
}

/* _redraw_rebuilds_scrollback(): display.cli_rebuild_scrollback_on_redraw */
json_t *w17_clitermina_bf2d1a_rebuilds_scrollback(const json_t *cli_config) {
    json_t *display, *raw;
    int result = 0;
    display = json_obj_get(cli_config, "display");
    if (!display || display->type != JSON_OBJECT) return json_bool(false);
    raw = json_obj_get(display, "cli_rebuild_scrollback_on_redraw");
    if (!raw) return json_bool(false);
    if (raw->type == JSON_STRING) {
        char *low = strip_lower(raw->str_val);
        if (low) { result = in_truthy_set(low); free(low); }
    } else if (raw->type == JSON_BOOL) {
        result = raw->bool_val ? 1 : 0;
    } else if (raw->type == JSON_NUMBER) {
        result = raw->num_val != 0.0;
    } else if (raw->type == JSON_ARRAY || raw->type == JSON_OBJECT) {
        result = raw->c.count > 0;
    }
    return json_bool(result ? true : false);
}
