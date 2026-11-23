/*
 * run_idle_exit.c — C home for upstream Hermes gateway/run_idle_exit.py.
 *
 * PoP: w8_runidleexi_e437ab_idle_seconds @ gateway/run_idle_exit.py:idle_exit_seconds
 * PoP: w8_runidleexi_e437ab_client_busy @ gateway/run_idle_exit.py:_client_busy
 * PoP: w8_runidleexi_e437ab_exit_reason @ gateway/run_idle_exit.py:_begin_idle_exit
 */
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_runidleexi_e437ab.h"

#define W8_DEFAULT_IDLE_EXIT_SECONDS 600.0
static const char *const W8_IDLE_EXIT_STOP_PREFIX = "auto-started gateway idle";

/* Python float(value) coercion for the JSON scalar types used by the config value. */
static bool w8_py_float(const json_t *v, double *out) {
    if (!v) return false;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return true; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) return false;
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) return false;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return false;
        *out = d;
        return true;
    }
    return false; /* arrays / objects / null -> TypeError -> default */
}

json_t *w8_runidleexi_e437ab_idle_seconds(const json_t *value) {
    double seconds;
    if (!w8_py_float(value, &seconds)) seconds = W8_DEFAULT_IDLE_EXIT_SECONDS;
    /* Python max(0.0, seconds): NaN and negatives resolve to 0.0. */
    double result = (seconds > 0.0) ? seconds : 0.0;
    return json_number(result);
}

json_t *w8_runidleexi_e437ab_client_busy(const struct json_t *clients,
        const struct json_t *attaching, const struct json_t *last_activity,
        const struct json_t *now, const struct json_t *window) {
    double ncli = (clients && clients->type == JSON_NUMBER) ? clients->num_val : 0.0;
    if (ncli != 0.0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f client(s) attached", ncli);
        return json_string(buf);
    }
    if (attaching && attaching->type == JSON_BOOL && attaching->bool_val)
        return json_string("client attaching");
    double quiet = (now && now->type == JSON_NUMBER ? now->num_val : 0.0)
                   - (last_activity && last_activity->type == JSON_NUMBER ? last_activity->num_val : 0.0);
    double win = (window && window->type == JSON_NUMBER) ? window->num_val : 0.0;
    if (quiet < win) {
        char buf[96];
        snprintf(buf, sizeof(buf), "client activity %.0fs ago", quiet);
        return json_string(buf);
    }
    return json_null();
}

json_t *w8_runidleexi_e437ab_exit_reason(const struct json_t *window,
        const struct json_t *restart) {
    char buf[512];
    if (restart && restart->type == JSON_STRING && restart->str_val[0] != '\0')
        snprintf(buf, sizeof(buf), "%s; restarting to load %s",
                 W8_IDLE_EXIT_STOP_PREFIX, restart->str_val);
    else
        snprintf(buf, sizeof(buf), "%s for %.0fs (gateway.unmanaged_idle_exit_seconds)",
                 W8_IDLE_EXIT_STOP_PREFIX,
                 (window && window->type == JSON_NUMBER) ? window->num_val : 0.0);
    return json_string(buf);
}
