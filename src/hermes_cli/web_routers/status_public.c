/*
 * status_public.c -- real C11 port of the anonymous /api/status projection of
 * upstream Hermes hermes_cli/web_routers/status_public.py.
 *
 * PoP: w21_statuspubl_e796de_anonymous_entry @ hermes_cli/web_routers/status_public.py:_anonymous_platform_entry
 * PoP: w21_statuspubl_e796de_redact @ hermes_cli/web_routers/status_public.py:redact_status_for_unverified
 *
 * The two helpers this module delegates to are ported here because their whole
 * semantics decide the projection: gateway.status.normalize_updated_at (ISO
 * strings only -- the module never passes a number) and
 * agent.monitoring.gateway_health.classify_exit_reason.
 */
#include "hermes_json.h"
#include "noc_w21_statuspubl_e796de.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sp_truthy(const json_t *v) {
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

static int sp_is_py_int(const json_t *v) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 1;
    if (v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* `str(value or "")`, lower-cased on request, for the scalar shapes. */
static const char *sp_str(const json_t *v, char *buf, size_t cap, int lower) {
    const char *text;
    size_t i;
    if (!sp_truthy(v)) { if (cap) buf[0] = '\0'; return buf; }
    switch (v->type) {
    case JSON_STRING: text = v->str_val ? v->str_val : ""; break;
    case JSON_BOOL:   text = v->bool_val ? "True" : "False"; break;
    case JSON_NUMBER:
        if (sp_is_py_int(v)) snprintf(buf, cap, "%lld", (long long)v->num_val);
        else snprintf(buf, cap, "%.17g", v->num_val);
        text = buf;
        break;
    default: text = ""; break;
    }
    if (!lower) {
        if (text != buf) snprintf(buf, cap, "%s", text);
        return buf;
    }
    if (text != buf) snprintf(buf, cap, "%s", text);
    for (i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    return buf;
}

/* --- gateway.status.normalize_updated_at (string branch) ------------------ */

static int sp_digits(const char *s, int pos, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (!isdigit((unsigned char)s[pos + i])) return 0;
    }
    return 1;
}

static int sp_at(const char *s, int pos, int n) {
    int value = 0, i;
    for (i = 0; i < n; i++) value = value * 10 + (s[pos + i] - '0');
    return value;
}

static int sp_days_in_month(int year, int month) {
    static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2) {
        int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return DAYS[month - 1];
}

/* `datetime.fromisoformat(raw).isoformat()` with a UTC tzinfo when naive.
 * 1 on a parseable stamp (written to out), 0 where Python returns None. */
static int sp_normalize_stamp(const char *value, char *out, size_t cap) {
    char work[128];
    char normalized[128];
    int pos = 0, year, month, day, hour = 0, minute = 0, second = 0, micro = 0;
    char tzsign = '+';
    int tzh = 0, tzm = 0, have_tz = 0, n = 0;
    size_t len;

    if (!value) return 0;
    snprintf(work, sizeof(work), "%s", value);
    len = strlen(work);
    /* raw = value.strip() */
    {
        size_t start = 0;
        while (start < len && isspace((unsigned char)work[start])) start++;
        while (len > start && isspace((unsigned char)work[len - 1])) len--;
        memmove(work, work + start, len - start);
        work[len - start] = '\0';
        len = len - start;
    }
    if (len >= 2 && (work[len - 1] == 'Z' || work[len - 1] == 'z')) {
        /* raw = raw[:-1] + "+00:00" */
        if (len + 6 >= sizeof(work)) return 0;
        work[len - 1] = '\0';
        strcat(work, "+00:00");
        len = strlen(work);
    }
    if (len < 10) return 0;

    if (!sp_digits(work, 0, 4) || work[4] != '-' || !sp_digits(work, 5, 2) ||
        work[7] != '-' || !sp_digits(work, 8, 2)) {
        return 0;
    }
    year = sp_at(work, 0, 4);
    month = sp_at(work, 5, 2);
    day = sp_at(work, 8, 2);
    if (year < 1 || month < 1 || month > 12) return 0;
    if (day < 1 || day > sp_days_in_month(year, month)) return 0;
    pos = 10;

    if (pos < (int)len) {
        if (work[pos] != 'T' && work[pos] != 't' && work[pos] != ' ') return 0;
        pos++;
        if (pos + 5 > (int)len || !sp_digits(work, pos, 2) || work[pos + 2] != ':' ||
            !sp_digits(work, pos + 3, 2)) {
            return 0;
        }
        hour = sp_at(work, pos, 2);
        minute = sp_at(work, pos + 3, 2);
        if (hour > 23 || minute > 59) return 0;
        pos += 5;
        if (pos < (int)len && work[pos] == ':') {
            pos++;
            if (pos + 2 > (int)len || !sp_digits(work, pos, 2)) return 0;
            second = sp_at(work, pos, 2);
            if (second > 59) return 0;
            pos += 2;
            if (pos < (int)len && (work[pos] == '.' || work[pos] == ',')) {
                int digits = 0;
                pos++;
                while (pos < (int)len && isdigit((unsigned char)work[pos]) && digits < 6) {
                    micro = micro * 10 + (work[pos] - '0');
                    digits++;
                    pos++;
                }
                if (digits == 0) return 0;
                while (pos < (int)len && isdigit((unsigned char)work[pos])) pos++;
                while (digits < 6) { micro *= 10; digits++; }
            }
        }
        if (pos < (int)len) {
            if (work[pos] != '+' && work[pos] != '-') return 0;
            tzsign = work[pos++];
            if (pos + 2 > (int)len || !sp_digits(work, pos, 2)) return 0;
            tzh = sp_at(work, pos, 2);
            pos += 2;
            if (pos >= (int)len || work[pos] != ':') return 0;
            pos++;
            if (pos + 2 > (int)len || !sp_digits(work, pos, 2)) return 0;
            tzm = sp_at(work, pos, 2);
            pos += 2;
            if (tzh > 23 || tzm > 59) return 0;
            have_tz = 1;
        }
    }
    if (pos != (int)len) return 0;
    (void)have_tz;

    n = snprintf(normalized, sizeof(normalized), "%04d-%02d-%02dT%02d:%02d:%02d",
                 year, month, day, hour, minute, second);
    if (micro != 0) {
        n += snprintf(normalized + n, sizeof(normalized) - (size_t)n, ".%06d", micro);
    }
    snprintf(normalized + n, sizeof(normalized) - (size_t)n, "%c%02d:%02d", tzsign, tzh, tzm);
    snprintf(out, cap, "%s", normalized);
    return 1;
}

/* --- agent.monitoring.gateway_health.classify_gateway_error --------------- */

static int sp_has(const char *text, const char *needle) {
    return strstr(text, needle) != NULL;
}

static const char *sp_classify_error(const char *text) {
    if (sp_has(text, "auth") || sp_has(text, "token") || sp_has(text, "unauthorized") ||
        sp_has(text, "forbidden") || sp_has(text, "401") || sp_has(text, "403")) {
        return "auth_failed";
    }
    if (sp_has(text, "rate") && sp_has(text, "limit")) return "rate_limited";
    if (sp_has(text, "timeout") || sp_has(text, "timed out")) return "timeout";
    if (sp_has(text, "network") || sp_has(text, "connection") || sp_has(text, "dns") ||
        sp_has(text, "socket") || sp_has(text, "connect call failed") ||
        sp_has(text, "failed to connect") || sp_has(text, "cannot connect") ||
        sp_has(text, "unreachable") || sp_has(text, "name resolution")) {
        return "network_error";
    }
    if (sp_has(text, "config") || sp_has(text, "missing") || sp_has(text, "invalid")) {
        return "invalid_config";
    }
    if (sp_has(text, "startup")) return "startup_failed";
    if (sp_has(text, "fatal")) return "platform_fatal";
    return "unknown";
}

/* `classify_exit_reason(raw, state=..., restart_requested=False)`. */
static json_t *sp_exit_reason(const json_t *raw, const json_t *state) {
    char state_buf[128], raw_buf[512];
    const char *state_name = sp_str(state, state_buf, sizeof(state_buf), 1);
    const char *text;
    const char *classified;

    if (raw == NULL && strcmp(state_name, "startup_failed")) return json_null();
    text = sp_str(raw, raw_buf, sizeof(raw_buf), 1);
    classified = sp_classify_error(text);
    if (!strcmp(state_name, "startup_failed")) {
        return json_string(strcmp(classified, "unknown") ? classified : "startup_failed");
    }
    if (sp_has(text, "signal") || sp_has(text, "sigterm") || sp_has(text, "sigint")) {
        return json_string("signal");
    }
    if (!strcmp(state_name, "stopped") &&
        (sp_has(text, "shutdown") || sp_has(text, "stop"))) {
        return json_string("planned_stop");
    }
    return json_string(classified);
}

/* --- status_public ------------------------------------------------------- */

static const char *const SP_PUBLIC_STATES[] = {
    "connected", "connecting", "disabled", "disconnected", "fatal",
    "paused", "retrying", "running", "ok", NULL
};

/* `_PUBLIC_CODE_RE.fullmatch(code)` for `[a-z][a-z0-9_]{0,63}`. */
static int sp_public_code(const char *code) {
    size_t i, len;
    if (!code) return 0;
    len = strlen(code);
    if (len < 1 || len > 64) return 0;
    if (code[0] < 'a' || code[0] > 'z') return 0;
    for (i = 1; i < len; i++) {
        char c = code[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

static int sp_public_state(const char *state) {
    int i;
    for (i = 0; SP_PUBLIC_STATES[i]; i++) {
        if (!strcmp(state, SP_PUBLIC_STATES[i])) return 1;
    }
    return 0;
}

json_t *w21_statuspubl_e796de_anonymous_entry(const json_t *value) {
    static const char *const STAMP_KEYS[] = {"retrying_since", "updated_at"};
    json_t *entry = json_object();
    json_t *state, *code, *attention;
    int i;

    if (!value || value->type != JSON_OBJECT) return entry;

    state = json_obj_get(value, "state");
    if (state && state->type != JSON_NULL) {
        if (state->type == JSON_STRING && sp_public_state(state->str_val)) {
            json_set(entry, "state", json_string(state->str_val));
        } else {
            json_set(entry, "state", json_string("unknown"));
        }
    }
    code = json_obj_get(value, "error_code");
    if (code && code->type == JSON_STRING && sp_public_code(code->str_val)) {
        json_set(entry, "error_code", json_string(code->str_val));
    }
    attention = json_obj_get(value, "needs_attention");
    if (attention && attention->type == JSON_BOOL) {
        json_set(entry, "needs_attention", json_bool(attention->bool_val));
    }
    for (i = 0; i < 2; i++) {
        json_t *stamp = json_obj_get(value, STAMP_KEYS[i]);
        char buf[128];
        if (stamp && stamp->type == JSON_STRING && sp_normalize_stamp(stamp->str_val, buf, sizeof(buf))) {
            json_set(entry, STAMP_KEYS[i], json_string(buf));
        }
    }
    return entry;
}

json_t *w21_statuspubl_e796de_redact(const json_t *status) {
    json_t *out = json_object();
    json_t *platforms;
    size_t i;

    if (status && status->type == JSON_OBJECT) {
        for (i = 0; i < status->c.count; i++) {
            const char *key = status->c.keys[i];
            if (!key) continue;
            json_set(out, key, json_copy(status->c.items[i]));
        }
    }

    /* status["gateway_platforms"] = {k: _anonymous_platform_entry(v) for ...} */
    platforms = json_obj_get(status, "gateway_platforms");
    {
        json_t *projected = json_object();
        if (platforms && platforms->type == JSON_OBJECT) {
            for (i = 0; i < platforms->c.count; i++) {
                const char *key = platforms->c.keys[i];
                if (!key) continue;
                json_set(projected, key, w21_statuspubl_e796de_anonymous_entry(platforms->c.items[i]));
            }
        }
        json_set(out, "gateway_platforms", projected);
    }

    /* reason = status.get("gateway_exit_reason"); reduce unless already a code */
    {
        json_t *reason = json_obj_get(status, "gateway_exit_reason");
        if (reason && reason->type != JSON_NULL &&
            !(reason->type == JSON_STRING && sp_public_code(reason->str_val))) {
            json_set(out, "gateway_exit_reason",
                     sp_exit_reason(reason, json_obj_get(status, "gateway_state")));
        }
    }
    json_set(out, "multiplex_standalone_reason", json_null());
    return out;
}
