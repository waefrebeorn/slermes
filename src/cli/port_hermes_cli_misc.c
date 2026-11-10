/*
 * port_hermes_cli_misc.c — thin pure helpers from three CLI/plumbing modules.
 */

#include "port_hermes_cli_misc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dx(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}

/* PoP: misc_display_source @ hermes_cli/auth_commands.py:_display_source */
char *misc_display_source(const char *source)
{
    if (!source)
        return dx("", 0);
    if (strncmp(source, "manual:", 7) == 0) {
        const char *rest = source + 7;
        return dx(rest, strlen(rest));
    }
    return dx(source, strlen(source));
}

/* PoP: misc_configured_backend @ agent/delegation_registry.py:configured_backend */
char *misc_configured_backend(const char *config_json)
{
    json_t *cfg = config_json ? json_parse(config_json, NULL) : NULL;
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *raw = json_obj_get(cfg, "backend");
        if (raw && raw->type == JSON_STRING && raw->str_val) {
            /* lower_key(raw) if isinstance(raw,str) and raw.strip() else "default" */
            size_t len = strlen(raw->str_val), s = 0;
            while (s < len && isspace((unsigned char)raw->str_val[s])) s++;
            if (s < len) { /* has non-space content */
                size_t e = len;
                while (e > s && isspace((unsigned char)raw->str_val[e - 1])) e--;
                char *out = dx(raw->str_val + s, e - s);
                if (out) {
                    for (size_t i = 0; i < e - s; i++)
                        out[i] = (char)tolower((unsigned char)out[i]);
                }
                if (cfg) json_free(cfg);
                return out ? out : dx("default", 7);
            }
        }
    }
    if (cfg) json_free(cfg);
    return dx("default", 7);
}

/* PoP: misc_safe_int @ agent/context_compressor_telemetry.py:_safe_int */
long long misc_safe_int(const json_t *value)
{
    if (!value)
        return -1;
    switch (value->type) {
    case JSON_NUMBER:
        return (long long)value->num_val; /* int(1.5) truncates */
    case JSON_BOOL:
        return value->bool_val ? 1 : 0;
    case JSON_STRING: {
        const char *s = value->str_val ? value->str_val : "";
        size_t len = strlen(s), i = 0;
        while (i < len && isspace((unsigned char)s[i])) i++;
        if (i >= len)
            return -1;
        int neg = 0;
        if (s[i] == '-' || s[i] == '+') { neg = (s[i] == '-'); i++; }
        if (i >= len)
            return -1;
        long long v = 0;
        int any = 0;
        for (; i < len && s[i] >= '0' && s[i] <= '9'; i++) {
            v = v * 10 + (s[i] - '0');
            any = 1;
        }
        if (!any)
            return -1;
        /* int(...) allows trailing whitespace but not internal junk */
        while (i < len && isspace((unsigned char)s[i]))
            i++;
        if (i != len)
            return -1; /* e.g. "3.5" or "12 34" */
        return neg ? -v : v;
    }
    default:
        return -1;
    }
}
