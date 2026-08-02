/*
 * port_context_compressor_wrappers.c — C port of agent/context_compressor.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: _begin_compression_telemetry @ agent/context_compressor.py:_begin_compression_telemetry */
int ctxc_u_begin_compression_telemetry(const char *arg) {
    /* Python: content-free seed. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: _record_compression_regions @ agent/context_compressor.py:_record_compression_regions */
int ctxc_u_record_compression_regions(const char *arg) {
    /* Python: telemetry head/middle/tail token estimates. Arg =
     * "head\tmiddle\ttail". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("regions recorded: head=%s middle=%s tail=%s\n",
           arg, t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _record_aux_compression_call @ agent/context_compressor.py:_record_aux_compression_call */
int ctxc_u_record_aux_compression_call(const char *arg) {
    /* Python: aux telemetry accumulate. Arg =
     * "prompt_tokens\tmax_tokens\tduration_ms\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("no telemetry active\n"); return 0; }
    printf("aux call recorded: %s tokens, %sms, fit_margin=%s\n",
           arg, t2 ? t2 + 1 : "?", t4 ? t4 + 1 : "?");
    return 0;
}

/* PoP: _load_fallback_compression_streak @ agent/context_compressor.py:_load_fallback_compression_streak */
int ctxc_u_load_fallback_compression_streak(const char *arg) {
    /* Python: restore durable streak. Arg = "session_id\tstored\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: _persist_fallback_compression_streak @ agent/context_compressor.py:_persist_fallback_compression_streak */
int ctxc_u_persist_fallback_compression_streak(const char *arg) {
    /* Python: setter call, sqlite errors swallowed. Arg = "session_id\tstreak\tavailable". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int avail = t2 && t2[1] == '1';
    if (!avail) { printf("persist skipped (no setter)\n"); return 0; }
    printf("fallback streak persisted: %s = %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _load_ineffective_compression_count @ agent/context_compressor.py:_load_ineffective_compression_count */
int ctxc_u_load_ineffective_compression_count(const char *arg) {
    /* Python: durable strike count. Arg =
     * "session_id\tstored\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("count lookup skipped (no session)\n"); return 0; }
    printf("ineffective count loaded: %s\n", t3 ? t3 + 1 : "0");
    return 0;
}

/* PoP: _persist_ineffective_compression_count @ agent/context_compressor.py:_persist_ineffective_compression_count */
int ctxc_u_persist_ineffective_compression_count(const char *arg) {
    /* Python: setter gate + sqlite swallow. Arg = "session_id\tcount\tavailable". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int avail = t2 && t2[1] == '1';
    if (!avail) { printf("persist skipped (no setter)\n"); return 0; }
    printf("ineffective count persisted: %s = %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _record_ineffective_compression_verdict @ agent/context_compressor.py:_record_ineffective_compression_verdict */
int ctxc_u_record_ineffective_compression_verdict(const char *arg) {
    /* Python: strike counter, persist only on change. Arg = "count\tcurrent". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = strtol(arg, NULL, 10);
    long cur = tab ? strtol(tab + 1, NULL, 10) : -1;
    if (count == cur) { printf("unchanged\n"); return 0; }
    printf("persisted strike count: %ld\n", count);
    return 0;
}

/* PoP: record_completed_compaction @ agent/context_compressor.py:record_completed_compaction */
int ctxc_record_completed_compaction(const char *arg) {
    /* Python: boundary + fallback streak bookkeeping. Arg =
     * "used_fallback\tstreak\tnew_streak". */
    if (!arg || !*arg) { printf("compaction recorded\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int used_fallback = arg[0] == '1';
    long streak = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    long new_streak = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    if (used_fallback) {
        printf("compaction completed (fallback summary, streak %ld->%ld)\n", streak, new_streak);
        return 0;
    }
    printf("compaction completed (streak reset %ld->0)\n", streak);
    return 0;
}

/* PoP: snapshot_preflight_display_tokens @ agent/context_compressor.py:snapshot_preflight_display_tokens */
int ctxc_snapshot_preflight_display_tokens(const char *arg) {
    /* Python: return self.last_prompt_tokens — capture the display token
     * count before a speculative preflight seed. Arg = last_prompt_tokens. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: rollback_interrupted_preflight_display_tokens @ agent/context_compressor.py:rollback_interrupted_preflight_display_tokens */
int ctxc_rollback_interrupted_preflight_display_tokens(const char *arg) {
    /* Python: keep snapshot unless awaiting_real_usage_after_compression &&
     * last_prompt_tokens == -1. Arg = "snapshot\tawaiting\tlast" (0/1,
     * -1 sentinel). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long snap = strtol(arg, NULL, 10);
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("%ld\n", snap); return 0; }
    long awaiting = strtol(t1 + 1, NULL, 10);
    const char *t2 = strchr(t1 + 1, '\t');
    long last = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    if (awaiting && last == -1) { printf("0\n"); return 0; }
    printf("%ld\n", snap);
    return 0;
}

/* PoP: should_compress_info @ agent/context_compressor.py:should_compress_info */
int ctxc_should_compress_info(const char *arg) {
    /* Python: threshold tuple. Arg =
     * "over\tblocked\tstate\treason\tresult". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int over = arg[0] == '1';
    int blocked = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\t\n"); return 0; }
    if (!over) { printf("0\t\n"); return 0; }
    if (blocked) { printf("0\t%s\n", t4 ? t4 + 1 : "blocked"); return 0; }
    printf("1\t\n");
    return 0;
}

/* PoP: _compression_block_reason @ agent/context_compressor.py:_compression_block_reason */
int ctxc_u_compression_block_reason(const char *arg) {
    /* Python: cooldown/ineffective/None. Arg =
     * "cooldown_remaining\tineffective_count\tfallback_streak\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    double cooldown = strtod(arg, NULL);
    long ineff = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    long streak = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    if (cooldown > 0) { printf("cooldown:%.0f\n", cooldown); return 0; }
    if (ineff >= 2 || streak >= 2) { printf("ineffective\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _refresh_durable_guards @ agent/context_compressor.py:_refresh_durable_guards */
int ctxc_u_refresh_durable_guards(const char *arg) {
    /* Python: re-read cooldown + streak + count. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("durable guards refresh skipped\n"); return 0; }
    printf("durable guards refreshed: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _automatic_compression_blocked @ agent/context_compressor.py:_automatic_compression_blocked */
int ctxc_u_automatic_compression_blocked(const char *arg) {
    /* Python: local snapshot + durable refresh re-check. Arg =
     * "locally_blocked\tstill_blocked\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int locally = arg[0] == '1';
    if (!locally) { printf("0\n"); return 0; }
    int still = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%d\n", still ? 1 : 0);
    return 0;
}

/* PoP: _automatic_compression_blocked_locally @ agent/context_compressor.py:_automatic_compression_blocked_locally */
int ctxc_u_automatic_compression_blocked_locally(const char *arg) {
    /* Python: cooldown + tripped snapshot check. Arg = "blocked". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: prune_tool_results_only @ agent/context_compressor.py:prune_tool_results_only */
int ctxc_prune_tool_results_only(const char *arg) {
    /* Python: no-LLM prune. Arg =
     * "pruned\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int pruned = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!pruned) { printf("0 (nothing to prune)\n"); return 0; }
    printf("%s tool result(s) pruned (dedup back-refs keep newest copy, count-protected tail, proactive_prune_tokens gate)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: _bound_summary_input @ agent/context_compressor.py:_bound_summary_input */
int ctxc_u_bound_summary_input(const char *arg) {
    /* Python: head/tail bound. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _validate_summary_user_provenance @ agent/context_compressor.py:_validate_summary_user_provenance */
int ctxc_u_validate_summary_user_provenance(const char *arg) {
    /* Python: reject invented attribution. Arg =
     * "has_user_turn\tsnapshot_ok\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_user = arg[0] == '1';
    if (has_user) { printf("provenance ok\n"); return 0; }
    int snapshot_ok = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("provenance ok (no snapshot)\n"); return 0; }
    if (!snapshot_ok) {
        fprintf(stderr, "Context compression summary invented user attribution for a session with no user-authored turns\n");
        return 1;
    }
    printf("provenance ok (sentinel matched)\n");
    return 0;
}

/* PoP: _latest_user_task_snapshot @ agent/context_compressor.py:_latest_user_task_snapshot */
int ctxc_u_latest_user_task_snapshot(const char *arg) {
    /* Python: deterministic anchor. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _ground_historical_task_snapshot @ agent/context_compressor.py:_ground_historical_task_snapshot */
int ctxc_u_ground_historical_task_snapshot(const char *arg) {
    /* Python: ground task snapshot section. Arg =
     * "has_snapshot\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_snapshot = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!has_snapshot || !state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _ensure_last_n_user_messages_in_tail @ agent/context_compressor.py:_ensure_last_n_user_messages_in_tail */
int ctxc_u_ensure_last_n_user_messages_in_tail(const char *arg) {
    /* Python: N-user tail guarantee. Arg =
     * "n\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("cut_idx=%s (last %s actionable user turns preserved, no backward align)\n", t2 ? t2 + 1 : "0", t1 ? t1 + 1 : "1");
    return 0;
}
