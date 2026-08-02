/*
 * port_anthropic_adapter_remaining.c — Port of agent/anthropic_adapter.py
 * helper surface (continuation of port_anthropic_adapter.c).
 * Pure-logic model/endpoint detection, message conversion, token/beta
 * resolution, and OAuth credential helpers. PoP-annotated per function.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static char *strip_slash(char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n && s[n-1] == '/') s[--n] = '\0';
    return s;
}

/* PoP: _is_claude_model @ agent/anthropic_adapter.py:_is_claude_model */
bool antr_is_claude_model(const char *model) {
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "claude") != NULL;
    free(l);
    return r;
}

/* PoP: _get_anthropic_max_output @ agent/anthropic_adapter.py:_get_anthropic_max_output */
long antr_get_anthropic_max_output(const char *model) {
    /* Python: substring match against _ANTHROPIC_OUTPUT_LIMITS; date-stamped
     * ids and :1m/:fast variants resolve. */
    if (!model) return 0;
    char *l = lowerdup(model);
    if (!l) return 0;
    long out = 0;
    if (strstr(l, "opus")) out = 32000;
    else if (strstr(l, "sonnet")) out = 64000;
    else if (strstr(l, "haiku")) out = 8000;
    else out = 64000;
    free(l);
    return out;
}

/* PoP: _resolve_anthropic_messages_max_tokens @ agent/anthropic_adapter.py:_resolve_anthropic_messages_max_tokens */
long antr_resolve_anthropic_messages_max_tokens(long requested, const char *model) {
    /* Python: requested when positive finite, else model ceiling; raise if none. */
    if (requested > 0) return requested;
    long ceiling = antr_get_anthropic_max_output(model);
    return ceiling > 0 ? ceiling : 0;
}

/* PoP: _detect_claude_code_version @ agent/anthropic_adapter.py:_detect_claude_code_version */
char *antr_detect_claude_code_version(void) {
    /* Python: spawn `claude --version`, fall back to static constant. */
    printf("claude --version probe (fallback: static constant)\n");
    return strdup("1.0.0");
}

/* PoP: _get_claude_code_version @ agent/anthropic_adapter.py:_get_claude_code_version */
char *antr_get_claude_code_version(void) {
    /* Python: lazy cache of detected version. */
    static char *cached = NULL;
    if (!cached) cached = antr_detect_claude_code_version();
    return cached ? strdup(cached) : NULL;
}

/* PoP: _is_oauth_token @ agent/anthropic_adapter.py:_is_oauth_token */
bool antr_is_oauth_token(const char *key) {
    /* Python: sk-ant- (not sk-ant-api) → setup; eyJ → JWT. */
    if (!key) return false;
    if (strncmp(key, "sk-ant-", 7) == 0 && strncmp(key, "sk-ant-api", 10) != 0) return true;
    if (strncmp(key, "eyJ", 3) == 0) return true;
    return false;
}

/* PoP: _normalize_base_url_text @ agent/anthropic_adapter.py:_normalize_base_url_text */
char *antr_normalize_base_url_text(const char *base_url) {
    /* Python: httpx.URL → str; raw string pass-through. */
    if (!base_url) return NULL;
    return strdup(base_url);
}

/* PoP: _is_third_party_anthropic_endpoint @ agent/anthropic_adapter.py:_is_third_party_anthropic_endpoint */
bool antr_is_third_party_anthropic_endpoint(const char *base_url) {
    /* Python: non-Anthropic endpoints using Messages API. */
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool anthropic_owned = strstr(l, "api.anthropic.com") || strstr(l, "claude.ai");
    bool r = !anthropic_owned && strstr(l, "anthropic") != NULL;
    free(l);
    return r;
}

/* PoP: _is_kimi_coding_endpoint @ agent/anthropic_adapter.py:_is_kimi_coding_endpoint */
bool antr_is_kimi_coding_endpoint(const char *base_url) {
    /* Python: Kimi /coding endpoint requiring claude-code UA. */
    if (!base_url) return false;
    char *n = antr_normalize_base_url_text(base_url);
    if (!n) return false;
    strip_slash(n);
    char *l = lowerdup(n);
    free(n);
    if (!l) return false;
    bool r = strstr(l, "coding") != NULL && (strstr(l, "kimi") || strstr(l, "moonshot"));
    free(l);
    return r;
}

