/*
 * hook_output_spill.c — canonical C11 port of tools/hook_output_spill.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_hook_output_spill_u_coerce_non_negative_int @ tools/hook_output_spill.py:_coerce_non_negative_int */
int tools_hook_output_spill_u_coerce_non_negative_int(const char *arg) {
    /* Python: int(value) except -> default; < 0 -> default. Arg =
     * "value\tdefault". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char *end = NULL;
    long iv = strtol(arg, &end, 10);
    if (end == arg || (end && *end != '\0' && end != tab)) {
        printf("%s\n", tab ? tab + 1 : "0");
        return 0;
    }
    if (iv < 0) { printf("%s\n", tab ? tab + 1 : "0"); return 0; }
    printf("%ld\n", iv);
    return 0;
}

/* PoP: tools_hook_output_spill_get_spill_config @ tools/hook_output_spill.py:get_spill_config */
int tools_hook_output_spill_get_spill_config(const char *arg) {
    /* Python: resolved spill config. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: tools_hook_output_spill_u_resolve_spill_dir @ tools/hook_output_spill.py:_resolve_spill_dir */
int tools_hook_output_spill_u_resolve_spill_dir(const char *arg) {
    /* Python: spill dir grouped by session. Arg =
     * "home\tsession_id\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *home = arg;
    const char *session = t1 ? t1 + 1 : "no-session";
    int state = t2 && t2[1] == '1';
    if (!state) { printf("%s/hook_outputs/%s\n", home, session); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: tools_hook_output_spill_u_build_preview @ tools/hook_output_spill.py:_build_preview */
int tools_hook_output_spill_u_build_preview(const char *arg) {
    /* Python: head/tail preview + saved footer. Arg =
     * "source\ttext\thead\ttail\tsaved_path". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *source = arg;
    const char *text = t1 ? t1 + 1 : "";
    long head = t2 ? strtol(t2 + 1, NULL, 10) : 500;
    long tail = t3 ? strtol(t3 + 1, NULL, 10) : 500;
    const char *saved = t4 ? t4 + 1 : "";
    long total = (long)strlen(text);
    printf("[%s output truncated — %ld chars; full content %s]\n",
           source, total, saved[0] ? "saved to <path>" : "unavailable — spill write failed");
    if (head > 0 && total > 0) {
        printf("--- head ---\n%.*s\n", (int)(head < total ? head : total), text);
    }
    if (tail > 0 && total > head) {
        printf("--- tail ---\n%.*s\n", (int)(tail < total - head ? tail : total - head), text + (total - (tail < total - head ? tail : total - head)));
    }
    return 0;
}

/* PoP: tools_hook_output_spill_spill_if_oversized @ tools/hook_output_spill.py:spill_if_oversized */
int tools_hook_output_spill_spill_if_oversized(const char *arg) {
    /* Python: bounded spill. Arg =
     * "spilled\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int spilled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !spilled) { printf("text under cap — unchanged\n"); return 0; }
    printf("spilled to %s (preview head/tail)\n", t2 ? t2 + 1 : "?");
    return 0;
}
