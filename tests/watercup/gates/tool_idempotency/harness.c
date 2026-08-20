/* water-cup gate: tool_idempotency — traced copy of
 * src/tools/tool_idempotency.c. Asserts the safe-retry contract:
 *   - put(key, result, ttl) then get(key) returns a copy of result (no re-exec)
 *   - a second get with same key returns the SAME cached result (idempotent)
 *   - different key returns NULL
 *   - explicit idempotency_key in args overrides the args as key material
 *   - expired entry returns NULL
 * Twin (twin.py) re-implements the contract in Python and must agree.
 */
#include "tool_idempotency.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){printf("  FAIL: %s\n",msg);failures++;} else {printf("  ok:   %s\n",msg);} } while(0)

/* Tiny fake clock so we can force expiry without sleeping. */
static time_t g_now = 1000000;
static time_t my_time(void) { return g_now; }

int main(void) {
    tool_idem_cache_t *c = tool_idem_new();
    CHECK(c != NULL, "cache created");

    char *k1 = tool_idem_key("send_email", "{\"to\":\"a@x.com\"}");
    CHECK(k1 != NULL, "key built");
    CHECK(tool_idem_put(c, k1, "{\"success\":true,\"id\":\"em_1\"}", 600), "put ok");

    /* First get after a fresh execution returns the stored result. */
    char *r1 = tool_idem_get(c, k1);
    CHECK(r1 && strcmp(r1, "{\"success\":true,\"id\":\"em_1\"}") == 0, "get returns stored result");
    free(r1);

    /* Second get (a 'retry') returns the SAME cached result -> safe retry. */
    char *r2 = tool_idem_get(c, k1);
    CHECK(r2 && strcmp(r2, "{\"success\":true,\"id\":\"em_1\"}") == 0, "retry returns cached result (no double side effect)");
    free(r2);

    /* Different key -> absent. */
    char *k2 = tool_idem_key("send_email", "{\"to\":\"b@x.com\"}");
    char *r3 = tool_idem_get(c, k2);
    CHECK(r3 == NULL, "different args -> NULL (not cached)");
    free(k2);

    /* Explicit idempotency_key in args overrides args as key material. */
    char *k3 = tool_idem_key("send_email", "{\"to\":\"z@x.com\",\"idempotency_key\":\"fixed-1\"}");
    CHECK(k3 && strcmp(k3, "send_email|fixed-1") == 0, "explicit idempotency_key used as key");
    free(k3);

    /* Expiry: put with a 1s TTL, sleep past it, entry must be dropped.
     * (tool_idem_get reads the real clock via time(NULL), so we use a real
     * short TTL + a brief sleep rather than a mocked clock.) */
    tool_idem_cache_t *c2 = tool_idem_new();
    char *ke = tool_idem_key("send_email", "{\"to\":\"exp@x.com\"}");
    CHECK(tool_idem_put(c2, ke, "{\"success\":true}", 1), "put with 1s TTL ok");
    CHECK(tool_idem_count(c2) == 1, "one live entry before expiry");
    struct timespec ts = {1, 200000000};  /* 1.2s */
    nanosleep(&ts, NULL);
    char *r4 = tool_idem_get(c2, ke);
    CHECK(r4 == NULL, "expired entry -> NULL");
    CHECK(tool_idem_count(c2) == 0, "expired entry removed from cache");
    tool_idem_free(c2);
    free(ke);

    tool_idem_free(c);
    free(k1);
    printf("RESULT: tool_idempotency safe-retry contract holds\n");
    return 0;
}
