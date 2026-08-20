/*
 * test_empty_response_guard.c — behavioral oracle for empty_response_guard.c
 *
 * Build:
 *   cc -D_POSIX_C_SOURCE=200809L -I../include -I../lib/libjson \
 *      ../src/agent/empty_response_guard.c ../src/agent/usage_pricing.c \
 *      test_empty_response_guard.c ../lib/libjson/json.o -lm -o t && ./t
 */
#include "empty_response_guard.h"
#include "hermes_json.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CK(cond) do { if(!(cond)){ printf("FAIL: %s (line %d)\n", #cond, __LINE__); fails++; } else { printf("ok: %s\n", #cond); } } while(0)
static int fails = 0;

/* build a usage json {prompt_tokens, completion_tokens/output_tokens, ...}.
 * reasoning_tokens go under output_tokens_details, matching how
 * normalize_usage extracts them (and real OpenAI responses). */
static json_t *mk_usage(long long in, long long out, long long reasoning) {
    json_t *u = json_object();
    json_set(u, "prompt_tokens", json_number((double)in));
    json_set(u, "completion_tokens", json_number((double)out));
    json_set(u, "output_tokens", json_number((double)out));
    if (reasoning >= 0) {
        json_t *od = json_object();
        json_set(od, "reasoning_tokens", json_number((double)reasoning));
        json_set(u, "output_tokens_details", od);
    }
    return u;
}

int main(void) {
    empty_guard_t g;
    empty_guard_init(&g);

    /* resolve_guard_settings: defaults */
    bool en; double th;
    resolve_guard_settings(NULL, &en, &th);
    CK(en == true);
    CK(fabs(th - 0.25) < 1e-9);

    /* resolve_guard_settings: disabled + custom threshold */
    json_t *cfg = json_object();
    json_set(cfg, "enabled", json_bool(false));
    json_set(cfg, "cost_threshold_usd", json_number(1.5));
    resolve_guard_settings(cfg, &en, &th);
    CK(en == false);
    CK(fabs(th - 1.5) < 1e-9);

    /* resolve_guard_settings: YAML-quoted "false" string */
    json_t *cfg2 = json_object();
    json_set(cfg2, "enabled", json_string("false"));
    resolve_guard_settings(cfg2, &en, &th);
    CK(en == false);

    /* guard_enabled reflects config */
    CK(guard_enabled(&g) == true);
    g.enabled = false;
    CK(guard_enabled(&g) == false);
    g.enabled = true;

    /* --- deterministic empty: two consecutive empties, same signature --- */
    empty_guard_init(&g);
    json_t *u0 = mk_usage(100, 0, 0);  /* 100 in, 0 out, 0 reasoning */
    record_empty_attempt(&g, 0, "gpt-4o", "openai", "stop", u0, "openai", NULL);
    CK(deterministic_empty(&g) == false);  /* only 1 attempt */
    json_t *u1 = mk_usage(120, 0, 0);
    record_empty_attempt(&g, 1, "gpt-4o", "openai", "stop", u1, "openai", NULL);
    CK(deterministic_empty(&g) == true);   /* 2 empties, same sig, usage present */

    /* --- fail open: unknown usage never deterministic --- */
    empty_guard_init(&g);
    record_empty_attempt(&g, 0, "gpt-4o", "openai", "stop", NULL, "openai", NULL);
    record_empty_attempt(&g, 1, "gpt-4o", "openai", "stop", NULL, "openai", NULL);
    CK(deterministic_empty(&g) == false);

    /* --- fail open: non-zero output (think-block stripped) not deterministic --- */
    empty_guard_init(&g);
    json_t *u2 = mk_usage(100, 5, 0);  /* generated something */
    record_empty_attempt(&g, 0, "gpt-4o", "openai", "stop", u2, "openai", NULL);
    record_empty_attempt(&g, 1, "gpt-4o", "openai", "stop", u2, "openai", NULL);
    CK(deterministic_empty(&g) == false);

    /* --- reasoning-only response is NOT a deterministic empty --- */
    empty_guard_init(&g);
    json_t *u3 = mk_usage(100, 0, 50);  /* reasoning tokens present */
    record_empty_attempt(&g, 0, "o3", "openai", "stop", u3, "openai", NULL);
    record_empty_attempt(&g, 1, "o3", "openai", "stop", u3, "openai", NULL);
    CK(deterministic_empty(&g) == false);

    /* --- different signature breaks determinism --- */
    empty_guard_init(&g);
    record_empty_attempt(&g, 0, "gpt-4o", "openai", "stop", u0, "openai", NULL);
    record_empty_attempt(&g, 1, "gpt-4o", "anthropic", "stop", u0, "anthropic", NULL);
    CK(deterministic_empty(&g) == false);

    /* --- streak clears when retry_count==0 --- */
    empty_guard_init(&g);
    record_empty_attempt(&g, 0, "gpt-4o", "openai", "stop", u0, "openai", NULL);
    record_empty_attempt(&g, 1, "gpt-4o", "openai", "stop", u1, "openai", NULL);
    CK(deterministic_empty(&g) == true);
    record_empty_attempt(&g, 0, "gpt-4o", "openai", "stop", u0, "openai", NULL);  /* new streak */
    CK(deterministic_empty(&g) == false);

    /* --- empty_retry_budget: default 3, disabled -> 3 --- */
    empty_guard_init(&g);
    CK(empty_retry_budget(&g, "gpt-4o", "openai", u0, "openai", NULL) == 3);
    g.enabled = false;
    CK(empty_retry_budget(&g, "gpt-4o", "openai", u0, "openai", NULL) == 3);
    g.enabled = true;

    /* --- streak_cost_usd accumulates --- */
    empty_guard_init(&g);
    record_empty_attempt(&g, 0, "gpt-4o", "openai", "stop", u0, "openai", NULL);
    double sc = streak_cost_usd(&g);
    CK(sc >= 0.0);  /* known pricing (gpt-4o) => positive */

    empty_guard_free(&g);
    json_free(cfg); json_free(cfg2); json_free(u0); json_free(u1); json_free(u2); json_free(u3);

    printf("\n%s (%d failures)\n", fails ? "ORACLE FAILED" : "ORACLE PASSED", fails);
    return fails ? 1 : 0;
}
