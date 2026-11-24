/*
 * tools.c — C11 home for upstream hermes_cli/web_routers/tools.py.
 *
 * Two differential gates share this one file:
 *
 *   w32_tools_f39c80_p1 — the terminal-backend decision half plus the toolset
 *     model catalog: the config-or-env read every probe is built on, the
 *     docker/ssh/modal/daytona probes as pure (status, detail) decisions over
 *     their seam results, the GET /api/tools/terminal/backends row build and
 *     its active-backend normalisation, the PUT validation, and the catalog
 *     side (plugin resolution, current-selection resolution, the models
 *     payload, the model-write validation, provider-row resolution, the
 *     known-toolset gate and the fixed dispatch maps).
 *   w32_tools_f39c80_p2 — the toolsets matrix / config panel / write half:
 *     the GET /api/tools/toolsets row build, the section normaliser, the
 *     enable/disable plan with its install-on-enable post-setup pick, the
 *     provider matrix payload, the .env write plan and the per-capability
 *     provider selection validation with the managed-Nous pin.
 *
 * SEAMS (I/O and other modules' catalogs — never modelled here): env reads
 * (get_env_value), config.yaml load/save, the profile scope, config-write
 * scope and CONFIG_MUTATION_LOCK, .env get/save, find_docker()/run_capture
 * subprocess IO, the agent terminal registry probe, the plugin backend rows,
 * hermes_cli.tools_config's provider catalogs (visibility, active-provider,
 * readiness, keys, effective toolsets, platform tools save), IMAGEGEN_BACKENDS
 * / _plugin_*_catalog, apply_provider_selection, spawn_profile_action, the
 * Nous subscription/entitlement readers and the web dispatchers' active
 * backend resolution.  The oracles patch those and compare only what the
 * values decide once they have answered.
 *
 * PoP: w32_tools_f39c80_p1_cfg_value         @ hermes_cli/web_routers/tools.py:_terminal_cfg_value
 * PoP: w32_tools_f39c80_p1_probe_ssh         @ hermes_cli/web_routers/tools.py:_probe_ssh_backend
 * PoP: w32_tools_f39c80_p1_probe_modal       @ hermes_cli/web_routers/tools.py:_probe_modal_backend
 * PoP: w32_tools_f39c80_p1_probe_daytona     @ hermes_cli/web_routers/tools.py:_probe_daytona_backend
 * PoP: w32_tools_f39c80_p1_probe_docker      @ hermes_cli/web_routers/tools.py:_probe_docker_backend
 * PoP: w32_tools_f39c80_p1_backends_payload  @ hermes_cli/web_routers/tools.py:get_terminal_backends._read
 * PoP: w32_tools_f39c80_p1_select_backend    @ hermes_cli/web_routers/tools.py:select_terminal_backend._run
 * PoP: w32_tools_f39c80_p1_no_models         @ hermes_cli/web_routers/tools.py:_no_models
 * PoP: w32_tools_f39c80_p1_model_plugin      @ hermes_cli/web_routers/tools.py:_resolve_toolset_model_plugin
 * PoP: w32_tools_f39c80_p1_catalog_section   @ hermes_cli/web_routers/tools.py:_MODEL_CATALOG_TOOLSETS
 * PoP: w32_tools_f39c80_p1_models_payload    @ hermes_cli/web_routers/tools.py:get_toolset_models._read
 * PoP: w32_tools_f39c80_p1_model_select      @ hermes_cli/web_routers/tools.py:select_toolset_model._run
 * PoP: w32_tools_f39c80_p1_provider_row      @ hermes_cli/web_routers/tools.py:_find_toolset_provider_row
 * PoP: w32_tools_f39c80_p1_require_known     @ hermes_cli/web_routers/tools.py:_require_known_toolset
 * PoP: w32_tools_f39c80_p1_constants         @ hermes_cli/web_routers/tools.py:_BACKEND_PROBES / _MODEL_CATALOG_TOOLSETS
 * PoP: w32_tools_f39c80_p2_dict_section      @ hermes_cli/web_routers/tools.py:_dict_section
 * PoP: w32_tools_f39c80_p2_toolsets_rows     @ hermes_cli/web_routers/tools.py:get_toolsets
 * PoP: w32_tools_f39c80_p2_allowed_env_keys  @ hermes_cli/web_routers/tools.py:save_toolset_env._run
 * PoP: w32_tools_f39c80_p2_save_env          @ hermes_cli/web_routers/tools.py:save_toolset_env
 * PoP: w32_tools_f39c80_p2_toggle            @ hermes_cli/web_routers/tools.py:toggle_toolset
 * PoP: w32_tools_f39c80_p2_toolset_config    @ hermes_cli/web_routers/tools.py:get_toolset_config._read
 * PoP: w32_tools_f39c80_p2_provider_capability @ hermes_cli/web_routers/tools.py:select_toolset_provider._run
 */
#include "hermes_json.h"
#include "noc_w32_tools_f39c80_p1.h"
#include "noc_w32_tools_f39c80_p2.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── shared value helpers ───────────────────────────────────────────────── */

static char *tt_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = (char *)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

/* Python truthiness over the JSON subset. */
static bool tt_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
    case JSON_NULL:   return false;
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return false;
}

static const json_t *tt_jget(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    return json_obj_get(obj, key);
}

/* Python str() over the JSON subset the router stringifies. */
static char *tt_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tt_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return tt_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return tt_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        char tmp[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
        else
            snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
        return tt_dup(tmp);
    }
    default: {
        char *ser = json_dumps(v, 0);
        char *out = tt_dup(ser ? ser : "");
        free(ser);
        return out;
    }
    }
}

/* Python repr() for strings, as an error detail carries them. */
static char *tt_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return tt_dup("");
    size_t j = 0;
    out[j++] = quote;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\\') { out[j++] = '\\'; out[j++] = '\\'; }
        else if (c == quote) { out[j++] = '\\'; out[j++] = c; }
        else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
        else if (c == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
        else if (c == '\t') { out[j++] = '\\'; out[j++] = 't'; }
        else out[j++] = c;
    }
    out[j++] = quote;
    out[j] = '\0';
    return out;
}

