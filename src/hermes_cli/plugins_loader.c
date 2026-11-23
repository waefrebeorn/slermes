/*
 * plugins_loader.c — C home for upstream Hermes hermes_cli/plugins_loader.py.
 * PoP: w12_pluginsloa_e4b6ac_dist_name       @ hermes_cli/plugins_loader.py:_dist_name
 * PoP: w12_pluginsloa_e4b6ac_load_error_text @ hermes_cli/plugins_loader.py:_load_error_text
 * PoP: w12_pluginsloa_e4b6ac_platform_name   @ hermes_cli/plugins_loader.py:PluginLoaderMixin._platform_name_from_manifest
 */
#include "noc_w12_pluginsloa_e4b6ac.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char *py_strip(char *s)
{
    size_t n;
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

void w12_pluginsloa_e4b6ac_dist_name(const char *req, char *out, size_t cap)
{
    char tmp[2048];
    size_t n, cut = 0, i;
    snprintf(tmp, sizeof(tmp), "%s", req ? req : "");
    n = strlen(tmp);
    for (i = 0; i < n; i++) {
        char ch = tmp[i];
        if (ch == '<' || ch == '>' || ch == '=' || ch == '!' || ch == '~' ||
            ch == '[' || ch == ';' || isspace((unsigned char)ch)) {
            cut = i; break;
        }
    }
    if (i == n) cut = n;
    tmp[cut] = '\0';                 /* NUL at the shortened length */
    snprintf(out, cap, "%s", py_strip(tmp));
}

void w12_pluginsloa_e4b6ac_load_error_text(bool systemexit, const json_t *code,
                          const char *text, char *out, size_t cap)
{
    char repr[512];
    if (!systemexit) { snprintf(out, cap, "%s", text ? text : ""); return; }
    if (!code || code->type == JSON_NULL) {
        snprintf(repr, sizeof(repr), "None");
    } else if (code->type == JSON_STRING) {
        snprintf(repr, sizeof(repr), "'%s'", code->str_val ? code->str_val : "");
    } else if (code->type == JSON_BOOL) {
        snprintf(repr, sizeof(repr), "%s", code->bool_val ? "True" : "False");
    } else if (code->type == JSON_NUMBER) {
        double d = code->num_val;
        if (d == (double)(long long)d) snprintf(repr, sizeof(repr), "%lld", (long long)d);
        else snprintf(repr, sizeof(repr), "%g", d);
    } else {
        snprintf(repr, sizeof(repr), "None");
    }
    snprintf(out, cap, "SystemExit(%s) raised during import/register()", repr);
}

void w12_pluginsloa_e4b6ac_platform_name(const char *name, const char *path, char *out, size_t cap)
{
    const char *s = name ? name : "";     /* manifest.name or "" */
    size_t n = strlen(s), pn, start;
    const char *suffix = "-platform";
    const size_t sl = 9;
    if (n >= sl && !strcmp(s + n - sl, suffix)) {
        snprintf(out, cap, "%.*s", (int)(n - sl), s);
        return;
    }
    if (path && path[0]) {
        const char *p = path;
        pn = strlen(p);
        while (pn && (p[pn - 1] == '/' || p[pn - 1] == '\\')) pn--;
        start = pn;
        while (start > 0 && p[start - 1] != '/' && p[start - 1] != '\\') start--;
        snprintf(out, cap, "%.*s", (int)(pn - start), p + start);
        return;
    }
    snprintf(out, cap, "%s", s);
}
