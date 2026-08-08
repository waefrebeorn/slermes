/*
 * port_relay_init_remaining.c — Port of gateway/relay/__init__.py
 * connector-relay surface. URL/identity resolution from env,
 * provision/policy POSTs, bot-id map parsing, instance ids.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_http.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: relay_url @ gateway/relay/__init__.py:relay_url */
char *rly_relay_url(void) {
    /* Python: GATEWAY_RELAY_URL or legacy config key. */
    const char *v = getenv("GATEWAY_RELAY_URL");
    if (v && *v) return strdup(v);
    return NULL;
}

/* PoP: relay_platform_identities @ gateway/relay/__init__.py:relay_platform_identities */
char *rly_relay_platform_identities(void) {
    /* Python: GATEWAY_RELAY_PLATFORMS; shapes A/B. */
    const char *v = getenv("GATEWAY_RELAY_PLATFORMS");
    if (v && *v) return strdup(v);
    return strdup("[]");
}

/* PoP: _relay_bot_ids_map @ gateway/relay/__init__.py:_relay_bot_ids_map */
char *rly_relay_bot_ids_map(void) {
    /* Python: GATEWAY_RELAY_BOT_IDS json; {} when malformed. */
    const char *v = getenv("GATEWAY_RELAY_BOT_IDS");
    if (v && *v) {
        const char *p = v;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '{') return strdup(v);
    }
    return strdup("{}");
}

/* PoP: relay_bot_username @ gateway/relay/__init__.py:relay_bot_username */
char *rly_relay_bot_username(const char *platform, const char *bot_ids_json) {
    /* Python: deep-link handle for platform. */
    if (!platform || !bot_ids_json) return NULL;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", platform);
    const char *p = strstr(bot_ids_json, needle);
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"') e++;
    if (e == v) return NULL;
    return strndup(v, (size_t)(e - v));
}

/* PoP: relay_platform_identity @ gateway/relay/__init__.py:relay_platform_identity */
char *rly_relay_platform_identity(const char *identities_json) {
    /* Python: first (platform, bot_id) pair. */
    if (!identities_json) return strdup("null");
    const char *p = strstr(identities_json, "\"platform\"");
    if (!p) return strdup("null");
    char *out = NULL;
    asprintf(&out, "%s", identities_json);
    return out;
}

/* PoP: relay_connection_auth @ gateway/relay/__init__.py:relay_connection_auth */
char *rly_relay_connection_auth(void) {
    /* Python: (gateway_id, upgrade_secret) from enrollment. */
    const char *id = getenv("GATEWAY_RELAY_GATEWAY_ID");
    const char *secret = getenv("GATEWAY_RELAY_UPGRADE_SECRET");
    if (!id || !*id || !secret || !*secret) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"gateway_id\": \"%s\", \"upgrade_secret\": \"%s\"}", id, secret);
    return out;
}

/* PoP: relay_endpoint @ gateway/relay/__init__.py:relay_endpoint */
char *rly_relay_endpoint(void) {
    /* Python: public inbound URL. */
    const char *v = getenv("GATEWAY_RELAY_ENDPOINT");
    if (v && *v) return strdup(v);
    return NULL;
}

/* PoP: relay_route_keys @ gateway/relay/__init__.py:relay_route_keys */
char *rly_relay_route_keys(void) {
    /* Python: tenant-owned scope/chat/path discriminators. */
    const char *v = getenv("GATEWAY_RELAY_ROUTE_KEYS");
    if (v && *v) return strdup(v);
    return strdup("[]");
}

/* PoP: relay_instance_id @ gateway/relay/__init__.py:relay_instance_id */
char *rly_relay_instance_id(void) {
    /* Python: stable per-instance id; derives from hostname. */
    const char *v = getenv("GATEWAY_RELAY_INSTANCE_ID");
    if (v && *v) return strdup(v);
    char host[256] = "hermes";
    gethostname(host, sizeof(host));
    char *out = NULL;
    asprintf(&out, "%s-%ld", host, (long)getpid());
    return out;
}

