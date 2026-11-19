#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/turn_context.py: 5 missing functions ── */

/* preflight_request_tokens: deferred (multi-estimator orchestration). */
int preflight_request_tokens(void) { return 0; }

/* fail_closed_after_preflight_timeout: deferred (window-fit orchestration). */
int fail_closed_after_preflight_timeout(void) { return 0; }

/* ================================================================
 * tail_revive1 ports (wave 3). Same seam contract as
 * context_compressor_gaps.c (tail1_seam_fn, NULL == None,
 * {"__raise__": cls, "msg": str} exceptions, BaseException kinds pass
 * through `except Exception`). Agent attributes are injected via
 * tcg_state_install().
 * ================================================================ */

#ifndef TAIL1_SEAM_FN_DEFINED
#define TAIL1_SEAM_FN_DEFINED
typedef json_t *(*tail1_seam_fn)(void *ctx, const char *name, const json_t *args);
#endif

static json_t *tcg_state = NULL;          /* owned copy of agent attrs */
static tail1_seam_fn tcg_seam_fn = NULL;
static void *tcg_seam_ctx = NULL;

void tcg_state_install(const json_t *state)
{
    if (tcg_state) json_free(tcg_state);
    tcg_state = state ? json_copy(state) : NULL;
}

void tcg_seams_install(tail1_seam_fn fn, void *ctx)
{
    tcg_seam_fn = fn;
    tcg_seam_ctx = ctx;
}

static int tcg_jtruth(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static int tcg_is_raise(const json_t *v)
{
    if (!v || v->type != JSON_OBJECT) return 0;
    json_t *k = json_obj_get(v, "__raise__");
    return k && k->type == JSON_STRING;
}

static int tcg_is_base_exc(const json_t *v)
{
    if (!tcg_is_raise(v)) return 0;
    const char *k = json_obj_get(v, "__raise__")->str_val;
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}

static json_t *tcg_seam(const char *name, json_t *args)
{
    json_t *r;
    if (!tcg_seam_fn) {
        r = json_object();
        json_set(r, "__raise__", json_string("RuntimeError"));
        json_set(r, "msg", json_string("tail1 seam not installed"));
    } else {
        r = tcg_seam_fn(tcg_seam_ctx, name, args);
    }
    json_free(args);
    return r;
}

/* PoP: agent_stale_thinking_on_wire @ agent/turn_context.py:_agent_stale_thinking_on_wire */
json_t *agent_stale_thinking_on_wire(void)
{
    /* _str_attr(agent, k) == getattr(agent, k, "") or "" */
    static const char *keys[] = { "api_mode", "provider", "model", "base_url" };
    json_t *args = json_array();
    for (size_t i = 0; i < 4; i++) {
        json_t *v = tcg_state ? json_obj_get(tcg_state, keys[i]) : NULL;
        json_append(args, tcg_jtruth(v) ? json_copy(v) : json_string(""));
    }
    json_t *r = tcg_seam("stale_thinking_reaches_wire", args);
    if (tcg_is_raise(r)) {
        if (tcg_is_base_exc(r)) return r;
        json_free(r);
        return json_bool(true); /* except Exception: conservative True */
    }
    return r;
}

/* PoP: review_fork_first_request_pending @ agent/turn_context.py:_review_fork_first_request_pending */
int review_fork_first_request_pending(const json_t *agent)
{
    json_t *defer = agent ? json_obj_get(agent, "_review_defer_compaction_before_first_response") : NULL;
    json_t *got = agent ? json_obj_get(agent, "_turn_received_provider_response") : NULL;
    /* bool(getattr(..., False) and not getattr(..., False)) */
    return tcg_jtruth(defer) && !tcg_jtruth(got);
}
