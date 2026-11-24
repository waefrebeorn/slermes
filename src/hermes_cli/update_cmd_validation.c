/*
 * update_cmd_validation.c -- real C11 port of hermes_cli/update_cmd_validation.py.
 *
 * PoP: w22_updatecmdv_ce00f8_probe_failure @ hermes_cli/update_cmd_validation.py:_probe_failure
 * PoP: w22_updatecmdv_ce00f8_failures @ hermes_cli/update_cmd_validation.py:_critical_module_import_failures
 * PoP: w22_updatecmdv_ce00f8_validate @ hermes_cli/update_cmd_validation.py:_validate_critical_modules_import
 * PoP: w22_updatecmdv_ce00f8_probe_timeout @ hermes_cli/update_cmd_validation.py:_critical_module_import_failures
 * PoP: w22_updatecmdv_ce00f8_modules @ hermes_cli/update_cmd_validation.py:_UPDATE_CRITICAL_MODULES
 *
 * The subprocess is the seam (its stdout and return code are injected); the
 * probe text, the marker, the payload decode and the first-failure pick are
 * ported. Payload decoding is strict like json.loads: a complete value
 * followed by anything but whitespace is a malformed payload, not a partial
 * read.
 */
#include "hermes_json.h"
#include "noc_w22_updatecmdv_ce00f8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const W22_UPDATECMDV_CE00F8_MODULES[5] = {
    "hermes_cli.main", "run_agent", "model_tools", "toolsets", NULL
};

static const char MARKER_PREFIX[] = "__HERMES_IMPORT_HEALTH_";
#define MARKER_PREFIX_LEN 23u /* strlen("__HERMES_IMPORT_HEALTH_") */
#define MARKER_HEX_LEN 32u
#define MARKER_TOTAL_LEN (MARKER_PREFIX_LEN + MARKER_HEX_LEN + 2u)

static int upd_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int upd_lower_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

/* Offset of the last `__HERMES_IMPORT_HEALTH_<32 lowercase hex>__`, or -1. */
static long upd_last_marker(const char *s) {
    size_t n, i;
    long found = -1;
    if (!s) return -1;
    n = strlen(s);
    for (i = 0; i + MARKER_TOTAL_LEN <= n; i++) {
        size_t j;
        int ok = 1;
        if (memcmp(s + i, MARKER_PREFIX, MARKER_PREFIX_LEN) != 0) continue;
        for (j = 0; j < MARKER_HEX_LEN; j++) {
            if (!upd_lower_hex(s[i + MARKER_PREFIX_LEN + j])) { ok = 0; break; }
        }
        if (ok && strncmp(s + i + MARKER_PREFIX_LEN + MARKER_HEX_LEN, "__", 2) == 0) {
            found = (long)i;
        }
    }
    return found;
}

/* End offset of the top-level JSON value starting at `start`, 0 if unbalanced. */
static size_t upd_value_end(const char *s, size_t start) {
    size_t p = start;
    char open;
    if (!s) return 0;
    while (s[p] && upd_ws(s[p])) p++;
    open = s[p];
    if (open == '{' || open == '[') {
        char close = (open == '{') ? '}' : ']';
        int depth = 0;
        int in_str = 0;
        for (; s[p]; p++) {
            char c = s[p];
            if (in_str) {
                if (c == '\\' && s[p + 1]) { p++; continue; }
                if (c == '"') in_str = 0;
                continue;
            }
            if (c == '"') { in_str = 1; continue; }
            if (c == open) depth++;
            else if (c == close) {
                depth--;
                if (depth == 0) return p + 1;
            }
        }
        return 0;
    }
    if (open == '"') {
        for (p++; s[p]; p++) {
            if (s[p] == '\\' && s[p + 1]) { p++; continue; }
            if (s[p] == '"') return p + 1;
        }
        return 0;
    }
    while (s[p] && !upd_ws(s[p])) p++;
    return p;
}

static json_t *upd_probe_failure(const char *kind, const char *detail) {
    json_t *out = json_object();
    json_t *tuple = json_array();
    json_append(tuple, kind ? json_string(kind) : json_null());
    json_append(tuple, detail ? json_string(detail) : json_null());
    json_set(out, "critical-module probe", tuple);
    return out;
}

json_t *w22_updatecmdv_ce00f8_probe_failure(const char *kind, const char *detail) {
    return upd_probe_failure(kind, detail);
}

json_t *w22_updatecmdv_ce00f8_probe_timeout(void) {
    return upd_probe_failure("TimeoutExpired", "timed out before reporting import health");
}

/* Decode the payload into the failure map, or NULL when it is malformed. */
static json_t *upd_decode(const char *payload) {
    json_t *failures, *out;
    size_t i, end;

    if (!payload) return NULL;
    end = upd_value_end(payload, 0);
    if (!end) return NULL;
    for (i = end; payload[i]; i++) {
        if (!upd_ws(payload[i])) return NULL; /* json.loads rejects trailing bytes */
    }
    failures = json_parse(payload, NULL);
    if (!failures) return NULL;
    if (!failures || failures->type != JSON_ARRAY) { json_free(failures); return NULL; }
    out = json_object();
    for (i = 0; i < (size_t)json_len(failures); i++) {
        json_t *item = json_get(failures, i);
        json_t *tuple;
        size_t j;
        if (!item || item->type != JSON_ARRAY || json_len(item) != 3) {
            json_free(out);
            json_free(failures);
            return NULL;
        }
        for (j = 0; j < 3; j++) {
            json_t *v = json_get(item, j);
            if (!v || v->type != JSON_STRING) {
                json_free(out);
                json_free(failures);
                return NULL;
            }
        }
        tuple = json_array();
        json_append(tuple, json_string(json_get(item, 1)->str_val));
        json_append(tuple, json_string(json_get(item, 2)->str_val));
        /* duplicate modules: last tuple wins, first position kept (json_set) */
        json_set(out, json_get(item, 0)->str_val, tuple);
    }
    json_free(failures);
    return out;
}

json_t *w22_updatecmdv_ce00f8_failures(const char *stdout_text, const char *returncode) {
    long at;
    const char *payload;
    json_t *decoded;
    char detail[160];
    long long rc = returncode ? strtoll(returncode, NULL, 10) : 0;

    at = upd_last_marker(stdout_text);
    if (at < 0) {
        snprintf(detail, sizeof(detail),
                 "terminated before reporting import health (exit code %lld)", rc);
        return upd_probe_failure("ProbeTerminated", detail);
    }
    payload = stdout_text + at + (long)MARKER_TOTAL_LEN;
    decoded = upd_decode(payload);
    if (!decoded) return upd_probe_failure("MalformedPayload", "reported malformed import health data");
    return decoded;
}

json_t *w22_updatecmdv_ce00f8_validate(const char *stdout_text, const char *returncode) {
    json_t *failures = w22_updatecmdv_ce00f8_failures(stdout_text, returncode);
    json_t *out = json_array();
    const char *module = json_object_get_key_at(failures, 0);

    if (module) {
        json_t *tuple = json_object_get_at(failures, 0);
        json_t *detail = (tuple && tuple->type == JSON_ARRAY) ? json_get(tuple, 1) : NULL;
        json_append(out, json_bool(0));
        json_append(out, json_string(module));
        json_append(out, json_string(detail && detail->type == JSON_STRING ? detail->str_val : ""));
    } else {
        json_append(out, json_bool(1));
        json_append(out, json_null());
        json_append(out, json_null());
    }
    json_free(failures);
    return out;
}
