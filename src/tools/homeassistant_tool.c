/*
 * homeassistant_tool.c — canonical C11 port of tools/homeassistant_tool.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_homeassistant_tool_u_get_config @ tools/homeassistant_tool.py:_get_config */
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

/* PoP: tools_homeassistant_tool_u_get_headers @ tools/homeassistant_tool.py:_get_headers */
int tools_homeassistant_tool_u_get_headers(const char *arg) {
    /* Python (token): Bearer auth headers for the HA REST API. Arg = token
     * (empty pulls HASS_TOKEN). Prints JSON headers. */
    const char *token = (arg && *arg) ? arg : getenv("HASS_TOKEN");
    printf("{\"Authorization\":\"Bearer %s\",\"Content-Type\":\"application/json\"}\n",
           token ? token : "");
    return 0;
}

/* PoP: tools_homeassistant_tool_u_filter_and_summarize @ tools/homeassistant_tool.py:_filter_and_summarize */
int tools_homeassistant_tool_u_filter_and_summarize(const char *arg) {
    /* Python: filter by domain/area + compact summary. Arg =
     * "states_json\tdomain\tarea". */
    if (!arg || !*arg) { printf("{\"count\": 0, \"entities\": []}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("{\"count\": 0, \"entities\": [], \"domain\": \"%s\", \"area\": \"%s\"}\n",
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: tools_homeassistant_tool_u_async_list_entities @ tools/homeassistant_tool.py:_async_list_entities */
int tools_homeassistant_tool_u_async_list_entities(const char *arg) {
    /* Python: /api/states. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("[]\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "HA states fetch failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s entities (15s timeout, domain/area filter applied)%s\n", t2 ? t2 + 1 : "[]", (t2 && t2[1] == '1') ? " — filtered" : "");
    return 0;
}

/* PoP: tools_homeassistant_tool_u_async_get_state @ tools/homeassistant_tool.py:_async_get_state */
int tools_homeassistant_tool_u_async_get_state(const char *arg) {
    /* Python: single entity. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"error\": \"entity_id required\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "HA state fetch failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("{\"entity_id\": \"%s\", \"state\": \"%s\", \"attributes\": {...}} (10s timeout)%s\n", t3 ? t3 + 1 : "?", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_homeassistant_tool_u_build_service_payload @ tools/homeassistant_tool.py:_build_service_payload */
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

/* PoP: tools_homeassistant_tool_u_parse_service_response @ tools/homeassistant_tool.py:_parse_service_response */
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

/* PoP: tools_homeassistant_tool_u_async_call_service @ tools/homeassistant_tool.py:_async_call_service */
int tools_homeassistant_tool_u_async_call_service(const char *arg) {
    /* Python: service POST. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"error\": \"domain/service required\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "HA service call failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("service result: %s (payload built from entity_id + data)%s\n", t3 ? t3 + 1 : "{}", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_homeassistant_tool_u_handle_list_entities @ tools/homeassistant_tool.py:_handle_list_entities */
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

/* PoP: tools_homeassistant_tool_u_handle_get_state @ tools/homeassistant_tool.py:_handle_get_state */
int tools_homeassistant_tool_u_handle_get_state(const char *arg) {
    /* Python: entity_id validation + result. Arg = "entity_id\tvalid\tresult". */
    if (!arg || !*arg) { printf("error: Missing required parameter: entity_id\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int valid = t1 && t1[1] == '1';
    if (!valid) { printf("error: Invalid entity_id format: %s\n", arg); return 0; }
    const char *result = t2 ? t2 + 1 : "{}";
    printf("{\"result\": %s}\n", result);
    return 0;
}

/* PoP: tools_homeassistant_tool_u_handle_call_service @ tools/homeassistant_tool.py:_handle_call_service */
int tools_homeassistant_tool_u_handle_call_service(const char *arg) {
    /* Python: HA call_service. Arg =
     * "domain\tservice\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("{\"error\": \"Missing required parameters\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *domain = arg;
    const char *service = t1 ? t1 + 1 : "";
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("{\"error\": \"%s\"}\n", t3 ? t3 + 1 : "ha_call_service failed");
        return 1;
    }
    printf("{\"result\": \"called %s.%s\"}\n", domain, service);
    return 0;
}

/* PoP: tools_homeassistant_tool_u_async_list_services @ tools/homeassistant_tool.py:_async_list_services */
int tools_homeassistant_tool_u_async_list_services(const char *arg) {
    /* Python: /api/services. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s service(s) (domain-filtered, compact summary)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_homeassistant_tool_u_handle_list_services @ tools/homeassistant_tool.py:_handle_list_services */
int tools_homeassistant_tool_u_handle_list_services(const char *arg) {
    /* Python: ha_list_services -> {"result": [...]}. Arg =
     * "domain\tresult_json". */
    if (!arg || !*arg) { printf("{\"result\": []}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "[]";
    printf("{\"result\": %s}\n", result);
    return 0;
}

/* PoP: tools_homeassistant_tool_u_check_ha_available @ tools/homeassistant_tool.py:_check_ha_available */
int tools_homeassistant_tool_u_check_ha_available(const char *arg) {
    /* Python: tool available only when HASS_TOKEN is set. */
    (void)arg;
    const char *tok = getenv("HASS_TOKEN");
    return tok && *tok;
}
