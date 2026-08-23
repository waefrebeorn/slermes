/* gate: xai_retirement_find_retired_xai_refs — traced copy of
 * src/xai_retirement.c::hermes_xai_find_retired / check_slot.
 * LIVE twin: hermes_cli/xai_retirement.py::find_retired_xai_refs.
 *
 * Input: "principal~delegation~vision~curator" ('~'-separated model slots;
 * empty field = slot unset). The harness fills a hermes_config_t exactly the
 * way the real port's config walk reads it and traces check_slot per slot.
 *
 * Rung order (must match twin.py EXACTLY):
 *   per slot in fixed order [principal.model, delegation.model,
 *   auxiliary.vision.model, auxiliary.curator.model]:
 *     B1  looks-like-xAI (normalized starts with "grok-")
 *     L   one tick per retired-table comparison
 *     B2    table entry matches normalized model
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

#define NUM_SLOTS 4
static const char *SLOT_PATHS[NUM_SLOTS] = {
    "principal.model", "delegation.model",
    "auxiliary.vision.model", "auxiliary.curator.model"
};

/* Same table order/content as src/xai_retirement.c RETIRED_MODELS[]. */
static const char *RET_REPL[] = {
    "grok-4.3", "grok-4.3", "grok-4.3", "grok-4.3", "grok-4.3",
    "grok-4.3", "grok-4.3", "grok-4.3", "grok-4.3", "grok-imagine-image-quality", NULL
};
static const char *RET_NAMES[] = {
    "grok-4", "grok-4-0709", "grok-4-fast", "grok-4-fast-reasoning",
    "grok-4-fast-non-reasoning", "grok-4-1-fast", "grok-4-1-fast-reasoning",
    "grok-4-1-fast-non-reasoning", "grok-code-fast-1", "grok-imagine-image-pro", NULL
};

static void normalize(const char *model, char *out, size_t cap) {
    if (!model || !model[0]) { out[0] = '\0'; return; }
    const char *p = model;
    while (*p && isspace((unsigned char)*p)) p++;
    size_t i = 0;
    const char *prefixes[] = {"x-ai/", "xai/", NULL};
    for (int pi = 0; prefixes[pi]; pi++) {
        size_t plen = strlen(prefixes[pi]);
        if (strncasecmp(p, prefixes[pi], plen) == 0) { p += plen; break; }
    }
    for (; *p && i < cap - 1; p++)
        out[i++] = (char)tolower((unsigned char)*p);
    while (i > 0 && isspace((unsigned char)out[i - 1])) i--;
    out[i] = '\0';
}

static void find_retired(char models[][128]) {
    WCUP_BEGIN("find_retired_xai_refs");
    char issues[16][640];
    int count = 0;

    for (int s = 0; s < NUM_SLOTS; s++) {
        const char *mv = models[s];
        char norm[128];
        normalize(mv, norm, sizeof norm);
        if (!norm[0] || strncmp(norm, "grok-", 5) != 0) {   /* B1 looks-like-xAI */
            WCUP_BRANCH(0);
            continue;
        }
        WCUP_BRANCH(1);
        for (int i = 0; RET_NAMES[i]; i++) {                /* L per comparison */
            WCUP_LOOP_TICK();
            if (strcmp(norm, RET_NAMES[i]) != 0) {          /* B2 match */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            const char *eff = (i == 4 || i == 7) ? "none" : "";
            snprintf(issues[count], 640, "%s|%s|%s|%s|",
                     SLOT_PATHS[s], mv, RET_REPL[i], eff);
            count++;
            break;
        }
    }

    /* value: ';'-joined issue records (empty string when none) */
    char val[8192];
    val[0] = '\0';
    for (int i = 0; i < count; i++)
        strcat(val, issues[i]);
    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("find_retired_xai_refs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s find_retired_xai_refs <m0~m1~m2~m3>...\n", argv[0]); return 2; }
    for (int a = 2; a < argc; a++) {
        char models[NUM_SLOTS][128];
        memset(models, 0, sizeof models);
        char buf[1024];
        snprintf(buf, sizeof buf, "%s", argv[a]);
        char *field = buf;
        for (int s = 0; s < NUM_SLOTS; s++) {
            char *tilde = strchr(field, '~');
            if (tilde) *tilde = '\0';
            snprintf(models[s], 128, "%s", field);
            field = tilde ? tilde + 1 : field + strlen(field);
        }
        find_retired(models);
    }
    return 0;
}