static char *tt_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tt_dup("None");
    if (v->type == JSON_BOOL) return tt_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return tt_repr_str(v->str_val);
    return tt_str(v);
}

static bool tt_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip(); mutates and returns the first kept byte. */
static char *tt_strip_inplace(char *s) {
    if (!s) return s;
    size_t n = strlen(s), i = 0;
    while (i < n && tt_is_space(s[i])) i++;
    size_t j = n;
    while (j > i && tt_is_space(s[j - 1])) j--;
    s[j] = '\0';
    return s + i;
}

static void tt_lower_inplace(char *s) {
    if (!s) return;
    for (size_t i = 0; s[i]; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
}

/* str(v or dflt).strip() as a fresh allocation. */
static char *tt_strip_of(const json_t *v, const char *dflt) {
    char *raw = tt_truthy(v) ? tt_str(v) : tt_dup(dflt ? dflt : "");
    char *base = tt_strip_inplace(raw);
    char *out = tt_dup(base);
    free(raw);
    return out;
}

static void tt_set(json_t *obj, const char *key, json_t *val) {
    json_set(obj, key, val ? val : json_null());
}

/* `obj.get(key, dflt)` — takes ownership of dflt. */
static json_t *tt_getd(const json_t *obj, const char *key, json_t *dflt) {
    const json_t *v = tt_jget(obj, key);
    if (v) return json_copy(v);
    return dflt ? dflt : json_null();
}

static bool tt_str_eq(const json_t *v, const char *s) {
    return v && v->type == JSON_STRING && v->str_val && s && !strcmp(v->str_val, s);
}

static bool tt_in_strlist(const json_t *list, const char *needle) {
    if (!list || list->type != JSON_ARRAY || !needle) return false;
    for (size_t i = 0; i < json_len(list); i++) {
        const json_t *v = json_get(list, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, needle)) return true;
    }
    return false;
}

static int tt_cmp_str(const void *a, const void *b) {
    const char *const *x = (const char *const *)a;
    const char *const *y = (const char *const *)b;
    return strcmp(*x, *y);
}

/* sorted({s for s in list if isinstance(s, str)}) as a fresh JSON array. */
static json_t *tt_sorted_unique(const json_t *list) {
    json_t *out = json_array();
    if (!list || list->type != JSON_ARRAY) return out;
    size_t n = json_len(list);
    char **buf = (char **)calloc(n ? n : 1, sizeof(char *));
    if (!buf) return out;
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(list, i);
        if (!v || v->type != JSON_STRING) continue;
        bool seen = false;
        for (size_t k = 0; k < used; k++)
            if (!strcmp(buf[k], v->str_val ? v->str_val : "")) { seen = true; break; }
        if (!seen) buf[used++] = (char *)(v->str_val ? v->str_val : "");
    }
    if (used) qsort(buf, used, sizeof(char *), tt_cmp_str);
    for (size_t i = 0; i < used; i++) json_append(out, json_string(buf[i]));
    free(buf);
    return out;
}

/* is_truthy_value(value, default=...): None -> default, str -> the shared
 * truthy-string set after strip().lower(), anything else -> bool(value). */
static bool tt_is_truthy_value(const json_t *v, bool dflt) {
    if (!v || v->type == JSON_NULL) return dflt;
    if (v->type == JSON_STRING) {
        char *raw = tt_dup(v->str_val ? v->str_val : "");
        char *base = tt_strip_inplace(raw);
        tt_lower_inplace(base);
        bool hit = !strcmp(base, "1") || !strcmp(base, "true") ||
                   !strcmp(base, "yes") || !strcmp(base, "on");
        free(raw);
        return hit;
    }
    return tt_truthy(v);
}

static json_t *tt_err(const char *detail) {
    json_t *o = json_object();
    tt_set(o, "ok", json_bool(false));
    tt_set(o, "status", json_number(400));
    tt_set(o, "detail", json_string(detail ? detail : ""));
    return o;
}

static json_t *tt_ok(void) {
    json_t *o = json_object();
    tt_set(o, "ok", json_bool(true));
    return o;
}

/* "a, b, c" from a JSON array of strings. */
static char *tt_join_strings(const json_t *arr) {
    size_t total = 0;
    if (!arr || arr->type != JSON_ARRAY) return tt_dup("");
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        total += (v && v->type == JSON_STRING ? strlen(v->str_val) : 0) + 2;
    }
    char *out = (char *)calloc(total + 1, 1);
    if (!out) return tt_dup("");
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING) {
            if (i) strcat(out, ", ");
            strcat(out, v->str_val);
        }
    }
    return out;
}

/* ── p1: terminal-backend decisions ─────────────────────────────────────── */

json_t *w32_tools_f39c80_p1_cfg_value(const json_t *terminal_cfg, const json_t *cfg_key,
                                      const json_t *env_var, const json_t *env_map) {
    const char *key = (cfg_key && cfg_key->type == JSON_STRING) ? cfg_key->str_val : NULL;
    const char *env_name = (env_var && env_var->type == JSON_STRING) ? env_var->str_val : NULL;
    char *out = NULL;

    /* value = terminal_cfg.get(key); `value is not None and str(value).strip()` */
    const json_t *value = tt_jget(terminal_cfg, key);
    if (value && value->type != JSON_NULL) {
        char *raw = tt_str(value);
        char *base = tt_strip_inplace(raw);
        if (*base) out = tt_dup(base);
        free(raw);
    }
    if (!out) {
        const json_t *ev = tt_jget(env_map, env_name);
        char *raw = tt_truthy(ev) ? tt_str(ev) : tt_dup("");   /* get_env_value(name) or "" */
        char *base = tt_strip_inplace(raw);
        out = tt_dup(base);
        free(raw);
    }
    json_t *node = json_string(out ? out : "");
    free(out);
    return node;
}

static json_t *tt_probe_result(const char *status, const char *detail) {
    json_t *o = json_object();
    tt_set(o, "status", json_string(status));
    tt_set(o, "detail", json_string(detail ? detail : ""));
    return o;
}

