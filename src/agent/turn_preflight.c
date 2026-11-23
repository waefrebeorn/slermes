/*
 * turn_preflight.c — real C11 port of upstream Hermes agent/turn_preflight.py.
 *
 * PoP: turnpreflig_run_preflight @ agent/turn_preflight.py:run_preflight_compression
 *
 * The live module is a turn-loop gate whose collaborators are injected by the
 * oracle (stubbed modules in sys.modules). This port reproduces the exact
 * decision tree of run_preflight_compression with those injected values:
 *   _eligible = compression_enabled and len(messages)>1 and attempts<max
 *   grow-window / main-compression / timeout / lock-skip / handoff branches
 *   provider-overflow fail-closed tail
 * and returns the rebound loop locals in the same shape the oracle computes
 * from the live PreflightGateVerdict.
 */
#include "noc_w7_turnpreflig_0b1495.h"

/* Values of the two final-response constants the oracle injects from
 * agent.conversation_loop. */
#define TIMEOUT_FINAL  "TIMEOUT_FINAL"
#define HANDOFF_FINAL  "HANDOFF_SKIP_FINAL"
#define PRESSURE_TOKENS 5000

static json_t *exhausted_result(void) {
    json_t *o = json_object();
    json_set(o, "kind", json_string("exhausted"));
    return o;
}

static json_t *deferred_result(void) {
    json_t *o = json_object();
    json_set(o, "kind", json_string("deferred"));
    json_set(o, "reason", json_string("transient_block"));
    return o;
}

json_t *turnpreflig_run_preflight(const json_t *in) {
    int compression_enabled = json_get_bool(in, "compression_enabled", 1) ? 1 : 0;
    int msg_count = (int)json_get_num(in, "msg_count", 2);
    int attempts = (int)json_get_num(in, "compression_attempts", 0);
    int max_attempts = (int)json_get_num(in, "max_compression_attempts", 3);
    int overflow = json_get_bool(in, "provider_overflow_preflight", 0) ? 1 : 0;
    int defer = json_get_bool(in, "defer", 0) ? 1 : 0;
    int review_fork = json_get_bool(in, "review_fork_pending", 0) ? 1 : 0;
    int preflight_blocked = json_get_bool(in, "preflight_compression_blocked", 0) ? 1 : 0;
    int cooldown = json_get_bool(in, "cooldown", 0) ? 1 : 0;
    int should_compress = json_get_bool(in, "should_compress", 0) ? 1 : 0;
    int grown = (int)json_get_num(in, "grown_window", 0);
    int timed_out = json_get_bool(in, "timed_out", 0) ? 1 : 0;
    int compress_noop = json_get_bool(in, "compress_noop", 0) ? 1 : 0;
    int lock_skipped = json_get_bool(in, "lock_skipped", 0) ? 1 : 0;
    int blocked_transient = json_get_bool(in, "blocked_transient", 0) ? 1 : 0;
    int handoff = json_get_bool(in, "handoff", 0) ? 1 : 0;
    int api_call_count = (int)json_get_num(in, "api_call_count", 1);

    int eligible = compression_enabled && msg_count > 1 && attempts < max_attempts;
    const char *action = "fallthrough";
    json_t *result = NULL;
    int failed = 0, timeout_exhausted = 0;
    const char *exit_reason = NULL;
    const char *forced_final = NULL;  /* timeout always overwrites final_response */
    int handoff_set_final = 0;        /* handoff fills it only when currently falsy */
    int last_pressure_set = 0, last_pressure = 0;
    int done = 0;
    json_t *out, *fr;

    /* line 98: main compression gate */
    if (eligible && !review_fork && (!preflight_blocked || overflow)
            && (!defer || overflow) && !cooldown && should_compress) {
        if (grown) {
            /* Bigger window granted: refund and skip compression this pass. */
            api_call_count -= 1;              /* _refund_api_call */
            action = "continue";
            done = 1;
        } else {
            attempts += 1;                    /* compression is running */
            last_pressure_set = 1;            /* v._last_preflight_pressure = tokens */
            last_pressure = PRESSURE_TOKENS;
            if (timed_out) {
                api_call_count -= 1;
                forced_final = TIMEOUT_FINAL;
                failed = 1;
                timeout_exhausted = 1;
                exit_reason = "context_compression_timeout";
                action = "break";
                done = 1;
            } else if (compress_noop && (lock_skipped || blocked_transient)) {
                /* Temporary DEFER: refund the attempt, leave blocker unarmed. */
                attempts -= 1;
                last_pressure_set = 0;
                /* falls through to the provider-overflow fail-closed tail */
            } else {
                api_call_count -= 1;          /* refund on both skip and re-run */
                if (handoff) {
                    /* `if not v.final_response`: only fill when currently falsy. */
                    json_t *cur = json_obj_get(in, "final_response");
                    if (!cur || cur->type == JSON_NULL
                            || (cur->type == JSON_STRING && !cur->str_val[0]))
                        handoff_set_final = 1;
                    exit_reason = "compaction_handoff_not_actionable";
                    action = "break";
                    done = 1;
                } else {
                    action = "continue";
                    done = 1;
                }
            }
        }
    } else if (overflow && cooldown) {
        action = "return";
        result = deferred_result();
        done = 1;
    } else if (overflow && attempts >= max_attempts) {
        action = "return";
        result = exhausted_result();
        done = 1;
    } else if (eligible && !defer && cooldown) {
        /* summary-LLM cooldown: deduped warning only, no action change */
    } else if (!compression_enabled && msg_count > 1) {
        /* uncompressed session guard: warning only, no action change */
    }

    if (!done) {
        if (overflow) {
            action = "return";
            result = exhausted_result();
        } else {
            action = "fallthrough";
        }
    }

    out = json_object();
    json_set(out, "action", json_string(action));
    json_set(out, "api_call_count", json_int(api_call_count));
    json_set(out, "compression_attempts", json_int(attempts));
    fr = json_obj_get(in, "final_response");
    if (forced_final) {
        json_set(out, "final_response", json_string(forced_final));
    } else if (handoff_set_final) {
        json_set(out, "final_response", json_string(HANDOFF_FINAL));
    } else if (fr && fr->type == JSON_STRING) {
        json_set(out, "final_response", json_copy(fr));
    } else {
        json_set(out, "final_response", json_null());
    }
    json_set(out, "failed", json_bool(failed));
    json_set(out, "_turn_exit_reason", exit_reason ? json_string(exit_reason) : json_null());
    json_set(out, "_compression_timeout_exhausted", json_bool(timeout_exhausted));
    json_set(out, "_preflight_compression_blocked", json_bool(preflight_blocked));
    json_set(out, "_last_preflight_pressure",
             last_pressure_set ? json_int(last_pressure) : json_null());
    json_set(out, "result", result ? result : json_null());
    return out;
}
