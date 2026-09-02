/* gate: agent_prompt_builder_build_nous_subscription_prompt — traced copy of
 * src/agent/prompt_builder.c::build_nous_subscription_prompt
 * (LIVE twin: agent/prompt_builder.py:build_nous_subscription_prompt).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: always taken
 *   VSTR(prompt_text)
 */
#include <stdio.h>
#include <string.h>
#include "watercup.h"

/* from shim */
extern const char *build_nous_subscription_prompt(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    for (int i = 0; i < 3; i++) {
        const char *r = build_nous_subscription_prompt();
        WCUP_BEGIN("build_nous_subscription_prompt");
        WCUP_BRANCH(1);
        WCUP_VALSTR(r);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("build_nous_subscription_prompt");
    }
    return 0;
}