json_t *w32_tools_f39c80_p1_probe_ssh(const json_t *terminal_cfg, const json_t *env_map) {
    json_t *host_node = w32_tools_f39c80_p1_cfg_value(terminal_cfg, json_string("ssh_host"),
                                                     json_string("TERMINAL_SSH_HOST"), env_map);
    json_t *user_node = w32_tools_f39c80_p1_cfg_value(terminal_cfg, json_string("ssh_user"),
                                                     json_string("TERMINAL_SSH_USER"), env_map);
    const char *host = (host_node && host_node->type == JSON_STRING) ? host_node->str_val : "";
    const char *user = (user_node && user_node->type == JSON_STRING) ? user_node->str_val : "";

    json_t *out;
    if (!host[0] || !user[0]) {
        char *missing = NULL;
        if (!host[0]) missing = tt_dup("terminal.ssh_host");
        if (!user[0]) {
            if (missing) {
                size_t need = strlen(missing) + sizeof(" and terminal.ssh_user") + 1;
                char *joined = (char *)malloc(need);
                if (joined) snprintf(joined, need, "%s and terminal.ssh_user", missing);
                free(missing);
                missing = joined;
            } else {
                missing = tt_dup("terminal.ssh_user");
            }
        }
        size_t need = strlen(missing ? missing : "") +
                      sizeof("Set  in config.yaml (or the matching TERMINAL_SSH_* env vars).") + 1;
        char *detail = (char *)malloc(need);
        if (detail)
            snprintf(detail, need, "Set %s in config.yaml (or the matching TERMINAL_SSH_* env vars).",
                     missing ? missing : "");
        out = tt_probe_result("needs_setup", detail ? detail : "");
        free(detail);
        free(missing);
    } else {
        size_t need = strlen(user) + strlen(host) + 2;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "%s@%s", user, host);
        out = tt_probe_result("ready", detail ? detail : "");
        free(detail);
    }
    json_free(host_node);
    json_free(user_node);
    return out;
}

json_t *w32_tools_f39c80_p1_probe_modal(const json_t *has_direct, const json_t *direct_raises,
                                        const json_t *env_map) {
    if (!tt_truthy(direct_raises) && tt_truthy(has_direct)) return tt_probe_result("ready", "");
    if (tt_truthy(tt_jget(env_map, "MODAL_TOKEN_ID")) &&
        tt_truthy(tt_jget(env_map, "MODAL_TOKEN_SECRET")))
        return tt_probe_result("ready", "");
    return tt_probe_result(
        "needs_setup",
        "Modal credentials not found — set MODAL_TOKEN_ID and MODAL_TOKEN_SECRET (or run `modal setup`).");
}

json_t *w32_tools_f39c80_p1_probe_daytona(const json_t *env_map) {
    if (tt_truthy(tt_jget(env_map, "DAYTONA_API_KEY"))) return tt_probe_result("ready", "");
    return tt_probe_result("needs_setup", "Set DAYTONA_API_KEY to use the Daytona backend.");
}

json_t *w32_tools_f39c80_p1_probe_docker(const json_t *state) {
    if (!tt_truthy(tt_jget(state, "found")))
        return tt_probe_result("needs_setup",
                               "Docker CLI not found — install Docker Desktop, docker-ce, or Podman.");
    char *runtime = tt_strip_of(tt_jget(state, "runtime"), "");
    const char *outcome = json_get_str(state, "outcome", "ok");
    json_t *out;
    if (!strcmp(outcome, "timeout")) {
        size_t need = strlen(runtime) + sizeof(" not responding (timed out).") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "%s not responding (timed out).", runtime);
        out = tt_probe_result("needs_setup", detail ? detail : "");
        free(detail);
    } else if (!strcmp(outcome, "error")) {
        char *error = tt_strip_of(tt_jget(state, "error"), "");
        size_t need = strlen(runtime) + strlen(error) + sizeof(" probe failed: ") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "%s probe failed: %s", runtime, error);
        out = tt_probe_result("unavailable", detail ? detail : "");
        free(detail);
        free(error);
    } else if (json_get_num(state, "returncode", 0) == 0.0) {
        out = tt_probe_result("ready", "");
    } else {
        char *hint = tt_strip_of(tt_jget(state, "hint"), "");
        size_t need = strlen(runtime) + strlen(hint) + sizeof(" not reachable — .") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "%s not reachable — %s.", runtime, hint);
        out = tt_probe_result("needs_setup", detail ? detail : "");
        free(detail);
        free(hint);
    }
    free(runtime);
    return out;
}

json_t *w32_tools_f39c80_p1_backends_payload(const json_t *rows, const json_t *config,
                                             const json_t *probes) {
    const json_t *terminal = tt_jget(config, "terminal");
    if (!terminal || terminal->type != JSON_OBJECT) terminal = NULL;

    /* active = str(terminal_cfg.get("backend") or "local").strip().lower() */
    const json_t *backend = tt_jget(terminal, "backend");
    char *raw = tt_truthy(backend) ? tt_str(backend) : tt_dup("local");
    char *active = tt_strip_inplace(raw);
    tt_lower_inplace(active);

    bool known = false;
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++)
            if (tt_str_eq(tt_jget(json_get(rows, i), "name"), active)) { known = true; break; }
    }
    if (!known) {
        free(raw);
        raw = tt_dup("local");
        active = raw;
    }

    json_t *out = json_object();
    tt_set(out, "active", json_string(active));

    json_t *list = json_array();
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            const json_t *row = json_get(rows, i);
            const char *name = json_get_str(row, "name", "");
            const json_t *probe = tt_jget(probes, name);
            json_t *entry = json_object();
            tt_set(entry, "name", json_string(name));
            tt_set(entry, "label", tt_getd(row, "label", json_string("")));
            tt_set(entry, "description", tt_getd(row, "description", json_string("")));
            tt_set(entry, "active", json_bool(!strcmp(name, active)));
            if (probe && probe->type == JSON_OBJECT) {
                tt_set(entry, "status", tt_getd(probe, "status", json_string("unavailable")));
                tt_set(entry, "detail", tt_getd(probe, "detail", json_string("")));
            } else {
                /* The seam's own unknown-backend answer. */
                size_t need = strlen(name) + sizeof("Unknown backend: ") + 1;
                char *msg = (char *)malloc(need);
                if (msg) snprintf(msg, need, "Unknown backend: %s", name);
                tt_set(entry, "status", json_string("unavailable"));
                tt_set(entry, "detail", json_string(msg ? msg : ""));
                free(msg);
            }
            json_append(list, entry);
        }
    }
    tt_set(out, "backends", list);
    free(raw);
    return out;
}

