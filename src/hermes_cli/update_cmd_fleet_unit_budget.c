/*
 * update_cmd_fleet_unit_budget.c - C11 port of upstream
 * hermes_cli/update_cmd_fleet_unit_budget.py (gate w26_updatecmdf_887c4a).
 *
 * PoP: w26_updatecmdf_887c4a_restart_timeout @ hermes_cli/update_cmd_fleet_unit_budget.py:systemd_restart_timeout
 * PoP: w26_updatecmdf_887c4a_unit_job_budget @ hermes_cli/update_cmd_fleet_unit_budget.py:unit_job_budget
 * PoP: w26_updatecmdf_887c4a_unit_job_state @ hermes_cli/update_cmd_fleet_unit_budget.py:unit_job_state
 * PoP: w26_updatecmdf_887c4a_settled @ hermes_cli/update_cmd_fleet_unit_budget.py:_settled
 * PoP: w26_updatecmdf_887c4a_service_restart_sec @ hermes_cli/update_cmd_fleet_unit_budget.py:service_restart_sec
 * PoP: w26_updatecmdf_887c4a_constants @ hermes_cli/update_cmd_fleet_unit_budget.py:<module constants>
 *
 * The seam is `update_cmd_fleet._systemctl`: this port never spawns it.  The
 * caller passes what the call would have returned -- its returncode and stdout,
 * or the FileNotFoundError / TimeoutExpired it raises -- and the functions
 * decide exactly what the live ones decide, including the 90s-per-phase default
 * for unknown or infinite limits and the signed-32-bit poll() ceiling.
 * parse_systemd_duration_to_us is ported inline (gateway/shutdown_forensics.py)
 * because every budget here is derived from it.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_updatecmdf_887c4a.h"

/* _SYSTEMCTL_RESTART_TIMEOUT_MAX = (2**31 - 1) // 1000 - 1 */
#define UCM_RESTART_TIMEOUT_MAX 2147482.0
/* _UNIT_JOB_HARD_CAP_S */
#define UCM_UNIT_JOB_HARD_CAP 900.0
/* UNIT_JOB_STILL_RUNNING */
#define UCM_UNIT_JOB_STILL_RUNNING 124.0
/* systemd's per-phase fallback when a limit is unknown or infinite */
#define UCM_DEFAULT_PHASE 90.0

static const char *const UCM_TRANSITIONAL[] = {
    "activating", "deactivating", "reloading", "refreshing",
};
#define UCM_TRANSITIONAL_N 4

/* --- small string helpers ------------------------------------------------ */

/* Python str.strip() over ASCII whitespace. */
static char *ucm_strip(const char *s) {
    const char *end;
    size_t n;
    char *out;
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    n = (size_t)(end - s);
    out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Is the (already stripped) token all ASCII digits? */
static int ucm_all_digits(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < '0' || c > '9') return 0;
    }
    return 1;
}

/* gateway.shutdown_forensics.parse_systemd_duration_to_us: "1min 30s" -> us,
 * a bare number -> seconds, None on anything unexpected.  Returns 0 for None. */
static long long ucm_duration_us(const char *raw) {
    static const struct { const char *unit; double mult; } UNITS[] = {
        {"us", 1.0}, {"ms", 1000.0}, {"s", 1000000.0}, {"sec", 1000000.0},
        {"min", 60000000.0}, {"h", 3600000000.0}, {"hr", 3600000000.0},
        {"d", 86400000000.0}, {"w", 604800000000.0},
        {"month", 2629800000000.0}, {"y", 31557600000000.0},
    };
    const size_t nunits = sizeof(UNITS) / sizeof(UNITS[0]);
    long long total = 0;
    char token[32];
    char digits[64];
    size_t token_len = 0, digits_len = 0;
    const char *p;

    if (!raw || !*raw) return 0;
    token[0] = '\0'; digits[0] = '\0';

#define UCM_FLUSH()                                                             \
    do {                                                                        \
        double mult = 1000000.0;                                                \
        size_t _i;                                                              \
        if (token_len) {                                                        \
            mult = -1.0;                                                        \
            for (_i = 0; _i < nunits; _i++)                                     \
                if (!strcmp(UNITS[_i].unit, token)) { mult = UNITS[_i].mult; break; } \
        }                                                                       \
        if (mult < 0.0 || !digits_len) return 0;                                \
        total += (long long)(atof(digits) * mult);                              \
        digits[0] = '\0'; digits_len = 0; token[0] = '\0'; token_len = 0;       \
    } while (0)

    for (p = raw;; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\0' || isspace(c)) {
            if (digits_len) UCM_FLUSH();
            if (c == '\0') break;
            continue;
        }
        if ((c >= '0' && c <= '9') || c == '.') {
            if (token_len) UCM_FLUSH();
            if (digits_len + 1 < sizeof(digits)) { digits[digits_len++] = (char)c; digits[digits_len] = '\0'; }
            continue;
        }
        if (isalpha(c)) {
            if (token_len + 1 < sizeof(token)) {
                token[token_len++] = (char)tolower(c);
                token[token_len] = '\0';
            }
            continue;
        }
        /* punctuation ends a pending number the way whitespace does */
        if (digits_len) UCM_FLUSH();
    }
