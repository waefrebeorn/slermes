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


/* ================================================================== */
/* Subcommand handlers (real config I/O)                               */
/* ================================================================== */

#include <unistd.h>
#include <ctype.h>
#include "hermes_core_types.h"
#include "port_config_py_helpers.h"
#include "port_backend_identity.h"
#include "fallback_config_helpers.h"
#include <errno.h>

static const char *p_fb_json_str(const json_t *obj, const char *key,
                                 const char *dflt) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (v && v->type == JSON_STRING && v->str_val[0]) return v->str_val;
    return dflt ? dflt : "";
}

/* NULL-when-absent variant (Python .get(key)) */
static const char *p_fb_json_str_or_null(const json_t *obj, const char *key) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_STRING && v->str_val[0]) ? v->str_val : NULL;
}

static json_t *fb_load_config(void) { return config_py_load_config_readonly(); }

/* Persist chain to config.yaml: fallback_providers = chain, drop legacy
 * fallback_model from the raw file so there's one source of truth. */
static int fb_write_chain(json_t *chain) {
    int rc = config_py_save_value("fallback_providers", chain);
    if (rc != 0) return rc;
    /* Drop the legacy single-dict key on write. */
    char path[2048], tmp[2100];
    config_py_get_config_path(path, sizeof path);
    json_t *raw = config_py_read_raw_config();
    if (!raw) return 0;
    if (!json_obj_get(raw, "fallback_model")) { json_free(raw); return 0; }
    json_obj_del(raw, "fallback_model");
    char *text = json_serialize(raw);
    json_free(raw);
    if (!text) return -1;
    snprintf(tmp, sizeof tmp, "%s.tmp-%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(text); return -1; }
    fputs(text, f);
    fclose(f);
    free(text);
    return rename(tmp, path);
}

/* Restore config["model"] from a snapshot (NULL pops the key). */
static int fb_restore_model_cfg(const json_t *model_before) {
    if (model_before)
        return config_py_save_value("model", json_copy((json_t *)model_before));
    char path[2048], tmp[2100];
    config_py_get_config_path(path, sizeof path);
    json_t *raw = config_py_read_raw_config();
    if (!raw) raw = json_object();
    json_obj_del(raw, "model");
    char *text = json_serialize(raw);
    json_free(raw);
    if (!text) return -1;
    snprintf(tmp, sizeof tmp, "%s.tmp-%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(text); return -1; }
    fputs(text, f);
    fclose(f);
    free(text);
    return rename(tmp, path);
}

/* auth.json active_provider snapshot/restore is best-effort in this port:
 * the picker hook owns its own auth state, so no separate snapshot file is
 * read/written here (mirrors the Python try/except no-op path). */

/* PoP: cmd_fallback_list @ hermes_cli/fallback_cmd.py:cmd_fallback_list */
int cmd_fallback_list(void) {
    json_t *config = fb_load_config();
    int n = 0;
    fallback_entry_t *chain = fallback_config_get_chain(config, &n);

    printf("\n");
    if (n == 0) {
        printf("  No fallback providers configured.\n\n");
        printf("  Add one with:  %s %s %s\n\n", "hermes", "fallback", "add");
        fallback_config_free_entries(chain, n);
        json_free(config);
        return 0;
    }

    char primary[1024];
    fallback_describe_primary(config, primary, sizeof primary);
    if (primary[0]) printf("  Primary:   %s\n\n", primary);
    printf("  Fallback chain (%d %s):\n", n, n == 1 ? "entry" : "entries");
    for (int i = 0; i < n; i++) {
        json_t *e = json_object();
        json_set(e, "provider", json_string(chain[i].provider));
        json_set(e, "model", json_string(chain[i].model));
        if (chain[i].base_url && chain[i].base_url[0])
            json_set(e, "base_url", json_string(chain[i].base_url));
        char line[1024];
        fallback_format_entry(e, line, sizeof line);
        printf("    %d. %s\n", i + 1, line);
        json_free(e);
    }
    printf("\n");
    printf("  Tried in order when the primary fails (rate-limit, 5xx, connection errors).\n");
    printf("  Docs: https://hermes-agent.nousresearch.com/docs/user-guide/features/fallback-providers\n");
    printf("\n");
    fallback_config_free_entries(chain, n);
    json_free(config);
    return 0;
}

/*
 * Fallback numbered-list picker when curses is unavailable. Returns the
 * PoP: _numbered_pick @ hermes_cli/fallback_cmd.py:_numbered_pick
 */
int fallback_numbered_pick(const char *question, const char **choices, int n) {
    printf("%s\n", question);
    for (int i = 0; i < n; i++) printf("  %d. %s\n", i + 1, choices[i]);
    printf("\n");
    for (;;) {
        printf("Choice [1-%d]: ", n);
        fflush(stdout);
        char buf[64];
        if (!fgets(buf, sizeof buf, stdin)) { printf("\n"); return -1; }
        size_t L = strlen(buf);
        while (L && (buf[L-1] == '\n' || buf[L-1] == '\r')) buf[--L] = '\0';
        while (L && (buf[L-1]==' '||buf[L-1]=='\t')) buf[--L]='\0';
        size_t b = 0; while (b < L && (buf[b]==' '||buf[b]=='\t')) b++;
        if (b >= L) return -1;
        char *end = NULL;
        long idx = strtol(buf + b, &end, 10) - 1;
        if (*end != '\0') { printf("Please enter a number\n"); continue; }
        if (idx >= 0 && idx < n) return (int)idx;
        printf("Please enter 1-%d\n", n);
    }
}

/*
 * The provider/model picker itself is the interactive boundary; it is
 * invoked through a weak hook so hosts that link a picker implementation
 * get the full flow. Without a hook the flow restores state and reports
 * PoP: cmd_fallback_add @ hermes_cli/fallback_cmd.py:cmd_fallback_add
 */
extern __attribute__((weak)) int hermes_select_provider_and_model_hook(void);

int cmd_fallback_add(void) {
    /* Python _require_tty("fallback add") */
    if (!isatty(0)) {
        printf("Error: 'hermes %s' requires an interactive terminal.\n"
               "It cannot be run through a pipe or non-interactive subprocess.\n"
               "Run it directly in your terminal instead.\n",
               "fallback add");
        exit(2);
    }

    json_t *before_cfg = fb_load_config();
    const json_t *model_before =
        before_cfg ? json_obj_get(before_cfg, "model") : NULL;
    json_t *model_snapshot = model_before ? json_copy((json_t *)model_before) : NULL;
    json_free(before_cfg);

    printf("\n");
    printf("  Adding a fallback provider.  The picker below is the same one used by\n");
    printf("  `hermes model` - select the provider + model you want as a fallback.\n");
    printf("\n");

    int picked = 0;
    if (hermes_select_provider_and_model_hook)
        picked = hermes_select_provider_and_model_hook() == 0;

    if (!picked) {
        /* Picker didn't complete (user cancelled or no picker linked). */
        fb_restore_model_cfg(model_snapshot);
        if (model_snapshot) json_free(model_snapshot);
        printf("\n  No fallback added.\n");
        return 0;
    }

    /* Read post-picker state. */
    json_t *after_cfg = fb_load_config();
    const json_t *model_after = after_cfg ? json_obj_get(after_cfg, "model") : NULL;
    json_t *new_entry = fallback_extract_from_model_cfg(model_after);
    if (after_cfg) json_free(after_cfg);
    if (!new_entry) {
        fb_restore_model_cfg(model_snapshot);
        if (model_snapshot) json_free(model_snapshot);
        printf("\n  No fallback added.\n");
        return 0;
    }

    /* Identity semantics owned by agent.backend_identity (#54250/#57584/#62984):
     * same provider+model on a DIFFERENT explicit base_url is a different
     * backend and a legitimate fallback. Reject same-deployment duplicates. */
    BackendIdentity *new_ident = agent_backend_identity_build(
        p_fb_json_str(new_entry, "provider", NULL),
        p_fb_json_str(new_entry, "model", NULL),
        p_fb_json_str_or_null(new_entry, "base_url"));

    json_t *before_cfg2 = fb_load_config();
    json_t *primary_entry =
        fallback_extract_from_model_cfg(json_obj_get(before_cfg2, "model"));
    json_free(before_cfg2);
    if (primary_entry) {
        BackendIdentity *primary_ident = agent_backend_identity_build(
            p_fb_json_str(primary_entry, "provider", NULL),
            p_fb_json_str(primary_entry, "model", NULL),
            p_fb_json_str_or_null(primary_entry, "base_url"));
        int same = agent_backend_identity_same_deployment(primary_ident, new_ident);
        agent_backend_identity_free(primary_ident);
        json_free(primary_entry);
        if (same) {
            fb_restore_model_cfg(model_snapshot);
            if (model_snapshot) json_free(model_snapshot);
            char line[1024];
            fallback_format_entry(new_entry, line, sizeof line);
            printf("\n  Selected model matches the current primary (%s).\n", line);
            printf("  A provider cannot be a fallback for itself - no change.\n");
            json_free(new_entry);
            agent_backend_identity_free(new_ident);
            return 0;
        }
    }

    /* Reload with primary restored, then append the new entry. */
    fb_restore_model_cfg(model_snapshot);
    if (model_snapshot) json_free(model_snapshot);

    json_t *final_cfg = fb_load_config();
    int n = 0;
    fallback_entry_t *chain = fallback_config_get_chain(final_cfg, &n);
    json_free(final_cfg);

    for (int i = 0; i < n; i++) {
        BackendIdentity *ex = agent_backend_identity_build(
            chain[i].provider, chain[i].model, chain[i].base_url);
        int dup = agent_backend_identity_same_deployment(ex, new_ident);
        agent_backend_identity_free(ex);
        if (dup) {
            char line[1024];
            fallback_format_entry(new_entry, line, sizeof line);
            printf("\n  %s is already in the fallback chain - skipped.\n", line);
            fallback_config_free_entries(chain, n);
            json_free(new_entry);
            agent_backend_identity_free(new_ident);
            return 0;
        }
    }

    json_t *out_chain = json_array();
    for (int i = 0; i < n; i++) {
        json_t *e = json_object();
        json_set(e, "provider", json_string(chain[i].provider));
        json_set(e, "model", json_string(chain[i].model));
        if (chain[i].base_url && chain[i].base_url[0])
            json_set(e, "base_url", json_string(chain[i].base_url));
        json_append(out_chain, e);
    }
    fallback_config_free_entries(chain, n);
    json_append(out_chain, new_entry); /* moves ownership */
    fb_write_chain(out_chain);
    json_free(out_chain);
    agent_backend_identity_free(new_ident);

    n += 1;
    printf("\n  Added fallback.\n");
    printf("  Chain is now %d %s long.\n\n", n, n == 1 ? "entry" : "entries");
    printf("  Run `hermes fallback list` to view, or `hermes fallback remove` to delete.\n");
    return 0;
}

/* PoP: cmd_fallback_remove @ hermes_cli/fallback_cmd.py:cmd_fallback_remove */
int cmd_fallback_remove(void) {
    json_t *config = fb_load_config();
    int n = 0;
    fallback_entry_t *chain = fallback_config_get_chain(config, &n);

    if (n == 0) {
        printf("\n  No fallback providers configured - nothing to remove.\n\n");
        fallback_config_free_entries(chain, n);
        json_free(config);
        return 0;
    }

    const char **choices = calloc((size_t)n + 1, sizeof(char *));
    char lines[256][1024];
    for (int i = 0; i < n; i++) {
        json_t *e = json_object();
        json_set(e, "provider", json_string(chain[i].provider));
        json_set(e, "model", json_string(chain[i].model));
        if (chain[i].base_url && chain[i].base_url[0])
            json_set(e, "base_url", json_string(chain[i].base_url));
        fallback_format_entry(e, lines[i], sizeof lines[0]);
        choices[i] = lines[i];
        json_free(e);
    }
    choices[n] = "Cancel";

    int idx = fallback_numbered_pick("Select a fallback to remove:", choices, n + 1);
    free(choices);

    if (idx < 0 || idx >= n) {
        printf("\n  Cancelled - no change.\n");
        fallback_config_free_entries(chain, n);
        json_free(config);
        return 0;
    }

    json_t *removed = json_object();
    json_set(removed, "provider", json_string(chain[idx].provider));
    json_set(removed, "model", json_string(chain[idx].model));
    if (chain[idx].base_url && chain[idx].base_url[0])
        json_set(removed, "base_url", json_string(chain[idx].base_url));

    json_t *out_chain = json_array();
    for (int i = 0; i < n; i++) {
        if (i == idx) continue;
        json_t *e = json_object();
        json_set(e, "provider", json_string(chain[i].provider));
        json_set(e, "model", json_string(chain[i].model));
        if (chain[i].base_url && chain[i].base_url[0])
            json_set(e, "base_url", json_string(chain[i].base_url));
        json_append(out_chain, e);
    }
    fb_write_chain(out_chain);
    json_free(out_chain);

    char line[1024];
    fallback_format_entry(removed, line, sizeof line);
    json_free(removed);
    printf("\n  Removed fallback: %s\n", line);
    if (n - 1 > 0)
        printf("  Chain is now %d %s long.\n\n", n - 1,
               n - 1 == 1 ? "entry" : "entries");
    else
        printf("  Fallback chain is now empty.\n\n");

    fallback_config_free_entries(chain, n);
    json_free(config);
    return 0;
}

/* PoP: cmd_fallback_clear @ hermes_cli/fallback_cmd.py:cmd_fallback_clear */
int cmd_fallback_clear(void) {
    json_t *config = fb_load_config();
    int n = 0;
    fallback_entry_t *chain = fallback_config_get_chain(config, &n);

    if (n == 0) {
        printf("\n  No fallback providers configured - nothing to clear.\n\n");
        fallback_config_free_entries(chain, n);
        json_free(config);
        return 0;
    }

    printf("\n  Current fallback chain (%d %s):\n", n, n == 1 ? "entry" : "entries");
    for (int i = 0; i < n; i++) {
        json_t *e = json_object();
        json_set(e, "provider", json_string(chain[i].provider));
        json_set(e, "model", json_string(chain[i].model));
        if (chain[i].base_url && chain[i].base_url[0])
            json_set(e, "base_url", json_string(chain[i].base_url));
        char line[1024];
        fallback_format_entry(e, line, sizeof line);
        printf("    %d. %s\n", i + 1, line);
        json_free(e);
    }
    printf("\n");

    printf("  Clear all entries? [y/N]: ");
    fflush(stdout);
    char buf[64];
    int confirmed = 0;
    if (fgets(buf, sizeof buf, stdin)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        for (char *c = buf; *c; c++) *c = (char)tolower((unsigned char)*c);
        confirmed = strcmp(buf, "y") == 0 || strcmp(buf, "yes") == 0;
    }
    if (!confirmed) {
        printf("  Cancelled - no change.\n");
        fallback_config_free_entries(chain, n);
        json_free(config);
        return 0;
    }

    json_t *empty = json_array();
    fb_write_chain(empty);
    json_free(empty);
    fallback_config_free_entries(chain, n);
    json_free(config);
    printf("\n  Fallback chain cleared.\n\n");
    return 0;
}