json_t *w32_tools_f39c80_p1_select_backend(const json_t *rows, const json_t *backend) {
    char *raw = tt_truthy(backend) ? tt_str(backend) : tt_dup("");
    char *base = tt_strip_inplace(raw);
    char *norm = tt_dup(base);
    tt_lower_inplace(norm);

    bool valid = false;
    json_t *names = json_array();
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            const json_t *row = json_get(rows, i);
            const char *name = json_get_str(row, "name", "");
            if (!strcmp(name, norm)) valid = true;
            if (!tt_in_strlist(names, name)) json_append(names, json_string(name));
        }
    }

    json_t *out;
    if (valid) {
        out = tt_ok();
        tt_set(out, "backend", json_string(norm));
    } else {
        json_t *sorted = tt_sorted_unique(names);
        char *joined = tt_join_strings(sorted);
        char *repr = tt_repr(backend);
        size_t need = strlen(repr) + strlen(joined) +
                      sizeof("Unknown terminal backend: . Use one of: ") + 1;
        char *detail = (char *)malloc(need);
        if (detail)
            snprintf(detail, need, "Unknown terminal backend: %s. Use one of: %s", repr, joined);
        out = tt_err(detail ? detail : "");
        free(detail);
        free(repr);
        free(joined);
        json_free(sorted);
    }
    json_free(names);
    free(norm);
    free(raw);
    return out;
}

/* ── p1: toolset model catalog ──────────────────────────────────────────── */

json_t *w32_tools_f39c80_p1_no_models(const json_t *name) {
    json_t *o = json_object();
    tt_set(o, "name", name ? json_copy(name) : json_null());
    tt_set(o, "has_models", json_bool(false));
    tt_set(o, "models", json_array());
    tt_set(o, "current", json_null());
    tt_set(o, "default", json_null());
    return o;
}

json_t *w32_tools_f39c80_p1_model_plugin(const json_t *ts_key, const json_t *row) {
    const char *ts = (ts_key && ts_key->type == JSON_STRING) ? ts_key->str_val : "";
    const json_t *pick = NULL;
    if (!strcmp(ts, "image_gen")) {
        const json_t *a = tt_jget(row, "image_gen_plugin_name");
        const json_t *b = tt_jget(row, "imagegen_backend");
        pick = tt_truthy(a) ? a : b;
    } else if (!strcmp(ts, "video_gen")) {
        pick = tt_jget(row, "video_gen_plugin_name");
    }
    return pick ? json_copy(pick) : json_null();
}

json_t *w32_tools_f39c80_p1_catalog_section(const json_t *name) {
    const char *n = (name && name->type == JSON_STRING) ? name->str_val : "";
    if (!strcmp(n, "image_gen") || !strcmp(n, "video_gen")) return json_string(n);
    return json_null();
}

static bool tt_obj_has_key(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && key && json_obj_get(obj, key) != NULL;
}

json_t *w32_tools_f39c80_p1_models_payload(const json_t *name, const json_t *config,
                                           const json_t *row, const json_t *catalog,
                                           const json_t *default_model) {
    const char *nm = (name && name->type == JSON_STRING) ? name->str_val : "";
    json_t *section_node = w32_tools_f39c80_p1_catalog_section(name);
    if (!section_node || section_node->type != JSON_STRING) {
        json_free(section_node);
        return w32_tools_f39c80_p1_no_models(name);
    }
    const char *section = section_node->str_val;

    json_t *plugin_node = w32_tools_f39c80_p1_model_plugin(name, row);
    if (!tt_truthy(plugin_node)) {
        json_free(plugin_node);
        json_free(section_node);
        return w32_tools_f39c80_p1_no_models(name);
    }

    /* current: <section>.model when non-blank, else the default when the
     * catalog carries it. */
    char *current = NULL;
    const json_t *section_cfg = tt_jget(config, section);
    if (section_cfg && section_cfg->type == JSON_OBJECT) {
        const json_t *raw_model = tt_jget(section_cfg, "model");
        if (raw_model && raw_model->type == JSON_STRING) {
            char *copy = tt_dup(raw_model->str_val);
            char *base = tt_strip_inplace(copy);
            if (*base) current = tt_dup(base);
            free(copy);
        }
    }
    if (!current || !tt_obj_has_key(catalog, current)) {
        free(current);
        current = NULL;
        if (default_model && default_model->type == JSON_STRING &&
            tt_obj_has_key(catalog, default_model->str_val))
            current = tt_dup(default_model->str_val);
    }

    json_t *models = json_array();
    if (catalog && catalog->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(catalog); i++) {
            const char *model_id = json_object_get_key_at(catalog, i);
            const json_t *meta = json_object_get_at(catalog, i);
            if (!model_id) continue;
            if (!meta || meta->type != JSON_OBJECT) meta = NULL;
            json_t *entry = json_object();
            tt_set(entry, "id", json_string(model_id));
            tt_set(entry, "display", tt_getd(meta, "display", json_string(model_id)));
            tt_set(entry, "speed", tt_getd(meta, "speed", json_string("")));
            tt_set(entry, "strengths", tt_getd(meta, "strengths", json_string("")));
            tt_set(entry, "price", tt_getd(meta, "price", json_string("")));
            json_append(models, entry);
        }
    }

    json_t *out = json_object();
    tt_set(out, "name", json_string(nm));
    tt_set(out, "has_models", json_bool(json_len(models) > 0));
    tt_set(out, "provider", tt_getd(row, "name", json_null()));
    tt_set(out, "plugin", json_copy(plugin_node));
    tt_set(out, "models", models);
    tt_set(out, "current", current ? json_string(current) : json_null());
    tt_set(out, "default", default_model ? json_copy(default_model) : json_null());

    free(current);
    json_free(plugin_node);
    json_free(section_node);
    return out;
}

