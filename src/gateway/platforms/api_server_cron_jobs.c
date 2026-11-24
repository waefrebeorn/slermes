/*
 * api_server_cron_jobs.c - C11 port of upstream gateway/platforms/api_server_cron_jobs.py
 * (gate w25_apiserverc_947063).
 *
 * PoP: w25_apiserverc_947063_allowed_fields @ gateway/platforms/api_server_cron_jobs.py:handle_update_job{sanitized whitelist}
 * PoP: w25_apiserverc_947063_sanitize @ gateway/platforms/api_server_cron_jobs.py:handle_update_job{sanitized}
 * PoP: w25_apiserverc_947063_invalid_update @ gateway/platforms/api_server_cron_jobs.py:_invalid_update
 * PoP: w25_apiserverc_947063_job_status @ gateway/platforms/api_server_cron_jobs.py:job_response
 *
 * aiohttp's web.Response, the store callables (facade._cron_update) and the
 * prompt scanner (adapter._validate_cron_prompt) are seams: a fixed store
 * outcome and a fixed prompt error go in, the status/body decision comes out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_apiserverc_947063.h"

/* adapter._UPDATE_ALLOWED_FIELDS, sorted for a stable array. */
static const char *const AP_ALLOWED_FIELDS[] = {
    "deliver", "enabled", "name", "prompt", "repeat",
    "schedule", "skill", "skills", "timezone",
};
#define AP_ALLOWED_COUNT ((size_t)(sizeof AP_ALLOWED_FIELDS / sizeof AP_ALLOWED_FIELDS[0]))

/* The three self-judgeable string fields, in upstream order. */
static const char *const AP_STRING_FIELDS[] = {"name", "prompt", "schedule"};

/* Python truthiness of a JSON value. */
static int ap_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python `key in dict`: presence, whatever the value is. */
static int ap_has(const json_t *obj, const char *key) {
    size_t i;
    if (!obj || obj->type != JSON_OBJECT) return 0;
    for (i = 0; i < json_object_size(obj); i++) {
        const char *k = json_object_get_key_at(obj, i);
        if (k && strcmp(k, key) == 0) return 1;
    }
    return 0;
}

static int ap_allowed(const json_t *allowed, const char *key) {
    size_t i, n;
    if (!allowed || allowed->type != JSON_ARRAY) {
        for (i = 0; i < AP_ALLOWED_COUNT; i++)
            if (strcmp(AP_ALLOWED_FIELDS[i], key) == 0) return 1;
        return 0;
    }
    n = json_len(allowed);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(allowed, i);
        if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, key) == 0)
            return 1;
    }
    return 0;
}

json_t *w25_apiserverc_947063_allowed_fields(void) {
    size_t i;
    json_t *out = json_array();
    for (i = 0; i < AP_ALLOWED_COUNT; i++)
        json_append(out, json_string(AP_ALLOWED_FIELDS[i]));
    return out;
}

json_t *w25_apiserverc_947063_sanitize(const json_t *req) {
    const json_t *body = json_obj_get(req, "body");
    const json_t *allowed = json_obj_get(req, "allowed");
    json_t *out;
    size_t i;

    if (!body || body->type != JSON_OBJECT) return json_null();
    out = json_object();
    for (i = 0; i < json_object_size(body); i++) {
        const char *key = json_object_get_key_at(body, i);
        json_t *value = json_object_get_at(body, i);
        if (!key || !ap_allowed(allowed, key)) continue;
        json_set(out, key, value ? json_copy(value) : json_null());
    }
    return out;
}

/* {"status":400,"error":<text>} */
static json_t *ap_error(int status, const char *text) {
    json_t *o = json_object();
    json_set(o, "status", json_number((double)status));
    json_set(o, "error", json_string(text));
    return o;
}

json_t *w25_apiserverc_947063_invalid_update(const json_t *req) {
    const json_t *sanitized = json_obj_get(req, "sanitized");
    const json_t *max_len = json_obj_get(req, "max_name_length");
    const json_t *prompt_error = json_obj_get(req, "prompt_error");
    double max_name = 200.0;
    char buf[160];
    size_t i;

    if (max_len && max_len->type == JSON_NUMBER) max_name = max_len->num_val;
    if (!sanitized || sanitized->type != JSON_OBJECT || json_object_size(sanitized) == 0)
        return ap_error(400, "No valid fields to update");
    for (i = 0; i < sizeof AP_STRING_FIELDS / sizeof AP_STRING_FIELDS[0]; i++) {
        const char *key = AP_STRING_FIELDS[i];
        const json_t *v;
        if (!ap_has(sanitized, key)) continue;
        v = json_obj_get(sanitized, key);
        if (!v || v->type != JSON_STRING) {
            snprintf(buf, sizeof buf, "%c%s must be a string",
                     (char)(key[0] - 'a' + 'A'), key + 1);
            return ap_error(400, buf);
        }
    }
    if (ap_has(sanitized, "name")) {
        const json_t *name = json_obj_get(sanitized, "name");
        size_t n = name && name->str_val ? strlen(name->str_val) : 0;
        if ((double)n > max_name) {
            snprintf(buf, sizeof buf,
                     "Name must be \xe2\x89\xa4 %.0f characters", max_name);
            return ap_error(400, buf);
        }
    }
    if (ap_has(sanitized, "prompt") && prompt_error &&
        prompt_error->type == JSON_STRING && prompt_error->str_val[0] != '\0')
        return ap_error(400, prompt_error->str_val);
    return json_null();
}

json_t *w25_apiserverc_947063_job_status(const json_t *req) {
    const json_t *job = json_obj_get(req, "job");
    const json_t *notify = json_obj_get(req, "notify");
    const json_t *error = json_obj_get(req, "error");
    json_t *out = json_object();
    json_t *body;

    if (error && error->type == JSON_OBJECT) {
        const char *kind = json_get_str(error, "kind", "");
        const char *message = json_get_str(error, "message", "");
        body = json_object();
        if (strcmp(kind, "invalid") == 0) {
            json_set(out, "status", json_number(400.0));
            json_set(body, "error", json_string(message));
        } else {
            /* adapter._cron_error_response(e): a redacted 500. */
            json_set(out, "status", json_number(500.0));
            json_set(body, "error", json_string("internal error"));
        }
        json_set(out, "body", body);
        json_set(out, "notified", json_bool(false));
        return out;
    }
    if (!ap_truthy(job)) {
        body = json_object();
        json_set(body, "error", json_string("Job not found"));
        json_set(out, "status", json_number(404.0));
        json_set(out, "body", body);
        json_set(out, "notified", json_bool(false));
        return out;
    }
    body = json_object();
    json_set(body, "job", json_copy(job));
    json_set(out, "status", json_number(200.0));
    json_set(out, "body", body);
    json_set(out, "notified", json_bool(ap_truthy(notify) ? true : false));
    return out;
}
