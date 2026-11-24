/*
 * cli_modern_layout.c — C port of upstream Hermes hermes_cli/cli_modern_layout.py.
 *
 * PoP: w16_climodern_279cc8_layout_from_config @ hermes_cli/cli_modern_layout.py:layout_from_config
 * PoP: w16_climodern_279cc8_busy_input_mode_from_config @ hermes_cli/cli_modern_layout.py:busy_input_mode_from_config
 * PoP: w16_climodern_279cc8_fg @ hermes_cli/cli_modern_layout.py:_fg
 * PoP: w16_climodern_279cc8_reasoning_text_line @ hermes_cli/cli_modern_layout.py:reasoning_text_line
 * PoP: w16_climodern_279cc8_constants @ hermes_cli/cli_modern_layout.py:_SPIN/_RST/_BOLD/_DIM/_RED/_SEPARATOR_WIDTH/_REASONING_PREVIEW_LINES
 */
#include "noc_w16_climodern_279cc8.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static bool py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:  return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return false;
    }
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}
static void strip_lower_inplace(char *s) {
    if (!s) return;
    char *a = s, *z = s + strlen(s);
    while (a < z && isspace((unsigned char)*a)) a++;
    while (z > a && isspace((unsigned char)z[-1])) z--;
    size_t len = (size_t)(z - a);
    memmove(s, a, len);
    s[len] = '\0';
    for (size_t i = 0; i < len; i++) s[i] = (char)tolower((unsigned char)s[i]);
}
/* str((display or {}).get(key, default) or default).strip().lower() */
static char *config_str(const json_t *display, const char *key, const char *def) {
    json_t *d = (display && display->type == JSON_OBJECT && py_truthy(display)) ? (json_t *)display : NULL;
    json_t *v = d ? json_obj_get(d, key) : NULL;
    char *s = (v && py_truthy(v)) ? py_str(v) : dupstr(def);
    strip_lower_inplace(s);
    return s;
}

json_t *w16_climodern_279cc8_layout_from_config(const json_t *display) {
    char *s = config_str(display, "cli_layout", "modern");
    bool modern = strcmp(s, "classic") != 0;
    free(s);
    return json_bool(modern);
}

json_t *w16_climodern_279cc8_busy_input_mode_from_config(const json_t *display) {
    char *s = config_str(display, "busy_input_mode", "interrupt");
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

static int hex2(const char *s) {
    int v = 0;
    for (int i = 0; i < 2; i++) {
        char c = s[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return -1;
        v = v * 16 + d;
    }
    return v;
}

json_t *w16_climodern_279cc8_fg(const char *hex_color, bool bold) {
    const char *h = hex_color ? hex_color : "";
    while (*h == '#') h++;
    if (strlen(h) < 6) return json_string(bold ? "\x1b[1m" : "");
    int r = hex2(h), g = hex2(h + 2), b = hex2(h + 4);
    if (r < 0 || g < 0 || b < 0) return json_string(bold ? "\x1b[1m" : "");
    char out[64];
    snprintf(out, sizeof(out), "\x1b[%s38;2;%d;%d;%dm", bold ? "1;" : "", r, g, b);
    return json_string(out);
}

json_t *w16_climodern_279cc8_reasoning_text_line(const char *line) {
    char *buf = (char *)malloc((line ? strlen(line) : 0) + 16);
    snprintf(buf, (line ? strlen(line) : 0) + 16, "\x1b[2m%s\x1b[0m", line ? line : "");
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

json_t *w16_climodern_279cc8_constants(void) {
    json_t *out = json_object();
    json_set(out, "spin", json_string("\xe2\xa0\x8b\xe2\xa0\x99\xe2\xa0\xb9\xe2\xa0\xb8\xe2\xa0\xbc\xe2\xa0\xb4\xe2\xa0\xa6\xe2\xa0\xa7\xe2\xa0\x87\xe2\xa0\x8f"));
    json_set(out, "rst", json_string("\x1b[0m"));
    json_set(out, "bold", json_string("\x1b[1m"));
    json_set(out, "dim", json_string("\x1b[2m"));
    json_set(out, "red", json_string("\x1b[38;2;239;83;80m"));
    json_set(out, "separator_width", json_number(40));
    json_set(out, "reasoning_preview_lines", json_number(10));
    return out;
}
