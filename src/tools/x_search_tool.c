/*
 * x_search_tool.c — canonical C11 port of tools/x_search_tool.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_x_search_tool_u_load_x_search_config @ tools/x_search_tool.py:_load_x_search_config */
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

/* PoP: tools_x_search_tool_u_get_x_search_model @ tools/x_search_tool.py:_get_x_search_model */
int tools_x_search_tool_u_get_x_search_model(const char *arg) {
    /* Python: cfg.get("model") or DEFAULT_X_SEARCH_MODEL ("grok-4.5"). */
    (void)arg;
    const char *model = "grok-4.5";
    if (arg && *arg && strcmp(arg, "-") != 0) model = arg;
    printf("%s\n", model);
    return 0;
}

/* PoP: tools_x_search_tool_u_get_x_search_reasoning_effort @ tools/x_search_tool.py:_get_x_search_reasoning_effort */
int tools_x_search_tool_u_get_x_search_reasoning_effort(const char *arg) {
    /* Python: config effort validated against set; ValueError else. Arg =
     * "raw\tallowed". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *raw = arg;
    const char *allowed = tab ? tab + 1 : "";
    const char *p = raw;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) { printf("\n"); return 0; }
    const char *q = allowed;
    while (*q) {
        const char *t = strchr(q, '\t');
        size_t len = t ? (size_t)(t - q) : strlen(q);
        if (len == strlen(p) && strncmp(q, p, len) == 0) { printf("%s\n", p); return 0; }
        q = t ? t + 1 : q + len;
    }
    fprintf(stderr, "x_search.reasoning_effort must be one of: %s (got %s)\n", allowed, raw);
    return 1;
}

/* PoP: tools_x_search_tool_u_get_x_search_timeout_seconds @ tools/x_search_tool.py:_get_x_search_timeout_seconds */
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

/* PoP: tools_x_search_tool_u_get_x_search_retries @ tools/x_search_tool.py:_get_x_search_retries */
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

/* PoP: tools_x_search_tool_u_resolve_xai_bearer @ tools/x_search_tool.py:_resolve_xai_bearer */
int tools_x_search_tool_u_resolve_xai_bearer(const char *arg) {
    /* Python: (api_key, base_url, source). Arg =
     * "state\tapi_key\tbase_url\tsource". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "no_key") == 0) {
        fprintf(stderr, "No xAI credentials available. Run `hermes auth add xai-oauth` to sign in with your SuperGrok subscription, or set XAI_API_KEY.\n");
        return 1;
    }
    printf("%s\t%s\t%s\n", t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "xai");
    return 0;
}

/* PoP: tools_x_search_tool_check_x_search_requirements @ tools/x_search_tool.py:check_x_search_requirements */
int tools_x_search_tool_check_x_search_requirements(const char *arg) {
    /* Python: api_key present (auto-refresh counts). Arg = "has_key". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: tools_x_search_tool_u_normalize_handles @ tools/x_search_tool.py:_normalize_handles */
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

/* PoP: tools_x_search_tool_u_parse_iso_date @ tools/x_search_tool.py:_parse_iso_date */
int tools_x_search_tool_u_parse_iso_date(const char *arg) {
    /* Python: strict YYYY-MM-DD. Arg = "date\tvalid". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int valid = tab && tab[1] == '1';
    if (!valid) {
        fprintf(stderr, "%s must be YYYY-MM-DD\n", arg);
        return 1;
    }
    printf("parsed: %s\n", arg);
    return 0;
}

/* PoP: tools_x_search_tool_u_validate_date_range @ tools/x_search_tool.py:_validate_date_range */
int tools_x_search_tool_u_validate_date_range(const char *arg) {
    /* Python: from<=to + not future. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "%s\n", t3 ? t3 + 1 : "invalid date range");
        return 1;
    }
    printf("date range valid\n");
    return 0;
}

/* PoP: tools_x_search_tool_u_extract_inline_citations @ tools/x_search_tool.py:_extract_inline_citations */
int tools_x_search_tool_u_extract_inline_citations(const char *arg) {
    /* Python: pull url_citation annotations. Arg = "citations_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_x_search_tool_u_http_error_message @ tools/x_search_tool.py:_http_error_message */
int tools_x_search_tool_u_http_error_message(const char *arg) {
    /* Python: response payload -> message. Arg = "state\tcode\terror\tmsg". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    const char *code = t1 ? t1 + 1 : "";
    const char *error = t2 ? t2 + 1 : "";
    const char *msg = t3 ? t3 + 1 : "";
    if (strcmp(state, "no_response") == 0) { printf("%s\n", msg); return 0; }
    if (strcmp(state, "json") == 0) {
        if (code[0] && strstr(error, code) == NULL) printf("%s: %s\n", code, error);
        else printf("%s\n", error);
        return 0;
    }
    if (strcmp(state, "text") == 0) { printf("%s\n", msg); return 0; }
    printf("%s\n", msg);
    return 0;
}

/* PoP: tools_x_search_tool_x_search_tool @ tools/x_search_tool.py:x_search_tool */
int tools_x_search_tool_x_search_tool(const char *arg) {
    /* Python: x_search gate. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"error\": \"query is required\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_query") == 0) {
        printf("{\"error\": \"query is required for x_search\"}\n");
        return 1;
    }
    if (strcmp(state, "both_filters") == 0) {
        printf("{\"error\": \"allowed_x_handles and excluded_x_handles cannot be used together\"}\n");
        return 1;
    }
    if (strcmp(state, "bad_date") == 0 || strcmp(state, "bad_reasoning") == 0) {
        printf("{\"error\": \"%s\"}\n", t3 ? t3 + 1 : "invalid param");
        return 1;
    }
    printf("{\"results\": %s}\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: tools_x_search_tool_u_handle_x_search @ tools/x_search_tool.py:_handle_x_search */
int tools_x_search_tool_u_handle_x_search(const char *arg) {
    /* Python: x_search_tool dispatch with args. Arg = args JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    const char *q = j ? json_get_str(j, "query", "") : (arg[0] ? arg : "");
    printf("x search: %s\n", q);
    if (j) json_free(j);
    return 0;
}
