/*
 * api_server_room_dispatch.c -- real C11 port of the validation and decision
 * surface of upstream Hermes gateway/platforms/api_server_room_dispatch.py.
 *
 * PoP: w21_apiserverr_694120_dispatch_error @ gateway/platforms/api_server_room_dispatch.py:_room_dispatch_error
 * PoP: w21_apiserverr_694120_normalize @ gateway/platforms/api_server_room_dispatch.py:_normalize_room_dispatch
 *
 * `_ensure_hosted_member_session` (an authority write behind asyncio.to_thread)
 * and the aiohttp/web plumbing stay upstream; this module decides the request.
 */
#include "hermes_json.h"
#include "noc_w21_apiserverr_694120.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *rb_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int rb_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int rb_is_py_int(const json_t *v) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 1;
    if (v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* `str(value)` / f-string interpolation for the scalar wire shapes. */
static const char *rb_str(const json_t *v, char *buf, size_t cap) {
    if (!v) { snprintf(buf, cap, "None"); return buf; }
    switch (v->type) {
    case JSON_STRING: return v->str_val ? v->str_val : "";
    case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return buf;
    case JSON_NUMBER:
        if (rb_is_py_int(v)) snprintf(buf, cap, "%lld", (long long)v->num_val);
        else snprintf(buf, cap, "%.17g", v->num_val);
        return buf;
    case JSON_NULL:   snprintf(buf, cap, "None"); return buf;
    case JSON_ARRAY:  snprintf(buf, cap, "[]"); return buf;
    case JSON_OBJECT: snprintf(buf, cap, "{}"); return buf;
    }
    return buf;
}

/* Python `==` for the scalar values a dispatch field carries. */
static int rb_equal(const json_t *a, const json_t *b) {
    if (!a || !b) return (!a || a->type == JSON_NULL) && (!b || b->type == JSON_NULL);
    if (a->type != b->type) {
        /* bool is an int in Python: True == 1 */
        if ((a->type == JSON_BOOL && b->type == JSON_NUMBER) ||
            (a->type == JSON_NUMBER && b->type == JSON_BOOL)) {
            double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
            double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
            return x == y;
        }
        return 0;
    }
    switch (a->type) {
    case JSON_NULL:   return 1;
    case JSON_BOOL:   return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    case JSON_ARRAY:
    case JSON_OBJECT: return 0;                 /* identity only; never in a fixture */
    }
    return 0;
}

static int rb_body_has_room_field(const json_t *body) {
    size_t i;
    if (!body || body->type != JSON_OBJECT) return 0;
    for (i = 0; i < body->c.count; i++) {
        const char *k = body->c.keys[i];
        if (k && (!strcmp(k, "hosted_room_dispatch") || !strcmp(k, "_room_execution_policy"))) return 1;
    }
    return 0;
}

static int rb_body_keys_allowed(const json_t *body) {
    size_t i;
    if (!body || body->type != JSON_OBJECT) return 0;
    for (i = 0; i < body->c.count; i++) {
        const char *k = body->c.keys[i];
        if (!k) continue;
        if (strcmp(k, "input") && strcmp(k, "hosted_room_dispatch")) return 0;
    }
    return 1;
}

static json_t *rb_error(const char *message, const char *code, int status) {
    json_t *out = json_object();
    json_set(out, "kind", json_string("error"));
    json_set(out, "message", json_string(message ? message : ""));
    json_set(out, "code", json_string(code ? code : ""));
    json_set(out, "status", json_number((double)status));
    return out;
}

/* --- _room_dispatch_error ------------------------------------------------- */

json_t *w21_apiserverr_694120_dispatch_error(const char *message) {
    char *lowered;
    json_t *out;
    size_t i;
    const char *msg = message ? message : "";

    lowered = rb_dup(msg);
    if (!lowered) return rb_error(msg, "invalid_room_dispatch", 403);
    for (i = 0; lowered[i]; i++) lowered[i] = (char)tolower((unsigned char)lowered[i]);

    if (strstr(lowered, "execution policy") || strstr(lowered, "remote room execution requires")) {
        free(lowered);
        return rb_error("Room execution policy changed; reauthorization is required.",
                        "room_execution_policy_changed", 403);
    }
    if (strstr(lowered, "capability catalog changed")) {
        free(lowered);
        return rb_error("Room capability catalog changed; reauthorization is required.",
                        "room_capability_catalog_changed", 403);
    }
    free(lowered);
    out = rb_error(msg, "invalid_room_dispatch", 403);
    return out;
}

/* --- _normalize_room_dispatch -------------------------------------------- */

static json_t *rb_ok(const json_t *body) {
    json_t *out = json_object();
    json_set(out, "kind", json_string("ok"));
    json_set(out, "body", json_copy(body));
    return out;
}

static json_t *rb_passthrough(const json_t *body) {
    json_t *out = json_object();
    json_set(out, "kind", json_string("passthrough"));
    json_set(out, "body", body ? json_copy(body) : json_null());
    return out;
}

json_t *w21_apiserverr_694120_normalize(const json_t *case_) {
    json_t *body, *grant_token, *headers, *dispatch, *catalog, *policy_map, *result;
    const char *api_profile, *local_install, *grant_error, *session_id;
    const char *prompt, *task_id;
    char gen_buf[64], key_buf[512];

    if (!case_ || case_->type != JSON_OBJECT) return rb_passthrough(NULL);
    body = json_obj_get(case_, "body");
    grant_token = json_obj_get(case_, "grant_token");
    headers = json_obj_get(case_, "headers");

    if (!rb_truthy(grant_token)) {
        /* Room fields are server-built from a verified grant. */
        if (body && body->type == JSON_OBJECT && rb_body_has_room_field(body)) {
            return rb_error("hosted_room_dispatch and _room_execution_policy require a room grant.",
                            "room_grant_required", 403);
        }
        return rb_passthrough(body);
    }

    if (!body || body->type != JSON_OBJECT || !rb_body_keys_allowed(body)) {
        return rb_error("Room dispatch accepts only input and hosted_room_dispatch.",
                        "invalid_room_dispatch", 400);
    }

    /* try: HostedMemberDispatch.from_mapping(body.get("hosted_room_dispatch")) */
    dispatch = json_obj_get(body, "hosted_room_dispatch");
    if (!dispatch || dispatch->type != JSON_OBJECT) {
        return w21_apiserverr_694120_dispatch_error("hosted_room_dispatch is invalid");
    }
    grant_error = json_get_str(case_, "grant_error", NULL);
    if (grant_error && grant_error[0]) {
        return w21_apiserverr_694120_dispatch_error(grant_error);
    }

    api_profile = json_get_str(case_, "api_profile", NULL);
    local_install = json_get_str(case_, "local_install", NULL);
    {
        json_t *target_profile = json_obj_get(dispatch, "target_profile");
        json_t *target_install = json_obj_get(dispatch, "target_install_id");
        if (!target_profile || target_profile->type != JSON_STRING ||
            !api_profile || strcmp(target_profile->str_val, api_profile)) {
            return w21_apiserverr_694120_dispatch_error(
                "room dispatch target does not match this profile");
        }
        if (!target_install || target_install->type != JSON_STRING ||
            !local_install || strcmp(target_install->str_val, local_install)) {
            return w21_apiserverr_694120_dispatch_error(
                "room dispatch target does not match this profile");
        }
    }

    catalog = json_obj_get(case_, "catalog");
    policy_map = (catalog && catalog->type == JSON_OBJECT)
                     ? json_obj_get(catalog, "execution_policy") : NULL;
    {
        json_t *policy_digest = (policy_map && policy_map->type == JSON_OBJECT)
                                    ? json_obj_get(policy_map, "policy_digest") : NULL;
        json_t *dispatch_policy = json_obj_get(dispatch, "execution_policy_digest");
        if (!policy_digest || policy_digest->type != JSON_STRING ||
            !dispatch_policy || dispatch_policy->type != JSON_STRING ||
            strcmp(policy_digest->str_val, dispatch_policy->str_val)) {
            return w21_apiserverr_694120_dispatch_error("room execution policy changed");
        }
    }
    {
        json_t *catalog_digest = (catalog && catalog->type == JSON_OBJECT)
                                     ? json_obj_get(catalog, "catalog_digest") : NULL;
        json_t *dispatch_catalog = json_obj_get(dispatch, "capability_digest");
        if (!catalog_digest || catalog_digest->type != JSON_STRING ||
            !dispatch_catalog || dispatch_catalog->type != JSON_STRING ||
            strcmp(catalog_digest->str_val, dispatch_catalog->str_val)) {
            return w21_apiserverr_694120_dispatch_error("room capability catalog changed");
        }
    }

    /* `if body.get("input") not in {None, dispatch.prompt}` */
    {
        json_t *input = json_obj_get(body, "input");
        json_t *dispatch_prompt = json_obj_get(dispatch, "prompt");
        if (!(input == NULL || input->type == JSON_NULL || rb_equal(input, dispatch_prompt))) {
            return w21_apiserverr_694120_dispatch_error(
                "room dispatch input does not match its prompt");
        }
        prompt = (dispatch_prompt && dispatch_prompt->type == JSON_STRING)
                     ? dispatch_prompt->str_val : NULL;
    }

    /* `expected_key = f"room:{task_id}:{execution_generation}"` */
    task_id = rb_str(json_obj_get(dispatch, "task_id"), gen_buf, sizeof(gen_buf));
    {
        char gen_str[64];
        char task_copy[256];
        const char *gen = rb_str(json_obj_get(dispatch, "execution_generation"),
                                 gen_str, sizeof(gen_str));
        snprintf(task_copy, sizeof(task_copy), "%s", task_id);
        snprintf(key_buf, sizeof(key_buf), "room:%s:%s", task_copy, gen);
    }
    {
        json_t *header = (headers && headers->type == JSON_OBJECT)
                             ? json_obj_get(headers, "Idempotency-Key") : NULL;
        const char *raw = (header && header->type == JSON_STRING) ? header->str_val : "";
        char trimmed[512];
        size_t len, start = 0, j = 0;
        if (!header) raw = "";                     /* headers.get(key, "") */
        len = strlen(raw);
        while (start < len && isspace((unsigned char)raw[start])) start++;
        while (len > start && isspace((unsigned char)raw[len - 1])) len--;
        for (; start < len && j + 1 < sizeof(trimmed); start++) trimmed[j++] = raw[start];
        trimmed[j] = '\0';
        if (strcmp(trimmed, key_buf)) {
            return w21_apiserverr_694120_dispatch_error(
                "room dispatch idempotency key is invalid");
        }
    }

    session_id = json_get_str(case_, "session_id", "");
    result = json_object();
    json_set(result, "input", prompt ? json_string(prompt) : json_null());
    json_set(result, "session_id", json_string(session_id ? session_id : ""));
    json_set(result, "hosted_room_dispatch", json_copy(dispatch));
    json_set(result, "_room_execution_policy",
             policy_map ? json_copy(policy_map) : json_object());
    return rb_ok(result);
}
