/* gate: run_last_transcript_timestamp — traced copy of
 * src/gateway/run.c::gw_last_transcript_timestamp
 * (LIVE twin: gateway/run.py::_last_transcript_timestamp).
 *
 * Input: one argv token = a JSON array of message objects, each
 * {"role": "...", "timestamp": <number>} (timestamp/role optional).
 * Empty string encodes NULL history.
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: history empty/null                    -> -1.0
 *   per row from END:
 *     B2: row is an object with a role        -> 0 if yes... (see below)
 *     B3: role is session_meta or system      -> skip row
 *     B4: row has a timestamp                 -> V(ts) return
 *     B5: usable row without timestamp        -> return -1.0 (None LIVE)
 *   V: result timestamp ("none" sentinel for None/-1)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../../lib/libjson/json.h"
#include "watercup.h"

static void finish(wcup_t *_wc, double v) {
    char vs[64];
    snprintf(vs, sizeof vs, "%.17g", v);
    wcup_valstr(_wc, vs);
    char cup[32];
    wcup_finalize(_wc, cup, (int)sizeof cup);
    printf("%s\n", cup);
}

static void wc_last_ts(const char *history_json) {
    WCUP_BEGIN("last_transcript_timestamp");
    json_t *history = NULL;
    size_t n = 0;
    if (!history_json || !*history_json ||
        !(history = json_parse(history_json, NULL)) ||
        history->type != JSON_ARRAY) {
        WCUP_BRANCH(1);                                    /* B1 */
        finish(&_wc, -1.0);
        json_free(history);
        WCUP_END("last_transcript_timestamp");
        return;
    }
    WCUP_BRANCH(0);
    n = json_len(history);
    for (size_t ri = n; ri-- > 0; ) {                      /* reversed(history) */
        json_t *msg = json_get(history, ri);
        int has_role = (msg && msg->type == JSON_OBJECT &&
                        json_obj_get(msg, "role") &&
                        json_obj_get(msg, "role")->type == JSON_STRING);
        /* C scans the object for "role"; a non-dict row or missing role has
         * no usable role on either side. */
        WCUP_BRANCH(has_role ? 0 : 1);                     /* B2 */
        if (!has_role) continue;                           /* LIVE: not dict / no role -> skip */
        const char *role = json_obj_get(msg, "role")->str_val;
        int skip = (strcmp(role, "session_meta") == 0 || strcmp(role, "system") == 0);
        WCUP_BRANCH(skip);                                 /* B3 */
        if (skip) continue;
        json_t *ts = json_obj_get(msg, "timestamp");
        int have_ts = (ts && ts->type == JSON_NUMBER);
        WCUP_BRANCH(have_ts);                              /* B4 */
        if (have_ts) {
            finish(&_wc, ts->num_val);
            json_free(history);
            WCUP_END("last_transcript_timestamp");
            return;
        }
        WCUP_BRANCH(1);                                    /* B5: legacy row w/o ts */
        finish(&_wc, -1.0);
        json_free(history);
        WCUP_END("last_transcript_timestamp");
        return;
    }
    WCUP_BRANCH(1);                                        /* loop exhausted */
    finish(&_wc, -1.0);
    json_free(history);
    WCUP_END("last_transcript_timestamp");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s last_transcript_timestamp <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_last_ts(argv[i]);
    return 0;
}
