/*
 * google_code_assist.c — Google Code Assist API client wrappers.
 *
 * Port of Python agent/google_code_assist.py (451 lines).
 * Provides thin wrappers for the Code Assist HTTP API.
 *
 * MIT License — WuBu Slermes Project
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Port of Python agent/google_code_assist.py:_build_headers()
 *   — implemented in provider.c (generic build_headers pattern)
 * Port of Python agent/google_code_assist.py:_client_metadata()
 * Port of Python agent/google_code_assist.py:_post_json()
 * Port of Python agent/google_code_assist.py:_is_vpc_sc_violation()
 * Port of Python agent/google_code_assist.py:_parse_load_response()
 * Port of Python agent/google_code_assist.py:load_code_assist()
 * Port of Python agent/google_code_assist.py:retrieve_user_quota()
 * Port of Python agent/google_code_assist.py:onboard_user()
 * Port of Python agent/google_code_assist.py:resolve_project_context()
 */

/* ---- Constants ---- */
#define STANDARD_TIER_ID "ST-PREMIUM"
#define FREE_TIER_ID "ST-FREE"
#define LEGACY_TIER_ID "ST-LEGACY"
#define CODE_ASSIST_ENDPOINT "https://codeassist.googleapis.com"
#define FALLBACK_ENDPOINT_1 "https://us-central1-aiplatform.googleapis.com"
#define FALLBACK_ENDPOINT_2 "https://europe-west1-aiplatform.googleapis.com"
#define MAX_ENDPOINTS 3
#define ONBOARD_POLL_ATTEMPTS 12
#define ONBOARD_POLL_INTERVAL_MS 5000

/* Port of Python: _client_metadata() */
json_t *client_metadata(void) {
    json_t *meta = json_object();
    if (!meta) return NULL;
    json_set(meta, "ideType", json_string("IDE_UNSPECIFIED"));
    json_set(meta, "platform", json_string("PLATFORM_UNSPECIFIED"));
    json_set(meta, "pluginType", json_string("GEMINI"));
    return meta;
}

/* Port of Python: is_vpc_sc_violation() */
bool is_vpc_sc_violation(const char *body) {
    if (!body || !*body) return false;
    if (strstr(body, "SECURITY_POLICY_VIOLATED")) return true;

    json_t *parsed = json_parse(body, NULL);
    if (!parsed) return false;

    json_t *error = json_obj_get(parsed, "error");
    bool result = false;
    if (error && error->type == JSON_OBJECT) {
        json_t *details = json_obj_get(error, "details");
        if (details && details->type == JSON_ARRAY) {
            size_t n = json_len(details);
            for (size_t i = 0; i < n; i++) {
                json_t *item = json_get(details, i);
                if (item && item->type == JSON_OBJECT) {
                    json_t *reason = json_obj_get(item, "reason");
                    if (reason && reason->type == JSON_STRING &&
                        strcmp(reason->str_val, "SECURITY_POLICY_VIOLATED") == 0) {
                        result = true;
                        break;
                    }
                }
            }
        }
        if (!result) {
            json_t *msg = json_obj_get(error, "message");
            if (msg && msg->type == JSON_STRING &&
                strstr(msg->str_val, "SECURITY_POLICY_VIOLATED")) {
                result = true;
            }
        }
    }

    json_free(parsed);
    return result;
}

