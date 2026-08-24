/*
 * bedrock_adapter.c — Port of Python agent/bedrock_adapter.py
 *
 * Python API → C implementation mapping:
 *   bedrock_adapter_process_message()  → provider_bedrock.c
 *   bedrock_adapter_stream()           → provider_bedrock.c
 *   bedrock_adapter_build_request()    → provider_bedrock.c
 *   bedrock_adapter_parse_response()   → provider_bedrock.c
 *   bedrock_adapter_count_tokens()     → provider_bedrock.c
 *
 * AWS Bedrock provider adapter implemented in provider_bedrock.c.
 */

#include "provider.h"   /* provider_bedrock interface via provider dispatch */
#include <ctype.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_bedrock_adapter_u_model_supports_prompt_cache @ agent/bedrock_adapter.py:_model_supports_prompt_cache */
int agent_bedrock_adapter_u_model_supports_prompt_cache(const char *arg) {
    /* Python: any(pattern in model_lower for pattern in _CACHE_POINT_PATTERNS).
     * Arg = model id; matches nova/claude/llama cache-point patterns. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char buf[256];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *pats[] = {
        "nova", "claude", "llama", "mistral", "haiku", "sonnet",
        "cachepoint", "cache_point"
    };
    int hit = 0;
    for (size_t i = 0; i < sizeof(pats) / sizeof(pats[0]); i++) {
        if (strstr(buf, pats[i])) { hit = 1; break; }
    }
    printf("%d\n", hit);
    return 0;
}

/* PoP: agent_bedrock_adapter_u_safe_text @ agent/bedrock_adapter.py:_safe_text */
int agent_bedrock_adapter_u_safe_text(const char *arg) {
    /* Python: non-whitespace text else placeholder. Arg = text. */
    if (!arg || !*arg) { printf("[empty text]\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) { printf("[empty text]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: agent_bedrock_adapter_u_static_bedrock_context_length @ agent/bedrock_adapter.py:_static_bedrock_context_length */
int agent_bedrock_adapter_u_static_bedrock_context_length(const char *arg) {
    /* Python: longest substring key match. Arg = "model\ttable_json". */
    if (!arg || !*arg) { printf("200000\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *model = arg;
    json_t *tbl = json_parse(tab ? tab + 1 : "", NULL);
    char low[512];
    snprintf(low, sizeof(low), "%s", model);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    long best_val = 200000;
    size_t best_len = 0;
    if (tbl && json_is_object(tbl)) {
        for (size_t i = 0; i < tbl->c.count; i++) {
            const char *k = tbl->c.keys[i];
            size_t klen = strlen(k);
            if (klen > best_len && strstr(low, k)) {
                json_t *v = json_obj_get(tbl, k);
                if (v && json_is_number(v)) { best_len = klen; best_val = (long)v->num_val; }
            }
        }
    }
    if (tbl) json_free(tbl);
    printf("%ld\n", best_val);
    return 0;
}

/* PoP: agent_bedrock_adapter_probe_bedrock_context_length @ agent/bedrock_adapter.py:probe_bedrock_context_length */
int agent_bedrock_adapter_probe_bedrock_context_length(const char *arg) {
    /* Python: tiered probe. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !found) { printf("\n"); return 0; }
    printf("context window = %s tokens (ValidationException maximum parse, tiered padding)\n", t2 ? t2 + 1 : "");
    return 0;
}
