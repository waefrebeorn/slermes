/*
 * test_reasoning_summaries.c — behavioral oracle for
 * src/agent/reasoning_summaries.c.
 * Cross-checked against the real Python agent/reasoning_summaries.py:
 *   separate_glued_reasoning_blocks.
 */
#include "reasoning_summaries.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
static int g_pass = 0;

static void check(const char *name, const char *prev, const char *delta,
                  const char *expected /* NULL => delta unchanged */) {
    char *got = rs_separate_glued_reasoning_blocks(prev, delta);
    const char *exp = expected ? expected : delta;
    if (got && strcmp(got, exp) == 0) {
        g_pass++;
    } else {
        g_fail++;
        printf("FAIL %s: got=[%s] exp=[%s]\n", name, got ? got : "(null)", exp);
    }
    free(got);
}

int main(void) {
    /* Case 1: two glued heading-only parts -> break inserted */
    check("glue_headings",
          "**Investigating likely culprit PRs**",
          "**Inspecting message schema**",
          "\n\n**Inspecting message schema**");

    /* Case 2: prose body butting against next heading */
    check("glue_prose",
          "**Investigating likely culprit PRs**...interaction!",
          "**Next**",
          "\n\n**Next**");

    /* Case 3: previous already ends whitespace -> no break */
    check("already_sep",
          "**One**\n",
          "**Two**",
          "**Two**");

    /* Case 4: delta is NOT a heading -> unchanged */
    check("not_heading",
          "**One**",
          "continuing thought",
          "continuing thought");

    /* Case 5: delta opens emphasis but no CLOSED heading -> unchanged
     * (token-streamed fragment). */
    check("open_fragment",
          "**One**",
          "**bold",
          "**bold");

    /* Case 6: empty previous -> delta unchanged */
    check("empty_prev",
          "",
          "**Two**",
          "**Two**");

    /* Case 7: empty delta -> "" */
    check("empty_delta",
          "**One**",
          "",
          "");

    /* Case 8: NULL previous -> delta unchanged */
    check("null_prev",
          NULL,
          "**Two**",
          "**Two**");

    /* Case 9: multiple heading parts chained */
    check("chain",
          "**A****B**",
          "**C**",
          "\n\n**C**");

    if (g_fail == 0)
        printf("ORACLE PASSED (%d checks)\n", g_pass);
    else
        printf("ORACLE FAILED (%d failures, %d passed)\n", g_fail, g_pass);
    return g_fail ? 1 : 0;
}
