/*
 * browser_preview_signins.c - C11 port of upstream
 * hermes_cli/browser_preview_signins.py (gate w25_browserpre_4511a0).
 *
 * PoP: w25_browserpre_4511a0_portable @ hermes_cli/browser_preview_signins.py:_portable
 * PoP: w25_browserpre_4511a0_export @ hermes_cli/browser_preview_signins.py:export_signins
 * PoP: w25_browserpre_4511a0_run_code @ hermes_cli/browser_preview_signins.py:run_export_signins
 * PoP: w25_browserpre_4511a0_constants @ hermes_cli/browser_preview_signins.py:<module constants>
 *
 * The CDP websocket, the OS keychain decryption inside the real browser, the
 * Electron stdout pipe and the consent lookup are seams; they arrive as fixed
 * inputs (cdp_url + err, the raw cookie list or the read error).  What is
 * ported is the decision: which cookie records survive into the partition
 * (a partitioned/CHIPS cookie set unpartitioned would widen its scope), which
 * of the three statuses the export reports, and the CLI exit code.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_browserpre_4511a0.h"

static const char *const BP_COOKIE_FIELDS[W25_BROWSERPRE_4511A0_FIELDS] = {
    "name", "value", "domain", "path", "expires",
    "httpOnly", "secure", "session", "sameSite",
};

static int bp_truthy(const json_t *v) {
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

/* _portable: None for a record the partition cannot hold faithfully. */
json_t *w25_browserpre_4511a0_portable(const json_t *cookie) {
    json_t *out;
    size_t i;

    if (!cookie || cookie->type != JSON_OBJECT) return json_null();
    if (bp_truthy(json_obj_get(cookie, "partitionKey"))) return json_null();
    if (!bp_truthy(json_obj_get(cookie, "name"))) return json_null();
    if (!bp_truthy(json_obj_get(cookie, "domain"))) return json_null();

    out = json_object();
    for (i = 0; i < W25_BROWSERPRE_4511A0_FIELDS; i++) {
        const json_t *v = json_obj_get(cookie, BP_COOKIE_FIELDS[i]);
        if (v) json_set(out, BP_COOKIE_FIELDS[i], json_copy(v));
    }
    return out;
}

json_t *w25_browserpre_4511a0_export(const json_t *state) {
    const json_t *cdp, *err, *read_error, *raw;
    json_t *out, *cookies;

    cdp = state && state->type == JSON_OBJECT ? json_obj_get(state, "cdp_url") : NULL;
    err = state && state->type == JSON_OBJECT ? json_obj_get(state, "err") : NULL;
    read_error = state && state->type == JSON_OBJECT ? json_obj_get(state, "read_error") : NULL;
    raw = state && state->type == JSON_OBJECT ? json_obj_get(state, "raw") : NULL;

    if (!cdp || cdp->type == JSON_NULL) {
        out = json_object();
        if (bp_truthy(err)) {
            json_set(out, "status", json_string("error"));
            json_set(out, "error", json_copy(err));
        } else {
            json_set(out, "status", json_string("disabled"));
        }
        return out;
    }

    if (read_error && read_error->type != JSON_NULL) {
        char *msg;
        const char *detail;
        size_t need;
        out = json_object();
        detail = read_error->type == JSON_STRING
                     ? (read_error->str_val ? read_error->str_val : "")
                     : "";
        need = strlen("could not read cookies from the real-profile browser: ") + strlen(detail) + 1;
        msg = (char *)malloc(need);
        if (!msg) return out;
        snprintf(msg, need, "could not read cookies from the real-profile browser: %s", detail);
        json_set(out, "status", json_string("error"));
        json_set(out, "error", json_string(msg));
        free(msg);
        return out;
    }

    cookies = json_array();
    {
        size_t i, n = (raw && raw->type == JSON_ARRAY) ? json_len(raw) : 0, skipped = 0;
        for (i = 0; i < n; i++) {
            json_t *p = w25_browserpre_4511a0_portable(json_get(raw, i));
            if (p && p->type != JSON_NULL) json_append(cookies, p);
            else { json_free(p); skipped++; }
        }
        out = json_object();
        json_set(out, "status", json_string("ok"));
        json_set(out, "cookies", json_copy(cookies));
        json_set(out, "skipped", json_number((double)skipped));
    }
    json_free(cookies);
    return out;
}

/* run_export_signins: 2 when the output is not a --json pipe, else per status. */
json_t *w25_browserpre_4511a0_run_code(const json_t *state) {
    const json_t *as_json, *isatty, *result, *status;
    int json_flag = 0, tty = 0;

    as_json = state && state->type == JSON_OBJECT ? json_obj_get(state, "json") : NULL;
    isatty = state && state->type == JSON_OBJECT ? json_obj_get(state, "isatty") : NULL;
    result = state && state->type == JSON_OBJECT ? json_obj_get(state, "result") : NULL;

    if (as_json) {
        if (as_json->type == JSON_BOOL) json_flag = as_json->bool_val;
        else if (as_json->type == JSON_NUMBER) json_flag = as_json->num_val != 0.0;
        else if (as_json->type == JSON_STRING) json_flag = as_json->str_val && as_json->str_val[0];
    }
    if (isatty && isatty->type == JSON_BOOL) tty = isatty->bool_val;
    else if (isatty && isatty->type == JSON_NUMBER) tty = isatty->num_val != 0.0;
    else if (isatty && isatty->type == JSON_STRING) tty = isatty->str_val && isatty->str_val[0];

    if (!json_flag || tty) return json_number(2);
    status = (result && result->type == JSON_OBJECT) ? json_obj_get(result, "status") : NULL;
    if (status && status->type == JSON_STRING && status->str_val && !strcmp(status->str_val, "error"))
        return json_number(1);
    return json_number(0);
}

json_t *w25_browserpre_4511a0_constants(void) {
    json_t *o = json_object();
    json_t *fields = json_array();
    size_t i;
    json_set(o, "cdp_timeout_s", json_number(W25_BROWSERPRE_4511A0_CDP_TIMEOUT_S));
    for (i = 0; i < W25_BROWSERPRE_4511A0_FIELDS; i++)
        json_append(fields, json_string(BP_COOKIE_FIELDS[i]));
    json_set(o, "cookie_fields", fields);
    return o;
}
