/**
 * port_nous_billing.c — Port of Python: cli.py (Nous billing helpers)
 *
 * Real C implementations for Nous billing portal HTTP calls.
 */

#ifndef SRC_CLI_PORT_NOUS_BILLING_C
#define SRC_CLI_PORT_NOUS_BILLING_C

#include "hermes_logger.h"
#include "libhttp/http.h"
#include "hermes_json.h"
#include "hermes_billing.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

/* ================================================================
 *  Internal: HTTP helpers via curl popen
 * ================================================================ */

/* PoP: http_post @ tools/browser_camofox.py:_post */
static char *http_post(const char *url, const char *auth_header, const char *json_body) {
    if (!url) return NULL;

    char resp_path[256];
    snprintf(resp_path, sizeof(resp_path),
             "/tmp/billing_post_%d_%ld.json", getpid(), (long)time(NULL));

    /* Escape JSON body for shell */
    size_t body_len = json_body ? strlen(json_body) : 0;
    char *escaped = malloc(body_len * 2 + 1);
    if (!escaped) return strdup("{\"error\":\"oom\"}");
    size_t j = 0;
    for (size_t i = 0; i < body_len; i++) {
        if (json_body[i] == '\'' || json_body[i] == '\"' || json_body[i] == '\\' || json_body[i] == '$' || json_body[i] == '`') {
            escaped[j++] = '\\';
        }
        escaped[j++] = json_body[i];
    }
    escaped[j] = '\0';

    /* Build curl command */
    char cmd[16384];
    int n = snprintf(cmd, sizeof(cmd),
        "curl -s -w '\\n%%{http_code}' -X POST '%s' "
        "-H 'Content-Type: application/json' "
        "-H 'Authorization: %s' "
        "-d '%s' "
        "> '%s' 2>/dev/null",
        url, auth_header ? auth_header : "", escaped, resp_path);
    free(escaped);

    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        unlink(resp_path);
        return strdup("{\"error\":\"command too long\"}");
    }

    int ret = system(cmd);
    if (ret != 0) {
        unlink(resp_path);
        return strdup("{\"error\":\"curl command failed\"}");
    }

    /* Read response */
    FILE *f = fopen(resp_path, "r");
    if (!f) {
        return strdup("{\"error\":\"failed to read response\"}");
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 1024 * 1024) {
        fclose(f);
        unlink(resp_path);
        return strdup("{\"error\":\"invalid response size\"}");
    }

    char *buf = malloc(fsize + 1);
    if (!buf) {
        fclose(f);
        unlink(resp_path);
        return strdup("{\"error\":\"oom\"}");
    }
    fread(buf, 1, fsize, f);
    buf[fsize] = '\0';
    fclose(f);
    unlink(resp_path);

    /* Split body and HTTP code (last line) */
    char *last_nl = strrchr(buf, '\n');
    if (last_nl) {
        *last_nl = '\0';
    }
    return buf;
}

