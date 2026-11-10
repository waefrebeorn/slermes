/*
 * port_dingtalk_auth.c
 *
 * Pure helpers ported from hermes_cli/dingtalk_auth.py - DingTalk device-flow
 * OAuth registration.
 *
 * The module's real work is HTTP (requests.post against the registration API).
 * What is portable is the response interpretation:
 *
 *   dt_normalize_poll_status <- dingtalk_auth.py:_POLL_STATUSES + poll_registration
 *   dt_str_field            <- poll_registration's field extraction
 *
 * The network call stays in Python; a C body would have to invent it.
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* dingtalk_auth.py:_POLL_STATUSES */
static const char *const DT_POLL_STATUSES[] = {
    "WAITING", "SUCCESS", "FAIL", "EXPIRED", NULL
};

/* ===========================================================================
 *  poll_registration's status line  (dingtalk_auth.py:poll_registration)
 *
 *      status_raw = str(data.get("status", "")).strip().upper()
 *      result = {"status": status_raw if status_raw in _POLL_STATUSES else "UNKNOWN"}
 *
 *  Any status outside the four known values becomes "UNKNOWN" - an unrecognised
 *  value must not be passed through to the caller as if it were a real state.
 *  Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: poll_registration @ hermes_cli/dingtalk_auth.py:poll_registration */
char *dt_normalize_poll_status(const char *status)
{
    const char *raw = (status && status[0]) ? status : "";

    /* str() of a non-string would be the repr; the Python side only ever hands
     * this a string from the JSON body, so NULL/"" is the empty case. */
    char up[128];
    size_t n = 0;
    for (const char *p = raw; *p && n < sizeof up - 1; p++)
        up[n++] = (char)toupper((unsigned char)*p);
    up[n] = '\0';

    /* strip() */
    size_t start = 0, end = n;
    while (start < end && isspace((unsigned char)up[start])) start++;
    while (end > start && isspace((unsigned char)up[end - 1])) end--;

    for (int i = 0; DT_POLL_STATUSES[i]; i++) {
        size_t sl = strlen(DT_POLL_STATUSES[i]);
        if (sl == end - start && strncmp(up + start, DT_POLL_STATUSES[i], sl) == 0) {
            char *out = (char *)malloc(sl + 1);
            if (!out) return NULL;
            memcpy(out, DT_POLL_STATUSES[i], sl + 1);
            return out;
        }
    }
    char *out = (char *)malloc(8);
    if (!out) return NULL;
    strcpy(out, "UNKNOWN");
    return out;
}

/* ===========================================================================
 *  poll_registration's field extraction  (dingtalk_auth.py:poll_registration)
 *
 *      {key: str(data.get(key, "")).strip() or None
 *       for key in ("client_id", "client_secret", "fail_reason")}
 *
 *  Empty-after-strip becomes NULL (Python's `or None`), which is how the
 *  caller distinguishes "absent" from "present but blank".
 * =========================================================================== */

/* PoP: poll_registration @ hermes_cli/dingtalk_auth.py:poll_registration */
char *dt_str_field(const json_t *data, const char *key)
{
    if (!data || data->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(data, key);
    if (!v) return NULL;

    const char *raw = "";
    char numbuf[64];
    if (v->type == JSON_STRING) raw = v->str_val ? v->str_val : "";
    else if (v->type == JSON_NUMBER) { snprintf(numbuf, sizeof numbuf, "%.17g", v->num_val); raw = numbuf; }
    else if (v->type == JSON_BOOL) raw = v->bool_val ? "True" : "False";
    else return NULL;   /* null / object / array -> str() is not a plain field */

    size_t len = strlen(raw), start = 0, end = len;
    while (start < end && isspace((unsigned char)raw[start])) start++;
    while (end > start && isspace((unsigned char)raw[end - 1])) end--;

    if (end == start) return NULL;   /* `or None` */

    char *out = (char *)malloc(end - start + 1);
    if (!out) return NULL;
    memcpy(out, raw + start, end - start);
    out[end - start] = '\0';
    return out;
}
