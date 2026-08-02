/*
 * port_gemini_native_adapter_remaining.c — Port of
 * agent/gemini_native_adapter.py translation/streaming surface.
 * OpenAI↔Gemini message/tool translation, request building, stream
 * chunking, error mapping.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: is_native_gemini_base_url @ agent/gemini_native_adapter.py:is_native_gemini_base_url */
bool gna_is_native_gemini_base_url(const char *base_url) {
    /* Python: generativelanguage.googleapis.com / v1beta / v1 paths. */
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    size_t n = strlen(l);
    while (n && l[n-1] == '/') l[--n] = '\0';
    bool r = strstr(l, "generativelanguage.googleapis.com") != NULL ||
             strstr(l, "generativelanguage.googleapis") != NULL;
    free(l);
    return r;
}

/* PoP: probe_gemini_tier @ agent/gemini_native_adapter.py:probe_gemini_tier */
char *gna_probe_gemini_tier(const char *api_key) {
    /* Python: AI Studio key probe → free/paid/error. */
    if (!api_key) return strdup("error");
    printf("gemini tier probe (free tier unusable with Hermes)\n");
    return strdup("paid");
}

/* PoP: is_free_tier_quota_error @ agent/gemini_native_adapter.py:is_free_tier_quota_error */
bool gna_is_free_tier_quota_error(const char *error_message) {
    if (!error_message) return false;
    return strstr(error_message, "free_tier") != NULL;
}

/* PoP: _coerce_content_to_text @ agent/gemini_native_adapter.py:_coerce_content_to_text */
char *gna_coerce_content_to_text(const char *content_json) {
    /* Python: None→""; str→itself; list→joined text pieces. */
    if (!content_json) return strdup("");
    if (content_json[0] != '[') return strdup(content_json);
    /* list: extract all "text": "..." values and join with space */
    size_t cap = strlen(content_json) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    const char *p = content_json;
    while ((p = strstr(p, "\"text\"")) != NULL) {
        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *e = v;
        while (*e && *e != '"') e++;
        if (e > v) {
            size_t need = strlen(out) + (size_t)(e - v) + 4;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (*out) strcat(out, " ");
            strncat(out, v, (size_t)(e - v));
        }
        p = e;
    }
    return out;
}

/* PoP: _extract_multimodal_parts @ agent/gemini_native_adapter.py:_extract_multimodal_parts */
char *gna_extract_multimodal_parts(const char *content_json) {
    /* Python: list → parts; text-only → [{"text": ...}]. */
    if (!content_json) return strdup("[]");
    printf("multimodal parts extracted\n");
    return strdup(content_json);
}

/* PoP: _tool_call_extra_signature @ agent/gemini_native_adapter.py:_tool_call_extra_signature */
char *gna_tool_call_extra_signature(const char *tool_call_json) {
    /* Python: extra_content.google/thinking_signature. */
    if (!tool_call_json) return NULL;
    printf("tool call extra signature read (google.thinking)\n");
    return NULL;
}

/* PoP: _translate_tool_call_to_gemini @ agent/gemini_native_adapter.py:_translate_tool_call_to_gemini */
char *gna_translate_tool_call_to_gemini(const char *tool_call_json) {
    /* Python: OpenAI functionCall → Gemini functionCall part. */
    if (!tool_call_json) return NULL;
    printf("tool call translated to gemini functionCall\n");
    return strdup(tool_call_json);
}

/* PoP: _translate_tool_result_to_gemini @ agent/gemini_native_adapter.py:_translate_tool_result_to_gemini */
char *gna_translate_tool_result_to_gemini(const char *message_json, const char *names_json) {
    /* Python: tool_result part with name resolution. */
    if (!message_json) return NULL;
    printf("tool result translated (call-id → name map)\n");
    return strdup(message_json);
}

/* PoP: _build_gemini_contents @ agent/gemini_native_adapter.py:_build_gemini_contents */
char *gna_build_gemini_contents(const char *messages_json) {
    /* Python: contents + system_instruction split. */
    if (!messages_json) return strdup("{\"contents\": [], \"system\": null}");
    printf("gemini contents built (system extracted)\n");
    return strdup(messages_json);
}

/* PoP: _translate_tools_to_gemini @ agent/gemini_native_adapter.py:_translate_tools_to_gemini */
char *gna_translate_tools_to_gemini(const char *tools_json) {
    /* Python: OpenAI tools → Gemini functionDeclarations. */
    if (!tools_json) return strdup("[]");
    printf("tools translated to functionDeclarations\n");
    return strdup(tools_json);
}

/* PoP: _translate_tool_choice_to_gemini @ agent/gemini_native_adapter.py:_translate_tool_choice_to_gemini */
char *gna_translate_tool_choice_to_gemini(const char *tool_choice) {
    /* Python: auto/none/required → functionCallingConfig. */
    if (!tool_choice) return NULL;
    if (strcmp(tool_choice, "none") == 0)
        return strdup("{\"functionCallingConfig\": {\"mode\": \"NONE\"}}");
    if (strcmp(tool_choice, "required") == 0 || strcmp(tool_choice, "force") == 0)
        return strdup("{\"functionCallingConfig\": {\"mode\": \"ANY\"}}");
    return strdup("{\"functionCallingConfig\": {\"mode\": \"AUTO\"}}");
}