/* Port of Python _post_json() — POST JSON to Code Assist API */
json_t *google_code_assist_post_json(
    const char *url,
    json_t *body_json,
    const char *access_token,
    int timeout_seconds)
{
    if (!url || !access_token) return NULL;

    char *body_str = body_json ? json_serialize(body_json) : strdup("{}");
    if (!body_str) return NULL;

    char auth_hdr[2048];
    snprintf(auth_hdr, sizeof(auth_hdr),
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json\r\n"
             "User-Agent: google-api-nodejs-client/9.15.1 (gzip)\r\n"
             "X-Goog-Api-Client: gl-node/24.0.0",
             access_token);

    if (timeout_seconds <= 0) timeout_seconds = 30;

    http_t *h = http_new(timeout_seconds);
    if (!h) {
        free(body_str);
        return NULL;
    }

    http_resp_t *resp = http_post_json_auth(h, url, body_str, auth_hdr);
    free(body_str);

    if (!resp) {
        http_free(h);
        return NULL;
    }

    json_t *result = NULL;
    if (resp->body && resp->body[0]) {
        result = json_parse(resp->body, NULL);
    }
    if (!result) result = json_object();

    if (resp->status == 403 && resp->body) {
        if (is_vpc_sc_violation(resp->body)) {
            json_free(result);
            result = json_object();
            json_set(result, "_vpc_sc", json_bool(true));
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* Port of Python: _parse_load_response */
static json_t *parse_load_response(json_t *resp) {
    if (!resp || resp->type != JSON_OBJECT) {
        json_t *empty = json_object();
        json_set(empty, "current_tier_id", json_string(STANDARD_TIER_ID));
        return empty;
    }

    json_t *current_tier = json_obj_get(resp, "currentTier");
    const char *tier_id = STANDARD_TIER_ID;
    if (current_tier && current_tier->type == JSON_OBJECT) {
        const char *tid = json_get_str(current_tier, "id", NULL);
        if (tid && tid[0]) tier_id = tid;
    }

    const char *project = json_get_str(resp, "cloudaicompanionProject", "");

    json_t *allowed = json_obj_get(resp, "allowedTiers");
    json_t *allowed_ids = json_array();
    if (allowed && allowed->type == JSON_ARRAY) {
        size_t n = json_len(allowed);
        for (size_t i = 0; i < n; i++) {
            json_t *t = json_get(allowed, (int)i);
            if (t && t->type == JSON_OBJECT) {
                const char *tid = json_get_str(t, "id", NULL);
                if (tid && tid[0])
                    json_append(allowed_ids, json_string(tid));
            }
        }
    }

    json_t *result = json_object();
    json_set(result, "current_tier_id", json_string(tier_id));
    json_set(result, "cloudaicompanion_project", json_string(project));
    json_set(result, "allowed_tiers", allowed_ids);
    return result;
}

/* Port of Python: load_code_assist */
json_t *load_code_assist(const char *access_token, const char *project_id) {
    if (!access_token) return NULL;

    json_t *meta = client_metadata();
    if (!meta) return NULL;

    json_t *body = json_object();
    json_t *metadata = json_object();
    if (project_id && project_id[0])
        json_set(metadata, "duetProject", json_string(project_id));
    json_set(metadata, "ideType", json_string(json_get_str(meta, "ideType", "IDE_UNSPECIFIED")));
    json_set(metadata, "platform", json_string(json_get_str(meta, "platform", "PLATFORM_UNSPECIFIED")));
    json_set(metadata, "pluginType", json_string(json_get_str(meta, "pluginType", "GEMINI")));
    json_set(body, "metadata", metadata);
    if (project_id && project_id[0])
        json_set(body, "cloudaicompanionProject", json_string(project_id));
    json_free(meta);

    const char *endpoints[MAX_ENDPOINTS] = {
        CODE_ASSIST_ENDPOINT,
        FALLBACK_ENDPOINT_1,
        FALLBACK_ENDPOINT_2
    };

    json_t *result = NULL;
    for (int i = 0; i < MAX_ENDPOINTS; i++) {
        char url[512];
        snprintf(url, sizeof(url), "%s/v1internal:loadCodeAssist", endpoints[i]);

        json_t *resp = google_code_assist_post_json(url, body, access_token, 30);
        if (!resp) continue;

        json_t *vpc_sc = json_obj_get(resp, "_vpc_sc");
        if (vpc_sc && vpc_sc->type == JSON_BOOL && vpc_sc->bool_val) {
            json_free(resp);
            json_free(body);
            json_t *fallback = json_object();
            json_set(fallback, "current_tier_id", json_string(STANDARD_TIER_ID));
            json_set(fallback, "cloudaicompanion_project", json_string(project_id ? project_id : ""));
            json_set(fallback, "_vpc_sc", json_bool(true));
            return fallback;
        }

        result = parse_load_response(resp);
        json_free(resp);
        if (result) break;
    }

    json_free(body);
    return result;
}

/* Port of Python: retrieve_user_quota */
json_t *retrieve_user_quota(const char *access_token, const char *project_id) {
    if (!access_token) return NULL;

    json_t *body = json_object();
    if (project_id && project_id[0])
        json_set(body, "project", json_string(project_id));

    char url[512];
    snprintf(url, sizeof(url), "%s/v1internal:retrieveUserQuota", CODE_ASSIST_ENDPOINT);

    json_t *resp = google_code_assist_post_json(url, body, access_token, 30);
    json_free(body);
    if (!resp) return json_array();

    json_t *raw_buckets = json_obj_get(resp, "buckets");
    json_t *buckets = json_array();
    if (raw_buckets && raw_buckets->type == JSON_ARRAY) {
        size_t n = json_len(raw_buckets);
        for (size_t i = 0; i < n; i++) {
            json_t *b = json_get(raw_buckets, (int)i);
            if (b && b->type == JSON_OBJECT) {
                json_t *bucket = json_object();
                json_set(bucket, "modelId", json_string(json_get_str(b, "modelId", "")));
                json_set(bucket, "tokenType", json_string(json_get_str(b, "tokenType", "")));
                json_set(bucket, "remainingFraction", json_number(json_get_num(b, "remainingFraction", 0.0)));
                json_set(bucket, "resetTime", json_string(json_get_str(b, "resetTime", "")));
                json_append(buckets, bucket);
            }
        }
    }

    json_free(resp);
    return buckets;
}

/* Port of Python: onboard_user */
json_t *onboard_user(const char *access_token, const char *tier_id,
                     const char *project_id)
{
    if (!access_token || !tier_id) return NULL;

    json_t *meta = client_metadata();
    if (!meta) return NULL;

    json_t *body = json_object();
    json_set(body, "tierId", json_string(tier_id));
    json_set(body, "metadata", meta);
    if (project_id && project_id[0])
        json_set(body, "cloudaicompanionProject", json_string(project_id));

    char url[512];
    snprintf(url, sizeof(url), "%s/v1internal:onboardUser", CODE_ASSIST_ENDPOINT);

    json_t *resp = google_code_assist_post_json(url, body, access_token, 60);
    json_free(body);

    if (!resp) return NULL;

    /* Check if operation is already done */
    json_t *done = json_obj_get(resp, "done");
    if (done && done->type == JSON_BOOL && done->bool_val) {
        return resp;  /* Done immediately */
    }

    /* Poll for LRO completion */
    const char *op_name = json_get_str(resp, "name", NULL);
    if (!op_name || !op_name[0]) {
        return resp;  /* No name to poll — return as-is */
    }

    json_free(resp);

    for (int attempt = 0; attempt < ONBOARD_POLL_ATTEMPTS; attempt++) {
        struct timespec ts = {0, ONBOARD_POLL_INTERVAL_MS * 1000000L};
        nanosleep(&ts, NULL);

        char poll_url[1024];
        snprintf(poll_url, sizeof(poll_url), "%s/%s", CODE_ASSIST_ENDPOINT, op_name);

        json_t *poll_resp = google_code_assist_post_json(poll_url, NULL, access_token, 30);
        if (!poll_resp) continue;

        json_t *poll_done = json_obj_get(poll_resp, "done");
        if (poll_done && poll_done->type == JSON_BOOL && poll_done->bool_val) {
            return poll_resp;
        }

        json_free(poll_resp);
    }

    /* Timeout — return the last response we have (resp was freed above) */
    json_t *timeout_resp = json_object();
    json_set(timeout_resp, "_timeout", json_bool(true));
    json_set(timeout_resp, "tierId", json_string(tier_id));
    return timeout_resp;
}

/* Port of Python: resolve_project_context */
json_t *resolve_project_context(const char *access_token,
                                const char *configured_project_id,
                                const char *env_project_id)
{
    if (!access_token) return NULL;

    json_t *ctx = json_object();

    /* Short-circuit: caller provided project id */
    if (configured_project_id && configured_project_id[0]) {
        json_set(ctx, "project_id", json_string(configured_project_id));
        json_set(ctx, "tier_id", json_string(STANDARD_TIER_ID));
        json_set(ctx, "source", json_string("config"));
        return ctx;
    }

    /* Short-circuit: env has project id */
    if (env_project_id && env_project_id[0]) {
        json_set(ctx, "project_id", json_string(env_project_id));
        json_set(ctx, "tier_id", json_string(STANDARD_TIER_ID));
        json_set(ctx, "source", json_string("env"));
        return ctx;
    }

    /* Discover via loadCodeAssist */
    json_t *info = load_code_assist(access_token, NULL);
    if (!info) {
        json_set(ctx, "project_id", json_string(""));
        json_set(ctx, "tier_id", json_string(STANDARD_TIER_ID));
        json_set(ctx, "source", json_string("fallback"));
        return ctx;
    }

    const char *effective_project = json_get_str(info, "cloudaicompanion_project", "");
    const char *tier = json_get_str(info, "current_tier_id", STANDARD_TIER_ID);

    json_set(ctx, "project_id", json_string(effective_project));
    json_set(ctx, "tier_id", json_string(tier));
    json_set(ctx, "source", json_string("discovery"));

    /* If no tier assigned, onboard with free tier */
    if (!tier || !tier[0] || strcmp(tier, "") == 0) {
        json_t *onboard = onboard_user(access_token, FREE_TIER_ID, effective_project);
        if (onboard) {
            const char *new_tier = json_get_str(onboard, "tierId", FREE_TIER_ID);
            json_set(ctx, "tier_id", json_string(new_tier));
            json_set(ctx, "onboarded", json_bool(true));
            json_free(onboard);
        }
    }

    json_free(info);
    return ctx;
}