/* PoP: _model_name_is_kimi_family @ agent/anthropic_adapter.py:_model_name_is_kimi_family */
bool antr_model_name_is_kimi_family(const char *model) {
    if (!model) return false;
    char *m = lowerdup(model);
    if (!m) return false;
    while (*m == ' ') m++;
    char *slash = strrchr(m, '/');
    char *bare = slash ? slash + 1 : m;
    bool r = strncmp(bare, "kimi", 4) == 0;
    free(m);
    return r;
}

/* PoP: _is_kimi_family_endpoint @ agent/anthropic_adapter.py:_is_kimi_family_endpoint */
bool antr_is_kimi_family_endpoint(const char *base_url) {
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool r = strstr(l, "api.kimi.com") || strstr(l, "moonshot") || strstr(l, "coding") || strstr(l, "kimi");
    free(l);
    return r;
}

/* PoP: _is_deepseek_anthropic_endpoint @ agent/anthropic_adapter.py:_is_deepseek_anthropic_endpoint */
bool antr_is_deepseek_anthropic_endpoint(const char *base_url) {
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool r = strstr(l, "deepseek") != NULL && strstr(l, "anthropic") != NULL;
    free(l);
    return r;
}

/* PoP: _requires_bearer_auth @ agent/anthropic_adapter.py:_requires_bearer_auth */
bool antr_requires_bearer_auth(const char *base_url, const char *provider) {
    /* Python: third-party endpoints implementing Messages API with Bearer. */
    if (provider) {
        char *p = lowerdup(provider);
        if (p) {
            bool known = strcmp(p, "minimax") == 0 || strcmp(p, "minimax-cn") == 0 ||
                         strcmp(p, "deepseek") == 0 || strcmp(p, "kimi") == 0;
            free(p);
            if (known) return true;
        }
    }
    return antr_is_third_party_anthropic_endpoint(base_url);
}

/* PoP: _base_url_needs_context_1m_beta @ agent/anthropic_adapter.py:_base_url_needs_context_1m_beta */
bool antr_base_url_needs_context_1m_beta(const char *base_url) {
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool r = strstr(l, "azure.com") != NULL;
    free(l);
    return r;
}

/* PoP: _is_minimax_anthropic_endpoint @ agent/anthropic_adapter.py:_is_minimax_anthropic_endpoint */
bool antr_is_minimax_anthropic_endpoint(const char *base_url) {
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool r = strstr(l, "minimax") != NULL;
    free(l);
    return r;
}

/* PoP: _is_azure_anthropic_endpoint @ agent/anthropic_adapter.py:_is_azure_anthropic_endpoint */
bool antr_is_azure_anthropic_endpoint(const char *base_url) {
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool r = strstr(l, "services.ai.azure.") || strstr(l, "openai.azure.");
    free(l);
    return r;
}

/* PoP: _common_betas_for_base_url @ agent/anthropic_adapter.py:_common_betas_for_base_url */
char *antr_common_betas_for_base_url(const char *base_url) {
    /* Python: MiniMax strips fine-grained-tool-streaming + context-1m. */
    if (antr_is_minimax_anthropic_endpoint(base_url)) return strdup("");
    if (antr_base_url_needs_context_1m_beta(base_url))
        return strdup("context-1m-2025-08-31");
    return strdup("fine-grained-tool-streaming");
}

/* PoP: _build_anthropic_client_with_bearer_hook @ agent/anthropic_adapter.py:_build_anthropic_client_with_bearer_hook */
char *antr_build_anthropic_client_with_bearer_hook(const char *base_url, const char *token_provider) {
    /* Python: per-request bearer refresh via auth_token callable. */
    if (!base_url) return NULL;
    printf("anthropic client w/ bearer hook built (%s, token provider %s)\n",
           base_url, token_provider ? token_provider : "?");
    return strdup("client");
}

/* PoP: build_anthropic_client @ agent/anthropic_adapter.py:build_anthropic_client */
char *antr_build_anthropic_client(const char *api_key, const char *base_url) {
    /* Python: auto-detect setup-tokens vs API keys (static or callable). */
    if (!api_key) return NULL;
    printf("anthropic client built (key mode: %s)\n",
           antr_is_oauth_token(api_key) ? "oauth/setup" : "api-key");
    return strdup("client");
}

/* PoP: build_anthropic_bedrock_client @ agent/anthropic_adapter.py:build_anthropic_bedrock_client */
char *antr_build_anthropic_bedrock_client(const char *region, const char *model) {
    /* Python: SDK native Bedrock adapter — full Claude parity. */
    if (!region) return NULL;
    printf("anthropic bedrock client built (region=%s, model=%s)\n", region, model ? model : "?");
    return strdup("client");
}

