/*
 * port_nous_billing_remaining.c — Port of hermes_cli/nous_billing.py
 * billing API surface. Portal URL resolution, retry-after parsing,
 * billing state/charge/auto-top-up endpoints.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_http.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ hermes_cli/nous_billing.py:__init__ */
char *nb2_error_init(const char *message, const char *status, const char *error) {
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"status\": \"%s\", \"error\": \"%s\"}",
             message ? message : "", status ? status : "", error ? error : "");
    return out;
}

/* PoP: resolve_portal_base_url @ hermes_cli/nous_billing.py:resolve_portal_base_url */
char *nb2_resolve_portal_base_url(void) {
    /* Python: login-time precedence env. */
    const char *v = getenv("HERMES_PORTAL_BASE_URL");
    if (v && *v) return strdup(v);
    return strdup("https://portal.nousresearch.com");
}

/* PoP: _absolutize_portal_url @ hermes_cli/nous_billing.py:_absolutize_portal_url */
char *nb2_absolutize_portal_url(const char *portal_url) {
    /* Python: relative server url → absolute. */
    if (!portal_url) return NULL;
    if (strncmp(portal_url, "http://", 7) == 0 || strncmp(portal_url, "https://", 8) == 0)
        return strdup(portal_url);
    char *base = nb2_resolve_portal_base_url();
    char *out = NULL;
    asprintf(&out, "%s%s", base, portal_url[0] == '/' ? portal_url : "/");
    free(base);
    return out;
}

/* PoP: _retry_after_seconds @ hermes_cli/nous_billing.py:_retry_after_seconds */
long nb2_retry_after_seconds(const char *header_value) {
    /* Python: integer seconds or None. */
    if (!header_value || !*header_value) return -1;
    char *end = NULL;
    long v = strtol(header_value, &end, 10);
    if (end == header_value || *end != '\0') return -1;
    return v;
}

/* PoP: get_billing_state @ hermes_cli/nous_billing.py:get_billing_state */
char *nb2_get_billing_state(const char *base_url, const char *token) {
    /* Python: GET /api/billing/state — REAL http. */
    if (!base_url) return NULL;
    char *url = NULL;
    asprintf(&url, "%s/api/billing/state", base_url);
    http_t *h = http_new(20);
    if (!h) { free(url); return NULL; }
    char *hdr = NULL;
    if (token && *token) asprintf(&hdr, "Authorization: Bearer %s", token);
    http_resp_t *r = http_get(h, url, hdr);
    char *out = NULL;
    if (r && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    free(hdr);
    free(url);
    return out;
}

/* PoP: patch_auto_top_up @ hermes_cli/nous_billing.py:patch_auto_top_up */
char *nb2_patch_auto_top_up(const char *base_url, const char *token, const char *config_json) {
    /* Python: PATCH /api/billing/auto-top-up. */
    if (!base_url) return NULL;
    char *url = NULL;
    asprintf(&url, "%s/api/billing/auto-top-up", base_url);
    http_t *h = http_new(20);
    if (!h) { free(url); return NULL; }
    char *hdr = NULL;
    if (token && *token) asprintf(&hdr, "Authorization: Bearer %s", token);
    http_resp_t *r = http_request(h, HTTP_PUT, url, hdr, config_json ? config_json : "{}",
                                  config_json ? strlen(config_json) : 2);
    char *out = NULL;
    if (r && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    free(hdr);
    free(url);
    return out;
}

/* PoP: post_charge @ hermes_cli/nous_billing.py:post_charge */
char *nb2_post_charge(const char *base_url, const char *token, const char *payload_json, const char *idempotency_key) {
    /* Python: POST /api/billing/charge with Idempotency-Key. */
    if (!base_url) return NULL;
    char *url = NULL;
    asprintf(&url, "%s/api/billing/charge", base_url);
    http_t *h = http_new(30);
    if (!h) { free(url); return NULL; }
    char *hdr = NULL;
    if (token && *token)
        asprintf(&hdr, "Authorization: Bearer %s\nIdempotency-Key: %s",
                 token, idempotency_key ? idempotency_key : "");
    http_resp_t *r = http_request(h, HTTP_POST, url, hdr, payload_json ? payload_json : "{}",
                                  payload_json ? strlen(payload_json) : 2);
    char *out = NULL;
    if (r && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    free(hdr);
    free(url);
    return out;
}

/* PoP: get_charge_status @ hermes_cli/nous_billing.py:get_charge_status */
char *nb2_get_charge_status(const char *base_url, const char *token, const char *charge_id) {
    /* Python: GET /api/billing/charge/{id}. */
    if (!base_url || !charge_id) return NULL;
    char *url = NULL;
    asprintf(&url, "%s/api/billing/charge/%s", base_url, charge_id);
    http_t *h = http_new(20);
    if (!h) { free(url); return NULL; }
    char *hdr = NULL;
    if (token && *token) asprintf(&hdr, "Authorization: Bearer %s", token);
    http_resp_t *r = http_get(h, url, hdr);
    char *out = NULL;
    if (r && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    free(hdr);
    free(url);
    return out;
}
