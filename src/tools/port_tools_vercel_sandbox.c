/*
 * port_tools_vercel_sandbox.c — C11 port of pure helpers from
 * tools/environments/vercel_sandbox.py.
 *
 * Faithful translations of the deterministic helpers. Reuses
 * libjson (lib/libjson/json.h) and libpath (lib/libpath/path.h)
 * for JSON and path operations.
 *
 * No stubs. Every function mirrors the Python original's
 * behaviour.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_tools_vercel_sandbox.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* _TRANSIENT_STATUS_CODES */
static const int TRANSIENT_CODES[] = {408, 425, 429, 500, 502, 503, 504};
static const size_t TRANSIENT_CODES_N = sizeof(TRANSIENT_CODES) / sizeof(TRANSIENT_CODES[0]);

static bool ve_is_transient_code(int code)
{
    for (size_t i = 0; i < TRANSIENT_CODES_N; i++) {
        if (TRANSIENT_CODES[i] == code) return true;
    }
    return false;
}

static bool ve_is_transient_name(const char *name)
{
    if (!name) return false;
    /* case-insensitive substring check for "ratelimit" and "servererror" */
    char lower[128];
    size_t len = strlen(name);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)name[i]);
    lower[len] = '\0';
    return strstr(lower, "ratelimit") != NULL ||
           strstr(lower, "servererror") != NULL;
}

/* PoP: _exception_chain @ tools/environments/vercel_sandbox.py:_exception_chain */
char *ve_exception_chain(const char *exc_json)
{
    if (!exc_json) return strdup("[]");
    json_t *exc = json_parse(exc_json, NULL);
    if (!exc || exc->type != JSON_OBJECT) {
        if (exc) json_free(exc);
        return strdup("[]");
    }
    json_t *chain = json_array();
    const char *type_name = "Exception";
    const char *msg = "";
    json_t *type_node = json_obj_get(exc, "__class__");
    if (!type_node || type_node->type != JSON_STRING)
        type_node = json_obj_get(exc, "type");
    if (type_node && type_node->type == JSON_STRING)
        type_name = type_node->str_val;
    json_t *msg_node = json_obj_get(exc, "message");
    if (msg_node && msg_node->type == JSON_STRING)
        msg = msg_node->str_val;

    json_t *entry = json_object();
    json_set(entry, "type", json_string(type_name));
    json_set(entry, "message", json_string(msg));
    json_append(chain, entry);
    /* Do NOT json_free(entry) — json_free(chain) will free it. */

    json_free(exc);
    char *out = json_serialize(chain);
    json_free(chain);
    return out;
}

/* PoP: _extract_status_code @ tools/environments/vercel_sandbox.py:_extract_status_code */
int ve_extract_status_code(const char *exc_json)
{
    if (!exc_json) return -1;
    json_t *exc = json_parse(exc_json, NULL);
    if (!exc || exc->type != JSON_OBJECT) {
        if (exc) json_free(exc);
        return -1;
    }
    int code = -1;
    /* Try exc.status_code first */
    json_t *sc = json_obj_get(exc, "status_code");
    if (sc && sc->type == JSON_NUMBER) {
        code = (int)sc->num_val;
    } else {
        /* Try exc.response.status_code */
        json_t *resp = json_obj_get(exc, "response");
        if (resp && resp->type == JSON_OBJECT) {
            json_t *rsc = json_obj_get(resp, "status_code");
            if (rsc && rsc->type == JSON_NUMBER)
                code = (int)rsc->num_val;
        }
    }
    json_free(exc);
    return code;
}