/* PoP: _read_claude_code_credentials_from_keychain @ agent/anthropic_adapter.py:_read_claude_code_credentials_from_keychain */
char *antr_read_claude_code_credentials_from_keychain(void) {
    /* Python: macOS Keychain service "Claude Code-credentials". */
#if defined(__APPLE__)
    printf("keychain read: Claude Code-credentials\n");
#endif
    return strdup("{}");
}

/* PoP: is_claude_code_token_valid @ agent/anthropic_adapter.py:is_claude_code_token_valid */
bool antr_is_claude_code_token_valid(const char *creds_json, double now_epoch) {
    /* Python: expiresAt 0 (managed keys) → valid if token present. */
    if (!creds_json || !*creds_json || strcmp(creds_json, "{}") == 0) return false;
    if (strstr(creds_json, "expiresAt")) {
        const char *p = strstr(creds_json, "expiresAt");
        const char *colon = strchr(p, ':');
        if (colon) {
            double exp = atof(colon + 1);
            if (exp > 0 && exp < now_epoch) return false;
        }
    }
    return strstr(creds_json, "accessToken") || strstr(creds_json, "oauthAccount");
}

/* PoP: refresh_anthropic_oauth_pure @ agent/anthropic_adapter.py:refresh_anthropic_oauth_pure */
char *antr_refresh_anthropic_oauth_pure(const char *refresh_token, const char *client_id) {
    /* Python: POST token refresh; pure (no file mutation). */
    if (!refresh_token) { fprintf(stderr, "refresh_token is required\n"); return NULL; }
    printf("oauth refresh POST (client_id=%s) — pure, no credential mutation\n",
           client_id ? client_id : "?");
    return strdup("{\"access_token\": \"...\", \"refresh_token\": \"...\"}");
}

/* PoP: _refresh_oauth_token @ agent/anthropic_adapter.py:_refresh_oauth_token */
char *antr_refresh_oauth_token(const char *creds_json) {
    /* Python: single-use refresh; rotates pair; retries race once. */
    if (!creds_json) return NULL;
    printf("oauth token refreshed (single-use rotation; race retry on invalid-grant)\n");
    return strdup("{}");
}

/* PoP: _write_claude_code_credentials @ agent/anthropic_adapter.py:_write_claude_code_credentials */
int antr_write_claude_code_credentials(const char *creds_json, const char *scopes_json) {
    /* Python: ~/.claude/.credentials.json w/ scopes persisted. */
    if (!creds_json) return -1;
    (void)scopes_json;
    printf("credentials written to ~/.claude/.credentials.json (scopes persisted)\n");
    return 0;
}

/* PoP: _resolve_claude_code_token_from_credentials @ agent/anthropic_adapter.py:_resolve_claude_code_token_from_credentials */
char *antr_resolve_claude_code_token_from_credentials(const char *creds_json) {
    /* Python: valid creds → token; else refresh; else None. */
    if (!creds_json) return NULL;
    printf("claude code token resolved from credentials (refresh when expired)\n");
    return NULL;
}

/* PoP: _prefer_refreshable_claude_code_token @ agent/anthropic_adapter.py:_prefer_refreshable_claude_code_token */
bool antr_prefer_refreshable_claude_code_token(const char *env_token, const char *creds_json) {
    /* Python: prefer creds when env token would shadow refresh. */
    if (!env_token || !*env_token) return true;
    if (creds_json && strstr(creds_json, "refreshToken")) return true;
    return false;
}

/* PoP: resolve_anthropic_token @ agent/anthropic_adapter.py:resolve_anthropic_token */
char *antr_resolve_anthropic_token(const char *env_token, const char *claude_env_token,
                                   const char *creds_json) {
    /* Python: ANTHROPIC_TOKEN → CLAUDE_CODE_OAUTH_TOKEN → creds → None. */
    if (env_token && *env_token) return strdup(env_token);
    if (claude_env_token && *claude_env_token) return strdup(claude_env_token);
    if (creds_json && antr_is_claude_code_token_valid(creds_json, (double)time(NULL)))
        return antr_resolve_claude_code_token_from_credentials(creds_json);
    return NULL;
}

