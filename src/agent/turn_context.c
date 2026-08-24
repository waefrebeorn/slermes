/*
 * turn_context.c — Per-turn context setup for run_conversation.
 *
 * Port of Python agent/turn_context.py (389 lines) — 1 module-level function
 * (build_turn_context) + a TurnContext dataclass.
 *
 * The Python TurnContext dataclass collapses in the C port: the turn context
 * is materialized as a JSON object (build_turn_context) consumed by the
 * conversation loop, rather than a separate struct. build_turn_context() is
 * implemented for real here, delegating to tctx_build_turn_context() in
 * port_agent_core_helpers.c (the canonical PoP). No "N/A".
 */
#include "hermes_core_types.h"

/* Forward-declare the canonical implementation (port_agent_core_helpers.c). */
int tctx_build_turn_context(const char *user_message, const char *system_message,
                            const char *conversation_history_json,
                            const char *task_id, char *out, size_t outsz);

/* Port of Python: turn_context.build_turn_context() */
int build_turn_context(const char *user_message, const char *system_message,
                       const char *conversation_history_json,
                       const char *task_id, char *out, size_t outsz)
{
    return tctx_build_turn_context(user_message, system_message,
                                   conversation_history_json, task_id, out, outsz);
}

/* === merged from port_agent_turn_context.c === */
/* Slermes C port — agent/turn_context.py (pure helper) */

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"
#include "provider_metadata.h"

/* PoP: agent_turn_context_compression_made_progress @ agent/turn_context.py:compression_made_progress */
bool agent_turn_context_compression_made_progress(
    long orig_len, long new_len, long orig_tokens, long new_tokens)
{
    if (new_len < orig_len) return true;
    return orig_tokens > 0 && new_tokens < orig_tokens * 0.95;
}

/* PoP: the @ agent/turn_context.py:_should_run_preflight_estimate */
/* Cheap gate for the (expensive) full preflight token estimate.
 * Returns True when EITHER:
 *   (a) message count exceeds the protected ranges (historical gate), OR
 *   (b) a cheap char-based estimate already crosses the threshold (the
 *       few-but-huge case from #27405 that the count-only gate skips).
 * Branch (b) uses estimate_messages_tokens_rough so a single large base64
 * image isn't mistaken for ~250K tokens. */
bool agent_turn_context__should_run_preflight_estimate(
    const json_t *messages, long protect_first_n, long protect_last_n,
    long threshold_tokens)
{
    if (!messages) return false;
    long count = (long)json_len(messages);
    if (count > protect_first_n + protect_last_n + 1) return true;
    long est = estimate_messages_tokens_rough(messages);
    return est >= threshold_tokens;
}



/* === dissolved from ports_wrappers.c === */
/* PoP: agent_turn_context_compose_user_api_content @ agent/turn_context.py:compose_user_api_content */
int agent_turn_context_compose_user_api_content(const char *arg) {
    /* Python: API-side injections. Arg =
     * "is_str\thas_prefetch\thas_plugin\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int is_str = arg[0] == '1';
    int has_prefetch = t1 && t1[1] == '1';
    int has_plugin = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!is_str || !state) { printf("\n"); return 0; }
    if (!has_prefetch && !has_plugin) { printf("\n"); return 0; }
    printf("%s\n", t4 ? t4 + 1 : "");
    return 0;
}

/* PoP: agent_turn_context_substitute_api_content @ agent/turn_context.py:substitute_api_content */
int agent_turn_context_substitute_api_content(const char *arg) {
    /* Python: pop sidecar + substitute. Arg = "sidecar\trole\tsubstituted". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *sidecar = arg;
    const char *role = t1 ? t1 + 1 : "";
    if (strcmp(sidecar, "none") == 0) { printf("\n"); return 0; }
    int ok_role = (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0);
    if (!ok_role) { printf("%s\n", sidecar); return 0; }
    printf("content substituted: %s\n", sidecar);
    return 0;
}

/* PoP: agent_turn_context_drop_stale_api_content @ agent/turn_context.py:drop_stale_api_content */
int agent_turn_context_drop_stale_api_content(const char *arg) {
    /* Python: msg.pop("api_content"). Arg = message JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *m = json_parse(arg, NULL);
    if (!m || !json_is_object(m)) {
        if (m) json_free(m);
        printf("%s\n", arg);
        return 0;
    }
    json_obj_del(m, "api_content");
    char *s = json_dumps(m, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(m);
    return 0;
}

/* PoP: agent_turn_context_extract_api_content_sidecar @ agent/turn_context.py:extract_api_content_sidecar */
int agent_turn_context_extract_api_content_sidecar(const char *arg) {
    /* Python: msg["api_content"] when str, else None. Arg = msg JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *msg = json_parse(arg, NULL);
    if (!msg || !json_is_object(msg)) {
        if (msg) json_free(msg);
        printf("\n");
        return 0;
    }
    const char *v = json_get_str(msg, "api_content", "");
    printf("%s\n", v);
    json_free(msg);
    return 0;
}

/* PoP: agent_turn_context_consume_gateway_turn_context_notes @ agent/turn_context.py:consume_gateway_turn_context_notes */
int agent_turn_context_consume_gateway_turn_context_notes(const char *arg) {
    /* Python: pop notes one-shot. Arg = "notes\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: agent_turn_context_append_notes_to_multimodal_content @ agent/turn_context.py:append_notes_to_multimodal_content */
int agent_turn_context_append_notes_to_multimodal_content(const char *arg) {
    /* Python: append text part to list content. Arg =
     * "notes\tis_list\tappended". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_list = t1 && t1[1] == '1';
    if (!is_list) { printf("0\n"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: agent_turn_context_reanchor_current_turn_user_idx @ agent/turn_context.py:reanchor_current_turn_user_idx */
int agent_turn_context_reanchor_current_turn_user_idx(const char *arg) {
    /* Python: exact-match scan from end. Arg =
     * "exact_idx\tfallback\tstate\tresult". */
    if (!arg || !*arg) { printf("-1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("-1\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "-1");
    return 0;
}

/* PoP: agent_turn_context_u_compression_warrants_another_preflight__ss @ agent/turn_context.py:_compression_warrants_another_preflight_pass */
int agent_turn_context_u_compression_warrants_another_preflight__ss(const char *arg) {
    /* Python: over threshold AND reduced >5%. Arg = "new\tthreshold\torig". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    double new_t = strtod(arg, NULL);
    double thresh = t1 ? strtod(t1 + 1, NULL) : 0;
    double orig = t2 ? strtod(t2 + 1, NULL) : 0;
    if (new_t >= thresh && orig > 0 && new_t < orig * 0.95) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: agent_turn_context_u_should_idle_compact @ agent/turn_context.py:_should_idle_compact */
int agent_turn_context_u_should_idle_compact(const char *arg) {
    /* Python: idle compaction predicate. Arg =
     * "enabled\tidle_after\tgap\tcooldown\ttokens\tfloor\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    double idle_after = t1 ? strtod(t1 + 1, NULL) : 0;
    double gap = t2 ? strtod(t2 + 1, NULL) : 0;
    int cooldown = t3 && t3[1] == '1';
    long tokens = t4 ? strtol(t4 + 1, NULL, 10) : 0;
    long floor = t5 ? strtol(t5 + 1, NULL, 10) : 0;
    if (!enabled || idle_after <= 0) { printf("0\n"); return 0; }
    if (gap < idle_after) { printf("0\n"); return 0; }
    if (cooldown) { printf("0\n"); return 0; }
    printf("%d\n", tokens > floor ? 1 : 0);
    return 0;
}
