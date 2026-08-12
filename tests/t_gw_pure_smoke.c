/* t_gw_pure_smoke.c — runtime smoke test for newly ported pure gateway/run.py
 * helpers: compression_made_progress, as_thread_info.
 * gw_multiplex_profile_homes delegates to the already-tested
 * profile_profiles_to_serve(true); exercised by its own fixture coverage. */
#include "gateway_run_pure2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } \
    else { printf("ok: %s\n", m); } } while (0)

int main(void) {
    int failures = 0;

    /* compression_made_progress: row-count drop counts; token drop >5% counts. */
    CHECK(gw_compression_made_progress(10, 8, 1000, 1200), "row drop 10->8 = progress (tokens up)");
    CHECK(!gw_compression_made_progress(10, 10, 1000, 960), "no row drop, 4% token wobble (<5%) = not progress");
    CHECK(gw_compression_made_progress(10, 10, 1000, 940), "no row drop, 6% token drop = progress");
    CHECK(!gw_compression_made_progress(10, 10, 0, 0), "orig_tokens=0 -> progress needs row drop");

    /* as_thread_info */
    CHECK(gw_as_thread_info(NULL) == NULL, "as_thread_info NULL -> NULL");
    CHECK(gw_as_thread_info("[1,2]") == NULL, "as_thread_info ints -> NULL (non-string)");
    char *ti = gw_as_thread_info("[\"thread-42\",\"renamed thread\"]");
    CHECK(ti != NULL, "as_thread_info string pair -> ok");
    if (ti) {
        CHECK(strcmp(ti, "thread-42") == 0, "thread_id == thread-42");
        CHECK(strcmp(ti + strlen(ti) + 1, "renamed thread") == 0, "initial_name == renamed thread");
        free(ti);
    }
    CHECK(gw_as_thread_info("[\"only\"]") == NULL, "as_thread_info 1-elem -> NULL");
    CHECK(gw_as_thread_info("not array") == NULL, "as_thread_info non-array -> NULL");

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