#undef UCM_FLUSH
    return total > 0 ? total : 0;
}

/* `_systemctl(...)` as a seam: did it raise, and what came back. */
static int ucm_raised(const json_t *in) {
    const char *raise_as = json_get_str(in, "raise", "");
    return !strcmp(raise_as, "missing") || !strcmp(raise_as, "timeout");
}

static int ucm_rc(const json_t *in) {
    const json_t *rc = json_obj_get(in, "rc");
    double v = 0.0;
    if (rc && rc->type == JSON_NUMBER) v = rc->num_val;
    else if (rc && rc->type == JSON_BOOL) v = rc->bool_val ? 1.0 : 0.0;
    return (int)v;
}

/* Split `text` the way str.splitlines() does for the shapes systemd prints. */
typedef void (*ucm_line_fn)(const char *line, size_t len, void *ctx);

static void ucm_for_lines(const char *text, ucm_line_fn fn, void *ctx) {
    const char *start, *p;
    if (!text) return;
    start = text;
    for (p = text;; p++) {
        if (*p == '\0' || *p == '\n' || *p == '\r') {
            size_t len = (size_t)(p - start);
            if (len || *p == '\n' || *p == '\r') fn(start, len, ctx);
            if (*p == '\r' && p[1] == '\n') p++;
            if (*p == '\0') break;
            start = p + 1;
        }
    }
}

typedef struct {
    double stop;
    double start;
} ucm_budgets_t;

static void ucm_budget_line(const char *line, size_t len, void *ctx) {
    ucm_budgets_t *b = (ucm_budgets_t *)ctx;
    const char *eq;
    size_t key_len;
    char key[64];
    char *raw, *value;
    long long us;
    if (len == 0 || len >= sizeof(key)) return;
    eq = (const char *)memchr(line, '=', len);
    if (!eq) return;
    key_len = (size_t)(eq - line);
    if (key_len >= sizeof(key)) return;
    memcpy(key, line, key_len);
    key[key_len] = '\0';
    if (strcmp(key, "TimeoutStopUSec") && strcmp(key, "TimeoutStartUSec")) return;
    raw = (char *)malloc(len - key_len);          /* the remainder after '=' */
    if (!raw) return;
    memcpy(raw, eq + 1, len - key_len - 1);
    raw[len - key_len - 1] = '\0';
    value = ucm_strip(raw);
    free(raw);
    /* int(raw) for bare digits, else the shared duration parser (None on
     * infinity / unrecognised units). */
    if (ucm_all_digits(value)) {
        us = atoll(value);
    } else {
        us = ucm_duration_us(value);
    }
    free(value);
    if (us > 0) {
        if (!strcmp(key, "TimeoutStopUSec")) b->stop = (double)us / 1000000.0;
        else b->start = (double)us / 1000000.0;
    }
}

/* (TimeoutStopUSec, TimeoutStartUSec) in seconds, defaults 90s per phase. */
static void ucm_budgets(const json_t *in, double *stop, double *start) {
    ucm_budgets_t b;
    const char *show;
    b.stop = UCM_DEFAULT_PHASE;
    b.start = UCM_DEFAULT_PHASE;
    if (ucm_raised(in) || ucm_rc(in) != 0) {
        *stop = b.stop;
        *start = b.start;
        return;
    }
    show = json_get_str(in, "show", "");
    ucm_for_lines(show, ucm_budget_line, &b);
    *stop = b.stop;
    *start = b.start;
}

json_t *w26_updatecmdf_887c4a_restart_timeout(const json_t *in) {
    double stop, start, budget;
    int start_only = in ? (int)json_get_bool(in, "start_only", false) : 0;
    ucm_budgets(in, &stop, &start);
    budget = (start_only ? 0.0 : stop) + start + 15.0;
    if (budget > UCM_RESTART_TIMEOUT_MAX) budget = UCM_RESTART_TIMEOUT_MAX;
    return json_number(budget);
}

json_t *w26_updatecmdf_887c4a_unit_job_budget(const json_t *in) {
    double stop, start, budget;
    const char *verb = in ? json_get_str(in, "verb", "") : "";
    ucm_budgets(in, &stop, &start);
    budget = 2.0 * stop + ((!strcmp(verb, "start") || !strcmp(verb, "restart")) ? start : 0.0) + 30.0;
    if (budget > UCM_UNIT_JOB_HARD_CAP) budget = UCM_UNIT_JOB_HARD_CAP;
    return json_number(budget);
}

typedef struct {
    json_t *out;
} ucm_state_t;

