/*
 * web_server_skew_exit.c - C11 port of upstream
 * hermes_cli/web_server_skew_exit.py (gate w23_webservers_36f0c2).
 *
 * PoP: w23_webservers_36f0c2_poll_s @ hermes_cli/web_server_skew_exit.py:DEFAULT_SKEW_POLL_S
 * PoP: w23_webservers_36f0c2_confirmations @ hermes_cli/web_server_skew_exit.py:_CONFIRMATIONS
 * PoP: w23_webservers_36f0c2_should_retire @ hermes_cli/web_server_skew_exit.py:should_retire_for_skew
 * PoP: w23_webservers_36f0c2_observe @ hermes_cli/web_server_skew_exit.py:_observe
 * PoP: w23_webservers_36f0c2_run @ hermes_cli/web_server_skew_exit.py:_run_retirement_watchdog
 *
 * The watchdog is a daemon thread that polls a code-skew reader and a
 * retirement fence.  Both are seams here: the caller supplies the poll-by-poll
 * readings (what detect_code_skew would have answered, plus the update-lock
 * probe's answer or its raise) and the fence's prepare/commit verdicts, and
 * this file owns the loop: two consecutive reasons, then one prepare/commit
 * round trip, then should_exit.
 *
 * The time.sleep(poll_s) between polls has no observable effect on the trace
 * and is not modelled; the harness pins poll_s to 0 for the same reason.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_webservers_36f0c2.h"

#define WSE_POLL_S 30.0
#define WSE_CONFIRMATIONS 2

double w23_webservers_36f0c2_poll_s(void) { return WSE_POLL_S; }
int w23_webservers_36f0c2_confirmations(void) { return WSE_CONFIRMATIONS; }

/* Python truthiness of Skew = Optional[tuple[str, str]]. */
static int wse_skew_truthy(const json_t *skew)
{
    if (!skew) return 0;
    if (skew->type == JSON_ARRAY) return json_len(skew) != 0;
    if (skew->type == JSON_STRING) return skew->str_val && skew->str_val[0];
    if (skew->type == JSON_NULL) return 0;
    return 1;
}

/* str() of one half of the tuple, for the refusal-free cases fixtures use. */
static char *wse_text(const json_t *v)
{
    char buf[64];
    const char *s = "";
    size_t n;
    if (!v) return NULL;
    if (v->type == JSON_STRING) s = v->str_val ? v->str_val : "";
    else if (v->type == JSON_NULL) s = "None";
    else if (v->type == JSON_BOOL) s = v->bool_val ? "True" : "False";
    else if (v->type == JSON_NUMBER) {
        long long i = (long long)v->num_val;
        if ((double)i == v->num_val) snprintf(buf, sizeof(buf), "%lld", i);
        else snprintf(buf, sizeof(buf), "%g", v->num_val);
        s = buf;
    } else return NULL;
    n = strlen(s);
    {
        char *out = (char *)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, s, n + 1);
        return out;
    }
}

int w23_webservers_36f0c2_should_retire(const json_t *skew, int update_in_progress)
{
    return wse_skew_truthy(skew) && !update_in_progress;
}

/* "SSH-isolated backend loaded {} but the install is now at {}; ...".format(*skew) */
static char *wse_reason(const json_t *skew)
{
    static const char *head = "SSH-isolated backend loaded ";
    static const char *mid = " but the install is now at ";
    static const char *tail = "; retiring so its client respawns it on the new code.";
    char *boot, *disk, *out;
    size_t la, lb;
    if (!skew || skew->type != JSON_ARRAY) return NULL;
    boot = wse_text(json_get(skew, 0));
    disk = wse_text(json_get(skew, 1));
    if (!boot || !disk) {
        free(boot);
        free(disk);
        return NULL;
    }
    la = strlen(head) + strlen(boot) + strlen(mid);
    lb = strlen(disk) + strlen(tail);
    out = (char *)malloc(la + lb + 1);
    if (out) {
        memcpy(out, head, strlen(head));
        memcpy(out + strlen(head), boot, strlen(boot));
        memcpy(out + strlen(head) + strlen(boot), mid, strlen(mid));
        memcpy(out + la, disk, strlen(disk));
        memcpy(out + la + strlen(disk), tail, strlen(tail) + 1);
    }
    free(boot);
    free(disk);
    return out;
}