/* PoP: http_patch @ gateway/platforms/yuanbao.py:_patch */
static char *http_patch(const char *url, const char *auth_header, const char *json_body) {
    if (!url) return NULL;

    char resp_path[256];
    snprintf(resp_path, sizeof(resp_path),
             "/tmp/billing_patch_%d_%ld.json", getpid(), (long)time(NULL));

    /* Escape JSON body for shell */
    size_t body_len = json_body ? strlen(json_body) : 0;
    char *escaped = malloc(body_len * 2 + 1);
    if (!escaped) return strdup("{\"error\":\"oom\"}");
    size_t j = 0;
    for (size_t i = 0; i < body_len; i++) {
        if (json_body[i] == '\'' || json_body[i] == '\"' || json_body[i] == '\\' || json_body[i] == '$' || json_body[i] == '`') {
            escaped[j++] = '\\';
        }
        escaped[j++] = json_body[i];
    }
    escaped[j] = '\0';

    /* Build curl command */
    char cmd[16384];
    int n = snprintf(cmd, sizeof(cmd),
        "curl -s -w '\\n%%{http_code}' -X PATCH '%s' "
        "-H 'Content-Type: application/json' "
        "-H 'Authorization: %s' "
        "-d '%s' "
        "> '%s' 2>/dev/null",
        url, auth_header ? auth_header : "", escaped, resp_path);
    free(escaped);

    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        unlink(resp_path);
        return strdup("{\"error\":\"command too long\"}");
    }

    int ret = system(cmd);
    if (ret != 0) {
        unlink(resp_path);
        return strdup("{\"error\":\"curl command failed\"}");
    }

    /* Read response */
    FILE *f = fopen(resp_path, "r");
    if (!f) {
        return strdup("{\"error\":\"failed to read response\"}");
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 1024 * 1024) {
        fclose(f);
        unlink(resp_path);
        return strdup("{\"error\":\"invalid response size\"}");
    }

    char *buf = malloc(fsize + 1);
    if (!buf) {
        fclose(f);
        unlink(resp_path);
        return strdup("{\"error\":\"oom\"}");
    }
    fread(buf, 1, fsize, f);
    buf[fsize] = '\0';
    fclose(f);
    unlink(resp_path);

    /* Split body and HTTP code (last line) */
    char *last_nl = strrchr(buf, '\n');
    if (last_nl) {
        *last_nl = '\0';
    }
    return buf;
}

/* billing_fetch renamed to avoid shadowing lib/libhttp/http.h:http_get(). */
static char *billing_fetch(const char *url, const char *auth_header) {
    if (!url) return NULL;

    /* Use the internal HTTP client (libhttp) — no external curl binary. */
    http_t *h = http_new(30);
    if (!h) return strdup("{\"error\":\"http client init failed\"}");

    char headers[512];
    if (auth_header && *auth_header)
        snprintf(headers, sizeof(headers), "Authorization: %s\r\nAccept: application/json", auth_header);
    else
        snprintf(headers, sizeof(headers), "Accept: application/json");

    http_resp_t *r = http_get(h, url, headers);
    http_free(h);
    if (!r) return strdup("{\"error\":\"http request failed\"}");

    char *out = NULL;
    if (r->body && r->body[0]) {
        out = strdup(r->body);
    } else {
        out = strdup("{\"error\":\"empty response\"}");
    }
    http_resp_free(r);
    return out;
}

/* ================================================================
 *  JSON helpers
 * ================================================================ */

/* billing_json helpers live in billing_json_helpers.c (hermes_billing.h) */

static char *json_get_string(const char *json, const char *key, char *buf, size_t buf_sz) {
    if (!json || !key || !buf || buf_sz == 0) return NULL;
    buf[0] = '\0';
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p && *p != ':') p++;
    if (*p) p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    if (len >= buf_sz) len = buf_sz - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

/* ================================================================
 *  Port of Python: resolve_portal_base_url
 * ================================================================ */
/* PoP: resolve_portal_base_url @ hermes_cli/dashboard_register.py:_resolve_portal_base_url */

char *resolve_portal_base_url(void *ctx, void *state) {
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "resolve_portal_base_url: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "resolve_portal_base_url: resolving");
    (void)state; /* unused in this implementation */
    const char *env_url = getenv("NOUS_PORTAL_URL");
    if (env_url && *env_url) {
        hermes_log(LOG_INFO, "port", "resolve_portal_base_url: from env: %s", env_url);
        return strdup(env_url);
    }
    const char *url = "https://billing.nousresearch.com";
    hermes_log(LOG_INFO, "port", "resolve_portal_base_url: default: %s", url);
    return strdup(url);
}

/* ================================================================
 *  Port of Python: _retry_after_seconds
 * ================================================================ */

