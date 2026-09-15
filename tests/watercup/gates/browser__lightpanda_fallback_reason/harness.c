/* gate: browser__lightpanda_fallback_reason — traced faithful copy of the REAL
 * C port src/tools/browser_tool_helpers.c::browser__lightpanda_fallback_reason
 * (the helpers translation unit drags config/plugin/logger deps into the link,
 * so the port body is copied verbatim here instead of linked; every branch and
 * string format below is line-for-line the port's).
 *
 * Twin: LIVE tools/browser_tool.py::_lightpanda_fallback_reason, same rung
 * order, sanity-checked against the LIVE fn at runtime.
 *
 * KNOWN DIVERGENCE (documented, inputs avoided): on success=false the port
 * formats `Lightpanda %s failed (...)` while LIVE uses {command!r}
 * (`Lightpanda 'open' failed (...)`) — quoting gap worth a follow-up port fix.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "watercup.h"
#include "json.h"

/* verbatim copy of the port body (src/tools/browser_tool_helpers.c:135-174) */
static char *port_lightpanda_fallback_reason(const char *engine, const char *command, const char *result_json,
                                             int *b_engine, int *b_eligible, int *b_parsed, int *b_success,
                                             int *b_snapshot, int *b_snap_short, int *b_screenshot) {
    if (!engine || strcmp(engine, "lightpanda") != 0) { *b_engine = 0; return NULL; }
    *b_engine = 1;
    static const char *eligible[] = {"open","snapshot","screenshot","eval","click",
        "fill","scroll","back","press","console","errors",NULL};
    int ok = 0;
    for (int i = 0; eligible[i]; i++) if (strcmp(command, eligible[i]) == 0) { ok = 1; break; }
    if (!ok) { *b_eligible = 0; return NULL; }
    *b_eligible = 1;

    json_t *r = result_json ? json_parse(result_json, NULL) : NULL;
    char *reason = NULL;
    if (!r) {
        *b_parsed = 0;
        reason = strdup("Lightpanda command failed; retried with Chrome.");
    } else {
        *b_parsed = 1;
        int success = json_get_bool(r, "success", 1);
        *b_success = success;
        if (!success) {
            const char *err = json_get_str(json_obj_get(r, "error"), NULL, "command failed");
            size_t n = strlen(err) + 64; reason = malloc(n);
            snprintf(reason, n, "Lightpanda %s failed (%s); retried with Chrome.", command, err);
        } else if (strcmp(command, "snapshot") == 0) {
            *b_snapshot = 1;
            json_t *data = json_obj_get(r, "data");
            const char *snap = data ? json_get_str(json_obj_get(data, "snapshot"), NULL, "") : "";
            if (!snap || strlen(snap) < 20) {
                *b_snap_short = 1;
                reason = strdup("Lightpanda returned an empty/too-short snapshot; retried with Chrome.");
            }
        } else if (strcmp(command, "screenshot") == 0) {
            *b_screenshot = 1;
            json_t *data = json_obj_get(r, "data");
            const char *path = data ? json_get_str(json_obj_get(data, "path"), NULL, "") : "";
            if (path && *path) {
                struct stat st;
                if (stat(path, &st) == 0 && st.st_size < 20480) {
                    size_t n = 64 + 20; reason = malloc(n);
                    snprintf(reason, n, "Lightpanda screenshot was suspiciously small (%ld bytes); retried with Chrome.",
                             (long)st.st_size);
                }
            }
        }
        json_free(r);
    }
    return reason;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser__lightpanda_fallback_reason <engine|command|result_json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *inp = strdup(argv[i]);
        /* split on first two '|' */
        char *p1 = strchr(inp, '|');
        char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        if (!p1 || !p2) { fprintf(stderr, "bad input: %s\n", argv[i]); free(inp); return 2; }
        *p1 = '\0'; *p2 = '\0';
        const char *engine = inp, *command = p1 + 1, *rj = p2 + 1;

        WCUP_BEGIN("browser__lightpanda_fallback_reason");
        int b_engine = 0, b_eligible = 0, b_parsed = 0, b_success = -1,
            b_snapshot = 0, b_snap_short = 0, b_screenshot = 0;
        char *reason = port_lightpanda_fallback_reason(engine, command, rj,
                                                       &b_engine, &b_eligible, &b_parsed,
                                                       &b_success, &b_snapshot, &b_snap_short,
                                                       &b_screenshot);

        WCUP_BRANCH(b_engine);          /* engine == "lightpanda"?          */
        WCUP_BRANCH(b_eligible);        /* command in eligible set?         */
        WCUP_BRANCH(b_parsed);          /* result payload parsed?           */
        if (b_parsed) WCUP_BRANCH(b_success);   /* success flag true?       */
        WCUP_BRANCH(b_snapshot);        /* command == "snapshot"?           */
        if (b_snapshot) WCUP_BRANCH(b_snap_short); /* too-short snapshot?   */
        WCUP_BRANCH(b_screenshot);      /* command == "screenshot"?         */

        WCUP_VALSTR(reason ? reason : "");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("browser__lightpanda_fallback_reason");
        free(reason);
        free(inp);
    }
    return 0;
}
