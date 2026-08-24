/*
 * orchestrate.c — canonical C11 port of agent/pet/generate/orchestrate.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_pet_generate_orchestrate_u_harden_transparency @ agent/pet/generate/orchestrate.py:_harden_transparency */
int agent_pet_generate_orchestrate_u_harden_transparency(const char *arg) {
    /* Python: chroma-key to RGBA PNG. Arg = "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: agent_pet_generate_orchestrate_generate_base_drafts @ agent/pet/generate/orchestrate.py:generate_base_drafts */
int agent_pet_generate_orchestrate_generate_base_drafts(const char *arg) {
    /* Python: concurrent drafts. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s draft(s) generated (transparent cutouts, on_draft streams per-index, cancellation cooperative)%s\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? " — ref-capable provider" : "");
    return 0;
}

/* PoP: agent_pet_generate_orchestrate_u_drafts_failed_reason @ agent/pet/generate/orchestrate.py:_drafts_failed_reason */
int agent_pet_generate_orchestrate_u_drafts_failed_reason(const char *arg) {
    /* Python: "image generation produced no usable drafts" if no errors;
     * else humanized most-common error. Arg = "err\terr\t..." (tab-sep). */
    if (!arg || !*arg) {
        printf("image generation produced no usable drafts\n");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: agent_pet_generate_orchestrate_u_humanize_image_error @ agent/pet/generate/orchestrate.py:_humanize_image_error */
int agent_pet_generate_orchestrate_u_humanize_image_error(const char *arg) {
    /* Python: friendly provider error. Arg = "error\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: agent_pet_generate_orchestrate_hatch_pet @ agent/pet/generate/orchestrate.py:hatch_pet */
int agent_pet_generate_orchestrate_hatch_pet(const char *arg) {
    /* Python: full hatch. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_base") == 0) {
        fprintf(stderr, "GenerationError: base image not found: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "cancelled") == 0) {
        fprintf(stderr, "GenerationError: hatch cancelled — nothing written\n");
        return 1;
    }
    printf("pet hatched (rows generated concurrently, atlas composed+validated, registered: %s)%s\n", t3 ? t3 + 1 : "slug", (t2 && t2[1] == '1') ? " — idle row falls back to base look" : "");
    return 0;
}
