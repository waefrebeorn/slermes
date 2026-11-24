/*
 * web_server_owner_exit.c -- real C11 port of the supersession-retirement
 * decisions of upstream Hermes hermes_cli/web_server_owner_exit.py.
 *
 * PoP: w21_webservero_94d6ca_default_poll @ hermes_cli/web_server_owner_exit.py:DEFAULT_OWNER_POLL_S
 * PoP: w21_webservero_94d6ca_should_retire @ hermes_cli/web_server_owner_exit.py:should_retire_superseded
 * PoP: w21_webservero_94d6ca_observe_message @ hermes_cli/web_server_owner_exit.py:start_owner_watchdog
 * PoP: w21_webservero_94d6ca_simulate @ hermes_cli/web_server_owner_exit.py:start_owner_watchdog
 *
 * The lock reader and the retirement fence are the simulator's scripted inputs;
 * the thread/sleep plumbing of _run_retirement_watchdog is modelled as counts.
 */
#include "hermes_json.h"
#include "noc_w21_webservero_94d6ca.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* hermes_cli/dashboard_procs.py: _REAP_MIN_AGE_SECONDS */
static const double WO_REAP_MIN_AGE_SECONDS = 180.0;
/* hermes_cli/web_server_skew_exit.py: _CONFIRMATIONS */
static const int WO_CONFIRMATIONS = 2;
/* hermes_cli/web_server_owner_exit.py: DEFAULT_OWNER_POLL_S */
static const double WO_DEFAULT_OWNER_POLL_S = 15.0;

static const char WO_MESSAGE_FMT[] =
    "SSH-isolated backend %s was superseded by spawn %s; retiring.";

static char *wo_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* f-string str() of a JSON scalar. */
static char *wo_str_of(const json_t *v) {
    char buf[64];
    if (!v) return wo_dup("None");
    switch (v->type) {
    case JSON_NULL:   return wo_dup("None");
    case JSON_BOOL:   return wo_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return wo_dup(buf);
    case JSON_STRING: return wo_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:  return wo_dup("[]");
    case JSON_OBJECT: return wo_dup("{}");
    }
    return wo_dup("");
}

double w21_webservero_94d6ca_default_poll(void) {
    return WO_DEFAULT_OWNER_POLL_S;
}

/* `lock["spawnNonce"] != my_nonce` -- a non-str spawnNonce never equals a str. */
static int wo_nonce_differs(const json_t *lock, const char *my_nonce) {
    json_t *spawn = json_obj_get(lock, "spawnNonce");
    const char *mine = my_nonce ? my_nonce : "";
    if (!spawn || spawn->type != JSON_STRING) return 1;
    return strcmp(spawn->str_val ? spawn->str_val : "", mine) != 0;
}

json_t *w21_webservero_94d6ca_should_retire(const json_t *lock, const char *my_nonce,
                                            double age_s) {
    if (!lock || lock->type == JSON_NULL) return json_bool(0);      /* lock is not None */
    if (lock->type != JSON_OBJECT) return json_bool(0);
    if (!json_obj_get(lock, "spawnNonce")) {
        /* `lock["spawnNonce"]` raises KeyError before any comparison */
        json_t *out = json_object();
        json_set(out, "raised", json_string("KeyError"));
        json_set(out, "message", json_string("'spawnNonce'"));
        return out;
    }
    if (!wo_nonce_differs(lock, my_nonce)) return json_bool(0);
    return json_bool(age_s >= WO_REAP_MIN_AGE_SECONDS);
}

char *w21_webservero_94d6ca_observe_message(const char *nonce, const json_t *spawn_nonce) {
    char *spawn = wo_str_of(spawn_nonce);
    char *out;
    size_t need;
    if (!spawn) return NULL;
    need = strlen(WO_MESSAGE_FMT) + strlen(nonce ? nonce : "") + strlen(spawn) + 8;
    out = (char *)malloc(need);
    if (out) snprintf(out, need, WO_MESSAGE_FMT, nonce ? nonce : "", spawn);
    free(spawn);
    return out;
}

json_t *w21_webservero_94d6ca_simulate(const json_t *req) {
    const char *nonce = json_get_str(req, "nonce", "");
    json_t *polls = json_obj_get(req, "polls");
    json_t *fences = json_obj_get(req, "fences");
    long max_polls = (long)json_get_num(req, "max_polls", 1);
    size_t poll_count = (polls && polls->type == JSON_ARRAY) ? polls->c.count : 0;
    size_t fence_count = (fences && fences->type == JSON_ARRAY) ? fences->c.count : 0;
    size_t fence_i = 0;
    long polls_done = 0, seen = 0, prepare_calls = 0, commit_calls = 0, sleeps = 0;
    int retired = 0;
    char *message = NULL;

    while (!retired && polls_done < max_polls) {
        const json_t *poll = NULL;
        json_t *lock = NULL;
        char *reason = NULL;
        polls_done++;
        if ((size_t)(polls_done - 1) < poll_count) poll = json_get(polls, (size_t)(polls_done - 1));
        lock = poll ? json_obj_get(poll, "lock") : NULL;
        /* `lock is None or not should_retire_superseded(...)` */
        if (lock && lock->type == JSON_OBJECT) {
            double age_s = poll ? json_get_num(poll, "age_s", 0) : 0;
            json_t *verdict = w21_webservero_94d6ca_should_retire(lock, nonce, age_s);
            if (verdict && verdict->type == JSON_BOOL && verdict->bool_val) {
                free(message);
                message = w21_webservero_94d6ca_observe_message(nonce,
                                                                json_obj_get(lock, "spawnNonce"));
                reason = message;
            }
            json_free(verdict);
        }
        seen = reason ? seen + 1 : 0;
        if (seen >= WO_CONFIRMATIONS) {
            json_t *attempt = (fence_i < fence_count) ? json_get(fences, fence_i) : NULL;
            json_t *prep = attempt ? json_obj_get(attempt, "prepare") : NULL;
            prepare_calls++;
            if (prep && json_get_bool(prep, "ok", 0)) {
                json_t *commit = json_obj_get(attempt, "commit");
                commit_calls++;
                if (commit && json_get_bool(commit, "ok", 0)) {
                    retired = 1;
                    break;
                }
            }
            fence_i++;
        }
        sleeps++;
    }

    {
        json_t *out = json_object();
        json_set(out, "retired", json_bool(retired));
        json_set(out, "polls", json_number((double)polls_done));
        json_set(out, "prepare_calls", json_number((double)prepare_calls));
        json_set(out, "commit_calls", json_number((double)commit_calls));
        json_set(out, "sleeps", json_number((double)sleeps));
        /* the reason is only *logged* through the fence, i.e. on retirement */
        json_set(out, "message", (retired && message) ? json_string(message) : json_null());
        free(message);
        return out;
    }
}