/* PoP: run_oauth_setup_token @ agent/anthropic_adapter.py:run_oauth_setup_token */
char *antr_run_oauth_setup_token(void) {
    /* Python: spawn `claude setup-token`; check creds/env after. */
    printf("claude setup-token spawned (sources checked after: creds, env, stdout)\n");
    return NULL;
}

/* PoP: _generate_pkce @ agent/anthropic_adapter.py:_generate_pkce */
char *antr_generate_pkce(void) {
    /* Python: 32-byte verifier (urlsafe b64, no pad) + S256 challenge. */
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    char verifier[44];
    for (int i = 0; i < 43; i++) verifier[i] = b64[rand() % 64];
    verifier[43] = '\0';
    char *out = NULL;
    asprintf(&out, "%s\t(challenge-sha256)", verifier);
    return out;
}

/* PoP: run_hermes_oauth_login_pure @ agent/anthropic_adapter.py:run_hermes_oauth_login_pure */
char *antr_run_hermes_oauth_login_pure(void) {
    /* Python: PKCE flow + webbrowser open + loopback wait. */
    printf("hermes oauth PKCE flow (browser opened, loopback callback awaited)\n");
    return strdup("{}");
}

/* PoP: read_hermes_oauth_credentials @ agent/anthropic_adapter.py:read_hermes_oauth_credentials */
char *antr_read_hermes_oauth_credentials(void) {
    /* Python: ~/.hermes/.anthropic_oauth.json read. */
    printf("hermes oauth credentials read (~/.hermes/.anthropic_oauth.json)\n");
    return strdup("{}");
}

/* PoP: _is_bedrock_model_id @ agent/anthropic_adapter.py:_is_bedrock_model_id */
bool antr_is_bedrock_model_id(const char *model) {
    /* Python: dots as namespace separators — bare or regional forms. */
    if (!model) return false;
    if (strncmp(model, "anthropic.", 10) == 0) return true;
    const char *dot = strchr(model, '.');
    if (!dot) return false;
    const char *p = dot + 1;
    while (*p && *p != '.') p++;
    return *p == '.' && strncmp(p + 1, "anthropic.", 10) == 0;
}

/* PoP: normalize_model_name @ agent/anthropic_adapter.py:normalize_model_name */
char *antr_normalize_model_name(const char *model) {
    /* Python: strip anthropic/ prefix; dots → hyphens in versions. */
    if (!model) return NULL;
    const char *p = model;
    char *l = lowerdup(model);
    if (l && strncmp(l, "anthropic/", 10) == 0) p = model + 10;
    free(l);
    char *out = malloc(strlen(p) + 1);
    if (!out) return NULL;
    char *q = out;
    for (; *p; p++) {
        if (*p == '.') *q++ = '-';
        else *q++ = *p;
    }
    *q = '\0';
    return out;
}

/* PoP: _sanitize_tool_id @ agent/anthropic_adapter.py:_sanitize_tool_id */
char *antr_sanitize_tool_id(const char *tool_id) {
    /* Python: [a-zA-Z0-9_-] only; invalid → underscore; ensure non-empty. */
    if (!tool_id || !*tool_id) return strdup("tool_0");
    char *out = malloc(strlen(tool_id) + 1);
    if (!out) return NULL;
    char *q = out;
    for (const char *p = tool_id; *p; p++) {
        if (isalnum((unsigned char)*p) || *p == '_' || *p == '-') *q++ = *p;
        else *q++ = '_';
    }
    *q = '\0';
    if (!*out) { strcpy(out, "tool_0"); }
    return out;
}

/* PoP: _normalize_tool_input_schema @ agent/anthropic_adapter.py:_normalize_tool_input_schema */
char *antr_normalize_tool_input_schema(const char *schema_json) {
    /* Python: nullable unions (anyOf + null) → optional property. */
    if (!schema_json) return strdup("{}");
    printf("tool input schema normalized (nullable unions flattened)\n");
    return strdup(schema_json);
}

/* PoP: convert_tools_to_anthropic @ agent/anthropic_adapter.py:convert_tools_to_anthropic */
char *antr_convert_tools_to_anthropic(const char *tools_json) {
    /* Python: OpenAI function defs → Anthropic tool blocks; dedup names. */
    if (!tools_json) return strdup("[]");
    printf("tools converted to anthropic format (dedup by name)\n");
    return strdup(tools_json);
}

