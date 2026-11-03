/* Focused check of the curator_* aliases. The full test_curator.c drags in the
 * whole cron subsystem at link time, so the aliases are verified directly. */
#include "hermes_curator.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;
static void ck(const char *n, int c) {
    printf("  %s: %s\n", c ? "PASS" : "FAIL", n);
    c ? pass++ : fail++;
}

int main(void)
{
    printf("=== curator aliases ===\n");

    curator_state_t st;
    memset(&st, 0xFF, sizeof(st));
    curator_init_state(&st);
    ck("init: enabled", st.enabled == true);
    ck("init: not paused", st.paused == false);
    ck("init: run_count 0", st.run_count == 0);
    ck("init: last_run_at 0", st.last_run_at == 0);
    ck("init: duration 0", st.last_run_duration == 0.0);
    ck("init: summary empty", st.last_run_summary[0] == '\0');
    curator_init_state(NULL);
    ck("init NULL no-op", 1);

    char buf[64];
    curator_format_duration(0, buf, sizeof buf);
    ck("duration 0s", strcmp(buf, "0s") == 0);
    curator_format_duration(59, buf, sizeof buf);
    ck("duration 59s", strcmp(buf, "59s") == 0);
    curator_format_duration(60, buf, sizeof buf);
    ck("duration 1m 0s", strcmp(buf, "1m 0s") == 0);

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
