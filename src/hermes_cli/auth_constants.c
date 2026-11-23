/*
 * auth_constants.c — C port of upstream Hermes hermes_cli/auth_constants.py.
 *
 * PoP: w12_authconsta_eaa516_decode_jwt_claims @ hermes_cli/auth_constants.py:_decode_jwt_claims
 * PoP: w12_authconsta_eaa516_error @ hermes_cli/auth_constants.py:_provider_error_factory
 * PoP: w12_authconsta_eaa516_constants @ hermes_cli/auth_constants.py:<module constants>
 *
 * Self-contained base64url decode (no libbase64 dependency): the oracle links
 * only this object plus libjson, so every helper lives in this translation unit.
 */
#include "noc_w12_authconsta_eaa516.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static int b64val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;                       /* '=' padding and invalid chars discarded */
}

/* Mirrors base64.urlsafe_b64decode after the Python code pads the segment. */
static unsigned char *b64_decode_url(const char *s, size_t *out_len) {
    size_t len = strlen(s);
    unsigned char *out = (unsigned char *)malloc(len + 4);
    if (!out) { *out_len = 0; return NULL; }
    size_t n = 0;
    unsigned int acc = 0;
    int bits = 0;
    for (size_t i = 0; i < len; i++) {
        int v = b64val((unsigned char)s[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned int)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[n++] = (unsigned char)((acc >> bits) & 0xff);
            acc &= 0xffff;           /* keep only the bits still needed */
        }
    }
    out[n] = '\0';                   /* NUL at the NEW length */
    *out_len = n;
    return out;
}

struct json_t *w12_authconsta_eaa516_decode_jwt_claims(const char *token) {
    if (!token) return json_object();
    const char *d1 = strchr(token, '.');
    if (!d1) return json_object();
    const char *d2 = strchr(d1 + 1, '.');
    if (!d2 || strchr(d2 + 1, '.')) return json_object();  /* count('.') != 2 */
    size_t plen = (size_t)(d2 - (d1 + 1));
    char seg[8192];
    if (plen >= sizeof(seg)) return json_object();
    memcpy(seg, d1 + 1, plen);
    seg[plen] = '\0';
    size_t rawlen = 0;
    unsigned char *raw = b64_decode_url(seg, &rawlen);
    if (!raw) return json_object();
    char text[8192];
    if (rawlen >= sizeof(text)) { free(raw); return json_object(); }
    memcpy(text, raw, rawlen);
    text[rawlen] = '\0';
    free(raw);
    json_t *claims = json_parse(text, NULL);
    if (!claims || claims->type != JSON_OBJECT) {
        json_free(claims);
        return json_object();
    }
    return claims;
}

struct json_t *w12_authconsta_eaa516_error(const char *provider, const char *message,
                                           const json_t *code, int relogin) {
    json_t *o = json_object();
    json_set(o, "message", json_string(message ? message : ""));
    json_set(o, "provider", json_string(provider ? provider : ""));
    json_set(o, "code", (code && code->type == JSON_STRING) ? json_string(code->str_val) : json_null());
    json_set(o, "relogin_required", json_bool(relogin ? true : false));
    json_set(o, "retry_after", json_null());
    json_set(o, "retryable", json_null());
    return o;
}