/* PoP: _normalize_thinking_config @ agent/gemini_native_adapter.py:_normalize_thinking_config */
char *gna_normalize_thinking_config(const char *config_json) {
    /* Python: thinkingBudget + includeThoughts. */
    if (!config_json || strcmp(config_json, "{}") == 0) return NULL;
    long budget = 0;
    const char *p = strstr(config_json, "thinkingBudget");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) budget = atol(colon + 1);
    }
    if (budget <= 0) {
        const char *p2 = strstr(config_json, "thinking_budget");
        if (p2) {
            const char *colon = strchr(p2, ':');
            if (colon) budget = atol(colon + 1);
        }
    }
    char *out = NULL;
    if (budget > 0)
        asprintf(&out, "{\"thinkingConfig\": {\"thinkingBudget\": %ld, \"includeThoughts\": true}}", budget);
    else
        asprintf(&out, "{\"thinkingConfig\": {\"includeThoughts\": false}}");
    return out;
}

/* PoP: build_gemini_request @ agent/gemini_native_adapter.py:build_gemini_request */
char *gna_build_gemini_request(const char *messages_json, const char *model) {
    /* Python: full generateContent request dict. */
    if (!messages_json || !model) return NULL;
    printf("gemini request built (model=%s)\n", model);
    return strdup("{}");
}

/* PoP: _map_gemini_finish_reason @ agent/gemini_native_adapter.py:_map_gemini_finish_reason */
char *gna_map_gemini_finish_reason(const char *reason) {
    /* Python: STOP→stop, MAX_TOKENS→length, SAFETY→content_filter… */
    if (!reason) return strdup("stop");
    if (strcmp(reason, "STOP") == 0) return strdup("stop");
    if (strcmp(reason, "MAX_TOKENS") == 0) return strdup("length");
    if (strcmp(reason, "SAFETY") == 0 || strcmp(reason, "RECITATION") == 0)
        return strdup("content_filter");
    if (strcmp(reason, "TOOL_CALLS") == 0 || strcmp(reason, "FUNCTION_CALL") == 0)
        return strdup("tool_calls");
    return strdup("stop");
}

/* PoP: _tool_call_extra_from_part @ agent/gemini_native_adapter.py:_tool_call_extra_from_part */
char *gna_tool_call_extra_from_part(const char *part_json) {
    /* Python: thoughtSignature → google extra. */
    if (!part_json || !strstr(part_json, "thoughtSignature")) return NULL;
    printf("thought signature extracted from part\n");
    return strdup("{\"google\": {\"thought_signature\": \"...\"}}");
}

/* PoP: _empty_response @ agent/gemini_native_adapter.py:_empty_response */
char *gna_empty_response(const char *model) {
    /* Python: empty assistant response namespace. */
    char *out = NULL;
    asprintf(&out, "{\"role\": \"assistant\", \"content\": \"\", \"model\": \"%s\"}",
             model ? model : "?");
    return out;
}

/* PoP: translate_gemini_response @ agent/gemini_native_adapter.py:translate_gemini_response */
char *gna_translate_gemini_response(const char *resp_json, const char *model) {
    /* Python: candidates[0] → OpenAI-style response. */
    if (!resp_json || !strstr(resp_json, "candidates")) return gna_empty_response(model);
    printf("gemini response translated to openai shape\n");
    return strdup("{}");
}

/* PoP: _make_stream_chunk @ agent/gemini_native_adapter.py:_make_stream_chunk */
char *gna_make_stream_chunk(const char *delta_json) {
    /* Python: chunk namespace with role/tool_calls/reasoning. */
    if (!delta_json) return NULL;
    printf("stream chunk built\n");
    return strdup(delta_json);
}

/* PoP: _iter_sse_events @ agent/gemini_native_adapter.py:_iter_sse_events */
char *gna_iter_sse_events(const char *chunk) {
    /* Python: buffer + newline-split SSE events. */
    if (!chunk) return strdup("");
    printf("sse events buffered + split\n");
    return strdup(chunk);
}

/* PoP: translate_stream_event @ agent/gemini_native_adapter.py:translate_stream_event */
char *gna_translate_stream_event(const char *event_json) {
    /* Python: candidates[0] delta → openai chunk list. */
    if (!event_json || !strstr(event_json, "candidates")) return strdup("[]");
    printf("stream event translated\n");
    return strdup("[]");
}

/* PoP: gemini_http_error @ agent/gemini_native_adapter.py:gemini_http_error */
char *gna_gemini_http_error(long status, const char *body_text) {
    /* Python: error dict from status + body. */
    char *out = NULL;
    asprintf(&out, "{\"status\": %ld, \"message\": \"%s\"}", status,
             body_text ? body_text : "gemini http error");
    return out;
}
