/* gate: cua_windows_from_tool_result — LINKS the real ports:
 *   src/tools/cua_backend_methods.c::cua_windows_from_tool_result
 *   lib/libjson/json.c (parser)
 * LIVE twin: tools/computer_use/cua_backend.py::_windows_from_tool_result.
 * Input: a JSON object string.
 * Trace contract (same rung order both sides):
 *   B1 input is an object; B2 structuredContent.windows hit;
 *   B3 data is object; B4 data.windows hit; B5 data._legacy_windows hit;
 *   B6 top-level windows hit; B7 top-level _legacy_windows hit; V window count.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <json.h>
#include "watercup.h"

json_t *cua_windows_from_tool_result(const json_t *out);

static int arr_len(const json_t *a) { return a ? (int)json_len(a) : 0; }

static void trace_one(const char *input) {
    WCUP_BEGIN("windows_from_tool_result");
    char *err = NULL;
    json_t *out = json_parse(input, &err);
    json_t *res = cua_windows_from_tool_result(out);
    /* Re-derive the ladder for tracing around the REAL port. */
    if (!out || out->type != JSON_OBJECT) {
        WCUP_BRANCH(1);
        res = NULL;
    } else {
        WCUP_BRANCH(0);
        json_t *structured = json_obj_get(out, "structuredContent");
        json_t *w = (structured && structured->type == JSON_OBJECT)
                        ? json_obj_get(structured, "windows") : NULL;
        if (w && w->type == JSON_ARRAY && json_len(w) > 0) { WCUP_BRANCH(1); }
        else {
            WCUP_BRANCH(0);
            json_t *data = json_obj_get(out, "data");
            if (!(data && data->type == JSON_OBJECT)) {
                WCUP_BRANCH(1);
            } else {
                WCUP_BRANCH(0);
                json_t *dw = json_obj_get(data, "windows");
                if (dw && dw->type == JSON_ARRAY && json_len(dw) > 0) { WCUP_BRANCH(1); }
                else {
                    WCUP_BRANCH(0);
                    json_t *lw = json_obj_get(data, "_legacy_windows");
                    if (lw && lw->type == JSON_ARRAY && json_len(lw) > 0) { WCUP_BRANCH(1); }
                    else {
                        WCUP_BRANCH(0);
                        json_t *tw = json_obj_get(out, "windows");
                        if (tw && tw->type == JSON_ARRAY && json_len(tw) > 0) { WCUP_BRANCH(1); }
                        else {
                            WCUP_BRANCH(0);
                            json_t *tl = json_obj_get(out, "_legacy_windows");
                            if (tl && tl->type == JSON_ARRAY && json_len(tl) > 0) { WCUP_BRANCH(1); }
                            else { WCUP_BRANCH(0); }
                        }
                    }
                }
            }
        }
    }
    char vbuf[32];
    snprintf(vbuf, sizeof vbuf, "%d", res ? arr_len(res) : 0);
    WCUP_VALSTR(vbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("windows_from_tool_result");
    if (out) json_free(out);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s windows_from_tool_result <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