json_t *w32_tools_f39c80_p1_model_select(const json_t *name, const json_t *model,
                                         const json_t *config, const json_t *row,
                                         const json_t *catalog) {
    (void)config;
    char *name_str = tt_str(name);
    json_t *section_node = w32_tools_f39c80_p1_catalog_section(name);
    if (!section_node || section_node->type != JSON_STRING) {
        json_free(section_node);
        size_t need = strlen(name_str) + sizeof("Toolset has no model catalog: ") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "Toolset has no model catalog: %s", name_str);
        json_t *out = tt_err(detail ? detail : "");
        free(detail);
        free(name_str);
        return out;
    }
    json_free(section_node);

    char *model_id = tt_strip_of(model, "");
    if (!model_id[0]) {
        free(model_id);
        free(name_str);
        return tt_err("model is required");
    }

    json_t *plugin_node = w32_tools_f39c80_p1_model_plugin(name, row);
    if (!tt_truthy(plugin_node)) {
        size_t need = strlen(name_str) + sizeof("No model-capable backend is active for ") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "No model-capable backend is active for %s", name_str);
        json_t *out = tt_err(detail ? detail : "");
        free(detail);
        json_free(plugin_node);
        free(model_id);
        free(name_str);
        return out;
    }

    json_t *out;
    if (!tt_obj_has_key(catalog, model_id)) {
        char *mrepr = tt_repr_str(model_id);
        char *prepr = tt_repr(plugin_node);
        size_t need = strlen(mrepr) + strlen(prepr) + sizeof("Unknown model  for backend ") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "Unknown model %s for backend %s", mrepr, prepr);
        out = tt_err(detail ? detail : "");
        free(detail);
        free(mrepr);
        free(prepr);
    } else {
        out = tt_ok();
        tt_set(out, "name", json_string(name_str));
        tt_set(out, "model", json_string(model_id));
        tt_set(out, "plugin", json_copy(plugin_node));
    }
    json_free(plugin_node);
    free(model_id);
    free(name_str);
    return out;
}

json_t *w32_tools_f39c80_p1_provider_row(const json_t *rows, const json_t *provider,
                                         const json_t *active_flags) {
    if (!rows || rows->type != JSON_ARRAY) return json_null();
    if (tt_truthy(provider)) {
        const char *want = provider->type == JSON_STRING ? provider->str_val : "";
        for (size_t i = 0; i < json_len(rows); i++) {
            const json_t *row = json_get(rows, i);
            if (tt_str_eq(tt_jget(row, "name"), want)) return json_copy(row);
        }
        return json_null();
    }
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        const char *nm = json_get_str(row, "name", "");
        if (tt_truthy(tt_jget(active_flags, nm))) return json_copy(row);
    }
    return json_null();
}

json_t *w32_tools_f39c80_p1_require_known(const json_t *name, const json_t *known_names) {
    const char *nm = (name && name->type == JSON_STRING) ? name->str_val : "";
    bool known = false;
    if (known_names && known_names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(known_names); i++)
            if (tt_str_eq(json_get(known_names, i), nm)) { known = true; break; }
    }
    if (known) return tt_ok();
    size_t need = strlen(nm) + sizeof("Unknown toolset: ") + 1;
    char *detail = (char *)malloc(need);
    if (detail) snprintf(detail, need, "Unknown toolset: %s", nm);
    json_t *out = tt_err(detail ? detail : "");
    free(detail);
    return out;
}

json_t *w32_tools_f39c80_p1_constants(void) {
    json_t *out = json_object();
    json_t *catalog = json_object();
    tt_set(catalog, "image_gen", json_string("image_gen"));
    tt_set(catalog, "video_gen", json_string("video_gen"));
    tt_set(out, "model_catalog_toolsets", catalog);
    json_t *probes = json_array();
    static const char *names[] = {"local", "docker", "ssh", "modal", "daytona"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        json_append(probes, json_string(names[i]));
    tt_set(out, "backend_probes", probes);
    return out;
}

/* ── p2: toolsets matrix ────────────────────────────────────────────────── */

json_t *w32_tools_f39c80_p2_dict_section(const json_t *config, const json_t *key) {
    const char *k = (key && key->type == JSON_STRING) ? key->str_val : NULL;
    json_t *out = (config && config->type == JSON_OBJECT) ? json_copy(config) : json_object();
    if (!out) out = json_object();
    const json_t *existing = tt_jget(out, k);
    if (!existing || existing->type != JSON_OBJECT) tt_set(out, k, json_object());
    return out;
}

json_t *w32_tools_f39c80_p2_toolsets_rows(const json_t *rows, const json_t *platform_map,
                                          const json_t *platform_tools, const json_t *configured,
                                          const json_t *gui_labels, const json_t *platform_labels,
                                          const json_t *config, const json_t *config_only,
                                          const json_t *tools_raw, const json_t *tools_error) {
    json_t *result = json_array();
    if (!rows || rows->type != JSON_ARRAY) return result;

    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        const char *name = json_get_str(row, "name", "");
        const char *label = json_get_str(row, "label", "");
        const json_t *desc = tt_jget(row, "description");

        json_t *tools;
        if (tt_in_strlist(tools_error, name)) {
            tools = json_array();   /* resolve_toolset raised -> tools: [] */
        } else {
            tools = tt_sorted_unique(tt_jget(tools_raw, name));
        }

        const char *platform = json_get_str(platform_map, name, "cli");

        bool is_enabled;
        if (tt_in_strlist(config_only, name)) {
            /* Config-only capabilities (stt) switch their own section. */
            const json_t *section = tt_jget(config, name);
            if (!section || section->type != JSON_OBJECT) section = NULL;
            is_enabled = tt_is_truthy_value(tt_jget(section, "enabled"), true);
        } else {
            is_enabled = tt_in_strlist(tt_jget(platform_tools, platform), name);
        }

        const json_t *gui = tt_jget(gui_labels, label);   /* gui_toolset_label(label) */
        json_t *gui_label = gui ? json_copy(gui) : json_string(label);
        const char *plat_raw = json_get_str(platform_labels, platform, platform);
        const json_t *plat_gui = tt_jget(gui_labels, plat_raw);  /* gui_toolset_label(platform_label(...)) */
        json_t *plat_label = plat_gui ? json_copy(plat_gui) : json_string(plat_raw);

        json_t *entry = json_object();
        tt_set(entry, "name", json_string(name));
        tt_set(entry, "label", gui_label);
        tt_set(entry, "description", desc ? json_copy(desc) : json_null());
        tt_set(entry, "platform", json_string(platform));
        tt_set(entry, "platform_label", plat_label);
        tt_set(entry, "enabled", json_bool(is_enabled));
        tt_set(entry, "available", json_bool(is_enabled));
        tt_set(entry, "configured", json_bool(tt_truthy(tt_jget(configured, name))));
        tt_set(entry, "tools", tools);
        json_append(result, entry);
    }
    return result;
}