struct json_t *w12_authconsta_eaa516_constants(void) {
    json_t *o = json_object();
    json_set(o, "ACCESS_TOKEN_REFRESH_SKEW_SECONDS", json_number(120.0));
    json_set(o, "ACTUAL_LOCAL_NOAUTH_PLACEHOLDER", json_string("dummy-actual-local-api-key"));
    json_set(o, "AUTH_LOCK_TIMEOUT_SECONDS", json_number(15.0));
    json_set(o, "AUTH_STORE_VERSION", json_number(1.0));
    json_set(o, "CODEX_ACCESS_TOKEN_REFRESH_SKEW_SECONDS", json_number(120.0));
    json_set(o, "CODEX_OAUTH_CLIENT_ID", json_string("app_EMoamEEZ73f0CkXaXp7hrann"));
    json_set(o, "CODEX_OAUTH_TOKEN_URL", json_string("https://auth.openai.com/oauth/token"));
    json_set(o, "CODEX_RATE_LIMITED_CODE", json_string("codex_rate_limited"));
    json_set(o, "DEFAULT_ACTUAL_BASE_URL", json_string("https://api.actual.inc/v1"));
    json_set(o, "DEFAULT_ACTUAL_LOCAL_BASE_URL", json_string("http://127.0.0.1:8080/v1"));
    json_set(o, "DEFAULT_CODEX_BASE_URL", json_string("https://chatgpt.com/backend-api/codex"));
    json_set(o, "DEFAULT_NOUS_CLIENT_ID", json_string("hermes-cli"));
    json_set(o, "DEFAULT_NOUS_INFERENCE_URL", json_string("https://inference-api.nousresearch.com/v1"));
    json_set(o, "DEFAULT_NOUS_PORTAL_URL", json_string("https://portal.nousresearch.com"));
    json_set(o, "DEFAULT_NOUS_SCOPE", json_string("inference:invoke"));
    json_set(o, "DEFAULT_NOUS_WELCOME_URL", json_string("https://welcome-api.nousresearch.com/v1"));
    json_set(o, "DEFAULT_OLLAMA_CLOUD_BASE_URL", json_string("https://ollama.com/v1"));
    json_set(o, "DEFAULT_QWEN_BASE_URL", json_string("https://portal.qwen.ai/v1"));
    json_set(o, "DEFAULT_XAI_OAUTH_BASE_URL", json_string("https://api.x.ai/v1"));
    json_set(o, "DEVICE_AUTH_POLL_INTERVAL_CAP_SECONDS", json_number(1.0));
    json_set(o, "DEVICE_CODE_GRANT_TYPE", json_string("urn:ietf:params:oauth:grant-type:device_code"));
    json_set(o, "LMSTUDIO_NOAUTH_PLACEHOLDER", json_string("dummy-lm-api-key"));
    json_set(o, "MINIMAX_OAUTH_CLIENT_ID", json_string("78257093-7e40-4613-99e0-527b14b39113"));
    json_set(o, "MINIMAX_OAUTH_CN_BASE", json_string("https://api.minimaxi.com"));
    json_set(o, "MINIMAX_OAUTH_CN_INFERENCE", json_string("https://api.minimaxi.com/anthropic"));
    json_set(o, "MINIMAX_OAUTH_GLOBAL_BASE", json_string("https://api.minimax.io"));
    json_set(o, "MINIMAX_OAUTH_GLOBAL_INFERENCE", json_string("https://api.minimax.io/anthropic"));
    json_set(o, "MINIMAX_OAUTH_GRANT_TYPE", json_string("urn:ietf:params:oauth:grant-type:user_code"));
    json_set(o, "MINIMAX_OAUTH_REFRESH_SKEW_SECONDS", json_number(60.0));
    json_set(o, "MINIMAX_OAUTH_SCOPE", json_string("group_id profile model.completion"));
    json_set(o, "NOUS_AUTH_PATH_INVOKE_JWT", json_string("invoke_jwt"));
    json_set(o, "NOUS_BILLING_MANAGE_SCOPE", json_string("billing:manage"));
    json_set(o, "NOUS_DEVICE_CODE_SOURCE", json_string("device_code"));
    json_set(o, "NOUS_INFERENCE_INVOKE_SCOPE", json_string("inference:invoke"));
    json_set(o, "NOUS_INVOKE_JWT_MIN_TTL_SECONDS", json_number(120.0));
    json_set(o, "OAUTH_OVER_SSH_DOCS_URL", json_string("https://hermes-agent.nousresearch.com/docs/guides/oauth-over-ssh"));
    json_set(o, "OPENROUTER_AUTH_KEYS_URL", json_string("https://openrouter.ai/api/v1/auth/keys"));
    json_set(o, "OPENROUTER_AUTH_URL", json_string("https://openrouter.ai/auth"));
    json_set(o, "OPENROUTER_OAUTH_DOCS_URL", json_string("https://openrouter.ai/docs/guides/overview/auth/oauth"));
    json_set(o, "QWEN_ACCESS_TOKEN_REFRESH_SKEW_SECONDS", json_number(120.0));
    json_set(o, "QWEN_OAUTH_CLIENT_ID", json_string("f0304373b74a44d2b584a3fb70ca9e56"));
    json_set(o, "QWEN_OAUTH_TOKEN_URL", json_string("https://chat.qwen.ai/api/v1/oauth2/token"));
    json_set(o, "STEPFUN_STEP_PLAN_CN_BASE_URL", json_string("https://api.stepfun.com/step_plan/v1"));
    json_set(o, "STEPFUN_STEP_PLAN_INTL_BASE_URL", json_string("https://api.stepfun.ai/step_plan/v1"));
    json_set(o, "TYPE_CHECKING", json_bool(false));
    json_set(o, "WELCOME_CLAIM_FLAG", json_string("welcome_claim"));
    json_set(o, "XAI_ACCESS_TOKEN_REFRESH_SKEW_SECONDS", json_number(3600.0));
    json_set(o, "XAI_OAUTH_CLIENT_ID", json_string("b1a00492-073a-47ea-816f-4c329264a828"));
    json_set(o, "XAI_OAUTH_DEVICE_CODE_URL", json_string("https://auth.x.ai/oauth2/device/code"));
    json_set(o, "XAI_OAUTH_DISCOVERY_URL", json_string("https://auth.x.ai/.well-known/openid-configuration"));
    json_set(o, "XAI_OAUTH_ISSUER", json_string("https://auth.x.ai"));
    json_set(o, "XAI_OAUTH_SCOPE", json_string("openid profile email offline_access grok-cli:access api:access"));
    return o;
}
