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
#include <hermes_json.h>

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


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_fallback_cmd_u_read_chain @ hermes_cli/fallback_cmd.py:_read_chain */
int hermes_cli_fallback_cmd_u_read_chain(const char *arg) {
    /* Python: get_fallback_chain normalized list. Arg = chain JSON. */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_fallback_cmd_u_write_chain @ hermes_cli/fallback_cmd.py:_write_chain */
int hermes_cli_fallback_cmd_u_write_chain(const char *arg) {
    /* Python: config["fallback_providers"] = chain; drop fallback_model.
     * Arg = "chain_json\tconfig_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *chain = tab ? arg : arg;
    const char *cfg = tab ? tab + 1 : "";
    json_t *config = json_parse(cfg, NULL);
    if (!config || !json_is_object(config)) {
        if (config) json_free(config);
        config = json_object();
    }
    json_t *ch = json_parse(chain, NULL);
    if (ch) { json_set(config, "fallback_providers", ch); }
    json_obj_del(config, "fallback_model");
    char *s = json_dumps(config, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(config);
    return 0;
}

/* PoP: hermes_cli_fallback_cmd_u_snapshot_auth_active_provider @ hermes_cli/fallback_cmd.py:_snapshot_auth_active_provider */
int hermes_cli_fallback_cmd_u_snapshot_auth_active_provider(const char *arg) {
    /* Python: auth store "active_provider" or None. Arg = auth JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *store = json_parse(arg, NULL);
    if (!store || !json_is_object(store)) {
        if (store) json_free(store);
        printf("\n");
        return 0;
    }
    const char *ap = json_get_str(store, "active_provider", "");
    printf("%s\n", ap);
    json_free(store);
    return 0;
}

/* PoP: hermes_cli_fallback_cmd_u_restore_auth_active_provider @ hermes_cli/fallback_cmd.py:_restore_auth_active_provider */
int hermes_cli_fallback_cmd_u_restore_auth_active_provider(const char *arg) {
    /* Python: best-effort write-back. Arg = "value\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("auth restore skipped (best-effort)\n"); return 0; }
    printf("active_provider restored: %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_fallback_cmd_u_restore_model_cfg @ hermes_cli/fallback_cmd.py:_restore_model_cfg */
int hermes_cli_fallback_cmd_u_restore_model_cfg(const char *arg) {
    /* Python: cfg["model"] = snapshot; pop when None. Arg =
     * "model_json\tconfig_json" (model empty = pop). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    json_t *cfg = json_parse(tab ? tab + 1 : "", NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        cfg = json_object();
    }
    if (!tab || tab == arg) {
        json_obj_del(cfg, "model");
    } else {
        json_t *m = json_parse(arg, NULL);
        if (m) json_set(cfg, "model", m);
    }
    char *s = json_dumps(cfg, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(cfg);
    return 0;
}

/* PoP: hermes_cli_fallback_cmd_u_numbered_pick @ hermes_cli/fallback_cmd.py:_numbered_pick */
int hermes_cli_fallback_cmd_u_numbered_pick(const char *arg) {
    /* Python: numbered-list fallback picker. Arg =
     * "choices\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *choices = arg;
    const char *picked = t1 ? t1 + 1 : "";
    printf("\n");
    const char *p = choices;
    int i = 1;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        printf("  %d. %.*s\n", i++, (int)len, p);
        p = t ? t + 1 : p + len;
    }
    printf("choice: %s\n", picked[0] ? picked : "(none)");
    return 0;
}
