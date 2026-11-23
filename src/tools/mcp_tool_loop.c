/*
 * mcp_tool_loop.c -- faithful C11 port of upstream Hermes tools/mcp_tool_loop.py.
 *
 * PoP: w7_mcptoolloo_e73207_should_forward    @ tools/mcp_tool_loop.py:_mcp_loop_exception_handler
 * PoP: w7_mcptoolloo_e73207_wait_ready        @ tools/mcp_tool_loop.py:_wait_for_server_session_ready
 * PoP: w7_mcptoolloo_e73207_signal_reconnect  @ tools/mcp_tool_loop.py:_signal_reconnect
 *
 * The module is mostly asyncio/file-lock plumbing. The three pure decisions
 * that are portable and observable are implemented here: the benign
 * "Event loop is closed" suppression, the bounded ready-session poll, and the
 * thread-safe-vs-direct reconnect event set. Time/loop state is injected.
 */
#include "noc_w7_mcptoolloo_e73207.h"
#include <string.h>

static int flag(const json_t *o, const char *k, int dflt) {
    json_t *v = json_obj_get(o, k);
    if (!v) return dflt;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return dflt;
}

int w7_mcptoolloo_e73207_should_forward(int exc_is_runtime, const char *message) {
    if (!message) message = "";
    if (exc_is_runtime && strstr(message, "Event loop is closed")) return 0;
    return 1;
}

int w7_mcptoolloo_e73207_wait_ready(double timeout, const json_t *script) {
    double t = timeout > 0.0 ? timeout : 0.0;
    int iterations = (int)(t / 0.25);
    if (iterations < 1) iterations = 1;

    for (int i = 0; i < iterations; i++) {
        json_t *step = NULL;
        if (script && script->type == JSON_ARRAY && (size_t)i < json_len(script))
            step = script->c.items[i];

        int session = 0;
        if (step) session = (int)json_get_num(step, "session", 0.0);

        int is_ready = 1; /* getattr(srv,"_ready",None) with no is_set -> True */
        if (step) {
            int present = flag(step, "ready_present", 1);
            int raises = flag(step, "ready_raises", 0);
            if (present && !raises) is_ready = flag(step, "ready", 1);
        }

        /* session is not None, is not old_session (1), and is ready. */
        if (session == 2 && is_ready) return 1;
    }
    return 0;
}

int w7_mcptoolloo_e73207_signal_reconnect(int has_event, int event_is_asyncio, int loop_running) {
    if (!has_event) return 0;
    if (event_is_asyncio && loop_running) return 2;
    return 1;
}
