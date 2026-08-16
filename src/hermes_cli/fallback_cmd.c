/*
 * port_fallback_cmd.c — Faithful C11 port of pure helpers from
 * hermes_cli/fallback_cmd.py
 *
 * Ported: _format_entry, _extract_fallback_from_model_cfg, _describe_primary.
 * IO-coupled functions (cmd_fallback_*, _numbered_pick, _read_chain via
 * get_fallback_chain) left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "json.h"
#include "fallback_cmd.h"

/* Strip leading + trailing whitespace in place (Python .strip()). */
static void strip_inplace(char *s) {
    if (!s || !*s) return;
    char *end = s + strlen(s) - 1;
    while (end >= s && (*end==' '||*end=='\t'||*end=='\r'||*end=='\n')) { *end = '\0'; if (end > s) end--; }
    char *start = s;
    while (*start==' '||*start=='\t'||*start=='\r'||*start=='\n') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

/* PoP: fallback_format_entry @ hermes_cli/fallback_cmd.py:_format_entry */
void fallback_format_entry(const json_t *entry, char *out, size_t out_cap) {
    const char *provider = NULL;
    const char *model = NULL;
    const char *base = NULL;
    if (entry && entry->type == JSON_OBJECT) {
        const json_t *p = json_obj_get(entry, "provider");
        if (p && p->type == JSON_STRING) provider = p->str_val;
        const json_t *m = json_obj_get(entry, "model");
        if (m && m->type == JSON_STRING) model = m->str_val;
        const json_t *b = json_obj_get(entry, "base_url");
        if (b && b->type == JSON_STRING && b->str_val && b->str_val[0]) base = b->str_val;
    }
    /* Python: entry.get("provider", "?") — "?" only when key absent */
    const char *pval = (provider != NULL) ? provider : "?";
    const char *mval = (model != NULL) ? model : "?";
    if (base) {
        snprintf(out, out_cap, "%s  (via %s)  [%s]", mval, pval, base);
    } else {
        snprintf(out, out_cap, "%s  (via %s)", mval, pval);
    }
}

/* PoP: fallback_extract_from_model_cfg @ hermes_cli/fallback_cmd.py:_extract_fallback_from_model_cfg */
json_t *fallback_extract_from_model_cfg(const json_t *model_cfg) {
    if (!model_cfg || model_cfg->type != JSON_OBJECT) return NULL;
    char provider[256] = "";
    char model[256] = "";
    char base_url[256] = "";
    char api_mode[256] = "";

    const json_t *p = json_obj_get(model_cfg, "provider");
    if (p && p->type == JSON_STRING && p->str_val) strncpy(provider, p->str_val, sizeof(provider)-1);
    const json_t *m = json_obj_get(model_cfg, "default");
    if (!m || m->type != JSON_STRING) m = json_obj_get(model_cfg, "model");
    if (m && m->type == JSON_STRING && m->str_val) strncpy(model, m->str_val, sizeof(model)-1);

    strip_inplace(provider);
    strip_inplace(model);

    if (provider[0]=='\0' || model[0]=='\0') return NULL;

    const json_t *b = json_obj_get(model_cfg, "base_url");
    if (b && b->type == JSON_STRING && b->str_val) { strncpy(base_url, b->str_val, sizeof(base_url)-1); }
    strip_inplace(base_url);

    const json_t *am = json_obj_get(model_cfg, "api_mode");
    if (am && am->type == JSON_STRING && am->str_val) { strncpy(api_mode, am->str_val, sizeof(api_mode)-1); }
    strip_inplace(api_mode);

    json_t *entry = json_object();
    json_set(entry, "provider", json_string(provider));
    json_set(entry, "model", json_string(model));
    if (base_url[0] != '\0') json_set(entry, "base_url", json_string(base_url));
    if (api_mode[0] != '\0') json_set(entry, "api_mode", json_string(api_mode));
    return entry;
}

/* PoP: fallback_describe_primary @ hermes_cli/fallback_cmd.py:_describe_primary */
void fallback_describe_primary(const json_t *config, char *out, size_t out_cap) {
    out[0] = '\0';
    if (!config || config->type != JSON_OBJECT) return;
    const json_t *model_cfg = json_obj_get(config, "model");
    if (!model_cfg) return;
    if (model_cfg->type == JSON_OBJECT) {
        const char *pval = NULL;
        const char *mval = NULL;
        const json_t *p = json_obj_get(model_cfg, "provider");
        if (p && p->type == JSON_STRING) pval = p->str_val;
        const json_t *m = json_obj_get(model_cfg, "default");
        if (!m || m->type != JSON_STRING) m = json_obj_get(model_cfg, "model");
        if (m && m->type == JSON_STRING) mval = m->str_val;
        char provider[256] = "";
        char model[256] = "";
        if (pval) strncpy(provider, pval, sizeof(provider)-1);
        if (mval) strncpy(model, mval, sizeof(model)-1);
        strip_inplace(provider);
        strip_inplace(model);
        /* Python: (x or "?").strip() or "?" */
        if (provider[0]=='\0') strcpy(provider, "?");
        if (model[0]=='\0') strcpy(model, "?");
        snprintf(out, out_cap, "%s  (via %s)", model, provider);
        return;
    }
    if (model_cfg->type == JSON_STRING && model_cfg->str_val && model_cfg->str_val[0]) {
        char s[256];
        strncpy(s, model_cfg->str_val, sizeof(s)-1);
        s[sizeof(s)-1]='\0';
        strip_inplace(s);
        if (s[0] != '\0') { strncpy(out, s, out_cap-1); out[out_cap-1]='\0'; }
    }
}
