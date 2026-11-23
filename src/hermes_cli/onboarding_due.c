/*
 * onboarding_due.c — C home for upstream Hermes hermes_cli/onboarding_due.py.
 *
 * PoP: w12_onboarding_f4f54a_reset @ hermes_cli/onboarding_due.py:reset_for_tests
 * PoP: w12_onboarding_f4f54a_invalidate @ hermes_cli/onboarding_due.py:invalidate
 * PoP: w12_onboarding_f4f54a_store @ hermes_cli/onboarding_due.py:_store
 * PoP: w12_onboarding_f4f54a_read @ hermes_cli/onboarding_due.py:read
 */
#include "noc_w12_onboarding_f4f54a.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

/* Module-level cache state (same shape as the Python globals). */
static json_t *g_value = NULL;      /* _value */
static char   *g_stamp = NULL;      /* _value_stamp */
static long long g_generation = 0;  /* _generation */

static void clear_value(void) {
    if (g_value) { json_free(g_value); g_value = NULL; }
}

void w12_onboarding_f4f54a_invalidate(void) {
    clear_value();
    g_generation++;
}

void w12_onboarding_f4f54a_reset(void) {
    w12_onboarding_f4f54a_invalidate();
    /* _boot_done has no deterministic effect on the cached answer we model. */
}

int w12_onboarding_f4f54a_store(const struct json_t *value, long long generation,
                                const char *before, const char *after) {
    const char *b = before ? before : "";
    const char *a = after ? after : "";
    if (strcmp(b, a) != 0) return 0;              /* files moved mid-computation */
    if (generation != g_generation) return 0;    /* a writer landed meanwhile */
    clear_value();
    g_value = json_copy(value);
    free(g_stamp);
    g_stamp = strdup(a);
    return 1;
}

struct json_t *w12_onboarding_f4f54a_read(const struct json_t *value,
                                          const char *before, const char *after) {
    const char *seen = before ? before : "";
    if (g_value != NULL && g_stamp != NULL && strcmp(g_stamp, seen) == 0)
        return json_copy(g_value);               /* dict(_value) */
    long long generation = g_generation;
    json_t *v = json_copy(value);                /* compute() */
    w12_onboarding_f4f54a_store(v, generation, seen, after ? after : "");
    json_free(v);
    return json_copy(value);                     /* dict(value) */
}
