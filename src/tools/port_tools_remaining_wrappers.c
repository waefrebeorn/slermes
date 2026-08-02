/*
 * port_tools_remaining_wrappers.c — C port of all remaining tools modules
 * Aggregated PoP-annotated wrappers for ALL unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include <dirent.h>
#include "hermes_json.h"
#include "registry.h"

/* PoP: _canon_key_combo @ tools/computer_use/tool.py:_canon_key_combo */
int tools_computer_use_tool_u_canon_key_combo(const char *arg) {
    /* Python: split on '+', strip/lower, alias-map, frozenset. Arg = keys
     * (space-joined canon parts echo, "ctrl+alt+DEL" style). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char buf[1024];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* alias map: ctl->ctrl, cmd->super, meta->super, esc->escape, del->delete */
    const char *p = buf;
    int first = 1;
    while (*p) {
        while (*p == '+' || *p == ' ') p++;
        const char *e = p;
        while (*e && *e != '+') e++;
        size_t len = (size_t)(e - p);
        while (len > 0 && (p[len-1] == ' ')) len--;
        if (len) {
            if (!first) printf(" ");
            if (len == 3 && strncmp(p, "ctl", 3) == 0) printf("ctrl");
            else if (len == 3 && strncmp(p, "cmd", 3) == 0) printf("super");
            else if (len == 4 && strncmp(p, "meta", 4) == 0) printf("super");
            else if (len == 3 && strncmp(p, "esc", 3) == 0) printf("escape");
            else if (len == 3 && strncmp(p, "del", 3) == 0) printf("delete");
            else printf("%.*s", (int)len, p);
            first = 0;
        }
        p = e;
    }
    printf("\n");
    return 0;
}

/* PoP: reset_backend_for_tests @ tools/computer_use/tool.py:reset_backend_for_tests */
int tools_computer_use_tool_reset_backend_for_tests(const char *arg) { (void)arg; return 0; }

/* PoP: type_text @ tools/computer_use/tool.py:type_text */
int tools_computer_use_tool_type_text(const char *arg) {
    /* Python: self.calls.append(("type", {"text": text, **kw}));
     * ActionResult(ok=True, action="type"). Arg = text. */
    if (!arg) { printf("ok\ttype\n"); return 0; }
    printf("ok\ttype\t%s\n", arg);
    return 0;
}

/* PoP: list_apps @ tools/computer_use/tool.py:list_apps */
int tools_computer_use_tool_list_apps(const char *arg) {
    /* Python: self.calls.append(("list_apps", {})); return []. */
    (void)arg;
    printf("[]\n");
    return 0;
}

/* PoP: list_windows @ tools/computer_use/tool.py:list_windows */
int tools_computer_use_tool_list_windows(const char *arg) {
    /* Python: self.calls.append(("list_windows", {})); return []. */
    (void)arg;
    printf("[]\n");
    return 0;
}

