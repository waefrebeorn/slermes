/* gate: verification_stop_status_detail — links the REAL C port
 * src/hermes_cli/verification_stop_helpers.c::verification_stop_status_detail
 * and traces its dispatch ladder around the call.
 * LIVE twin: agent/verification_stop.py::_status_detail.
 *
 * Input argv: one status JSON object per input.
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: non-empty parseable status object      B2: evidence is an object
 *   B3: command present                        B4: summary present
 *   VSTR(detail text)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "watercup.h"

char *verification_stop_status_detail(const char *status_json);

/* Out-of-path externals owned by coding_context/recipe modules (only used by
 * verification_stop_verification_snapshot / recipe probing — never on the
 * status_detail path). */
char *coding_context_find_git_root(const char *p) { (void)p; return strdup("."); }
char *coding_context_find_marker_root(const char *p) { (void)p; return strdup("."); }
char *verify_manifest_path(const char *p) { (void)p; return strdup("/dev/null"); }
void *recipe_detect(const char *p, const char *q) { (void)p; (void)q; return NULL; }
void recipe_free(void *r) { (void)r; }

static void run_one(const char *status_json) {
    WCUP_BEGIN("_status_detail");

    json_t *st = status_json ? json_parse(status_json, NULL) : NULL;
    bool obj = st && st->type == JSON_OBJECT;
    bool has_ev = false, has_cmd = false, has_sum = false;
    if (obj) {
        json_t *ev = json_object_get(st, "evidence");
        has_ev = ev && ev->type == JSON_OBJECT;
        if (has_ev) {
            json_t *cmd = json_object_get(ev, "canonical_command");
            if (!cmd || cmd->type != JSON_STRING || !json_string_value(cmd)[0])
                cmd = json_object_get(ev, "command");
            has_cmd = cmd && cmd->type == JSON_STRING;
            json_t *sum = json_object_get(ev, "output_summary");
            has_sum = sum && sum->type == JSON_STRING;
        }
    }

    WCUP_BRANCH(obj);                                           /* B1 */
    WCUP_BRANCH(has_ev);                                        /* B2 */
    if (has_ev) {
        WCUP_BRANCH(has_cmd);                                   /* B3 */
        WCUP_BRANCH(has_sum);                                   /* B4 */
    }

    char *r = verification_stop_status_detail(status_json);
    WCUP_VALSTR(r ? r : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(r);
    if (st) json_free(st);
    WCUP_END("_status_detail");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <status_json...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
