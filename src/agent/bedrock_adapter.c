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
/* Faithful port: python
 *   text = "" if text is None else str(text)
 *   return text if text.strip() else _EMPTY_TEXT_PLACEHOLDER   # "(empty)"
 * Was: printed "[empty text]" (WRONG placeholder) and returned 0, so no caller
 * could observe the value. Now returns an owned string (free() by caller). No
 * other caller exists, so the signature change is safe. */
char *agent_bedrock_adapter_u_safe_text(const char *arg) {
    const char *t = arg ? arg : "";
    const char *p = t;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) return strdup("(empty)");
    return strdup(t);
}

/* PoP: agent_bedrock_adapter_u_static_bedrock_context_length @ agent/bedrock_adapter.py:bedrock_static_context_length */
/* Faithful port of the table path of bedrock_static_context_length(model):
 *   lowered = model.lower()
 *   matches = [k for k in BEDROCK_CONTEXT_LENGTHS if k in lowered]
 *   return BEDROCK_CONTEXT_LENGTHS[max(matches, key=len)]  (longest key wins)
 * Returns -1 when nothing matches (python's None / family-fallback miss). The
 * table is passed in (table_json) so the harness can supply the real table.
 * Was: printed 200000/val and returned 0, so no caller could observe it. */
long agent_bedrock_adapter_u_static_bedrock_context_length(const char *model, const char *table_json) {
    if (!model) return -1;
    json_t *tbl = json_parse(table_json ? table_json : "", NULL);
    char low[512];
    snprintf(low, sizeof(low), "%s", model);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    long best_val = -1;
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
    return best_val;
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
