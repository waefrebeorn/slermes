/* gate: chat_completion_helpers_status_code_from_payload — links the REAL C
 * port src/agent/chat_completion_helpers_ports.c::cch_status_code_from_payload.
 * LIVE twin: agent/chat_completion_helpers.py::_status_code_from_payload.
 *
 * Input argv: one payload JSON per input.
 * Branch ladder (must match twin.py EXACTLY): B1 = payload is an object,
 * then one rung per candidate slot in LIVE's candidate order
 * (status_code, status, http_status | error.status_code, error.status,
 * error.http_status, error.code | code) = B2..B9, each true when that slot
 * yields a valid status. VSTR(result code, "-1" == None).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "watercup.h"

int cch_status_code_from_payload(const json_t *payload);

/* Out-of-path external (cleanup path only, never on status extraction). */
void cleanup_task_resources(void *p) { (void)p; }

/* mirror of the port's candidate walk, for tracing only */
static bool slot_valid(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return false;
    json_t *v = json_obj_get(obj, key);
    if (!v) return false;
    char buf[64];
    if (v->type == JSON_STRING && v->str_val) snprintf(buf, sizeof buf, "%s", v->str_val);
    else if (v->type == JSON_NUMBER) snprintf(buf, sizeof buf, "%g", v->num_val);
    else return false;
    /* same acceptance rule as cch_status_code_from_value: pure int 100..599,
     * or a string containing \b[1-5]\d\d\b (optionally HTTP_STATUS/-prefixed) */
    char *end; long iv = strtol(buf, &end, 10);
    if (*end == '\0') return iv >= 100 && iv < 600;
    const char *p = buf;
    for (; *p; p++) {
        if ((*p >= '1' && *p <= '5') && p + 3 <= buf + strlen(buf)
            && ((p == buf) || !(*(p-1) >= '0' && *(p-1) <= '9'))
            && ((p+3 == buf+strlen(buf)) || !(*(p+3) >= '0' && *(p+3) <= '9'))
            && p[1] >= '0' && p[1] <= '9' && p[2] >= '0' && p[2] <= '9')
            return true;
    }
    return false;
}

static void run_one(const char *payload_json) {
    json_t *pl = payload_json ? json_parse(payload_json, NULL) : NULL;
    bool obj = pl && pl->type == JSON_OBJECT;
    json_t *err = obj ? json_obj_get(pl, "error") : NULL;
    bool errobj = err && err->type == JSON_OBJECT;

    WCUP_BEGIN("_status_code_from_payload");
    WCUP_BRANCH(obj);                                           /* B1 */
    static const char *top[] = {"status_code", "status", "http_status"};
    static const char *esub[] = {"status_code", "status", "http_status", "code"};
    for (int i = 0; i < 3; i++)
        WCUP_BRANCH(slot_valid(pl, top[i]));                    /* B2-B4 */
    for (int i = 0; i < 4; i++)
        WCUP_BRANCH(errobj && slot_valid(err, esub[i]));        /* B5-B8 */
    WCUP_BRANCH(slot_valid(pl, "code"));                        /* B9 */

    int r = cch_status_code_from_payload(pl);
    char vs[16]; snprintf(vs, sizeof vs, "%d", r);
    WCUP_VALSTR(vs);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    if (pl) json_free(pl);
    WCUP_END("_status_code_from_payload");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <payload_json...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