char *w23_webservers_36f0c2_observe(const json_t *reading)
{
    const json_t *skew = reading && reading->type == JSON_OBJECT ? json_obj_get(reading, "skew") : NULL;
    int update;
    /* `if not (skew and should_retire_for_skew(skew=skew, update_in_progress=update_probe()))`
     * short-circuits: the probe is never consulted without a skew. */
    if (!wse_skew_truthy(skew)) return NULL;
    /* _update_in_progress: an exception means "cannot prove the swap is over". */
    if (reading && json_get_bool(reading, "update_raises", false)) update = 1;
    else update = (reading && json_get_bool(reading, "update", false)) ? 1 : 0;
    if (!w23_webservers_36f0c2_should_retire(skew, update)) return NULL;
    return wse_reason(skew);
}

/* fence.<key>[i], clamped to the last entry, falsy when absent. */
static int wse_fence_flag(const json_t *fence, const char *key, long long idx)
{
    const json_t *vals = json_obj_get(fence, key);
    size_t n = json_len(vals);
    const json_t *v;
    if (!n) return 0;
    if (idx < 0) idx = 0;
    if ((size_t)idx >= n) idx = (long long)n - 1;
    v = json_get(vals, (size_t)idx);
    return v && v->type == JSON_BOOL ? (v->bool_val ? 1 : 0) : 0;
}

static char *wse_fence_token(const json_t *fence, long long idx)
{
    const json_t *vals = json_obj_get(fence, "tokens");
    size_t n = json_len(vals);
    const json_t *v;
    if (!n) return NULL;
    if (idx < 0) idx = 0;
    if ((size_t)idx >= n) idx = (long long)n - 1;
    v = json_get(vals, (size_t)idx);
    if (!v) return NULL;
    return wse_text(v);
}

json_t *w23_webservers_36f0c2_run(const json_t *world)
{
    const json_t *readings = NULL, *fence = NULL, *max_polls_v = NULL;
    json_t *out = json_object();
    json_t *log = json_array();
    json_t *tokens = json_array();
    long long max_polls, polls = 0, seen = 0, prepares = 0, commits = 0, idx = 0;
    long long nread = 0;
    int should_exit = 0;

    if (!out || !log || !tokens) {
        json_free(out);
        json_free(log);
        json_free(tokens);
        return NULL;
    }
    if (world && world->type == JSON_OBJECT) {
        readings = json_obj_get(world, "readings");
        fence = json_obj_get(world, "fence");
        max_polls_v = json_obj_get(world, "max_polls");
        should_exit = json_get_bool(world, "should_exit", false) ? 1 : 0;
    }
    nread = (long long)json_len(readings);
    if (max_polls_v && max_polls_v->type == JSON_NUMBER) {
        max_polls = (long long)max_polls_v->num_val;
    } else {
        /* the script bounds an unbounded loop: past its last reading nothing
         * can warrant retirement, so the trace is the same either way. */
        max_polls = nread;
    }

    while (!should_exit && (max_polls < 0 || polls < max_polls)) {
        const json_t *reading = idx < nread ? json_get(readings, (size_t)idx) : NULL;
        char *reason;
        idx++;
        polls++;
        reason = w23_webservers_36f0c2_observe(reading);
        seen = reason ? seen + 1 : 0;
        if (seen >= (long long)WSE_CONFIRMATIONS) {
            long long prep_i = prepares;
            int prepared;
            prepares++;
            prepared = wse_fence_flag(fence, "prepare", prep_i);
            if (prepared) {
                int committed;
                char *token = wse_fence_token(fence, prep_i);
                commits++;
                json_append(tokens, json_string(token ? token : ""));
                free(token);
                committed = wse_fence_flag(fence, "commit", commits - 1);
                if (committed) {
                    json_append(log, json_string(reason));
                    should_exit = 1;
                    free(reason);
                    break;
                }
            }
        }
        free(reason);
    }

    json_set(out, "should_exit", json_bool(should_exit ? true : false));
    json_set(out, "polls", json_number((double)polls));
    json_set(out, "prepare_calls", json_number((double)prepares));
    json_set(out, "commit_calls", json_number((double)commits));
    json_set(out, "tokens", tokens);
    json_set(out, "log", log);
    return out;
}