static void ucm_state_line(const char *line, size_t len, void *ctx) {
    ucm_state_t *st = (ucm_state_t *)ctx;
    const char *eq;
    char *key, *value;
    size_t key_len;
    if (len == 0) return;
    eq = (const char *)memchr(line, '=', len);
    if (!eq) return;
    key_len = (size_t)(eq - line);
    key = (char *)malloc(key_len + 1);
    if (!key) return;
    memcpy(key, line, key_len);
    key[key_len] = '\0';
    value = (char *)malloc(len - key_len);
    if (!value) { free(key); return; }
    memcpy(value, eq + 1, len - key_len - 1);
    value[len - key_len - 1] = '\0';
    json_set(st->out, key, json_string(value));   /* last occurrence wins */
    free(key);
    free(value);
}

json_t *w26_updatecmdf_887c4a_unit_job_state(const json_t *in) {
    ucm_state_t st;
    json_t *out = json_object();
    st.out = out;
    if (ucm_raised(in) || ucm_rc(in) != 0) return out;
    ucm_for_lines(in ? json_get_str(in, "show", "") : "", ucm_state_line, &st);
    return out;
}

json_t *w26_updatecmdf_887c4a_settled(const json_t *state) {
    const json_t *active = state ? json_obj_get(state, "ActiveState") : NULL;
    const json_t *job = state ? json_obj_get(state, "Job") : NULL;
    int i, found = 0;

    if (!active || active->type == JSON_NULL) return json_bool(false);
    if (active->type == JSON_STRING) {
        const char *name = active->str_val ? active->str_val : "";
        if (!name[0]) return json_bool(false);
        for (i = 0; i < UCM_TRANSITIONAL_N; i++)
            if (!strcmp(name, UCM_TRANSITIONAL[i])) found = 1;
        if (found) return json_bool(false);
    }
    /* `not state.get("Job")` */
    if (job && job->type != JSON_NULL) {
        if (job->type == JSON_STRING) {
            if (job->str_val && job->str_val[0]) return json_bool(false);
        } else if (job->type == JSON_NUMBER) {
            if (job->num_val != 0.0) return json_bool(false);
        } else if (job->type == JSON_BOOL) {
            if (job->bool_val) return json_bool(false);
        } else if (job->type == JSON_ARRAY) {
            if (json_len(job) > 0) return json_bool(false);
        } else if (job->type == JSON_OBJECT) {
            if (json_object_size(job) > 0) return json_bool(false);
        }
    }
    return json_bool(true);
}

json_t *w26_updatecmdf_887c4a_service_restart_sec(const json_t *in) {
    static const struct { const char *suffix; double mult; } SUFFIXES[] = {
        {"ms", 0.001}, {"us", 0.000001}, {"min", 60.0}, {"s", 1.0},
    };
    const size_t nsuffix = sizeof(SUFFIXES) / sizeof(SUFFIXES[0]);
    double def = in ? json_get_num(in, "default", 0.0) : 0.0;
    char *raw;
    double total = 0.0;
    int matched = 0;
    char *save, *part;

    if (ucm_raised(in)) return json_number(def);
    raw = ucm_strip(in ? json_get_str(in, "show", "") : "");
    if (!raw[0] || !strcmp(raw, "infinity")) { free(raw); return json_number(def); }

    save = raw;
    while ((part = save) != NULL) {
        char *space = strpbrk(save, " \t\n\r\f\v");
        size_t i;
        if (space) { *space = '\0'; save = space + 1; }
        else save = NULL;
        for (i = 0; i < nsuffix; i++) {
            size_t plen = strlen(part), slen = strlen(SUFFIXES[i].suffix);
            if (plen >= slen && !strcmp(part + plen - slen, SUFFIXES[i].suffix)) {
                char *endp = NULL;
                char buf[64];
                size_t keep = plen - slen;
                double value;
                if (keep < sizeof(buf)) {
                    memcpy(buf, part, keep);
                    buf[keep] = '\0';
                    value = strtod(buf, &endp);
                    if (endp && *endp == '\0' && keep > 0) {
                        total += value * SUFFIXES[i].mult;
                        matched = 1;
                    }
                }
                break;
            }
        }
        if (!save) break;
    }
    free(raw);
    return json_number(matched ? total : def);
}

json_t *w26_updatecmdf_887c4a_constants(void) {
    json_t *o = json_object();
    json_t *states = json_array();
    int i;
    json_set(o, "systemctl_restart_timeout_max", json_number(UCM_RESTART_TIMEOUT_MAX));
    json_set(o, "unit_job_hard_cap_s", json_number(UCM_UNIT_JOB_HARD_CAP));
    json_set(o, "unit_job_still_running", json_number(UCM_UNIT_JOB_STILL_RUNNING));
    for (i = 0; i < UCM_TRANSITIONAL_N; i++)
        json_append(states, json_string(UCM_TRANSITIONAL[i]));
    json_set(o, "transitional_states", states);
    return o;
}
