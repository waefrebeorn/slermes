/*
 * port_tools_config_helpers.c — pure helper ports of tools_config.py
 *
 * Faithful C ports of the deterministic display/config helpers. The
 * platform-restriction table is embedded as constant data matching the
 * Python _TOOLSET_PLATFORM_RESTRICTIONS dict.
 */

#include "tools_config_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* PoP: gui_toolset_label @ hermes_cli/tools_config.py:gui_toolset_label */
/* gui_toolset_label: strip a leading non-alphanumeric token (emoji/icon). */
char *gui_toolset_label(const char *label) {
    if (!label) return strdup("");
    char *text = strdup(label);
    if (!text) return strdup("");
    /* strip leading/trailing whitespace */
    char *s = text;
    while (*s == ' ' || *s == '\t') s++;
    size_t L = strlen(s);
    while (L > 0 && (s[L - 1] == ' ' || s[L - 1] == '\t')) { s[--L] = '\0'; }

    /* split on first whitespace run */
    char *sp = s;
    while (*sp && !(*sp == ' ' || *sp == '\t')) sp++;
    int has_two = 0;
    if (*sp) {
        /* skip the whitespace run */
        char *r = sp;
        while (*r == ' ' || *r == '\t') r++;
        has_two = (*r != '\0');
    }
    char *out;
    if (has_two) {
        /* first token must be non-alphanumeric (an icon) to strip it */
        int first_is_alnum = 0;
        for (char *p = s; p < sp; p++) {
            if (isalnum((unsigned char)*p)) { first_is_alnum = 1; break; }
        }
        if (!first_is_alnum) {
            /* skip whitespace after the icon token */
            char *r = sp;
            while (*r == ' ' || *r == '\t') r++;
            out = strdup(r);
        } else {
            out = strdup(s);
        }
    } else {
        out = strdup(s);
    }
    free(text);
    return out ? out : strdup("");
}

/* Platform restriction table (mirrors _TOOLSET_PLATFORM_RESTRICTIONS). */
typedef struct { const char *ts_key; const char *platform; } plat_restrict_t;
static const plat_restrict_t PLAT_RESTRICTIONS[] = {
    {"discord", "discord"},
    {"discord_admin", "discord"},
    {NULL, NULL},
};

/* PoP: toolset_allowed_for_platform @ hermes_cli/tools_config.py:_toolset_allowed_for_platform */
bool toolset_allowed_for_platform(const char *ts_key, const char *platform) {
    if (!ts_key || !platform) return true; /* absent entry => allowed everywhere */
    for (size_t i = 0; PLAT_RESTRICTIONS[i].ts_key; i++) {
        if (strcmp(PLAT_RESTRICTIONS[i].ts_key, ts_key) == 0) {
            /* restricted: only allowed on its listed platform */
            return strcmp(PLAT_RESTRICTIONS[i].platform, platform) == 0;
        }
    }
    return true;
}

/* PoP: parse_enabled_flag @ hermes_cli/tools_config.py:_parse_enabled_flag */
bool parse_enabled_flag(const char *value, bool default_val) {
    if (value == NULL) return default_val;
    /* Python distinguishes int vs str, but at the C boundary everything is a
     * string. The str branch already covers "1"/"0" (the only integer literals
     * that have a defined meaning); any other all-digit string is
     * unrecognized and falls through to default, matching Python. */
    char buf[32];
    size_t j = 0;
    for (const char *q = value; *q && j + 1 < sizeof(buf); q++) {
        char c = *q;
        if (c == ' ' || c == '\t') continue;
        buf[j++] = (char)tolower((unsigned char)c);
    }
    buf[j] = '\0';
    if (strcmp(buf, "true") == 0 || strcmp(buf, "1") == 0 ||
        strcmp(buf, "yes") == 0 || strcmp(buf, "on") == 0) return true;
    if (strcmp(buf, "false") == 0 || strcmp(buf, "0") == 0 ||
        strcmp(buf, "no") == 0 || strcmp(buf, "off") == 0) return false;
    return default_val;
}

/* PoP: format_imagegen_model_row @ hermes_cli/tools_config.py:_format_imagegen_model_row */
char *format_imagegen_model_row(const char *model_id,
                                 const char *speed,
                                 const char *strengths,
                                 const char *price,
                                 int width_model,
                                 int width_speed,
                                 int width_strengths) {
    const char *m = model_id ? model_id : "";
    const char *sp = speed ? speed : "";
    const char *st = strengths ? strengths : "";
    const char *pr = price ? price : "";
    /* layout: "{model:<Wm}  {speed:<Ws}  {strengths:<Wst}  {price}" */
    int need = width_model + 2 + width_speed + 2 + width_strengths + 2 + (int)strlen(pr) + 1;
    char *out = malloc((size_t)need > 0 ? (size_t)need : 1);
    if (!out) return strdup("");
    snprintf(out, (size_t)need, "%-*s  %-*s  %-*s  %s",
             width_model, m, width_speed, sp, width_strengths, st, pr);
    return out;
}