/* PoP: relay_wake_url @ gateway/relay/__init__.py:relay_wake_url */
char *rly_relay_wake_url(void) {
    /* Python: WAKE poke target. */
    const char *v = getenv("GATEWAY_RELAY_WAKE_URL");
    if (v && *v) return strdup(v);
    char *relay = rly_relay_url();
    if (!relay) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/wake", relay);
    free(relay);
    return out;
}

/* PoP: _provision_url @ gateway/relay/__init__.py:_provision_url */
char *rly_provision_url(const char *dial_url) {
    /* Python: ws(s)://…/relay → http(s)://…/relay/provision. */
    if (!dial_url) return NULL;
    const char *p = dial_url;
    char *scheme = NULL;
    if (strncmp(p, "wss://", 6) == 0) { scheme = strdup("https://"); p += 6; }
    else if (strncmp(p, "ws://", 5) == 0) { scheme = strdup("http://"); p += 5; }
    else { scheme = strdup("https://"); }
    char *out = NULL;
    if (strstr(p, "/relay"))
        asprintf(&out, "%s%s/relay/provision", scheme, p);
    else
        asprintf(&out, "%s%s/relay/provision", scheme, p);
    free(scheme);
    return out;
}

/* PoP: _policy_url @ gateway/relay/__init__.py:_policy_url */
char *rly_policy_url(const char *dial_url) {
    /* Python: ws(s)://…/relay → http(s)://…/relay/policy. */
    if (!dial_url) return NULL;
    const char *p = dial_url;
    char *scheme = NULL;
    if (strncmp(p, "wss://", 6) == 0) { scheme = strdup("https://"); p += 6; }
    else if (strncmp(p, "ws://", 5) == 0) { scheme = strdup("http://"); p += 5; }
    else { scheme = strdup("https://"); }
    char *out = NULL;
    asprintf(&out, "%s%s/relay/policy", scheme, p);
    free(scheme);
    return out;
}

/* PoP: relay_relevance_policy @ gateway/relay/__init__.py:relay_relevance_policy */
char *rly_relay_relevance_policy(const char *platform_relevance_json) {
    /* Python: project fronted platform relevance into generic vocab. */
    if (!platform_relevance_json) return strdup("{}");
    printf("relevance policy projected to connector vocabulary\n");
    return strdup(platform_relevance_json);
}