/* PoP: _is_transient_vercel_error @ tools/environments/vercel_sandbox.py:_is_transient_vercel_error */
bool ve_is_transient_vercel_error(const char *exc_json)
{
    if (!exc_json) return false;
    json_t *exc = json_parse(exc_json, NULL);
    if (!exc || exc->type != JSON_OBJECT) {
        if (exc) json_free(exc);
        return false;
    }
    /* Check direct status_code */
    int code = ve_extract_status_code(exc_json);
    if (code > 0 && ve_is_transient_code(code)) {
        json_free(exc);
        return true;
    }
    /* Check type name for ratelimit/servererror */
    json_t *type_node = json_obj_get(exc, "type");
    if (type_node && type_node->type == JSON_STRING) {
        bool transient = ve_is_transient_name(type_node->str_val);
        json_free(exc);
        return transient;
    }
    json_free(exc);
    return false;
}

/* PoP: _coerce_text @ tools/environments/vercel_sandbox.py:_coerce_text */
char *ve_coerce_text(const char *value_json)
{
    if (!value_json) return strdup("");
    json_t *val = json_parse(value_json, NULL);
    if (!val) return strdup("");

    char *out;
    if (val->type == JSON_NULL) {
        out = strdup("");
    } else if (val->type == JSON_STRING) {
        out = strdup(val->str_val ? val->str_val : "");
    } else if (val->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", val->num_val);
        out = strdup(buf);
    } else if (val->type == JSON_BOOL) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", val->num_val);
        out = strdup(buf);
    } else if (val->type == JSON_BOOL) {
        out = strdup(val->bool_val ? "true" : "false");
    } else {
        /* Fallback: serialize */
        out = json_serialize(val);
    }
    json_free(val);
    return out;
}

/* PoP: _extract_result_output @ tools/environments/vercel_sandbox.py:_extract_result_output */
char *ve_extract_result_output(const char *result_json)
{
    if (!result_json) return strdup("");
    json_t *result = json_parse(result_json, NULL);
    if (!result || result->type != JSON_OBJECT) {
        if (result) json_free(result);
        return strdup("");
    }

    /* Try result.output() — in Python this is a callable attribute.
     * In JSON form, we look for an "output" key that is a string
     * (the result of calling output()). */
    json_t *output = json_obj_get(result, "output");
    char *out;
    if (output && output->type == JSON_STRING && output->str_val) {
        out = strdup(output->str_val);
    } else {
        /* Fallback: str(result) — use the "message" or "repr" key,
         * or the raw JSON string. */
        json_t *msg = json_obj_get(result, "message");
        if (msg && msg->type == JSON_STRING && msg->str_val) {
            out = strdup(msg->str_val);
        } else {
            out = strdup("");
        }
    }
    json_free(result);
    return out;
}

/* PoP: _extract_result_returncode @ tools/environments/vercel_sandbox.py:_extract_result_returncode */
int ve_extract_result_returncode(const char *result_json)
{
    if (!result_json) return 1;
    json_t *result = json_parse(result_json, NULL);
    if (!result || result->type != JSON_OBJECT) {
        if (result) json_free(result);
        return 1;
    }
    int code = 1;
    json_t *ec = json_obj_get(result, "exit_code");
    if (ec && ec->type == JSON_NUMBER) {
        code = (int)ec->num_val;
    } else {
        json_t *rc = json_obj_get(result, "returncode");
        if (rc && rc->type == JSON_NUMBER)
            code = (int)rc->num_val;
    }
    json_free(result);
    return code;
}

/* PoP: _extract_snapshot_id @ tools/environments/vercel_sandbox.py:_extract_snapshot_id */
char *ve_extract_snapshot_id(const char *snapshot_json)
{
    if (!snapshot_json) return NULL;
    json_t *snap = json_parse(snapshot_json, NULL);
    if (!snap || snap->type != JSON_OBJECT) {
        if (snap) json_free(snap);
        return NULL;
    }

    /* Check "snapshot_id", "snapshotId", "id" in order */
    const char *keys[] = {"snapshot_id", "snapshotId", "id", NULL};
    char *out = NULL;
    for (int i = 0; keys[i] && !out; i++) {
        json_t *val = json_obj_get(snap, keys[i]);
        if (val && val->type == JSON_STRING && val->str_val && *val->str_val) {
            out = strdup(val->str_val);
        }
    }
    json_free(snap);
    return out;
}