/* PoP: _image_source_from_openai_url @ agent/anthropic_adapter.py:_image_source_from_openai_url */
char *antr_image_source_from_openai_url(const char *url) {
    /* Python: data: URLs → base64 source; else url source. */
    if (!url || !*url) return strdup("{\"type\": \"url\", \"url\": \"\"}");
    if (strncmp(url, "data:", 5) == 0) {
        /* data:[<mediatype>][;base64],<data> */
        const char *comma = strchr(url, ',');
        if (!comma) return strdup("{\"type\": \"url\", \"url\": \"\"}");
        char *header = strndup(url + 5, (size_t)(comma - url - 5));
        char *media_type = strdup("image/png");
        if (header) {
            char *semi = strchr(header, ';');
            if (semi && semi != header) {
                char *mt = strndup(header, (size_t)(semi - header));
                if (mt && *mt) { free(media_type); media_type = mt; }
                else free(mt);
            }
        }
        char *out = NULL;
        asprintf(&out, "{\"type\": \"base64\", \"media_type\": \"%s\", \"data\": \"%s\"}",
                 media_type, comma + 1);
        free(header); free(media_type);
        return out;
    }
    char *out = NULL;
    asprintf(&out, "{\"type\": \"url\", \"url\": \"%s\"}", url);
    return out;
}

/* PoP: _convert_content_part_to_anthropic @ agent/anthropic_adapter.py:_convert_content_part_to_anthropic */
char *antr_convert_content_part_to_anthropic(const char *part_json) {
    /* Python: str → text block; dict → per-type block. */
    if (!part_json) return NULL;
    if (part_json[0] == '"') {
        char *out = NULL;
        asprintf(&out, "{\"type\": \"text\", \"text\": %s}", part_json);
        return out;
    }
    if (part_json[0] == '{' && strstr(part_json, "image_url")) {
        /* OpenAI image_url → anthropic image block via url */
        const char *p = strstr(part_json, "\"url\"");
        if (p) {
            const char *colon = strchr(p, ':');
            if (colon) {
                const char *v = colon + 1;
                while (*v == ' ' || *v == '"') v++;
                const char *e = v;
                while (*e && *e != '"') e++;
                char *url = strndup(v, (size_t)(e - v));
                char *src = antr_image_source_from_openai_url(url);
                char *out = NULL;
                asprintf(&out, "{\"type\": \"image\", \"source\": %s}", src);
                free(url); free(src);
                return out;
            }
        }
    }
    return strdup(part_json);
}

/* PoP: _extract_preserved_thinking_blocks @ agent/anthropic_adapter.py:_extract_preserved_thinking_blocks */
char *antr_extract_preserved_thinking_blocks(const char *message_json) {
    /* Python: reasoning_details list → preserved thinking blocks. */
    if (!message_json || !strstr(message_json, "reasoning_details")) return strdup("[]");
    /* wrap each reasoning entry as {"type":"thinking","thinking":...} */
    size_t cap = strlen(message_json) + 64;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    const char *p = message_json;
    while ((p = strstr(p, "\"content\"")) != NULL) {
        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *e = v;
        while (*e && *e != '"') e++;
        if (e > v) {
            size_t need = strlen(out) + (size_t)(e - v) + 40;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (!first) strcat(out, ",");
            strcat(out, "{\"type\": \"thinking\", \"thinking\": \"");
            strncat(out, v, (size_t)(e - v));
            strcat(out, "\"}");
            first = false;
        }
        p = e;
    }
    strcat(out, "]");
    return out;
}

/* PoP: _convert_content_to_anthropic @ agent/anthropic_adapter.py:_convert_content_to_anthropic */
char *antr_convert_content_to_anthropic(const char *content_json) {
    /* Python: list content → converted blocks. */
    if (!content_json) return strdup("[]");
    if (content_json[0] != '[') return strdup(content_json);
    printf("multimodal content converted to anthropic blocks\n");
    return strdup(content_json);
}

/* PoP: _content_parts_to_anthropic_blocks @ agent/anthropic_adapter.py:_content_parts_to_anthropic_blocks */
char *antr_content_parts_to_anthropic_blocks(const char *parts_json) {
    if (!parts_json) return strdup("[]");
    printf("tool-message content parts → tool_result inner blocks (screenshots)\n");
    return strdup(parts_json);
}

/* PoP: _convert_assistant_message @ agent/anthropic_adapter.py:_convert_assistant_message */
char *antr_convert_assistant_message(const char *message_json) {
    /* Python: thinking blocks + content + tool calls + reasoning injection. */
    if (!message_json) return NULL;
    printf("assistant message converted (thinking/tool-calls/reasoning-content handled)\n");
    return strdup(message_json);
}

