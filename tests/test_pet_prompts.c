/*
 * test_pet_prompts.c — unit tests for the pure agent/pet/generate/prompts.py
 * prompt builders. Invariants derived from a Python oracle.
 */

#include "pet_prompts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

static void check(const char *label, int cond)
{
    if (!cond) { printf("FAIL: %s\n", label); g_fail++; }
    else printf("ok: %s\n", label);
}

int main(void)
{
    char *s;

    /* style_hint */
    s = pet_prompts_style_hint("auto");
    check("style(auto) prefix", s && strncmp(s, " Style: crisp 16-bit PIX", 23) == 0);
    check("style(auto) em-dash", s && strstr(s, "\xe2\x80\x94") != NULL);
    free(s);

    s = pet_prompts_style_hint("pixel");
    check("style(pixel)", s && strcmp(s, " Render in clean 16-bit pixel-art style with visible square pixels and a limited palette.") == 0);
    free(s);

    s = pet_prompts_style_hint("nope");
    check("style(unknown) empty", s && strcmp(s, "") == 0);
    free(s);

    s = pet_prompts_style_hint(NULL);
    check("style(None) auto", s && strncmp(s, " Style: crisp 16-bit PIX", 23) == 0);
    free(s);

    /* spacing_spec */
    int pose, gap;
    pet_prompts_spacing_spec(4, &pose, &gap);
    check("spacing(4) pose", pose == 269);
    check("spacing(4) gap", gap == 115);
    pet_prompts_spacing_spec(0, &pose, &gap);
    check("spacing(0) pose", pose == 1075);
    check("spacing(0) gap", gap == 461);

    /* build_base_prompt */
    s = pet_prompts_build_base(NULL, NULL, NULL);
    check("base(def) len", s && strlen(s) == 1410);
    check("base(def) concept", s && strncmp(s, "A stylized mascot pet character: a distinctive mascot creature", 55) == 0);
    free(s);

    s = pet_prompts_build_base("a red fox", NULL, NULL);
    check("base(a red fox) prefix", s && strncmp(s, "A stylized mascot pet character: a red f", 39) == 0);
    free(s);

    s = pet_prompts_build_base("x", "auto", "bolder colors");
    check("base(variation) nudge", s && strstr(s, "Make this design distinct: bolder colors.") != NULL);
    free(s);

    /* build_row_prompt */
    s = pet_prompts_build_row("idle", 4, "a fox", NULL);
    check("row(idle) len", s && strlen(s) == 3130);
    check("row(idle) action", s && strstr(s, "idle loop") != NULL);
    check("row(idle) posepx", s && strstr(s, "269px wide on a 1536px") != NULL);
    free(s);

    s = pet_prompts_build_row("bogus", 4, "a fox", NULL);
    check("row(bogus) default action", s && strstr(s, "simple idle pose") != NULL);
    free(s);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