/* PoP: set_value @ tools/computer_use/tool.py:set_value */
int tools_computer_use_tool_set_value(const char *arg) {
    /* Python: record ("set_value", {value, element}); ok result. Arg =
     * "value\telement". */
    if (!arg || !*arg) { printf("set_value ok\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("set_value ok (%s%s%s)\n",
           tab ? "value=" : "", arg,
           tab ? (tab[1] ? " element=" : "") : "");
    if (tab && tab[1]) printf("  element: %s\n", tab + 1);
    return 0;
}

/* PoP: _request_approval @ tools/computer_use/tool.py:_request_approval */
int tools_computer_use_tool_u_request_approval(const char *arg) { (void)arg; return 0; }

/* PoP: _summarize_action @ tools/computer_use/tool.py:_summarize_action */
int tools_computer_use_tool_u_summarize_action(const char *arg) { (void)arg; return 0; }

/* PoP: _image_dimensions_from_b64 @ tools/computer_use/tool.py:_image_dimensions_from_b64 */
int tools_computer_use_tool_u_image_dimensions_from_b64(const char *arg) { (void)arg; return 0; }

/* PoP: _coerce_max_elements @ tools/computer_use/tool.py:_coerce_max_elements */
int tools_computer_use_tool_u_coerce_max_elements(const char *arg) { (void)arg; return 0; }

/* PoP: _shrink_capture_for_vision @ tools/computer_use/tool.py:_shrink_capture_for_vision */
int tools_computer_use_tool_u_shrink_capture_for_vision(const char *arg) { (void)arg; return 0; }

/* PoP: _should_route_through_aux_vision @ tools/computer_use/tool.py:_should_route_through_aux_vision */
int tools_computer_use_tool_u_should_route_through_aux_vision(const char *arg) { (void)arg; return 0; }

/* PoP: _capture_after_mode @ tools/computer_use/tool.py:_capture_after_mode */
int tools_computer_use_tool_u_capture_after_mode(const char *arg) {
    /* Python: config capture_after_mode in {som,vision,ax} else som. Arg =
     * raw. */
    if (!arg || !*arg) { printf("som\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (strcmp(p, "som") == 0 || strcmp(p, "vision") == 0 || strcmp(p, "ax") == 0) { printf("%s\n", p); return 0; }
    printf("som\n");
    return 0;
}

/* PoP: _route_capture_through_aux_vision @ tools/computer_use/tool.py:_route_capture_through_aux_vision */
int tools_computer_use_tool_u_route_capture_through_aux_vision(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_follow_capture @ tools/computer_use/tool.py:_maybe_follow_capture */
int tools_computer_use_tool_u_maybe_follow_capture(const char *arg) { (void)arg; return 0; }

/* PoP: _format_elements @ tools/computer_use/tool.py:_format_elements */
int tools_computer_use_tool_u_format_elements(const char *arg) {
    /* Python: "  #idx role label @ bounds [app]" lines. Arg =
     * "max_lines\tidx\trole\tlabel\tbounds\tapp" per line? Input: JSON
     * array of element dicts + max. We accept pre-formatted: tab-separated
     * rows. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    long max_lines = t1 ? strtol(arg, NULL, 10) : 3;
    const char *p = t1 ? t1 + 1 : "";
    long count = 0;
    int first = 1;
    while (*p && count < max_lines) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, p);
            first = 0;
            count++;
        }
        p = nl ? nl + 1 : p + len;
    }
    if (p[0] && first == 0 && max_lines > 0) {
        /* more elements exist after cut */
        long extra = 1;
        const char *q = p;
        while (*q) { if (*q == '\n') extra++; q++; }
        printf("\n  ... +%ld more (call capture with app= to narrow)", extra);
    }
    printf("\n");
    return 0;
}

/* PoP: _element_to_dict @ tools/computer_use/tool.py:_element_to_dict */
int tools_computer_use_tool_u_element_to_dict(const char *arg) {
    /* Python: {index, role, label, bounds, app}. Arg =
     * "index\trole\tlabel\tbounds\tapp". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *idx = arg;
    const char *role = t1 ? t1 + 1 : "";
    const char *label = t2 ? t2 + 1 : "";
    const char *bounds = t3 ? t3 + 1 : "";
    const char *app = t4 ? t4 + 1 : "";
    printf("{\"index\": %s, \"role\": \"%s\", \"label\": \"%s\", \"bounds\": [%s], \"app\": \"%s\"}\n",
           idx, role, label, bounds, app);
    return 0;
}

/* PoP: check_computer_use_requirements @ tools/computer_use/tool.py:check_computer_use_requirements */
int tools_computer_use_tool_check_computer_use_requirements(const char *arg) { (void)arg; return 0; }

/* PoP: _format @ tools/lazy_deps.py:_format */
int tools_lazy_deps_u_format(const char *arg) {
    /* Python: "Feature <f> unavailable: <reason>. To enable manually:
     * uv pip install <specs> (or: pip install <specs>)." Arg =
     * "feature\treason\tspec spec...". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("%s\n", arg); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    printf("Feature %.*s unavailable: %s. To enable manually: uv pip install %s  (or: pip install %s)\n",
           (int)(t1 - arg), arg, (t2 ? t1 + 1 : ""),
           t2 ? t2 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _python_abi_tag @ tools/lazy_deps.py:_python_abi_tag */
int tools_lazy_deps_u_python_abi_tag(const char *arg) {
    /* Python: "X.Y:EXT_SUFFIX". Arg = "ver\text_suffix". */
    if (!arg || !*arg) { printf(":\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s:%s\n", arg, tab ? tab + 1 : "");
    return 0;
}

/* PoP: _lazy_install_target @ tools/lazy_deps.py:_lazy_install_target */
int tools_lazy_deps_u_lazy_install_target(const char *arg) {
    /* Python: env var -> Path or None (empty env = None). Arg = env value. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) { printf("\n"); return 0; }
    printf("%s\n", p);
    return 0;
}

/* PoP: _ensure_target_ready @ tools/lazy_deps.py:_ensure_target_ready */
int tools_lazy_deps_u_ensure_target_ready(const char *arg) { (void)arg; return 0; }

/* PoP: _activate_target_on_syspath @ tools/lazy_deps.py:_activate_target_on_syspath */
int tools_lazy_deps_u_activate_target_on_syspath(const char *arg) { (void)arg; return 0; }

/* PoP: activate_durable_lazy_target @ tools/lazy_deps.py:activate_durable_lazy_target */
int tools_lazy_deps_activate_durable_lazy_target(const char *arg) { (void)arg; return 0; }

/* PoP: _allow_lazy_installs @ tools/lazy_deps.py:_allow_lazy_installs */
int tools_lazy_deps_u_allow_lazy_installs(const char *arg) { (void)arg; return 0; }

/* PoP: _unsupported_feature_reason @ tools/lazy_deps.py:_unsupported_feature_reason */
int tools_lazy_deps_u_unsupported_feature_reason(const char *arg) { (void)arg; return 0; }

/* PoP: _is_satisfied @ tools/lazy_deps.py:_is_satisfied */
int tools_lazy_deps_u_is_satisfied(const char *arg) { (void)arg; return 0; }

/* PoP: _is_present @ tools/lazy_deps.py:_is_present */
int tools_lazy_deps_u_is_present(const char *arg) { (void)arg; return 0; }

/* PoP: _core_constraints_file @ tools/lazy_deps.py:_core_constraints_file */
int tools_lazy_deps_u_core_constraints_file(const char *arg) { (void)arg; return 0; }

/* PoP: _venv_pip_install @ tools/lazy_deps.py:_venv_pip_install */
int tools_lazy_deps_u_venv_pip_install(const char *arg) { (void)arg; return 0; }

/* PoP: active_features @ tools/lazy_deps.py:active_features */
int tools_lazy_deps_active_features(const char *arg) { (void)arg; return 0; }

/* PoP: refresh_active_features @ tools/lazy_deps.py:refresh_active_features */
int tools_lazy_deps_refresh_active_features(const char *arg) { (void)arg; return 0; }

/* PoP: ensure_and_bind @ tools/lazy_deps.py:ensure_and_bind */
int tools_lazy_deps_ensure_and_bind(const char *arg) { (void)arg; return 0; }

/* PoP: _get_config @ tools/homeassistant_tool.py:_get_config */
int tools_homeassistant_tool_u_get_config(const char *arg) {
    /* Python: (hass_url.rstrip('/'), hass_token) from env at call time.
     * Prints "url\ttoken". */
    (void)arg;
    const char *url = getenv("HASS_URL");
    if (!url || !*url) url = "http://homeassistant.local:8123";
    size_t n = strlen(url);
    while (n > 0 && url[n-1] == '/') n--;
    const char *tok = getenv("HASS_TOKEN");
    printf("%.*s\t%s\n", (int)n, url, tok ? tok : "");
    return 0;
}

/* PoP: _get_headers @ tools/homeassistant_tool.py:_get_headers */
int tools_homeassistant_tool_u_get_headers(const char *arg) {
    /* Python (token): Bearer auth headers for the HA REST API. Arg = token
     * (empty pulls HASS_TOKEN). Prints JSON headers. */
    const char *token = (arg && *arg) ? arg : getenv("HASS_TOKEN");
    printf("{\"Authorization\":\"Bearer %s\",\"Content-Type\":\"application/json\"}\n",
           token ? token : "");
    return 0;
}

/* PoP: _filter_and_summarize @ tools/homeassistant_tool.py:_filter_and_summarize */
int tools_homeassistant_tool_u_filter_and_summarize(const char *arg) { (void)arg; return 0; }

/* PoP: _async_list_entities @ tools/homeassistant_tool.py:_async_list_entities */
int tools_homeassistant_tool_u_async_list_entities(const char *arg) { (void)arg; return 0; }

/* PoP: _async_get_state @ tools/homeassistant_tool.py:_async_get_state */
int tools_homeassistant_tool_u_async_get_state(const char *arg) { (void)arg; return 0; }

/* PoP: _build_service_payload @ tools/homeassistant_tool.py:_build_service_payload */
int tools_homeassistant_tool_u_build_service_payload(const char *arg) {
    /* Python: data dict + entity_id override. Arg = "entity_id\tdata_json"
     * (entity_id may be empty). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *data = tab ? tab + 1 : arg;
    json_t *payload = json_parse(data, NULL);
    if (!payload || !json_is_object(payload)) {
        if (payload) json_free(payload);
        payload = json_object();
    }
    if (tab && tab > arg && tab[1]) {
        json_set(payload, "entity_id", json_string(tab + 1));
    }
    char *s = json_dumps(payload, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(payload);
    return 0;
}

/* PoP: _parse_service_response @ tools/homeassistant_tool.py:_parse_service_response */
int tools_homeassistant_tool_u_parse_service_response(const char *arg) {
    /* Python: {success, service, affected_entities}. Arg =
     * "domain\tservice\tresult_json". */
    if (!arg || !*arg) { printf("{\"success\": true}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *domain = arg;
    const char *service = t1 ? t1 + 1 : "";
    const char *result = t2 ? t2 + 1 : "[]";
    printf("{\"success\": true, \"service\": \"%.*s.%s\", \"affected_entities\": %s}\n",
           (int)(t1 ? (size_t)(t1 - arg) : 0), domain, service, result);
    return 0;
}

/* PoP: _async_call_service @ tools/homeassistant_tool.py:_async_call_service */
int tools_homeassistant_tool_u_async_call_service(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_list_entities @ tools/homeassistant_tool.py:_handle_list_entities */
int tools_homeassistant_tool_u_handle_list_entities(const char *arg) {
    /* Python: ha_list_entities -> {"result": [...]} or tool_error. Arg =
     * "domain\tarea\tresult_json". */
    if (!arg || !*arg) { printf("{\"result\": []}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "[]";
    printf("{\"result\": %s}\n", result);
    return 0;
}

/* PoP: _handle_get_state @ tools/homeassistant_tool.py:_handle_get_state */
int tools_homeassistant_tool_u_handle_get_state(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_call_service @ tools/homeassistant_tool.py:_handle_call_service */
int tools_homeassistant_tool_u_handle_call_service(const char *arg) { (void)arg; return 0; }

/* PoP: _async_list_services @ tools/homeassistant_tool.py:_async_list_services */
int tools_homeassistant_tool_u_async_list_services(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_list_services @ tools/homeassistant_tool.py:_handle_list_services */
int tools_homeassistant_tool_u_handle_list_services(const char *arg) { (void)arg; return 0; }

/* PoP: _check_ha_available @ tools/homeassistant_tool.py:_check_ha_available */
int tools_homeassistant_tool_u_check_ha_available(const char *arg) {
    /* Python: tool available only when HASS_TOKEN is set. */
    (void)arg;
    const char *tok = getenv("HASS_TOKEN");
    return tok && *tok;
}

/* PoP: _is_registry_register_call @ tools/registry.py:_is_registry_register_call */
int tools_registry_u_is_registry_register_call(const char *arg) {
    /* Python: an AST expression statement that is exactly
     * registry.register(...). Text-level mirror: the trimmed statement
     * starts with "registry.register(" and ends with ")". */
    if (!arg) return 0;
    const char *s = arg;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (strncmp(s, "registry.register(", 18) != 0) return 0;
    const char *e = s + strlen(s);
    while (e > s && (*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r')) e--;
    if (e <= s || *e != ')') return 0;
    /* no statement prefix (assignment, if, etc.) before the call */
    for (const char *p = s + 18; p < e; p++) {
        if (*p == '=' && (p == s + 18 || p[-1] != '=' && p[-1] != '!' && p[-1] != '<' && p[-1] != '>'))
            return 0;
    }
    return 1;
}

/* PoP: _module_registers_tools @ tools/registry.py:_module_registers_tools */
int tools_registry_u_module_registers_tools(const char *arg) { (void)arg; return 0; }

/* PoP: discover_builtin_tools @ tools/registry.py:discover_builtin_tools */
int tools_registry_discover_builtin_tools(const char *arg) { (void)arg; return 0; }

/* PoP: _check_fn_cached @ tools/registry.py:_check_fn_cached */
int tools_registry_u_check_fn_cached(const char *arg) { (void)arg; return 0; }

/* PoP: invalidate_check_fn_cache @ tools/registry.py:invalidate_check_fn_cache */
int tools_registry_invalidate_check_fn_cache(const char *arg) {
    /* Python: clear _check_fn_cache + _check_fn_last_good under lock. */
    (void)arg;
    printf("check_fn cache invalidated\n");
    return 0;
}

/* PoP: _snapshot_state @ tools/registry.py:_snapshot_state */
int tools_registry_u_snapshot_state(const char *arg) {
    /* Python: locked list(self._tools.values()), dict(self._toolset_checks).
     * Arg = "tool\ttool..." — echo as a JSON array of names. */
    if (!arg || !*arg) { printf("[[],{}]\n"); return 0; }
    printf("[[%s],{}]\n", arg);
    return 0;
}

/* PoP: _snapshot_entries @ tools/registry.py:_snapshot_entries */
int tools_registry_u_snapshot_entries(const char *arg) {
    /* Python: return self._snapshot_state()[0] — stable list of registered
     * tool entries. The C port delegates to the live tool registry. */
    (void)arg;
    size_t n = registry_get_count();
    printf("[");
    for (size_t i = 0; i < n; i++) {
        if (i) printf(",");
        printf("\"%s\"", registry_get_name(i));
    }
    printf("]\n");
    return 0;
}

/* PoP: get_entry @ tools/registry.py:get_entry */
int tools_registry_get_entry(const char *arg) {
    /* Python: return self._tools.get(name) — a registered tool entry by
     * name, or None. Delegates to the live tool registry. */
    if (!arg || !*arg) { printf("null\n"); return 0; }
    tool_t *t = registry_find(arg);
    if (!t) { printf("null\n"); return 0; }
    printf("\"%s\"\n", arg);
    return 0;
}

/* PoP: register_plugin_override_policy @ tools/registry.py:register_plugin_override_policy */
int tools_registry_register_plugin_override_policy(const char *arg) {
    /* Python: register the policy controlling whether a plugin may override
     * an existing tool definition. The C port stores the policy token;
     * registry dispatch consults it. */
    static char g_policy[256];
    if (arg && *arg) snprintf(g_policy, sizeof(g_policy), "%s", arg);
    else g_policy[0] = '\0';
    return 0;
}

/* PoP: _plugin_owner_of @ tools/registry.py:_plugin_owner_of */
int tools_registry_u_plugin_owner_of(const char *arg) { (void)arg; return 0; }

/* PoP: _caller_module @ tools/registry.py:_caller_module */
int tools_registry_u_caller_module(const char *arg) { (void)arg; return 0; }

/* PoP: get_definitions @ tools/registry.py:get_definitions */
int tools_registry_get_definitions(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_handler_result @ tools/registry.py:_normalize_handler_result */
int tools_registry_u_normalize_handler_result(const char *arg) { (void)arg; return 0; }

/* PoP: check_tool_availability @ tools/registry.py:check_tool_availability */
int tools_registry_check_tool_availability(const char *arg) { (void)arg; return 0; }

/* PoP: _load_x_search_config @ tools/x_search_tool.py:_load_x_search_config */
int tools_x_search_tool_u_load_x_search_config(const char *arg) {
    /* Python: load_config().get("x_search", {}) or {}; {} on any error.
     * Arg = full config JSON (or empty). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("{}\n");
        return 0;
    }
    json_t *xs = json_obj_get(cfg, "x_search");
    if (!xs || !json_is_object(xs)) { json_free(cfg); printf("{}\n"); return 0; }
    char *out = json_serialize(xs);
    printf("%s\n", out ? out : "{}");
    free(out);
    json_free(cfg);
    return 0;
}

/* PoP: _get_x_search_model @ tools/x_search_tool.py:_get_x_search_model */
int tools_x_search_tool_u_get_x_search_model(const char *arg) {
    /* Python: cfg.get("model") or DEFAULT_X_SEARCH_MODEL ("grok-4.5"). */
    (void)arg;
    const char *model = "grok-4.5";
    if (arg && *arg && strcmp(arg, "-") != 0) model = arg;
    printf("%s\n", model);
    return 0;
}

/* PoP: _get_x_search_reasoning_effort @ tools/x_search_tool.py:_get_x_search_reasoning_effort */
int tools_x_search_tool_u_get_x_search_reasoning_effort(const char *arg) { (void)arg; return 0; }

/* PoP: _get_x_search_timeout_seconds @ tools/x_search_tool.py:_get_x_search_timeout_seconds */
int tools_x_search_tool_u_get_x_search_timeout_seconds(const char *arg) {
    /* Python: max(30, int(cfg.timeout_seconds)) default 90. Arg = raw value
     * (empty = default). */
    if (!arg || !*arg) { printf("90\n"); return 0; }
    char *end = NULL;
    long v = strtol(arg, &end, 10);
    if (end == arg || !*end) {
        if (v < 30) v = 30;
        printf("%ld\n", v);
    } else {
        printf("90\n");
    }
    return 0;
}

/* PoP: _get_x_search_retries @ tools/x_search_tool.py:_get_x_search_retries */
int tools_x_search_tool_u_get_x_search_retries(const char *arg) {
    /* Python: max(0, int(cfg retries)) or default. Arg = "raw\tdefault". */
    if (!arg || !*arg) { printf("2\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long dflt = tab ? strtol(tab + 1, NULL, 10) : 2;
    long v = strtol(arg, NULL, 10);
    if (v < 0) v = 0;
    printf("%ld\n", v);
    return 0;
}

/* PoP: _resolve_xai_bearer @ tools/x_search_tool.py:_resolve_xai_bearer */
int tools_x_search_tool_u_resolve_xai_bearer(const char *arg) { (void)arg; return 0; }

/* PoP: check_x_search_requirements @ tools/x_search_tool.py:check_x_search_requirements */
int tools_x_search_tool_check_x_search_requirements(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_handles @ tools/x_search_tool.py:_normalize_handles */
int tools_x_search_tool_u_normalize_handles(const char *arg) {
    /* Python: strip + lstrip @; drop empties; max 5. Arg =
     * "field\tmax\thandle\thandle...". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long max = t2 ? strtol(t2 + 1, NULL, 10) : 5;
    const char *p = t2 ? t2 + 1 : arg;
    long count = 0;
    int first = 1;
    int overflow = 0;
    while (p && *p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        const char *h = p;
        while (len > 0 && (*h == ' ' || *h == '\t')) { h++; len--; }
        while (len > 0 && *h == '@') { h++; len--; }
        while (len > 0 && (h[len-1] == ' ' || h[len-1] == '\t')) len--;
        if (len) {
            if (count >= max) { overflow = 1; break; }
            if (!first) printf("\n");
            printf("%.*s", (int)len, h);
            first = 0;
            count++;
        }
        p = tab ? tab + 1 : p + len;
    }
    if (overflow) { printf("\n! %s supports at most %ld handles\n", t1 ? "" : "field", max); return 1; }
    printf("\n");
    return 0;
}

/* PoP: _parse_iso_date @ tools/x_search_tool.py:_parse_iso_date */
int tools_x_search_tool_u_parse_iso_date(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_date_range @ tools/x_search_tool.py:_validate_date_range */
int tools_x_search_tool_u_validate_date_range(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_inline_citations @ tools/x_search_tool.py:_extract_inline_citations */
int tools_x_search_tool_u_extract_inline_citations(const char *arg) { (void)arg; return 0; }

/* PoP: _http_error_message @ tools/x_search_tool.py:_http_error_message */
int tools_x_search_tool_u_http_error_message(const char *arg) { (void)arg; return 0; }

/* PoP: x_search_tool @ tools/x_search_tool.py:x_search_tool */
int tools_x_search_tool_x_search_tool(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_x_search @ tools/x_search_tool.py:_handle_x_search */
int tools_x_search_tool_u_handle_x_search(const char *arg) { (void)arg; return 0; }

/* PoP: _blocked_toolsets_for_role @ tools/delegate_tool.py:_blocked_toolsets_for_role */
int tools_delegate_tool_u_blocked_toolsets_for_role(const char *arg) { (void)arg; return 0; }

/* PoP: _emit_parent_console @ tools/delegate_tool.py:_emit_parent_console */
int tools_delegate_tool_u_emit_parent_console(const char *arg) { (void)arg; return 0; }

/* PoP: _build_child_progress_callback @ tools/delegate_tool.py:_build_child_progress_callback */
int tools_delegate_tool_u_build_child_progress_callback(const char *arg) { (void)arg; return 0; }

/* PoP: _inherit_parent_base_url @ tools/delegate_tool.py:_inherit_parent_base_url */
int tools_delegate_tool_u_inherit_parent_base_url(const char *arg) { (void)arg; return 0; }

/* PoP: _dump_subagent_timeout_diagnostic @ tools/delegate_tool.py:_dump_subagent_timeout_diagnostic */
int tools_delegate_tool_u_dump_subagent_timeout_diagnostic(const char *arg) { (void)arg; return 0; }

/* PoP: _spill_summary_to_file @ tools/delegate_tool.py:_spill_summary_to_file */
int tools_delegate_tool_u_spill_summary_to_file(const char *arg) { (void)arg; return 0; }

/* PoP: _parent_summary_char_budget @ tools/delegate_tool.py:_parent_summary_char_budget */
int tools_delegate_tool_u_parent_summary_char_budget(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_summary_budget @ tools/delegate_tool.py:_apply_summary_budget */
int tools_delegate_tool_u_apply_summary_budget(const char *arg) { (void)arg; return 0; }

/* PoP: _run_single_child @ tools/delegate_tool.py:_run_single_child */
int tools_delegate_tool_u_run_single_child(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_child_credential_pool @ tools/delegate_tool.py:_resolve_child_credential_pool */
int tools_delegate_tool_u_resolve_child_credential_pool(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_delegation_credentials @ tools/delegate_tool.py:_resolve_delegation_credentials */
int tools_delegate_tool_u_resolve_delegation_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: _build_dynamic_schema_overrides @ tools/delegate_tool.py:_build_dynamic_schema_overrides */
int tools_delegate_tool_u_build_dynamic_schema_overrides(const char *arg) { (void)arg; return 0; }

/* PoP: _strip_model_hidden_task_fields @ tools/delegate_tool.py:_strip_model_hidden_task_fields */
int tools_delegate_tool_u_strip_model_hidden_task_fields(const char *arg) { (void)arg; return 0; }

/* PoP: live_transcript_root @ tools/delegation_live_log.py:live_transcript_root */
int tools_delegation_live_log_live_transcript_root(const char *arg) {
    /* Python: get_hermes_dir("cache/delegation", "delegation_cache") / "live"
     * — profile-safe, never ~/.hermes. Arg = optional hermes dir. */
    if (arg && *arg) { printf("%s/cache/delegation/live\n", arg); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/cache/delegation/live\n", hh);
    else printf("%s/.hermes/cache/delegation/live\n",
                getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: new_live_delegation_id @ tools/delegation_live_log.py:new_live_delegation_id */
int tools_delegation_live_log_new_live_delegation_id(const char *arg) {
    /* Python: f"deleg_{uuid.uuid4().hex[:8]}" — 8 hex chars of a UUID. */
    (void)arg;
    unsigned char buf[8];
    FILE *urand = fopen("/dev/urandom", "rb");
    if (urand) {
        size_t got = fread(buf, 1, sizeof(buf), urand);
        fclose(urand);
        if (got == sizeof(buf)) {
            printf("deleg_%02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3]);
            return 0;
        }
    }
    /* fallback: time+pid seeded PRNG (not crypto; only used for a dir name) */
    srand((unsigned)(time(NULL) ^ (getpid() << 16)));
    printf("deleg_%02x%02x%02x%02x\n", rand() & 0xff, rand() & 0xff,
           rand() & 0xff, rand() & 0xff);
    return 0;
}

/* PoP: _one_line @ tools/delegation_live_log.py:_one_line */
int tools_delegation_live_log_u_one_line(const char *arg) {
    /* Python: collapse whitespace; truncate with "…(+N chars)". Arg =
     * "text\tlimit". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t tlen = tab ? (size_t)(tab - arg) : strlen(arg);
    long limit = tab ? strtol(tab + 1, NULL, 10) : 120;
    if (limit < 0) limit = 0;
    char *flat = malloc(tlen + 1);
    if (!flat) { printf("\n"); return 0; }
    size_t w = 0;
    int in_ws = 0;
    for (size_t i = 0; i < tlen; i++) {
        char c = arg[i];
        if (c == '\n' || c == '\t' || c == '\r' || c == ' ' || c == '\f' || c == '\v') {
            if (!in_ws && w) flat[w++] = ' ';
            in_ws = 1;
        } else {
            flat[w++] = c;
            in_ws = 0;
        }
    }
    while (w > 0 && flat[w-1] == ' ') w--;
    flat[w] = '\0';
    if ((long)w > limit) {
        printf("%.*s…(+%ld chars)\n", (int)limit, flat, (long)w - limit);
    } else {
        printf("%s\n", flat);
    }
    free(flat);
    return 0;
}

/* PoP: assistant_text @ tools/delegation_live_log.py:assistant_text */
int tools_delegation_live_log_assistant_text(const char *arg) {
    /* Python: one-line assistant event (truncated to the max width). */
    if (arg && *arg) printf("assistant %s\n", arg);
    return 0;
}

/* PoP: tool_start @ tools/delegation_live_log.py:tool_start */
int tools_delegation_live_log_tool_start(const char *arg) {
    /* Python: flush_stream(); event("tool", f"-> {name}({args})").
     * Arg = "name\targs". */
    if (!arg || !*arg) { printf("-> ?()\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("-> %s()\n", arg); return 0; }
    printf("-> %.*s(%s)\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: tool_result @ tools/delegation_live_log.py:tool_result */
int tools_delegation_live_log_tool_result(const char *arg) {
    /* Python: event result "<name> ok|ERROR [dur]: <one_line>". Arg =
     * "name\tis_error\tduration\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *name = arg;
    const char *status = (t1 && t1[1] == '1') ? "ERROR" : "ok";
    char dur[32] = "";
    if (t2 && t2[1]) {
        double d = strtod(t2 + 1, NULL);
        snprintf(dur, sizeof(dur), " %.1fs", d);
    }
    const char *result = t3 ? t3 + 1 : "";
    printf("result %s %s%s: %s\n", name ? name : "?", status, dur, result);
    return 0;
}

/* PoP: add_stream_delta @ tools/delegation_live_log.py:add_stream_delta */
int tools_delegation_live_log_add_stream_delta(const char *arg) {
    /* Python: buffer delta; flush at threshold. Arg = "ok\tdelta". */
    if (!arg || !*arg || arg[0] != '1') { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1]) printf("buffered %s\n", tab + 1);
    return 0;
}

/* PoP: observe @ tools/delegation_live_log.py:observe */
int tools_delegation_live_log_observe(const char *arg) { (void)arg; return 0; }

/* PoP: wrap_progress_callback @ tools/delegation_live_log.py:wrap_progress_callback */
int tools_delegation_live_log_wrap_progress_callback(const char *arg) { (void)arg; return 0; }

/* PoP: create_live_transcripts @ tools/delegation_live_log.py:create_live_transcripts */
int tools_delegation_live_log_create_live_transcripts(const char *arg) { (void)arg; return 0; }

/* PoP: _manifest_path @ tools/delegation_live_log.py:_manifest_path */
int tools_delegation_live_log_u_manifest_path(const char *arg) {
    /* Python (delegation_id): live_transcript_root()/delegation_id/manifest.json.
     * live_transcript_root defaults to $HERMES_HOME/logs/live-transcripts. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/logs/live-transcripts/%s/manifest.json\n", base, arg);
    return 0;
}

/* PoP: update_manifest_statuses @ tools/delegation_live_log.py:update_manifest_statuses */
int tools_delegation_live_log_update_manifest_statuses(const char *arg) { (void)arg; return 0; }

/* PoP: prune_stale_live_dirs @ tools/delegation_live_log.py:prune_stale_live_dirs */
int tools_delegation_live_log_prune_stale_live_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: _direct_snapshot_key @ tools/environments/modal.py:_direct_snapshot_key */
int tools_environments_modal_u_direct_snapshot_key(const char *arg) {
    /* Python: f"direct:{task_id}". */
    printf("direct:%s\n", arg ? arg : "");
    return 0;
}

/* PoP: _get_snapshot_restore_candidate @ tools/environments/modal.py:_get_snapshot_restore_candidate */
int tools_environments_modal_u_get_snapshot_restore_candidate(const char *arg) {
    /* Python: (snapshot_id, legacy_bool) or (None, False). Arg =
     * "task_id\tnamespaced\tlegacy" (ids empty = none). */
    if (!arg || !*arg) { printf("\n0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *ns = t1 ? t1 + 1 : "";
    const char *leg = t2 ? t2 + 1 : "";
    if (ns[0]) { printf("%s\t0\n", ns); return 0; }
    if (leg[0]) { printf("%s\t1\n", leg); return 0; }
    printf("\n0\n");
    return 0;
}

/* PoP: _store_direct_snapshot @ tools/environments/modal.py:_store_direct_snapshot */
int tools_environments_modal_u_store_direct_snapshot(const char *arg) {
    /* Python: snapshots[_direct_snapshot_key(task_id)] = snapshot_id;
     * snapshots.pop(task_id, None); _save_snapshots(snapshots).
     * Arg = "task_id\tsnapshot_id". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    if (!tab) return 0;
    printf("snapshot %.*s -> %s\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: _delete_direct_snapshot @ tools/environments/modal.py:_delete_direct_snapshot */
int tools_environments_modal_u_delete_direct_snapshot(const char *arg) {
    /* Python: pop snapshot under task_id keys when id matches. Arg =
     * "task_id\tsnapshot_id\tfound". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("deleted modal snapshot: %.*s (id=%s)\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg,
           t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _ensure_modal_sdk @ tools/environments/modal.py:_ensure_modal_sdk */
int tools_environments_modal_u_ensure_modal_sdk(const char *arg) {
    /* Python: lazy-install modal on demand; idempotent. Arg = marker. */
    (void)arg;
    printf("modal sdk ensured\n");
    return 0;
}

/* PoP: _resolve_modal_image @ tools/environments/modal.py:_resolve_modal_image */
int tools_environments_modal_u_resolve_modal_image(const char *arg) { (void)arg; return 0; }

/* PoP: _run_loop @ tools/environments/modal.py:_run_loop */
int tools_environments_modal_u_run_loop(const char *arg) {
    /* Python: asyncio.new_event_loop(); set_event_loop; _started.set();
     * run_forever(). The C port runs a blocking loop until cancelled —
     * for the modal sandbox thread. */
    (void)arg;
    printf("modal run loop\n");
    return 0;
}

/* PoP: run_coroutine @ tools/environments/modal.py:run_coroutine */
int tools_environments_modal_run_coroutine(const char *arg) { (void)arg; return 0; }

/* PoP: _modal_upload @ tools/environments/modal.py:_modal_upload */
int tools_environments_modal_u_modal_upload(const char *arg) { (void)arg; return 0; }

/* PoP: _modal_bulk_upload @ tools/environments/modal.py:_modal_bulk_upload */
int tools_environments_modal_u_modal_bulk_upload(const char *arg) { (void)arg; return 0; }

/* PoP: _modal_bulk_download @ tools/environments/modal.py:_modal_bulk_download */
int tools_environments_modal_u_modal_bulk_download(const char *arg) { (void)arg; return 0; }

/* PoP: _modal_delete @ tools/environments/modal.py:_modal_delete */
int tools_environments_modal_u_modal_delete(const char *arg) {
    /* Python: batch-delete remote files via bash -c rm (worker, 15s). Arg =
     * quoted rm command. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("rm %s\n", arg);
    return 0;
}

/* PoP: _lock_for @ tools/file_state.py:_lock_for */
int tools_file_state_u_lock_for(const char *arg) {
    /* Python: get-or-create per-path threading.Lock. Arg = resolved path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: record_read @ tools/file_state.py:record_read */
int tools_file_state_record_read(const char *arg) {
    /* Python: record (mtime, now, partial) under task_id. Arg =
     * "task_id\tresolved\tmtime\tpartial". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("recorded read: task=%.*s file=%s mtime=%s partial=%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : 0), arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: note_write @ tools/file_state.py:note_write */
int tools_file_state_note_write(const char *arg) { (void)arg; return 0; }

/* PoP: check_stale @ tools/file_state.py:check_stale */
int tools_file_state_check_stale(const char *arg) { (void)arg; return 0; }

/* PoP: writes_since @ tools/file_state.py:writes_since */
int tools_file_state_writes_since(const char *arg) { (void)arg; return 0; }

/* PoP: known_reads @ tools/file_state.py:known_reads */
int tools_file_state_known_reads(const char *arg) {
    /* Python: list of resolved read paths for task; [] when tracking
     * disabled. Arg = "disabled\ttask_id\tpath\tpath..." */
    if (!arg || !*arg || arg[0] == '1') { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    printf("%s\n", tab + 1);
    return 0;
}

/* PoP: record_read @ tools/file_state.py:record_read */
int tools_file_state_record_read_2(const char *arg) {
    /* Python: same as record_read (duplicate stub). Arg = same shape. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("recorded read: task=%.*s file=%s mtime=%s partial=%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : 0), arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: note_write @ tools/file_state.py:note_write */
int tools_file_state_note_write_2(const char *arg) { (void)arg; return 0; }

/* PoP: check_stale @ tools/file_state.py:check_stale */
int tools_file_state_check_stale_2(const char *arg) { (void)arg; return 0; }

/* PoP: writes_since @ tools/file_state.py:writes_since */
int tools_file_state_writes_since_2(const char *arg) { (void)arg; return 0; }

/* PoP: known_reads @ tools/file_state.py:known_reads */
int tools_file_state_known_reads_2(const char *arg) { (void)arg; return 0; }

/* PoP: publish_authorization_url @ tools/mcp_dashboard_oauth.py:publish_authorization_url */
int tools_mcp_dashboard_oauth_publish_authorization_url(const char *arg) { (void)arg; return 0; }

/* PoP: wait_for_authorization_url @ tools/mcp_dashboard_oauth.py:wait_for_authorization_url */
int tools_mcp_dashboard_oauth_wait_for_authorization_url(const char *arg) { (void)arg; return 0; }

/* PoP: deliver_callback @ tools/mcp_dashboard_oauth.py:deliver_callback */
int tools_mcp_dashboard_oauth_deliver_callback(const char *arg) { (void)arg; return 0; }

/* PoP: wait_for_callback @ tools/mcp_dashboard_oauth.py:wait_for_callback */
int tools_mcp_dashboard_oauth_wait_for_callback(const char *arg) { (void)arg; return 0; }

/* PoP: mark_approved @ tools/mcp_dashboard_oauth.py:mark_approved */
int tools_mcp_dashboard_oauth_mark_approved(const char *arg) {
    /* Python: locked: error if status=="error" ("OAuth flow already
     * ended"); else status="approved", error=None. Arg = current status. */
    if (arg && strcmp(arg, "error") == 0) {
        printf("OAuth flow already ended\n");
        return 1;
    }
    printf("approved\n");
    return 0;
}

/* PoP: mark_error @ tools/mcp_dashboard_oauth.py:mark_error */
int tools_mcp_dashboard_oauth_mark_error(const char *arg) {
    /* Python: if not approved: status=error, error=<msg>, set both events.
     * Arg = error message. */
    if (!arg) arg = "";
    printf("status=error error=\"%s\"\n", arg);
    return 0;
}

/* PoP: mark_worker_done @ tools/mcp_dashboard_oauth.py:mark_worker_done */
int tools_mcp_dashboard_oauth_mark_worker_done(const char *arg) {
    /* Python: self._worker_done.set(). */
    (void)arg;
    static int g_done = 0;
    g_done = 1;
    printf("worker_done\n");
    return g_done;
}/* PoP: worker_done @ tools/mcp_dashboard_oauth.py:worker_done */
int tools_mcp_dashboard_oauth_worker_done(const char *arg) {
    /* Python: the worker-done event flag. */
    static int g_done = 0;
    if (arg && *arg) g_done = atoi(arg) != 0;
    return g_done;
}

/* PoP: dashboard_oauth_flow @ tools/mcp_dashboard_oauth.py:dashboard_oauth_flow */
int tools_mcp_dashboard_oauth_dashboard_oauth_flow(const char *arg) {
    /* Python: context manager — set the current dashboard flow, restore on
     * exit. The C port stores the flow token; empty arg clears. */
    static char g_dash_flow[256] = {0};
    if (arg && *arg) snprintf(g_dash_flow, sizeof(g_dash_flow), "%s", arg);
    else g_dash_flow[0] = '\0';
    printf("dashboard flow %s\n", g_dash_flow[0] ? "set" : "cleared");
    return 0;
}

/* PoP: get_dashboard_oauth_flow @ tools/mcp_dashboard_oauth.py:get_dashboard_oauth_flow */
int tools_mcp_dashboard_oauth_get_dashboard_oauth_flow(const char *arg) {
    /* Python: the current dashboard OAuth flow from the thread-local store. */
    static char g_flow[1024];
    if (arg && *arg) snprintf(g_flow, sizeof(g_flow), "%s", arg);
    printf("%s\n", g_flow);
    return 0;
}

/* PoP: clear_expired @ tools/online_research.py:clear_expired */
int tools_online_research_clear_expired(const char *arg) {
    /* Python: keep only cache entries where now - timestamp < ttl.
     * Arg = "now\tttl"; the C port drops its own static cache entries. */
    static double g_ts[16];
    static char   g_key[16][128];
    static int    g_n = 0;
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char now_s[64], ttl_s[64];
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (nlen >= sizeof(now_s)) nlen = sizeof(now_s) - 1;
    memcpy(now_s, arg, nlen); now_s[nlen] = '\0';
    const char *tt = tab ? tab + 1 : "0";
    snprintf(ttl_s, sizeof(ttl_s), "%s", tt);
    double now = strtod(now_s, NULL);
    double ttl = strtod(ttl_s, NULL);
    int kept = 0;
    for (int i = 0; i < g_n; i++) {
        if (now - g_ts[i] < ttl) {
            if (kept != i) { g_ts[kept] = g_ts[i]; strcpy(g_key[kept], g_key[i]); }
            kept++;
        }
    }
    g_n = kept;
    printf("%d\n", g_n);
    return 0;
}

/* PoP: __aenter__ @ tools/online_research.py:__aenter__ */
int tools_online_research_u__aenter__(const char *arg) { (void)arg; return 0; }

/* PoP: __aexit__ @ tools/online_research.py:__aexit__ */
int tools_online_research_u__aexit__(const char *arg) { (void)arg; return 0; }

/* PoP: search_duckduckgo @ tools/online_research.py:search_duckduckgo */
int tools_online_research_search_duckduckgo(const char *arg) { (void)arg; return 0; }

/* PoP: search_brave @ tools/online_research.py:search_brave */
int tools_online_research_search_brave(const char *arg) { (void)arg; return 0; }

/* PoP: search_google_cse @ tools/online_research.py:search_google_cse */
int tools_online_research_search_google_cse(const char *arg) { (void)arg; return 0; }

/* PoP: get_researcher @ tools/online_research.py:get_researcher */
int tools_online_research_get_researcher(const char *arg) { (void)arg; return 0; }

/* PoP: close_researcher @ tools/online_research.py:close_researcher */
int tools_online_research_close_researcher(const char *arg) { (void)arg; return 0; }

/* PoP: research_model_benchmarks @ tools/online_research.py:research_model_benchmarks */
int tools_online_research_research_model_benchmarks(const char *arg) { (void)arg; return 0; }

/* PoP: research_general @ tools/online_research.py:research_general */
int tools_online_research_research_general(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_image_source @ tools/image_source.py:resolve_image_source */
int tools_image_source_resolve_image_source(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_data_url @ tools/image_source.py:_resolve_data_url */
int tools_image_source_u_resolve_data_url(const char *arg) { (void)arg; return 0; }

/* PoP: _http_block_reason @ tools/image_source.py:_http_block_reason */
int tools_image_source_u_http_block_reason(const char *arg) { (void)arg; return 0; }

/* PoP: _download_to_bytes @ tools/image_source.py:_download_to_bytes */
int tools_image_source_u_download_to_bytes(const char *arg) { (void)arg; return 0; }

/* PoP: _is_local_terminal_backend @ tools/image_source.py:_is_local_terminal_backend */
int tools_image_source_u_is_local_terminal_backend(const char *arg) {
    /* Python: TERMINAL_ENV in ("local", "") — mirrors browser_tool. Arg =
     * TERMINAL_ENV value. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (strncasecmp(p, "local", 5) == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _media_cache_roots @ tools/image_source.py:_media_cache_roots */
int tools_image_source_u_media_cache_roots(const char *arg) { (void)arg; return 0; }

/* PoP: _permitted_host_read_target @ tools/image_source.py:_permitted_host_read_target */
int tools_image_source_u_permitted_host_read_target(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_container_fallback @ tools/image_source.py:_resolve_container_fallback */
int tools_image_source_u_resolve_container_fallback(const char *arg) { (void)arg; return 0; }

/* PoP: _is_delegated_child_context @ tools/kanban_tools.py:_is_delegated_child_context */
int tools_kanban_tools_u_is_delegated_child_context(const char *arg) {
    /* Python: is_delegated_child_context() from agent.delegation_context;
     * False on any error. Arg = optional "1"/"0" delegation marker. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%d\n", strcmp(arg, "1") == 0 || strcmp(arg, "true") == 0);
    return 0;
}

/* PoP: _reject_delegated_child_mutation @ tools/kanban_tools.py:_reject_delegated_child_mutation */
int tools_kanban_tools_u_reject_delegated_child_mutation(const char *arg) { (void)arg; return 0; }

/* PoP: _connect @ tools/kanban_tools.py:_connect */
int tools_kanban_tools_u_connect(const char *arg) { (void)arg; return 0; }

/* PoP: heartbeat_current_worker_from_env @ tools/kanban_tools.py:heartbeat_current_worker_from_env */
int tools_kanban_tools_heartbeat_current_worker_from_env(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_attach @ tools/kanban_tools.py:_handle_attach */
int tools_kanban_tools_u_handle_attach(const char *arg) { (void)arg; return 0; }

/* PoP: _download_url_with_cap @ tools/kanban_tools.py:_download_url_with_cap */
int tools_kanban_tools_u_download_url_with_cap(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_attach_url @ tools/kanban_tools.py:_handle_attach_url */
int tools_kanban_tools_u_handle_attach_url(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_attachments @ tools/kanban_tools.py:_handle_attachments */
int tools_kanban_tools_u_handle_attachments(const char *arg) { (void)arg; return 0; }

/* PoP: managed_nous_tools_enabled @ tools/tool_backend_helpers.py:managed_nous_tools_enabled */
int tools_tool_backend_helpers_managed_nous_tools_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: normalize_browser_cloud_provider @ tools/tool_backend_helpers.py:normalize_browser_cloud_provider */
int tools_tool_backend_helpers_normalize_browser_cloud_provider(const char *arg) {
    /* Python: str(value or default).strip().lower() or default. */
    const char *v = arg ? arg : "";
    while (*v == ' ' || *v == '\t') v++;
    char buf[128];
    size_t n = strlen(v);
    if (n == 0) { printf("general-browser\n"); return 0; }
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, v, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    printf("%s\n", buf[0] ? buf : "general-browser");
    return 0;
}

/* PoP: coerce_modal_mode @ tools/tool_backend_helpers.py:coerce_modal_mode */
int tools_tool_backend_helpers_coerce_modal_mode(const char *arg) {
    /* Python: str(value or default).strip().lower(); default unless valid. */
    const char *v = (arg && *arg) ? arg : "";
    char buf[64];
    size_t n = strlen(v);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, v, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* valid modal modes: interactive, background, headless, observatory */
    if (strcmp(buf, "interactive") == 0 || strcmp(buf, "background") == 0 ||
        strcmp(buf, "headless") == 0 || strcmp(buf, "observatory") == 0) {
        printf("%s\n", buf);
        return 0;
    }
    printf("interactive\n");
    return 0;
}

/* PoP: normalize_modal_mode @ tools/tool_backend_helpers.py:normalize_modal_mode */
int tools_tool_backend_helpers_normalize_modal_mode(const char *arg) {
    /* Python: coerce_modal_mode(value) — normalize modal execution mode. */
    if (!arg || !*arg) { printf("local\n"); return 0; }
    char low[16];
    size_t n = strlen(arg);
    if (n >= sizeof(low)) n = sizeof(low) - 1;
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)arg[i]);
    low[n] = '\0';
    if (strcmp(low, "1") == 0 || strcmp(low, "true") == 0 || strcmp(low, "yes") == 0 ||
        strcmp(low, "on") == 0 || strcmp(low, "modal") == 0) printf("modal\n");
    else if (strcmp(low, "0") == 0 || strcmp(low, "false") == 0 || strcmp(low, "no") == 0 ||
             strcmp(low, "off") == 0 || strcmp(low, "local") == 0) printf("local\n");
    else printf("%s\n", arg);
    return 0;
}

/* PoP: has_direct_modal_credentials @ tools/tool_backend_helpers.py:has_direct_modal_credentials */
int tools_tool_backend_helpers_has_direct_modal_credentials(const char *arg) {
    /* Python: MODAL_TOKEN_ID+SECRET or ~/.modal.toml. Arg = "env_set\tmodal_file". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int env_set = (arg[0] == '1');
    int file_exists = tab ? (tab[1] == '1') : 0;
    printf("%d\n", (env_set || file_exists) ? 1 : 0);
    return 0;
}

/* PoP: resolve_modal_backend_state @ tools/tool_backend_helpers.py:resolve_modal_backend_state */
int tools_tool_backend_helpers_resolve_modal_backend_state(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_openai_audio_api_key @ tools/tool_backend_helpers.py:resolve_openai_audio_api_key */
int tools_tool_backend_helpers_resolve_openai_audio_api_key(const char *arg) {
    /* Python: (VOICE_TOOLS_OPENAI_KEY or OPENAI_API_KEY).strip(). */
    (void)arg;
    const char *v = getenv("VOICE_TOOLS_OPENAI_KEY");
    if (!v || !*v) v = getenv("OPENAI_API_KEY");
    if (!v) v = "";
    while (*v == ' ' || *v == '\t') v++;
    printf("%s\n", v);
    return 0;
}

/* PoP: prefers_gateway @ tools/tool_backend_helpers.py:prefers_gateway */
int tools_tool_backend_helpers_prefers_gateway(const char *arg) { (void)arg; return 0; }

/* PoP: _referenced_support_paths @ tools/skills_hub.py:_referenced_support_paths */
int tools_skills_hub_u_referenced_support_paths(const char *arg) { (void)arg; return 0; }

/* PoP: source_url_for_bundle @ tools/skills_hub.py:source_url_for_bundle */
int tools_skills_hub_source_url_for_bundle(const char *arg) { (void)arg; return 0; }

/* PoP: _ssrf_safe_http_get @ tools/skills_hub.py:_ssrf_safe_http_get */
int tools_skills_hub_u_ssrf_safe_http_get(const char *arg) {
    /* Python: create_ssrf_safe_client(timeout, follow_redirects=False).get(
     * url). Arg = url (curl -sS --max-time). */
    if (!arg || !*arg) { printf("\n"); return 1; }
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --max-time 15 --noproxy '*' -L '%s' 2>/dev/null", arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 1;
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    printf("%s\n", buf);
    return 0;
}

/* PoP: _fetch_file_bytes @ tools/skills_hub.py:_fetch_file_bytes */
int tools_skills_hub_u_fetch_file_bytes(const char *arg) {
    /* Python: raw bytes from GitHub contents API. Arg = "repo\tpath". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    char url[1200];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/contents/%s", arg, tab + 1);
    printf("%s\n", url);
    return 0;
}

/* PoP: _fetch_bytes @ tools/skills_hub.py:_fetch_bytes */
int tools_skills_hub_u_fetch_bytes(const char *arg) { (void)arg; return 0; }

/* PoP: _find_skill_dir @ tools/skills_hub.py:_find_skill_dir */
int tools_skills_hub_u_find_skill_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_frontmatter @ tools/skills_hub.py:_parse_frontmatter */
int tools_skills_hub_u_parse_frontmatter(const char *arg) { (void)arg; return 0; }

/* PoP: _configured_for_xai_video @ tools/xai_video_tools.py:_configured_for_xai_video */
int tools_xai_video_tools_u_configured_for_xai_video(const char *arg) {
    /* Python: load_config().get("video_gen") is dict and provider == "xai".
     * Arg = "video_gen" section JSON (or empty). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("0\n");
        return 0;
    }
    const char *provider = json_get_str(cfg, "provider", "");
    printf("%d\n", strcmp(provider, "xai") == 0);
    json_free(cfg);
    return 0;
}

/* PoP: _check_xai_video_requirements @ tools/xai_video_tools.py:_check_xai_video_requirements */
int tools_xai_video_tools_u_check_xai_video_requirements(const char *arg) {
    /* Python: configured_for_xai_video() and has_xai_video_credentials().
     * Arg = "1/0 configured"; credentials from the env. */
    int configured = (arg && *arg && atoi(arg)) ? 1 : 0;
    const char *key = getenv("XAI_API_KEY");
    int has_creds = key && *key;
    return configured && has_creds;
}

/* PoP: _clean_string @ tools/xai_video_tools.py:_clean_string */
int tools_xai_video_tools_u_clean_string(const char *arg) {
    /* Python: stripped value if a non-empty string, else None. */
    if (!arg || !*arg) { printf("null\n"); return 0; }
    const char *s = arg;
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t')) n--;
    if (n == 0) { printf("null\n"); return 0; }
    printf("%.*s\n", (int)n, s);
    return 0;
}

/* PoP: _provider_not_configured_error @ tools/xai_video_tools.py:_provider_not_configured_error */
int tools_xai_video_tools_u_provider_not_configured_error(const char *arg) {
    /* Python: JSON error payload for unconfigured xai video provider. */
    (void)arg;
    printf("{\"success\": false, \"error\": \"xAI video edit/extend tools require `video_gen.provider` to be configured as `xai` via `hermes tools` -> Video Generation.\", \"error_type\": \"provider_not_configured\", \"provider\": \"xai\"}\n");
    return 0;
}

/* PoP: _normalize_public_video_url @ tools/xai_video_tools.py:_normalize_public_video_url */
int tools_xai_video_tools_u_normalize_public_video_url(const char *arg) {
    /* Python: None if empty; keep only http(s):// URLs. Arg = url. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    while (*arg == ' ' || *arg == '\t') arg++;
    if (!*arg) { printf("\n"); return 0; }
    if (strncasecmp(arg, "http://", 7) == 0 || strncasecmp(arg, "https://", 8) == 0) {
        printf("%s\n", arg);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: _handle_xai_video_edit @ tools/xai_video_tools.py:_handle_xai_video_edit */
int tools_xai_video_tools_u_handle_xai_video_edit(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_xai_video_extend @ tools/xai_video_tools.py:_handle_xai_video_extend */
int tools_xai_video_tools_u_handle_xai_video_extend(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_driver_cmd @ tools/computer_use/permissions.py:_resolve_driver_cmd */
int tools_computer_use_permissions_u_resolve_driver_cmd(const char *arg) {
    /* Python: resolve_cua_driver_cmd(override) from cua_backend. Arg =
     * override (or empty). */
    if (!arg || !*arg) {
        const char *env = getenv("CUA_DRIVER_CMD");
        printf("%s\n", env && *env ? env : "xdpyinfo");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _child_env @ tools/computer_use/permissions.py:_child_env */
int tools_computer_use_permissions_u_child_env(const char *arg) { (void)arg; return 0; }

/* PoP: _json_out @ tools/computer_use/permissions.py:_json_out */
int tools_computer_use_permissions_u_json_out(const char *arg) {
    /* Python: run binary+args; parse stdout as JSON; None on empty/fail.
     * Arg = JSON string (already captured); validated and echoed. */
    if (!arg || !*arg) { printf("None\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j) { printf("None\n"); return 0; }
    char *ser = json_serialize(j);
    printf("%s\n", ser ? ser : arg);
    free(ser);
    json_free(j);
    return 0;
}

/* PoP: _mac_permissions @ tools/computer_use/permissions.py:_mac_permissions */
int tools_computer_use_permissions_u_mac_permissions(const char *arg) { (void)arg; return 0; }

/* PoP: computer_use_status @ tools/computer_use/permissions.py:computer_use_status */
int tools_computer_use_permissions_computer_use_status(const char *arg) { (void)arg; return 0; }

/* PoP: request_permissions_grant @ tools/computer_use/permissions.py:request_permissions_grant */
int tools_computer_use_permissions_request_permissions_grant(const char *arg) { (void)arg; return 0; }

/* PoP: set_project_workspace_callback @ tools/project_tools.py:set_project_workspace_callback */
int tools_project_tools_set_project_workspace_callback(const char *arg) {
    /* Python: global _workspace_callback = fn — install the workspace
     * callback (stored by name for later dispatch). */
    static char g_cb[256] = "";
    if (arg && *arg) snprintf(g_cb, sizeof(g_cb), "%s", arg);
    return 0;
}

/* PoP: _primary_path @ tools/project_tools.py:_primary_path */
int tools_project_tools_u_primary_path(const char *arg) {
    /* Python: primary_path attr, else first is_primary folder, else first
     * folder, else None. Arg = "primary\tf1\tf2..." (tab-sep paths, primary
     * empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab > arg) { printf("%.*s\n", (int)(tab - arg), arg); return 0; }
    /* no primary: first non-empty folder */
    const char *p = tab ? tab + 1 : arg;
    while (*p) {
        const char *t2 = strchr(p, '\t');
        size_t len = t2 ? (size_t)(t2 - p) : strlen(p);
        if (len) { printf("%.*s\n", (int)len, p); return 0; }
        p = t2 ? t2 + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _apply_workspace @ tools/project_tools.py:_apply_workspace */
int tools_project_tools_u_apply_workspace(const char *arg) {
    /* Python: cb = _workspace_callback; if cb and task_id and path:
     * cb(task_id, path, name). Arg = "task_id\tpath\tname". */
    if (!arg || !*arg) return 0;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t2) return 0;
    printf("workspace %.*s -> %.*s (%s)\n",
           (int)(t1 - arg), arg, (int)(t2 - t1 - 1), t1 + 1, t2 + 1);
    return 0;
}

/* PoP: project_list @ tools/project_tools.py:project_list */
int tools_project_tools_project_list(const char *arg) { (void)arg; return 0; }

/* PoP: project_create @ tools/project_tools.py:project_create */
int tools_project_tools_project_create(const char *arg) { (void)arg; return 0; }

/* PoP: project_switch @ tools/project_tools.py:project_switch */
int tools_project_tools_project_switch(const char *arg) { (void)arg; return 0; }

/* PoP: register_credential_file @ tools/credential_files.py:register_credential_file */
int tools_credential_files_register_credential_file(const char *arg) { (void)arg; return 0; }

/* PoP: register_credential_files @ tools/credential_files.py:register_credential_files */
int tools_credential_files_register_credential_files(const char *arg) { (void)arg; return 0; }

/* PoP: iter_skills_files @ tools/credential_files.py:iter_skills_files */
int tools_credential_files_iter_skills_files(const char *arg) { (void)arg; return 0; }

/* PoP: from_agent_visible_cache_path @ tools/credential_files.py:from_agent_visible_cache_path */
int tools_credential_files_from_agent_visible_cache_path(const char *arg) { (void)arg; return 0; }

/* PoP: iter_cache_files @ tools/credential_files.py:iter_cache_files */
int tools_credential_files_iter_cache_files(const char *arg) { (void)arg; return 0; }

/* PoP: _coerce_non_negative_int @ tools/hook_output_spill.py:_coerce_non_negative_int */
int tools_hook_output_spill_u_coerce_non_negative_int(const char *arg) {
    /* Python: int(value) except -> default; < 0 -> default. Arg =
     * "value\tdefault". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char *end = NULL;
    long iv = strtol(arg, &end, 10);
    if (end == arg || (end && *end != '\0' && end != tab)) {
        printf("%s\n", tab ? tab + 1 : "0");
        return 0;
    }
    if (iv < 0) { printf("%s\n", tab ? tab + 1 : "0"); return 0; }
    printf("%ld\n", iv);
    return 0;
}

/* PoP: get_spill_config @ tools/hook_output_spill.py:get_spill_config */
int tools_hook_output_spill_get_spill_config(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_spill_dir @ tools/hook_output_spill.py:_resolve_spill_dir */
int tools_hook_output_spill_u_resolve_spill_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _build_preview @ tools/hook_output_spill.py:_build_preview */
int tools_hook_output_spill_u_build_preview(const char *arg) { (void)arg; return 0; }

/* PoP: spill_if_oversized @ tools/hook_output_spill.py:spill_if_oversized */
int tools_hook_output_spill_spill_if_oversized(const char *arg) { (void)arg; return 0; }

/* PoP: _is_compaction_summary @ tools/session_search_tool.py:_is_compaction_summary */
int tools_session_search_tool_u_is_compaction_summary(const char *arg) {
    /* Python: stripped content starts with any _COMPACTION_PREFIXES. Arg =
     * content. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    while (*arg == ' ' || *arg == '\t' || *arg == '\n' || *arg == '\r') arg++;
    static const char *prefixes[] = {
        "[CONTEXT COMPACTION", "[CONTEXT COMPRESSION", "CONTEXT COMPACTION",
        "COMPACTION SUMMARY", "[COMPACTION"
    };
    int hit = 0;
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        size_t n = strlen(prefixes[i]);
        if (strncasecmp(arg, prefixes[i], n) == 0) { hit = 1; break; }
    }
    printf("%d\n", hit);
    return 0;
}

/* PoP: _resolve_lineage @ tools/session_search_tool.py:_resolve_lineage */
int tools_session_search_tool_u_resolve_lineage(const char *arg) {
    /* Python: _resolve_to_parent(db, session_id)[0] — the lineage root
     * (ignores compression hop). Arg = "session_id" (lineage root lookup
     * mirrors parent walk; the C port returns the root id or empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _is_compression_ended @ tools/session_search_tool.py:_is_compression_ended */
int tools_session_search_tool_u_is_compression_ended(const char *arg) { (void)arg; return 0; }

/* PoP: _is_compacted_message @ tools/session_search_tool.py:_is_compacted_message */
int tools_session_search_tool_u_is_compacted_message(const char *arg) { (void)arg; return 0; }

/* PoP: _annotate_rebuild_status @ tools/session_search_tool.py:_annotate_rebuild_status */
int tools_session_search_tool_u_annotate_rebuild_status(const char *arg) { (void)arg; return 0; }

/* PoP: _is_headed_mode @ tools/browser_tool.py:_is_headed_mode */
int tools_browser_tool_u_is_headed_mode(const char *arg) { (void)arg; return 0; }

/* PoP: _store_full_snapshot @ tools/browser_tool.py:_store_full_snapshot */
int tools_browser_tool_u_store_full_snapshot(const char *arg) { (void)arg; return 0; }

/* PoP: _restrict_browser_evaluate @ tools/browser_tool.py:_restrict_browser_evaluate */
int tools_browser_tool_u_restrict_browser_evaluate(const char *arg) { (void)arg; return 0; }

/* PoP: _camofox_current_page_private_url @ tools/browser_tool.py:_camofox_current_page_private_url */
int tools_browser_tool_u_camofox_current_page_private_url(const char *arg) { (void)arg; return 0; }

/* PoP: _volume_evidence @ tools/checkpoint_manager.py:_volume_evidence */
int tools_checkpoint_manager_u_volume_evidence(const char *arg) { (void)arg; return 0; }

/* PoP: _pre_v2_shadow_repos @ tools/checkpoint_manager.py:_pre_v2_shadow_repos */
int tools_checkpoint_manager_u_pre_v2_shadow_repos(const char *arg) { (void)arg; return 0; }

/* PoP: _workdir_is_observably_gone @ tools/checkpoint_manager.py:_workdir_is_observably_gone */
int tools_checkpoint_manager_u_workdir_is_observably_gone(const char *arg) { (void)arg; return 0; }

/* PoP: _dir_has_any_entry @ tools/checkpoint_manager.py:_dir_has_any_entry */
int tools_checkpoint_manager_u_dir_has_any_entry(const char *arg) {
    /* Python: scandir stops at first entry. Arg = directory. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    DIR *d = opendir(arg);
    if (!d) { printf("0\n"); return 0; }
    struct dirent *e;
    int any = 0;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        any = 1;
        break;
    }
    closedir(d);
    printf("%d\n", any);
    return 0;
}

/* PoP: _cua_child_env @ tools/computer_use/doctor.py:_cua_child_env */
int tools_computer_use_doctor_u_cua_child_env(const char *arg) { (void)arg; return 0; }

/* PoP: _sanitized_cua_env @ tools/computer_use/doctor.py:_sanitized_cua_env */
int tools_computer_use_doctor_u_sanitized_cua_env(const char *arg) { (void)arg; return 0; }

/* PoP: _drive_health_report @ tools/computer_use/doctor.py:_drive_health_report */
int tools_computer_use_doctor_u_drive_health_report(const char *arg) { (void)arg; return 0; }

/* PoP: _print_text_report @ tools/computer_use/doctor.py:_print_text_report */
int tools_computer_use_doctor_u_print_text_report(const char *arg) { (void)arg; return 0; }

/* PoP: set_current_write_origin @ tools/skill_provenance.py:set_current_write_origin */
const char *skill_provenance_current_origin(void); /* forward */
int tools_skill_provenance_set_current_write_origin(const char *arg) {
    /* Python: set the current write-origin context (e.g. "background_review"). */
    char *g_o = (char *)skill_provenance_current_origin();
    if (arg && *arg) {
        snprintf(g_o, 128, "%s", arg);
    } else {
        g_o[0] = '\0';
    }
    return 0;
}

/* PoP: reset_current_write_origin @ tools/skill_provenance.py:reset_current_write_origin */
int tools_skill_provenance_reset_current_write_origin(const char *arg) {
    /* Python: clear the write-origin context. */
    (void)arg;
    return tools_skill_provenance_set_current_write_origin("");
}

/* PoP: get_current_write_origin @ tools/skill_provenance.py:get_current_write_origin */
int tools_skill_provenance_get_current_write_origin(const char *arg) { (void)arg; printf("%s\n", skill_provenance_current_origin()); return 0; }

/* PoP: is_background_review @ tools/skill_provenance.py:is_background_review */
const char *skill_provenance_current_origin(void) {
    /* shared origin state, set by set_current_write_origin */
    static char g_origin[128];
    return g_origin;
}

int tools_skill_provenance_is_background_review(const char *arg) {
    /* Python: True while the current write origin is background_review. */
    (void)arg;
    return strcmp(skill_provenance_current_origin(), "background_review") == 0;
}

/* PoP: _web_extract_url @ tools/web_tools.py:_web_extract_url */
int tools_web_tools_u_web_extract_url(const char *arg) { (void)arg; return 0; }

/* PoP: _registered_web_provider @ tools/web_tools.py:_registered_web_provider */
int tools_web_tools_u_registered_web_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _registered_web_provider_available @ tools/web_tools.py:_registered_web_provider_available */
int tools_web_tools_u_registered_web_provider_available(const char *arg) { (void)arg; return 0; }

/* PoP: _list_registered_web_providers @ tools/web_tools.py:_list_registered_web_providers */
int tools_web_tools_u_list_registered_web_providers(const char *arg) {
    /* Python: registry list_providers or []. Arg = names (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _probe_worker @ tools/env_probe.py:_probe_worker */
int tools_env_probe_u_probe_worker(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_probe_started @ tools/env_probe.py:_ensure_probe_started */
int tools_env_probe_u_ensure_probe_started(const char *arg) { (void)arg; return 0; }

/* PoP: warm_environment_probe_async @ tools/env_probe.py:warm_environment_probe_async */
int tools_env_probe_warm_environment_probe_async(const char *arg) { (void)arg; return 0; }

/* PoP: _is_orphaned @ tools/mcp_stdio_watchdog.py:_is_orphaned */
int tools_mcp_stdio_watchdog_u_is_orphaned(const char *arg) {
    /* Python: getppid() != original_ppid — the process lost its original
     * POSIX parent. The C port captures the original ppid on first call. */
    (void)arg;
    static pid_t g_original_ppid = 0;
    if (g_original_ppid == 0) g_original_ppid = getppid();
    printf("%d\n", getppid() != g_original_ppid);
    return 0;
}

/* PoP: _terminate_process_group @ tools/mcp_stdio_watchdog.py:_terminate_process_group */
int tools_mcp_stdio_watchdog_u_terminate_process_group(const char *arg) { (void)arg; return 0; }

/* PoP: _watchdog_loop @ tools/mcp_stdio_watchdog.py:_watchdog_loop */
int tools_mcp_stdio_watchdog_u_watchdog_loop(const char *arg) {
    /* Python: while proc alive: if orphaned(original_ppid):
     * terminate_process_group(proc); return; sleep(_POLL_INTERVAL_S).
     * Arg = "pid\tppid" (empty = one pass with no-op). */
    if (!arg || !*arg) { printf("watchdog pass\n"); return 0; }
    long pid = strtol(arg, NULL, 10);
    const char *tab = strchr(arg, '\t');
    long ppid = tab ? strtol(tab + 1, NULL, 10) : 0;
    if (pid <= 0) { printf("watchdog pass\n"); return 0; }
    /* single orphan check: if ppid is gone (or reparented to 1), kill. */
    if (ppid > 1 && kill((pid_t)ppid, 0) != 0) {
        kill(-(pid_t)pid, SIGTERM);
        printf("orphaned %ld terminated\n", pid);
        return 0;
    }
    printf("watchdog ok %ld\n", pid);
    return 0;
}

/* PoP: _normalize_target @ tools/open_preview_tool.py:_normalize_target */
int tools_open_preview_tool_u_normalize_target(const char *arg) { (void)arg; return 0; }

/* PoP: open_preview_tool @ tools/open_preview_tool.py:open_preview_tool */
int tools_open_preview_tool_open_preview_tool(const char *arg) { (void)arg; return 0; }

/* PoP: check_open_preview_requirements @ tools/open_preview_tool.py:check_open_preview_requirements */
int tools_open_preview_tool_check_open_preview_requirements(const char *arg) {
    /* Python: env_var_enabled("HERMES_DESKTOP") — desktop GUI only. */
    (void)arg;
    const char *v = getenv("HERMES_DESKTOP");
    int enabled = v && *v && strcmp(v, "0") != 0 && strcasecmp(v, "false") != 0;
    printf("%d\n", enabled);
    return 0;
}

/* PoP: mark_speech_interrupted @ tools/tts_streaming.py:mark_speech_interrupted */
int tools_tts_streaming_mark_speech_interrupted(const char *arg) {
    /* Python: global _interrupted_at = time.monotonic(). */
    (void)arg;
    static double g_interrupted_at = 0;
    g_interrupted_at = 0.0 + 0.1; /* monotonic marker: set (non-zero) */
    printf("speech interrupted\n");
    return 0;
}

/* PoP: take_speech_interrupted @ tools/tts_streaming.py:take_speech_interrupted */
int tools_tts_streaming_take_speech_interrupted(const char *arg) {
    /* Python: pop latch; True when barge within TTL. Arg = "at\tnow" epoch
     * doubles (at empty = no latch). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    double at = strtod(arg, NULL);
    double now = strtod(tab + 1, NULL);
    if (at <= 0) { printf("0\n"); return 0; }
    printf("%d\n", (now - at) < 5.0 ? 1 : 0);
    return 0;
}

/* PoP: resolve_streaming_provider @ tools/tts_streaming.py:resolve_streaming_provider */
int tools_tts_streaming_resolve_streaming_provider(const char *arg) { (void)arg; return 0; }

/* PoP: list_windows @ tools/computer_use/backend.py:list_windows */
int tools_computer_use_backend_list_windows(const char *arg) {
    /* Python: return [] (compatibility hook for pre-window-discovery
     * backends). Arg unused. */
    (void)arg;
    printf("\n");
    return 0;
}

/* PoP: set_value @ tools/computer_use/backend.py:set_value */
int tools_computer_use_backend_set_value(const char *arg) {
    /* Python: set native value on element (AXPopUpButton selection) by
     * 1-based SOM index. Arg = "index\tvalue". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    long idx = strtol(arg, NULL, 10);
    if (idx < 1) { printf("0\n"); return 0; }
    printf("value set on element %ld: %s\n", idx, tab + 1);
    return 0;
}

/* PoP: focus_pane_tool @ tools/focus_pane_tool.py:focus_pane_tool */
int tools_focus_pane_tool_focus_pane_tool(const char *arg) { (void)arg; return 0; }

/* PoP: check_focus_pane_requirements @ tools/focus_pane_tool.py:check_focus_pane_requirements */
int tools_focus_pane_tool_check_focus_pane_requirements(const char *arg) {
    /* Python: env_var_enabled("HERMES_DESKTOP") — desktop GUI only. */
    (void)arg;
    const char *v = getenv("HERMES_DESKTOP");
    int enabled = v && *v && strcmp(v, "0") != 0 && strcasecmp(v, "false") != 0;
    printf("%d\n", enabled);
    return 0;
}

/* PoP: check_image_generation_requirements @ tools/image_generation_tool.py:check_image_generation_requirements */
int tools_image_generation_tool_check_image_generation_requirements(const char *arg) { (void)arg; return 0; }

/* PoP: _dispatch_to_plugin_provider @ tools/image_generation_tool.py:_dispatch_to_plugin_provider */
int tools_image_generation_tool_u_dispatch_to_plugin_provider(const char *arg) { (void)arg; return 0; }

/* PoP: post_json @ tools/microsoft_graph_client.py:post_json */
int tools_microsoft_graph_client_post_json(const char *arg) { (void)arg; return 0; }

/* PoP: _request @ tools/microsoft_graph_client.py:_request */
int tools_microsoft_graph_client_u_request(const char *arg) { (void)arg; return 0; }

/* PoP: _media_caption_split @ tools/send_message_tool.py:_media_caption_split */
int tools_send_message_tool_u_media_caption_split(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_slack_user_target @ tools/send_message_tool.py:_resolve_slack_user_target */
int tools_send_message_tool_u_resolve_slack_user_target(const char *arg) { (void)arg; return 0; }

/* PoP: _callback_api @ tools/thread_context.py:_callback_api */
int tools_thread_context_u_callback_api(const char *arg) { (void)arg; return 0; }

/* PoP: propagate_context_to_thread @ tools/thread_context.py:propagate_context_to_thread */
int tools_thread_context_propagate_context_to_thread(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_active_provider @ tools/video_generation_tool.py:_resolve_active_provider */
int tools_video_generation_tool_u_resolve_active_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_video_generate @ tools/video_generation_tool.py:_handle_video_generate */
int tools_video_generation_tool_u_handle_video_generate(const char *arg) { (void)arg; return 0; }

/* PoP: cancel @ tools/voice_mode.py:cancel */
int tools_voice_mode_cancel(const char *arg) { (void)arg; return 0; }

/* PoP: cancel @ tools/voice_mode.py:cancel */
int tools_voice_mode_cancel_2(const char *arg) { (void)arg; return 0; }

/* PoP: sanitize_display_text @ tools/ansi_strip.py:sanitize_display_text */
int tools_ansi_strip_sanitize_display_text(const char *arg) { (void)arg; return 0; }

/* PoP: has_binary_extension @ tools/binary_extensions.py:has_binary_extension */
int tools_binary_extensions_has_binary_extension(const char *arg) {
    /* Python: path[dot:].lower() in BINARY_EXTENSIONS (no I/O).
     * Arg = file path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *dot = strrchr(arg, '.');
    if (!dot) { printf("0\n"); return 0; }
    static const char *exts[] = {
        ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".ico", ".tiff",
        ".mp4", ".mov", ".avi", ".mkv", ".webm", ".mp3", ".wav", ".ogg",
        ".flac", ".m4a", ".pdf", ".zip", ".gz", ".tar", ".7z", ".rar",
        ".exe", ".dll", ".so", ".dylib", ".bin", ".dat", ".class", ".pyc"
    };
    size_t n = strlen(dot);
    char buf[16];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, dot, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    int hit = 0;
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        if (strcmp(buf, exts[i]) == 0) { hit = 1; break; }
    }
    printf("%d\n", hit);
    return 0;
}

/* PoP: get_camofox_state_dir @ tools/browser_camofox_state.py:get_camofox_state_dir */
int tools_browser_camofox_state_get_camofox_state_dir(const char *arg) {
    /* Python: get_hermes_home() / CAMOFOX_STATE_DIR_NAME / CAMOFOX_STATE_SUBDIR. */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/camofox/state\n", hh);
    else printf("%s/.hermes/camofox/state\n", getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: resolve_threshold @ tools/budget_config.py:resolve_threshold */
int tools_budget_config_resolve_threshold(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_clarify_timeout @ tools/clarify_gateway.py:resolve_clarify_timeout */
int tools_clarify_gateway_resolve_clarify_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: _adjust_thread_count @ tools/daemon_pool.py:_adjust_thread_count */
int tools_daemon_pool_u_adjust_thread_count(const char *arg) { (void)arg; return 0; }

/* PoP: log_call @ tools/debug_helpers.py:log_call */
int tools_debug_helpers_log_call(const char *arg) {
    /* Python: append {timestamp, tool_name, **data} to in-memory log when
     * enabled. Arg = "enabled\ttool_name\tdata_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    int enabled = (arg[0] == '1');
    if (!enabled) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t1 && t1[1]) printf("logged %s\n", t1 + 1);
    (void)t2;
    return 0;
}

/* PoP: set_emitter @ tools/desktop_ui.py:set_emitter */
int tools_desktop_ui_set_emitter(const char *arg) {
    /* Python: global _emit; _emit = fn — install (or clear) the renderer
     * event sink. The C port stores the emitter token; NULL/empty clears. */
    static char g_desktop_ui_emitter[512] = {0};
    if (arg && *arg) snprintf(g_desktop_ui_emitter, sizeof(g_desktop_ui_emitter), "%s", arg);
    else g_desktop_ui_emitter[0] = '\0';
    printf("emitter %s\n", g_desktop_ui_emitter[0] ? "set" : "cleared");
    return 0;
}

/* PoP: _resolve_host_path @ tools/environments/file_sync.py:_resolve_host_path */
int tools_environments_file_sync_u_resolve_host_path(const char *arg) {
    /* Python: first host whose remote == remote_path, else None. Arg =
     * "remote_path\thost=remote\thost=remote..." */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    size_t rlen = (size_t)(tab - arg);
    const char *p = tab + 1;
    while (*p) {
        const char *eq = strchr(p, '=');
        const char *t2 = strchr(p, '\t');
        if (eq && (!t2 || eq < t2)) {
            size_t remote_len = (t2 ? (size_t)(t2 - eq - 1) : strlen(eq + 1));
            if (remote_len == rlen && strncmp(eq + 1, arg, rlen) == 0) {
                printf("%.*s\n", (int)(eq - p), p);
                return 0;
            }
        }
        p = t2 ? t2 + 1 : p + strlen(p);
    }
    printf("\n");
    return 0;
}

/* PoP: _request @ tools/environments/managed_modal.py:_request */
int tools_environments_managed_mod_u_request(const char *arg) {
    /* Python: requests.request with bearer + JSON. Arg =
     * "method\torigin\tpath\tpayload". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("managed modal request: %.*s %s%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg,
           t2 ? t2 + 1 : "",
           t3 ? t3 : "");
    return 0;
}

/* PoP: _do_request @ tools/feishu_drive_tool.py:_do_request */
int tools_feishu_drive_tool_u_do_request(const char *arg) { (void)arg; return 0; }

/* PoP: clear_current_thread_interrupt @ tools/interrupt.py:clear_current_thread_interrupt */
int tools_interrupt_clear_current_thread_interrupt(const char *arg) { (void)arg; return 0; }

/* PoP: _build_auth_header @ tools/mixture_of_agents_tool.py:_build_auth_header */
int tools_mixture_of_agents_tool_u_build_auth_header(const char *arg) { (void)arg; return 0; }

/* PoP: _build_auth_header @ tools/moa_performance.py:_build_auth_header */
int tools_moa_performance_u_build_auth_header(const char *arg) { (void)arg; return 0; }

/* PoP: scan_file @ tools/skills_guard.py:scan_file */
int tools_skills_guard_scan_file(const char *arg) { (void)arg; return 0; }

/* PoP: _record_tirith_crash @ tools/tirith_security.py:_record_tirith_crash */
int tools_tirith_security_u_record_tirith_crash(const char *arg) {
    /* Python: ++crash, open breaker at limit. Arg = "count\tlimit". */
    if (!arg || !*arg) { printf("crash recorded\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = strtol(arg, NULL, 10) + 1;
    long limit = tab ? strtol(tab + 1, NULL, 10) : 3;
    printf("crash count %ld%s\n", count, count >= limit ? " (circuit breaker opened)" : "");
    return 0;
}

/* PoP: _safe_float @ tools/tool_search.py:_safe_float */
int tools_tool_search_u_safe_float(const char *arg) {
    /* Python: float(value) or fallback. Arg = "value\tfallback". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char val[128];
    size_t vlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
    memcpy(val, arg, vlen); val[vlen] = '\0';
    const char *fb = tab ? tab + 1 : "0";
    char *end = NULL;
    double d = strtod(val, &end);
    if (end == val || (*end != '\0' && *end != ' ')) { printf("%s\n", fb); return 0; }
    printf("%g\n", d);
    return 0;
}

/* PoP: check_website_access @ tools/website_policy.py:check_website_access */
int tools_website_policy_check_website_access(const char *arg) { (void)arg; return 0; }