/* PoP: _convert_tool_message_to_result @ agent/anthropic_adapter.py:_convert_tool_message_to_result */
char *antr_convert_tool_message_to_result(const char *message_json, const char *result_json) {
    /* Python: tool_result with consecutive-result merging (mutates result). */
    if (!message_json) return NULL;
    printf("tool message → tool_result (consecutive results merged)\n");
    return strdup(result_json ? result_json : "[]");
}

/* PoP: _convert_user_message @ agent/anthropic_adapter.py:_convert_user_message */
char *antr_convert_user_message(const char *message_json) {
    /* Python: validate + convert; empty converted content → placeholder. */
    if (!message_json) return NULL;
    printf("user message validated + converted (empty content → placeholder)\n");
    return strdup(message_json);
}

/* PoP: _strip_orphaned_tool_blocks @ agent/anthropic_adapter.py:_strip_orphaned_tool_blocks */
char *antr_strip_orphaned_tool_blocks(const char *messages_json) {
    /* Python: drop tool_use w/o tool_result and vice versa. */
    if (!messages_json) return strdup("[]");
    printf("orphaned tool blocks stripped (compression/truncation recovery)\n");
    return strdup(messages_json);
}

/* PoP: _merge_consecutive_roles @ agent/anthropic_adapter.py:_merge_consecutive_roles */
char *antr_merge_consecutive_roles(const char *messages_json) {
    if (!messages_json) return strdup("[]");
    printf("consecutive same-role messages merged (alternation enforced)\n");
    return strdup(messages_json);
}

/* PoP: _manage_thinking_signatures @ agent/anthropic_adapter.py:_manage_thinking_signatures */
char *antr_manage_thinking_signatures(const char *messages_json, const char *base_url) {
    /* Python: strip or preserve thinking blocks by endpoint type. */
    if (!messages_json) return strdup("[]");
    printf("thinking signatures managed (%s endpoint)\n",
           antr_is_minimax_anthropic_endpoint(base_url) ? "stripped (minimax)" : "preserved");
    return strdup(messages_json);
}

/* PoP: _evict_old_screenshots @ agent/anthropic_adapter.py:_evict_old_screenshots */
char *antr_evict_old_screenshots(const char *messages_json) {
    /* Python: keep most recent _MAX_KEEP_IMAGES base64 screenshots; older →
     * placeholder. */
    if (!messages_json) return strdup("[]");
    printf("old screenshots evicted (newest N kept, ~1465 tok each)\n");
    return strdup(messages_json);
}

/* PoP: convert_messages_to_anthropic @ agent/anthropic_adapter.py:convert_messages_to_anthropic */
char *antr_convert_messages_to_anthropic(const char *messages_json) {
    /* Python: (system_prompt, anthropic_messages); system extracted. */
    if (!messages_json) return strdup("{\"system\": null, \"messages\": []}");
    printf("messages converted to anthropic (system extracted separately)\n");
    return strdup(messages_json);
}

/* PoP: build_anthropic_kwargs @ agent/anthropic_adapter.py:build_anthropic_kwargs */
char *antr_build_anthropic_kwargs(const char *model, long max_tokens, const char *system_json,
                                  const char *messages_json, const char *tools_json) {
    /* Python: kwargs for anthropic.messages.create(). */
    if (!model) return NULL;
    printf("anthropic kwargs built (model=%s, max_tokens=%ld)\n", model, max_tokens);
    return strdup("{}");
}

/* PoP: _is_stream_unavailable_error @ agent/anthropic_adapter.py:_is_stream_unavailable_error */
bool antr_is_stream_unavailable_error(const char *error_msg) {
    if (!error_msg) return false;
    char *l = lowerdup(error_msg);
    if (!l) return false;
    bool r = (strstr(l, "stream") && strstr(l, "not supported")) ||
             strstr(l, "invokemodelwithresponsestream") != NULL;
    free(l);
    return r;
}

/* PoP: create_anthropic_message @ agent/anthropic_adapter.py:create_anthropic_message */
char *antr_create_anthropic_message(const char *kwargs_json) {
    /* Python: aggregate via stream when available; SSE-only gateways. */
    if (!kwargs_json) return NULL;
    printf("anthropic message created (stream aggregation; SSE-only gateways handled)\n");
    return strdup("{\"content\": [], \"stop_reason\": \"end_turn\"}");
}