json_t *w32_tools_f39c80_p2_allowed_env_keys(const json_t *providers) {
    json_t *keys = json_array();
    if (providers && providers->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(providers); i++) {
            const json_t *prov = json_get(providers, i);
            const json_t *env_vars = tt_jget(prov, "env_vars");
            if (!env_vars || env_vars->type != JSON_ARRAY) continue;
            for (size_t j = 0; j < json_len(env_vars); j++) {
                const char *key = json_get_str(json_get(env_vars, j), "key", NULL);
                if (key && !tt_in_strlist(keys, key)) json_append(keys, json_string(key));
            }
        }
    }
    return tt_sorted_unique(keys);
}

json_t *w32_tools_f39c80_p2_save_env(const json_t *name, const json_t *providers,
                                     const json_t *env_body, const json_t *env_set,
                                     const json_t *save_errors, const json_t *known) {
    char *name_str = tt_str(name);

    json_t *known_chk = w32_tools_f39c80_p1_require_known(name, known);
    if (!json_get_bool(known_chk, "ok", false)) return known_chk;
    json_free(known_chk);

    json_t *allowed = w32_tools_f39c80_p2_allowed_env_keys(providers);

    /* unknown = [k for k in body.env if k not in allowed] */
    json_t *unknown = json_array();
    if (env_body && env_body->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(env_body); i++) {
            const char *key = json_object_get_key_at(env_body, i);
            if (key && !tt_in_strlist(allowed, key)) json_append(unknown, json_string(key));
        }
    }
    if (json_len(unknown) > 0) {
        json_t *sorted = tt_sorted_unique(unknown);
        char *joined = tt_join_strings(sorted);
        size_t need = strlen(name_str) + strlen(joined) +
                      sizeof("Unknown env var(s) for toolset : ") + 1;
        char *detail = (char *)malloc(need);
        if (detail)
            snprintf(detail, need, "Unknown env var(s) for toolset %s: %s", name_str, joined);
        json_t *out = tt_err(detail ? detail : "");
        free(detail);
        free(joined);
        json_free(sorted);
        json_free(unknown);
        json_free(allowed);
        free(name_str);
        return out;
    }
    json_free(unknown);

    json_t *saved = json_array();
    json_t *skipped = json_array();
    json_t *error = NULL;
    if (env_body && env_body->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(env_body); i++) {
            const char *key = json_object_get_key_at(env_body, i);
            const json_t *value = json_object_get_at(env_body, i);
            char *trimmed = NULL;
            if (value && value->type == JSON_STRING) {
                char *copy = tt_dup(value->str_val);
                char *base = tt_strip_inplace(copy);
                trimmed = tt_dup(base);
                free(copy);
            }
            if (trimmed && trimmed[0]) {
                const json_t *raise_text = tt_jget(save_errors, key);
                if (raise_text) {
                    error = tt_err(raise_text->type == JSON_STRING ? raise_text->str_val : "");
                    free(trimmed);
                    break;
                }
                json_append(saved, json_string(key));
                free(trimmed);
            } else {
                free(trimmed);
                json_append(skipped, json_string(key));
            }
        }
    }
    if (error) {
        json_free(saved);
        json_free(skipped);
        json_free(allowed);
        free(name_str);
        return error;
    }

    json_t *status = json_object();
    for (size_t i = 0; i < json_len(allowed); i++) {
        const char *key = json_get(allowed, i)->str_val;
        tt_set(status, key, json_bool(tt_truthy(tt_jget(env_set, key))));
    }

    json_t *out = tt_ok();
    tt_set(out, "name", json_string(name_str));
    tt_set(out, "saved", saved);
    tt_set(out, "skipped", skipped);
    tt_set(out, "is_set", status);
    json_free(allowed);
    free(name_str);
    return out;
}

/* ── p2: toggle plan ────────────────────────────────────────────────────── */

json_t *w32_tools_f39c80_p2_toggle(const json_t *req) {
    const char *name = json_get_str(req, "name", "");
    const json_t *enabled_node = tt_jget(req, "enabled");
    bool enabled = tt_truthy(enabled_node);
    const json_t *known = tt_jget(req, "known");
    const json_t *config_only = tt_jget(req, "config_only");
    const json_t *platform_map = tt_jget(req, "platform_map");
    const json_t *platform_tools = tt_jget(req, "platform_tools");
    const char *platform = json_get_str(platform_map, name, "cli");
    bool is_config_only = tt_in_strlist(config_only, name);

    json_t *known_chk = w32_tools_f39c80_p1_require_known(json_string(name), known);
    if (!json_get_bool(known_chk, "ok", false)) return known_chk;
    json_free(known_chk);

    json_t *out = json_object();
    json_t *saved;
    if (is_config_only) {
        /* Config-only capabilities toggle their own section's enabled flag. */
        saved = json_object();
        tt_set(saved, "section", json_string(name));
        tt_set(saved, "enabled", json_bool(enabled));
    } else {
        const json_t *current = tt_jget(platform_tools, platform);
        json_t *merged = json_array();
        if (current && current->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(current); i++)
                json_append(merged, json_copy(json_get(current, i)));
        }
        /* enabled.add(name) / enabled.discard(name) */
        bool present = tt_in_strlist(merged, name);
        if (enabled && !present) json_append(merged, json_string(name));
        if (!enabled && present) {
            json_t *kept = json_array();
            for (size_t i = 0; i < json_len(merged); i++) {
                const json_t *v = json_get(merged, i);
                if (v->type == JSON_STRING && !strcmp(v->str_val, name)) continue;
                json_append(kept, json_copy(v));
            }
            json_free(merged);
            merged = kept;
        }
        saved = json_object();
        tt_set(saved, "platform", json_string(platform));
        tt_set(saved, "tools", tt_sorted_unique(merged));
        json_free(merged);
    }

    /* Install-on-enable: the first post_setup key whose install predicate is
     * UNSATISFIED, only for a non-config-only toolset being turned on. */
    json_t *post_setup_started = json_null();
    if (enabled && !is_config_only && tt_truthy(tt_jget(req, "has_category"))) {
        const json_t *providers = tt_jget(req, "providers");
        const json_t *installed = tt_jget(req, "installed");
        if (providers && providers->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(providers); i++) {
                const json_t *key = tt_jget(json_get(providers, i), "post_setup");
                if (!tt_truthy(key)) continue;
                const char *k = key->type == JSON_STRING ? key->str_val : "";
                if (tt_in_strlist(installed, k)) continue;
                json_free(post_setup_started);
                post_setup_started = json_string(k);
                break;
            }
        }
    }

    json_t *response = json_object();
    tt_set(response, "ok", json_bool(true));
    tt_set(response, "name", json_string(name));
    tt_set(response, "platform", json_string(platform));
    tt_set(response, "enabled", enabled_node ? json_copy(enabled_node) : json_null());
    tt_set(response, "post_setup_started", post_setup_started);

    tt_set(out, "response", response);
    tt_set(out, "saved", saved);
    return out;
}