int retry_after_seconds(void *ctx) {
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "retry_after_seconds: null context");
        return 60;
    }
    hermes_log(LOG_DEBUG, "port", "retry_after_seconds: called");
    /* Parse Retry-After header value */
    const char *retry_after = getenv("HERMES_RETRY_AFTER");
    int seconds = retry_after ? atoi(retry_after) : 60;
    if (seconds <= 0) seconds = 60;
    if (seconds > 3600) seconds = 3600;
    hermes_log(LOG_INFO, "port", "retry_after_seconds: waiting %d seconds", seconds);
    return seconds;
}

/* ================================================================
 *  Port of Python: get_billing_state
 * ================================================================ */

char *get_billing_state(void *ctx, double timeout) {
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "get_billing_state: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "get_billing_state: fetching");

    const char *api_key = getenv("NOUS_BILLING_KEY");
    if (!api_key || !*api_key) api_key = getenv("NOUS_API_KEY");
    if (!api_key || !*api_key) api_key = getenv("NOUS_BILLING_KEY");
    if (!api_key || !*api_key) {
        hermes_log(LOG_ERROR, "port", "get_billing_state: NOUS_BILLING_KEY not set");
        return strdup("{\"error\":\"NOUS_BILLING_KEY not set\"}");
    }

    char *portal_base = resolve_portal_base_url(ctx, NULL);
    if (!portal_base) {
        return strdup("{\"error\":\"could not resolve portal URL\"}");
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/billing/state", portal_base);
    free(portal_base);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

    char *resp = billing_fetch(url, auth_header);
    if (!resp) {
        return strdup("{\"error\":\"http request failed\"}");
    }

    return resp;
}

/* ================================================================
 *  Port of Python: post_charge
 * ================================================================ */

char *post_charge(void *ctx, double amount_usd, const char *idempotency_key) {
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "post_charge: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "post_charge: amount=%.2f key=%s", amount_usd, idempotency_key ? idempotency_key : "none");

    const char *api_key = getenv("NOUS_BILL");
    if (!api_key || !*api_key) api_key = getenv("NOUS_API_KEY");
    if (!api_key || !*api_key) api_key = getenv("NOUS_BILLING_KEY");
    if (!api_key || !*api_key) {
        hermes_log(LOG_ERROR, "port", "post_charge: NOUS_BILLING_KEY not set");
        return strdup("{\"error\":\"NOUS_BILLING_KEY not set\"}");
    }

    char *portal_base = resolve_portal_base_url(ctx, NULL);
    if (!portal_base) {
        return strdup("{\"error\":\"could not resolve portal URL\"}");
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/billing/charge", portal_base);
    free(portal_base);

    char json_body[1024];
    snprintf(json_body, sizeof(json_body),
             "{\"amountUsd\":%.2f%s%s}",
             amount_usd,
             idempotency_key ? ",\"idempotencyKey\":\"" : "",
             idempotency_key ? idempotency_key : "");

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

    char *resp = http_post(url, auth_header, json_body);
    if (!resp) {
        return strdup("{\"error\":\"http request failed\"}");
    }

    return resp;
}

/* ================================================================
 *  Port of Python: get_charge_status
 * ================================================================ */

char *get_charge_status(void *ctx, const char *charge_id) {
    if (!ctx || !charge_id) {
        hermes_log(LOG_WARNING, "port", "get_charge_status: null parameter");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "get_charge_status: charge_id=%s", charge_id);

    const char *api_key = getenv("NOUS_BILL");
    if (!api_key || !*api_key) api_key = getenv("NOUS_API_KEY");
    if (!api_key || !*api_key) api_key = getenv("NOUS_BILLING_KEY");
    if (!api_key || !*api_key) {
        hermes_log(LOG_ERROR, "port", "get_charge_status: NOUS_BILLING_KEY not set");
        return strdup("{\"error\":\"NOUS_BILLING_KEY not set\"}");
    }

    char *portal_base = resolve_portal_base_url(ctx, NULL);
    if (!portal_base) {
        return strdup("{\"error\":\"could not resolve portal URL\"}");
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/billing/charge/%s", portal_base, charge_id);
    free(portal_base);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

    char *resp = billing_fetch(url, auth_header);
    if (!resp) {
        return strdup("{\"error\":\"http request failed\"}");
    }

    return resp;
}

/* ================================================================
 *  Port of Python: patch_auto_top_up
 * ================================================================ */

char *patch_auto_top_up(void *ctx, bool enabled, double threshold, double top_up_amount) {
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "patch_auto_top_up: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "patch_auto_top_up: enabled=%d threshold=%.2f top_up=%.2f",
               enabled, threshold, top_up_amount);

    const char *api_key = getenv("NOUS_BILL");
    if (!api_key || !*api_key) api_key = getenv("NOUS_API_KEY");
    if (!api_key || !*api_key) api_key = getenv("NOUS_BILLING_KEY");
    if (!api_key || !*api_key) {
        hermes_log(LOG_ERROR, "port", "patch_auto_top_up: NOUS_BILLING_KEY not set");
        return strdup("{\"error\":\"NOUS_BILLING_KEY not set\"}");
    }

    char *portal_base = resolve_portal_base_url(ctx, NULL);
    if (!portal_base) {
        return strdup("{\"error\":\"could not resolve portal URL\"}");
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/billing/auto-top-up", portal_base);
    free(portal_base);

    char json_body[1024];
    snprintf(json_body, sizeof(json_body),
             "{\"enabled\":%s,\"thresholdUsd\":%.2f,\"topUpAmountUsd\":%.2f}",
             enabled ? "true" : "false", threshold, top_up_amount);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

    char *resp = http_patch(url, auth_header, json_body);
    if (!resp) {
        return strdup("{\"error\":\"http request failed\"}");
    }

    return resp;
}

/* ================================================================
 *  Port of Python: _absolutize_portal_url
 * ================================================================ */

char *absolutize_portal_url(void *ctx, const char *raw_portal_url) {
    if (!ctx || !raw_portal_url || !*raw_portal_url) {
        return NULL;
    }
    if (strncmp(raw_portal_url, "http://", 7) == 0 || strncmp(raw_portal_url, "https://", 8) == 0) {
        return strdup(raw_portal_url);
    }
    /* Relative URL - prepend portal base */
    char *base = resolve_portal_base_url(ctx, NULL);
    if (!base) {
        return strdup(raw_portal_url);
    }
    char *result = malloc(strlen(base) + strlen(raw_portal_url) + 2);
    if (!result) {
        free(base);
        return NULL;
    }
    sprintf(result, "%s/%s", base, raw_portal_url);
    free(base);
    return result;
}

/* ================================================================
 *  Port of Python hermes_cli/nous_billing.py: billing auth helpers
 * ================================================================ */

/* PoP: billing_not_logged_in_json @ hermes_cli/nous_billing.py:_billing_not_logged_in */
/* Build the canonical "not logged in" billing auth error as a malloc'd JSON
 * string, mirroring Python BillingAuthError(status=401, error="invalid_token").
 * Caller frees. */
char *billing_not_logged_in_json(void) {
    return strdup("{\"status\":401,\"error\":\"invalid_token\","
                  "\"message\":\"Not logged into Nous Portal \\u2014 run `hermes portal` to log in.\"}");
}

/* PoP: billing_resolve_token_and_base @ hermes_cli/nous_billing.py:_resolve_token_and_base */
/* Resolve (access_token, portal_base_url) for billing calls.
 * token_out receives the token (caller frees), base_out receives the base URL
 * (caller frees). Returns 0 on success, -1 if no usable Nous session.
 * Mirrors Python: uses the refresh-aware token resolver + resolve_portal_base_url,
 * with a short TTL cache to avoid re-reading the auth store every poll tick. */
static char g_token_cache_token[2048];
static char g_token_cache_base[1024];
static double g_token_cache_at = -1.0;
#define BILLING_TOKEN_CACHE_TTL 120.0

int billing_resolve_token_and_base(char **token_out, char **base_out, bool use_cache) {
    double now = (double)time(NULL);
    if (use_cache && g_token_cache_at > 0 && (now - g_token_cache_at) < BILLING_TOKEN_CACHE_TTL
        && g_token_cache_token[0]) {
        *token_out = strdup(g_token_cache_token);
        *base_out = strdup(g_token_cache_base);
        return 0;
    }

    char tok[2048];
    if (cli_tools_managed_tool_gateway_peek_nous_access_token(tok, sizeof(tok)) != 0 || !tok[0]) {
        return -1;
    }

    char *base = resolve_portal_base_url(NULL, NULL);
    if (!base) base = strdup(billing_fallback_portal_url());

    strncpy(g_token_cache_token, tok, sizeof(g_token_cache_token) - 1);
    g_token_cache_token[sizeof(g_token_cache_token) - 1] = '\0';
    strncpy(g_token_cache_base, base, sizeof(g_token_cache_base) - 1);
    g_token_cache_base[sizeof(g_token_cache_base) - 1] = '\0';
    g_token_cache_at = now;

    *token_out = strdup(tok);
    *base_out = base; /* transfers ownership */
    return 0;
}

/* PoP: billing_raise_for_error @ hermes_cli/nous_billing.py:_raise_for_error */
/* Map an HTTP error response to the right typed billing error, returned as a
 * malloc'd JSON string (caller frees). Mirrors Python's status->BillingError
 * dispatch: 401 -> auth, 403+insufficient_scope -> scope, 429/503 -> rate
 * limited, else generic. Returns NULL only on alloc failure (it always
 * returns an error payload, since it is only called on error). */
char *billing_raise_for_error(int status, const char *payload_json, const char *headers) {
    const char *error = billing_json_get_string(payload_json, "error");
    const char *message = billing_json_get_string(payload_json, "message");
    char *portal_url = absolutize_portal_url(NULL, billing_json_get_string(payload_json, "portalUrl"));
    int retry_after = retry_after_seconds((void *)headers);

    const char *kind = "error";
    const char *msg = message ? message : (error ? error : "");
    if (status == 401) { kind = "auth"; if (!msg[0]) msg = "Authentication required."; }
    else if (status == 403 && error && strcmp(error, "insufficient_scope") == 0) {
        kind = "scope"; if (!msg[0]) msg = "This action needs the billing:manage scope.";
    }
    else if (status == 429 || status == 503) {
        kind = "rate_limited"; if (!msg[0]) msg = "Rate limited \\u2014 try again shortly.";
    }
    if (!msg[0]) { msg = "Billing request failed."; }

    size_t cap = 1024;
    char *out = malloc(cap);
    if (!out) { free(portal_url); return NULL; }
    int n = snprintf(out, cap,
        "{\"status\":%d,\"error\":%s,\"kind\":\"%s\",\"message\":\"%s\",\"portalUrl\":%s%s%s}",
        status,
        error ? error : "null",
        kind,
        msg,
        portal_url ? "\"" : "",
        portal_url ? portal_url : "null",
        portal_url ? "\"" : "");
    if (retry_after > 0) {
        /* append retry_after (best-effort; keep simple) */
        size_t len = strlen(out);
        snprintf(out + len, cap - len, ",\"retryAfter\":%d}", retry_after);
    } else {
        size_t len = strlen(out);
        if (len > 0 && out[len-1] == '}') {
            out[len-1] = '\0';
            snprintf(out + len - 1, cap - (len - 1), ",\"retryAfter\":null}");
        }
    }
    free(portal_url);
    return out;
}

/* step_up_nous_billing_scope: real implementation lives in port_auth_na.c
 * (matches Python's step_up_nous_billing_scope: checks nous_token_has_billing_scope
 *  and logs manual re-login if absent). This file no longer duplicates it. */

#endif /* SRC_CLI_PORT_NOUS_BILLING_C */