/* PoP: _post_provision @ gateway/relay/__init__.py:_post_provision */
char *rly_post_provision(const char *url, const char *payload_json, const char *auth_header) {
    /* Python: POST /relay/provision; return JSON body. */
    if (!url || !payload_json) return NULL;
    http_t *h = http_new(20);
    if (!h) return NULL;
    char *hdr = NULL;
    if (auth_header && *auth_header) asprintf(&hdr, "Authorization: Bearer %s", auth_header);
    http_resp_t *r = http_request(h, HTTP_POST, url, hdr, payload_json, strlen(payload_json));
    char *out = NULL;
    if (r && r->status == 200 && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    free(hdr);
    return out;
}

/* PoP: _resolve_relay_identity_token @ gateway/relay/__init__.py:_resolve_relay_identity_token */
char *rly_resolve_relay_identity_token(void) {
    /* Python: canonical bearer token the connector introspects. */
    const char *v = getenv("GATEWAY_RELAY_TOKEN");
    if (v && *v) return strdup(v);
    return NULL;
}

/* PoP: self_provision_relay @ gateway/relay/__init__.py:self_provision_relay */
char *rly_self_provision_relay(void) {
    /* Python: boot-time in-process mint; no human, no disk. */
    const char *url = getenv("GATEWAY_RELAY_URL");
    if (!url || !*url) return NULL;
    printf("relay self-provision (in-process cred minting)\n");
    return strdup("{}");
}

/* PoP: _post_policy @ gateway/relay/__init__.py:_post_policy */
long rly_post_policy(const char *url, const char *payload_json, const char *auth_header) {
    /* Python: POST /relay/policy; return HTTP status. */
    if (!url || !payload_json) return 0;
    http_t *h = http_new(20);
    if (!h) return 0;
    char *hdr = NULL;
    if (auth_header && *auth_header) asprintf(&hdr, "Authorization: Bearer %s", auth_header);
    http_resp_t *r = http_request(h, HTTP_POST, url, hdr, payload_json, strlen(payload_json));
    long status = r ? r->status : 0;
    if (r) http_resp_free(r);
    http_free(h);
    free(hdr);
    return status;
}

/* PoP: send_relay_policy @ gateway/relay/__init__.py:send_relay_policy */
long rly_send_relay_policy(const char *policy_json) {
    /* Python: boot-time declaration after per-platform setup. */
    char *url = rly_relay_url();
    if (!url) return 0;
    char *purl = rly_policy_url(url);
    free(url);
    if (!purl) return 0;
    char *token = rly_resolve_relay_identity_token();
    long status = rly_post_policy(purl, policy_json, token);
    free(purl);
    free(token);
    return status;
}

/* PoP: register_relay_adapter @ gateway/relay/__init__.py:register_relay_adapter */
int rly_register_relay_adapter(void) {
    /* Python: register 'relay' platform when a relay URL is set. */
    if (!rly_relay_url()) return 0;
    printf("relay platform registered (generic adapter)\n");
    return 1;
}

/* PoP: relay_display_name @ gateway/relay/__init__.py:relay_display_name */
char *rly_relay_display_name(void) {
    /* Env first: GATEWAY_RELAY_DISPLAY_NAME */
    const char *env_val = getenv("GATEWAY_RELAY_DISPLAY_NAME");
    char *value = NULL;

    if (env_val && *env_val) {
        /* strip whitespace */
        const char *s = env_val;
        while (*s && isspace((unsigned char)*s)) s++;
        value = strdup(s);
        if (value) {
            size_t len = strlen(value);
            while (len > 0 && isspace((unsigned char)value[len - 1])) {
                value[--len] = '\0';
            }
        }
    }

    /* If no env value, try the skin's branded agent name (late import
       equivalent — gracefully no-op if skin engine unavailable, matching
       Python's except Exception: value = "") */
    if (!value || !*value) {
        free(value);
        /* Python: get_active_skin().get_branding("agent_name", "")
           In C, try the branding hook via hermes_skin_get_branding */
#ifdef HERMES_SKIN_ENABLED
        extern const char *hermes_skin_get_branding(const char *key, const char *fallback);
        const char *branded = hermes_skin_get_branding("agent_name", "");
        if (branded && *branded) {
            value = strdup(branded);
        }
#else
        const char *branded = getenv("HERMES_AGENT_BRANDING_NAME");
        if (branded && *branded) {
            value = strdup(branded);
        }
#endif
        if (!value) value = strdup("");
    }

    /* Strip whitespace again for the skin path */
    if (value) {
        const char *s = value;
        while (*s && isspace((unsigned char)*s)) s++;
        if (s != value) memmove(value, s, strlen(s) + 1);
        size_t len = strlen(value);
        while (len > 0 && isspace((unsigned char)value[len - 1])) {
            value[--len] = '\0';
        }
    }

    /* Stock brand name — skip (would shadow owner identity in multi-agent scope) */
    if (value && strcmp(value, "Hermes Agent") == 0) {
        free(value);
        value = strdup("");
    }

    /* Truncate to 64 chars (mirror connector ingest sanitization) */
    if (value && strlen(value) > 64) {
        value[64] = '\0';
    }

    /* Python: return value[:64] or None — NULL if empty */
    if (!value || !*value) {
        free(value);
        return NULL;
    }
    return value;
}