/* ── p2: config panel payload ───────────────────────────────────────────── */

json_t *w32_tools_f39c80_p2_toolset_config(const json_t *req) {
    const char *name = json_get_str(req, "name", "");
    const json_t *known = tt_jget(req, "known");
    const json_t *providers = tt_jget(req, "providers");
    const json_t *active_flags = tt_jget(req, "active_flags");
    const json_t *statuses = tt_jget(req, "statuses");
    const json_t *caps = tt_jget(req, "caps");
    const json_t *env_set = tt_jget(req, "env_set");

    json_t *known_chk = w32_tools_f39c80_p1_require_known(json_string(name), known);
    if (!json_get_bool(known_chk, "ok", false)) return known_chk;
    json_free(known_chk);

    json_t *rows = json_array();
    json_t *active_provider = json_null();
    bool is_web = !strcmp(name, "web");
    bool is_tts = !strcmp(name, "tts");

    if (providers && providers->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(providers); i++) {
            const json_t *prov = json_get(providers, i);
            const char *pname = json_get_str(prov, "name", "");

            json_t *env_vars = json_array();
            const json_t *declared = tt_jget(prov, "env_vars");
            if (declared && declared->type == JSON_ARRAY) {
                for (size_t j = 0; j < json_len(declared); j++) {
                    const json_t *e = json_get(declared, j);
                    const char *key = json_get_str(e, "key", "");
                    json_t *var = json_object();
                    tt_set(var, "key", json_string(key));
                    tt_set(var, "prompt", tt_getd(e, "prompt", json_string(key)));
                    tt_set(var, "url", tt_getd(e, "url", json_null()));
                    tt_set(var, "default", tt_getd(e, "default", json_null()));
                    tt_set(var, "is_set", json_bool(tt_truthy(tt_jget(env_set, key))));
                    json_append(env_vars, var);
                }
            }

            bool is_active = tt_truthy(tt_jget(active_flags, pname));
            if (is_active && active_provider->type == JSON_NULL) {
                json_free(active_provider);
                active_provider = json_string(pname);
            }

            json_t *row = json_object();
            tt_set(row, "name", json_string(pname));
            tt_set(row, "badge", tt_getd(prov, "badge", json_string("")));
            tt_set(row, "tag", tt_getd(prov, "tag", json_string("")));
            tt_set(row, "env_vars", env_vars);
            tt_set(row, "post_setup", tt_getd(prov, "post_setup", json_null()));
            tt_set(row, "requires_nous_auth",
                   json_bool(tt_truthy(tt_jget(prov, "requires_nous_auth"))));
            tt_set(row, "managed_nous_feature",
                   tt_getd(prov, "managed_nous_feature", json_null()));
            tt_set(row, "is_active", json_bool(is_active));
            tt_set(row, "status", tt_getd(statuses, pname, json_null()));

            if (is_web && tt_truthy(tt_jget(prov, "web_backend"))) {
                const json_t *backend = tt_jget(prov, "web_backend");
                tt_set(row, "web_backend", json_copy(backend));
                const json_t *cap = tt_jget(caps, backend->str_val);
                if (cap) {
                    tt_set(row, "capabilities", json_copy(cap));
                } else {
                    json_t *both = json_array();   /* unknown backend -> both */
                    json_append(both, json_string("search"));
                    json_append(both, json_string("extract"));
                    tt_set(row, "capabilities", both);
                }
            }
            if (is_tts && tt_truthy(tt_jget(prov, "tts_provider")))
                tt_set(row, "tts_provider", json_copy(tt_jget(prov, "tts_provider")));

            json_append(rows, row);
        }
    }

    json_t *payload = json_object();
    tt_set(payload, "name", json_string(name));
    tt_set(payload, "has_category", json_bool(tt_truthy(tt_jget(req, "has_category"))));
    tt_set(payload, "providers", rows);
    tt_set(payload, "active_provider", active_provider);

    if (is_web) {
        /* Active backends exactly as the dispatchers resolve them, plus
         * whether the call rides the Nous gateway. */
        const json_t *web_state = tt_jget(req, "web_state");
        const json_t *search_backend = tt_jget(web_state, "search_backend");
        const json_t *extract_backend = tt_jget(web_state, "extract_backend");
        bool search_is_firecrawl = tt_str_eq(search_backend, "firecrawl");
        bool extract_is_firecrawl = tt_str_eq(extract_backend, "firecrawl");
        bool search_managed = tt_truthy(tt_jget(web_state, "managed_search")) ||
                              (search_is_firecrawl &&
                               tt_truthy(tt_jget(web_state, "is_managed_search")));
        bool extract_managed = extract_is_firecrawl &&
                               tt_truthy(tt_jget(web_state, "is_managed_extract"));
        tt_set(payload, "active_search_backend",
               search_backend ? json_copy(search_backend) : json_null());
        tt_set(payload, "active_extract_backend",
               extract_backend ? json_copy(extract_backend) : json_null());
        tt_set(payload, "search_via_nous", json_bool(search_managed));
        tt_set(payload, "extract_via_nous", json_bool(extract_managed));
    }
    return payload;
}

