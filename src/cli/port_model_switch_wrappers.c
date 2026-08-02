/*
 * port_model_switch_wrappers.c — C port of hermes_cli/model_switch.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: _declared_model_ids @ hermes_cli/model_switch.py:_declared_model_ids */
int msw_u_declared_model_ids(const char *arg) { (void)arg; return 0; }

/* PoP: _save_discovered_models_to_config @ hermes_cli/model_switch.py:_save_discovered_models_to_config */
int msw_u_save_discovered_models_to_config(const char *arg) { (void)arg; return 0; }

/* PoP: _bare_custom_provider_def @ hermes_cli/model_switch.py:_bare_custom_provider_def */
int msw_u_bare_custom_provider_def(const char *arg) {
    /* Python: ProviderDef for custom endpoint or None. Arg = "base_url". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (!*p) { printf("\n"); return 0; }
    printf("custom endpoint provider: %s\n", p);
    return 0;
}

/* PoP: format_model_for_display @ hermes_cli/model_switch.py:format_model_for_display */
int msw_format_model_for_display(const char *arg) {
    /* Python: strip the Palantir Foundry opaque proxy prefix
     * "ri.language-model-service..language-model." and return the trailing
     * slug; everything else passes through untouched. DISPLAY-ONLY. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    static const char *const PREFIX = "ri.language-model-service..language-model.";
    size_t pl = strlen(PREFIX);
    if (strncmp(arg, PREFIX, pl) == 0 && arg[pl]) {
        printf("%s\n", arg + pl);
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: is_nous_hermes_non_agentic @ hermes_cli/model_switch.py:is_nous_hermes_non_agentic */
int msw_is_nous_hermes_non_agentic(const char *arg) {
    /* Python: regex on model name (Hermes 3/4 chat). Arg = model name. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p) {
        if (strncasecmp(p, "hermes-3", 8) == 0 || strncasecmp(p, "hermes-4", 8) == 0 ||
            strncasecmp(p, "hermes3", 7) == 0 || strncasecmp(p, "hermes4", 7) == 0) {
            printf("1\n");
            return 0;
        }
        p++;
    }
    printf("0\n");
    return 0;
}

/* PoP: _check_hermes_model_warning @ hermes_cli/model_switch.py:_check_hermes_model_warning */
int msw_u_check_hermes_model_warning(const char *arg) {
    /* Python: is_nous_hermes_non_agentic(model_name) -> warning text or "". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *m = arg;
    for (const char *p = m; *p; p++) {
        /* match (?:^|[/:])hermes[-_ ]?[34](?:[-_.:]|$) case-insensitively */
        if ((p == m || p[-1] == '/' || p[-1] == ':')
            && strncasecmp(p, "hermes", 6) == 0) {
            const char *q = p + 6;
            if (*q == '-' || *q == '_' || *q == ' ') q++;
            if (*q == '3' || *q == '4') {
                const char *r = q + 1;
                if (*r == '\0' || *r == '-' || *r == '.' || *r == '_' || *r == ':') {
                    printf("Nous Research Hermes 3 & 4 models are NOT agentic and are not designed for use with Hermes Agent. They lack the tool-calling capabilities required for agent workflows. Consider using an agentic model instead.\n");
                    return 0;
                }
            }
        }
    }
    printf("\n");
    return 0;
}

/* PoP: _load_direct_aliases @ hermes_cli/model_switch.py:_load_direct_aliases */
int msw_u_load_direct_aliases(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_direct_aliases @ hermes_cli/model_switch.py:_ensure_direct_aliases */
int msw_u_ensure_direct_aliases(const char *arg) {
    /* Python: lazy-load aliases in place. Arg = "loaded". */
    (void)arg;
    printf("direct aliases ensured\n");
    return 0;
}

/* PoP: parse_model_flags_detailed @ hermes_cli/model_switch.py:parse_model_flags_detailed */
int msw_parse_model_flags_detailed(const char *arg) { (void)arg; return 0; }

/* PoP: _model_sort_key @ hermes_cli/model_switch.py:_model_sort_key */
int msw_u_model_sort_key(const char *arg) { (void)arg; return 0; }

/* PoP: get_authenticated_provider_slugs @ hermes_cli/model_switch.py:get_authenticated_provider_slugs */
int msw_get_authenticated_provider_slugs(const char *arg) {
    /* Python: slugs from list_authenticated_providers or []. Arg = "slugs"
     * (tab-sep, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _resolve_alias_fallback @ hermes_cli/model_switch.py:_resolve_alias_fallback */
int msw_u_resolve_alias_fallback(const char *arg) {
    /* Python: try providers (default openrouter,nous), first hit wins. Arg =
     * "raw\tproviders\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("\n");
    return 0;
}

/* PoP: resolve_display_context_length @ hermes_cli/model_switch.py:resolve_display_context_length */
int msw_resolve_display_context_length(const char *arg) { (void)arg; return 0; }

/* PoP: _configured_provider_matches @ hermes_cli/model_switch.py:_configured_provider_matches */
int msw_u_configured_provider_matches(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_named_custom_model_id @ hermes_cli/model_switch.py:_resolve_named_custom_model_id */
int msw_u_resolve_named_custom_model_id(const char *arg) { (void)arg; return 0; }

/* PoP: _credential_pool_is_usable @ hermes_cli/model_switch.py:_credential_pool_is_usable */
int msw_u_credential_pool_is_usable(const char *arg) { (void)arg; return 0; }

/* PoP: _extra_headers_from_config @ hermes_cli/model_switch.py:_extra_headers_from_config */
int msw_u_extra_headers_from_config(const char *arg) {
    /* Python: {} if not dict; else normalize_extra_headers(
     * entry.get("extra_headers")). Arg = entry JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *entry = json_parse(arg, NULL);
    if (!entry || !json_is_object(entry)) {
        if (entry) json_free(entry);
        printf("{}\n");
        return 0;
    }
    json_t *eh = json_obj_get(entry, "extra_headers");
    if (!eh || !json_is_object(eh)) { json_free(entry); printf("{}\n"); return 0; }
    char *ser = json_serialize(eh);
    printf("%s\n", ser ? ser : "{}");
    free(ser);
    json_free(entry);
    return 0;
}

/* PoP: prewarm_picker_cache_async @ hermes_cli/model_switch.py:prewarm_picker_cache_async */
int msw_prewarm_picker_cache_async(const char *arg) { (void)arg; return 0; }

/* PoP: _prepend_moa_picker_provider @ hermes_cli/model_switch.py:_prepend_moa_picker_provider */
int msw_u_prepend_moa_picker_provider(const char *arg) { (void)arg; return 0; }
