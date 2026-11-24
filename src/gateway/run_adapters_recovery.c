/*
 * run_adapters_recovery.c — C port of upstream Hermes gateway/run_adapters_recovery.py.
 *
 * PoP: w17_runadapter_1b5147_unavailable_message @ gateway/run_adapters_recovery.py:_adapter_unavailable_message
 * PoP: w17_runadapter_1b5147_is_token_contention @ gateway/run_adapters_recovery.py:is_token_contention
 * PoP: w17_runadapter_1b5147_discount_suspended_wait @ gateway/run_adapters_recovery.py:discount_suspended_wait
 * PoP: w17_runadapter_1b5147_token_contention_reconnect_delay @ gateway/run_adapters_recovery.py:_token_contention_reconnect_delay
 *
 * Time is injected from the fixture (time.monotonic is not reproducible).
 */
#include "noc_w17_runadapter_1b5147.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python: f"No adapter available for enabled {platform.value}; check the plugin, dependencies, and credentials."
 *        + (" Retrying in the background." if retrying else "") */
struct json_t *w17_runadapter_1b5147_unavailable_message(const char *platform_value, int retrying) {
    char buf[2048];
    if (!platform_value) platform_value = "";
    int n = snprintf(buf, sizeof(buf),
        "No adapter available for enabled %s; check the plugin, dependencies, and credentials.",
        platform_value);
    if (retrying && n > 0 && (size_t)n < sizeof(buf)) {
        const char *tail = " Retrying in the background.";
        size_t tlen = strlen(tail);
        if ((size_t)n + tlen < sizeof(buf)) {
            memcpy(buf + n, tail, tlen + 1); /* includes NUL */
        }
    }
    return json_string(buf);
}

/* Python: (error_code or "").strip().lower().endswith("_polling_conflict") */
int w17_runadapter_1b5147_is_token_contention(const char *error_code) {
    if (!error_code) return 0;
    const char *p = error_code;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    static const char suffix[] = "_polling_conflict";
    size_t sl = sizeof(suffix) - 1;
    if ((size_t)(e - p) < sl) return 0;
    const char *start = e - sl;
    for (size_t i = 0; i < sl; i++) {
        if ((unsigned char)start[i] >= 'A' && start[i] <= 'Z') {
            if (start[i] + 32 != (unsigned char)suffix[i]) return 0;
        } else if ((unsigned char)start[i] != (unsigned char)suffix[i]) return 0;
    }
    return 1;
}

/* Python: overrun = time.monotonic() - waited_from - planned
 *         if overrun <= 60.0: return
 *         for info in entries: q=info.get("queued_at"); if q is not None and q <= waited_from+planned: q += overrun */
struct json_t *w17_runadapter_1b5147_discount_suspended_wait(const struct json_t *entries,
                                                   double waited_from, double planned, double now) {
    json_t *out = json_array();
    if (!json_is_array(entries)) return out;
    double overrun = now - waited_from - planned;
    double cutoff = waited_from + planned;
    size_t n = json_len(entries);
    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(entries, i);
        const json_t *q = json_is_object(e) ? json_obj_get(e, "queued_at") : NULL;
        if (!q || q->type == JSON_NULL) {
            json_append(out, json_null());
        } else if (q->type == JSON_NUMBER) {
            double v = q->num_val;
            if (overrun > 60.0 && v <= cutoff) v = v + overrun;
            json_append(out, json_number(v));
        } else {
            json_append(out, json_copy(q));
        }
    }
    return out;
}

/* ---- _token_contention_reconnect_delay ---- */
#define RA_DELAY_COUNT 3
static const double RA_DELAYS[RA_DELAY_COUNT] = {120.0, 300.0, 600.0};
#define RA_EPISODE_SECS 1800.0

static char *ra_fatal_message(const char *platform_value) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "%s bot token is in use elsewhere: another process (another machine or a leftover gateway) "
        "kept polling it through %d reconnects. Stop that poller, then run 'hermes gateway restart'.",
        platform_value ? platform_value : "", RA_DELAY_COUNT);
    size_t len = strlen(buf);
    char *s = (char *)malloc(len + 1);
    if (s) memcpy(s, buf, len + 1);
    return s;
}

static json_t *ra_strikes_out(const json_t *strikes) {
    json_t *o = json_object();
    if (json_is_object(strikes)) {
        for (size_t i = 0; i < json_object_size(strikes); i++) {
            const char *k = json_object_get_key_at(strikes, i);
            const json_t *v = json_object_get_at(strikes, i);
            json_t *pair = json_array();
            if (json_is_array(v)) {
                const json_t *c = json_get(v, 0);
                const json_t *l = json_get(v, 1);
                json_append(pair, json_number(c && c->type == JSON_NUMBER ? c->num_val : 0.0));
                json_append(pair, json_number(l && l->type == JSON_NUMBER ? l->num_val : 0.0));
            } else {
                json_append(pair, json_number(0.0));
                json_append(pair, json_number(0.0));
            }
            json_set(o, k ? k : "", pair);
        }
    }
    return o;
}

struct json_t *w17_runadapter_1b5147_token_contention_reconnect_delay(const struct json_t *fx) {
    const json_t *adapter = json_obj_get(fx, "adapter");
    int retryable = json_get_bool(adapter, "fatal_error_retryable", false) ? 1 : 0;
    const char *code = json_get_str(adapter, "fatal_error_code", "");
    const char *pval = json_get_str(adapter, "platform_value", "");
    const char *key = json_get_str(fx, "status_key", "");
    double now = json_get_num(fx, "now", 0.0);
    const json_t *strikes_in = json_obj_get(fx, "strikes");

    /* Gather the output strike table as a mutable json object. */
    json_t *strikes = ra_strikes_out(strikes_in);

    double delay = 0.0;
    int fatal = 0;
    char *fatal_msg = NULL;

    if (retryable && w17_runadapter_1b5147_is_token_contention(code)) {
        double count = 0.0, last = now;
        const json_t *cur = json_is_object(strikes) ? json_obj_get(strikes, key) : NULL;
        if (json_is_array(cur)) {
            const json_t *c = json_get(cur, 0), *l = json_get(cur, 1);
            count = c && c->type == JSON_NUMBER ? c->num_val : 0.0;
            last = l && l->type == JSON_NUMBER ? l->num_val : now;
        }
        count = (now - last > RA_EPISODE_SECS) ? 1.0 : count + 1.0;
        json_t *pair = json_array();
        json_append(pair, json_number(count));
        json_append(pair, json_number(now));
        json_set(strikes, key, pair);

        if (count <= RA_DELAY_COUNT) {
            delay = RA_DELAYS[(int)count - 1];
        } else {
            json_obj_del(strikes, key);
            fatal = 1;
            fatal_msg = ra_fatal_message(pval);
        }
    }

    json_t *out = json_object();
    json_set(out, "delay", json_number(delay));
    json_set(out, "strikes", strikes);
    json_set(out, "fatal", json_bool(fatal ? true : false));
    json_set(out, "fatal_message", fatal_msg ? json_string(fatal_msg) : json_null());
    free(fatal_msg);
    return out;
}