/* ── p2: per-capability provider selection ──────────────────────────────── */

/* MANAGED_FEATURE_COVERAGE_CATEGORY (hermes_cli.nous_subscription). */
static const char *tt_managed_category(const char *feature) {
    if (!feature) return NULL;
    if (!strcmp(feature, "web")) return "firecrawl";
    if (!strcmp(feature, "image_gen")) return "fal";
    if (!strcmp(feature, "video_gen")) return "fal-video";
    if (!strcmp(feature, "tts") || !strcmp(feature, "stt")) return "openai-audio";
    if (!strcmp(feature, "browser")) return "browser-use";
    if (!strcmp(feature, "modal")) return "modal";
    return NULL;
}

json_t *w32_tools_f39c80_p2_provider_capability(const json_t *req) {
    const char *name = json_get_str(req, "name", "");
    const json_t *capability = tt_jget(req, "capability");
    const json_t *provider = tt_jget(req, "provider");
    const char *provider_str = (provider && provider->type == JSON_STRING) ? provider->str_val : "";
    const json_t *known = tt_jget(req, "known");
    const json_t *providers = tt_jget(req, "providers");
    const json_t *caps = tt_jget(req, "caps");
    const json_t *features = tt_jget(req, "features");

    /* Body-level rejection happens before the worker thread. */
    if (capability && capability->type != JSON_NULL) {
        if (strcmp(name, "web"))
            return tt_err("capability selection is only supported for the web toolset");
        if (!tt_str_eq(capability, "search") && !tt_str_eq(capability, "extract")) {
            char *repr = tt_repr(capability);
            size_t need = strlen(repr) +
                          sizeof("Unknown capability:  (expected 'search' or 'extract')") + 1;
            char *detail = (char *)malloc(need);
            if (detail)
                snprintf(detail, need, "Unknown capability: %s (expected 'search' or 'extract')",
                         repr);
            json_t *out = tt_err(detail ? detail : "");
            free(detail);
            free(repr);
            return out;
        }
    }

    json_t *known_chk = w32_tools_f39c80_p1_require_known(json_string(name), known);
    if (!json_get_bool(known_chk, "ok", false)) return known_chk;
    json_free(known_chk);

    /* _provider_row(config) */
    const json_t *row = NULL;
    if (providers && providers->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(providers); i++) {
            const json_t *cand = json_get(providers, i);
            if (tt_str_eq(tt_jget(cand, "name"), provider_str)) { row = cand; break; }
        }
    }
    if (!row) {
        char *prepr = tt_repr(provider);
        char *nrepr = tt_repr_str(name);
        size_t need = strlen(prepr) + strlen(nrepr) + sizeof("Unknown provider  for toolset ") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "Unknown provider %s for toolset %s", prepr, nrepr);
        json_t *out = tt_err(detail ? detail : "");
        free(detail);
        free(nrepr);
        free(prepr);
        return out;
    }

    const json_t *backend_node = tt_jget(row, "web_backend");
    const char *backend = (backend_node && backend_node->type == JSON_STRING &&
                           backend_node->str_val[0]) ? backend_node->str_val : NULL;
    if (!backend) {
        char *prepr = tt_repr(provider);
        size_t need = strlen(prepr) + sizeof("Provider  has no web backend key") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "Provider %s has no web backend key", prepr);
        json_t *out = tt_err(detail ? detail : "");
        free(detail);
        free(prepr);
        return out;
    }

    const char *cap_str = (capability && capability->type == JSON_STRING) ? capability->str_val : "";
    const json_t *cap_list = tt_jget(caps, backend);
    bool supported = (cap_list && cap_list->type == JSON_ARRAY)
                         ? tt_in_strlist(cap_list, cap_str) : true;
    if (!supported) {
        size_t need = strlen(provider_str) + strlen(cap_str) + sizeof(" does not support ") + 1;
        char *detail = (char *)malloc(need);
        if (detail) snprintf(detail, need, "%s does not support %s", provider_str, cap_str);
        json_t *out = tt_err(detail ? detail : "");
        free(detail);
        return out;
    }

    /* The managed row's web_backend names the vendor serving it; writing that
     * would read as the user's OWN key. Its pin is "nous" (gateway route). */
    const char *pin = tt_truthy(tt_jget(row, "managed_nous_feature")) ? "nous" : backend;
    const char *feature = json_get_str(row, "managed_nous_feature", NULL);

    json_t *written = json_object();
    tt_set(written, "section", json_string("web"));
    {
        size_t need = strlen(cap_str) + sizeof("_backend") + 1;
        char *key = (char *)malloc(need);
        if (key) snprintf(key, need, "%s_backend", cap_str);
        tt_set(written, "key", json_string(key ? key : ""));
        free(key);
    }
    tt_set(written, "value", json_string(pin));

    json_t *response = json_object();
    tt_set(response, "ok", json_bool(true));
    tt_set(response, "name", json_string(name));
    tt_set(response, "provider", provider ? json_copy(provider) : json_null());
    if (capability && capability->type != JSON_NULL)
        tt_set(response, "capability", json_copy(capability));

    if (feature && feature[0]) {
        const json_t *account = tt_jget(features, "account");
        const char *category = tt_managed_category(feature);
        bool entitled = false;
        if (account && account->type == JSON_OBJECT &&
            tt_truthy(tt_jget(account, "logged_in"))) {
            if (category)
                entitled = tt_truthy(tt_jget(tt_jget(account, "entitled_for"), category));
            else
                entitled = tt_truthy(tt_jget(account, "tool_gateway_entitled"));
        }
        if (!entitled) {
            tt_set(response, "needs_nous_auth", json_bool(true));
            tt_set(response, "feature", json_string(feature));
        }
    }

    json_t *out = json_object();
    tt_set(out, "response", response);
    tt_set(out, "written", written);
    return out;
}
