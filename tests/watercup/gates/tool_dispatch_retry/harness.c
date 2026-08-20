/* water-cup gate: tool_dispatch_retry — verifies the agent loop's retry POLICY
 * (the concrete consumer of tool_classify_error). Traces the exact loop in
 * src/agent/conversation_loop.c::dispatch_with_retry:
 *   - on a TRANSIENT error, re-dispatch up to TOOL_RETRY_MAX times (exponential
 *     backoff); on PERMANENT/unknown, do NOT retry; on success, do NOT retry.
 * Uses the REAL tool_classify_error (links tool_result_classification.c) so the
 * policy is proven against the actual classifier. A mock dispatch counts calls
 * and can be made to return transient-then-success to prove the loop stops at
 * the first non-transient result. Twin.py re-implements the policy in Python.
 */
#include "tool_result_classification.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TOOL_RETRY_MAX 3

/* mock dispatch: returns the next canned result; counts invocations. */
static int g_calls = 0;
static const char **g_script = NULL;  /* NULL-terminated */
static int g_idx = 0;

static char *mock_dispatch(const char *name, const char *args, const char *sid) {
    (void)name; (void)args; (void)sid;
    const char *r = (g_script && g_script[g_idx]) ? g_script[g_idx++] : "{\"success\":true}";
    return strdup(r);
}

/* EXACT policy copy from conversation_loop.c::dispatch_with_retry. */
static char *dispatch_with_retry(const char *name, const char *args,
                                 const char *session_id) {
    char *result = mock_dispatch(name, args, session_id);
    for (int attempt = 1; attempt < TOOL_RETRY_MAX; attempt++) {
        if (tool_classify_error(result) != TOOL_ERR_TRANSIENT) break;
        struct timespec ts = { (time_t)(1u << (unsigned)(attempt - 1)), 0 };
        (void)ts; /* backoff elided in test */
        free(result);
        result = mock_dispatch(name, args, session_id);
    }
    return result;
}

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){printf("  FAIL: %s\n",msg);failures++;} else {printf("  ok:   %s\n",msg);} } while(0)

static void reset(const char **script) { g_calls = 0; g_idx = 0; g_script = script; }

int main(void) {
    /* 1) transient error repeated -> retries TOOL_RETRY_MAX times, final is transient */
    const char *s1[] = {"{\"error\":\"timeout\",\"_retry_class\":\"transient\"}",
                         "{\"error\":\"timeout\",\"_retry_class\":\"transient\"}",
                         "{\"error\":\"timeout\",\"_retry_class\":\"transient\"}", NULL};
    reset(s1);
    char *r1 = dispatch_with_retry("t", "{}", "s");
    CHECK(g_idx == TOOL_RETRY_MAX, "transient x3 -> dispatched TOOL_RETRY_MAX times");
    CHECK(strstr(r1, "transient") != NULL, "final result still transient");
    free(r1);

    /* 2) permanent error -> no retry (1 dispatch) */
    const char *s2[] = {"{\"error\":\"not found\",\"_retry_class\":\"permanent\"}", NULL};
    reset(s2);
    char *r2 = dispatch_with_retry("t", "{}", "s");
    CHECK(g_idx == 1, "permanent -> single dispatch (no retry)");
    free(r2);

    /* 3) success -> no retry */
    const char *s3[] = {"{\"success\":true}", NULL};
    reset(s3);
    char *r3 = dispatch_with_retry("t", "{}", "s");
    CHECK(g_idx == 1, "success -> single dispatch (no retry)");
    free(r3);

    /* 4) transient then success on 2nd call -> retried once, final success */
    const char *s4[] = {"{\"error\":\"timeout\",\"_retry_class\":\"transient\"}",
                         "{\"success\":true}", NULL};
    reset(s4);
    char *r4 = dispatch_with_retry("t", "{}", "s");
    CHECK(g_idx == 2, "transient->success -> 2 dispatches then stop");
    CHECK(strstr(r4, "success") != NULL, "final result is success (loop stopped)");
    free(r4);

    /* 5) unknown error (no _retry_class, fail-closed) -> no retry */
    const char *s5[] = {"{\"error\":\"something weird\"}", NULL};
    reset(s5);
    char *r5 = dispatch_with_retry("t", "{}", "s");
    CHECK(g_idx == 1, "unknown error -> single dispatch (fail-closed, no retry)");
    free(r5);

    if (failures) { printf("RESULT: %d FAILURES\n", failures); return 1; }
    printf("RESULT: tool_dispatch_retry policy correct (consumes tool_classify_error)\n");
    return 0;
}
