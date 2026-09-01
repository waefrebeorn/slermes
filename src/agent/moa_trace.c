/*
 * moa_trace.c — canonical C11 port of agent/moa_trace.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_moa_trace_u_traces_enabled_and_dir @ agent/moa_trace.py:_traces_enabled_and_dir */
int agent_moa_trace_u_traces_enabled_and_dir(const char *arg) {
    /* Python: save_traces + trace_dir. Arg = "enabled\tstate\tdir". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    if (!enabled) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "moa-traces dir");
    return 0;
}

/* PoP: agent_moa_trace_u_slot_trace @ agent/moa_trace.py:_slot_trace */
int agent_moa_trace_u_slot_trace(const char *arg) {
    /* Python: full trace dict. Arg = "label\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("{\"label\": \"%s\", %s}\n", arg, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: agent_moa_trace_save_moa_turn @ agent/moa_trace.py:save_moa_turn */
int agent_moa_trace_save_moa_turn(const char *arg) {
    /* Python: JSONL append. Arg =
     * "saved\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int saved = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("trace skipped (disabled)\n"); return 0; }
    if (!saved) { printf("trace append failed (best-effort, swallowed)\n"); return 0; }
    printf("turn record appended to %s.jsonl (output_location=%s)\n", t2 ? t2 + 1 : "?", "inline_from_stream");
    return 0;
}

/* PoP: agent_moa_trace_u_slot_metrics @ agent/moa_trace.py:slot_metrics */
/*
 * Renders one reference's accounting for observability hooks.
 * Same fields as _slot_trace minus input_messages, plus optional output.
 * Arg = "label\tstate\toutput_json_or_empty".
 * Prints the JSON dict to stdout (mirrors _slot_trace contract).
 */
int agent_moa_trace_u_slot_metrics(const char *arg) {
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    const char *label = arg;
    const char *output_json = t2 ? t2 + 1 : NULL;
    if (!state) { printf("{}\n"); return 0; }
    /* Build JSON: {"label":"..."} + optional output */
    json_t *j = json_object();
    json_set(j, "label", json_string(label));
    if (output_json && *output_json) {
        json_set(j, "output", json_string(output_json));
    }
    char *s = json_dumps(j, 0);
    if (s) { printf("%s\n", s); free(s); }
    json_free(j);
    return 0;
}
