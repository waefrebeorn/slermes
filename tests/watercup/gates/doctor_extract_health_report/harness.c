/* gate: doctor_extract_health_report — drives the REAL
 * src/gateway/cua_doctor.c::cua_doctor_extract_health_report (linked via
 * extra_src). LIVE twin: tools/computer_use/doctor.py::
 * _extract_health_report_from_result.
 *
 * Input: one MCP tools/call result JSON per cup ("-" encodes a non-object).
 * Rung order (must match twin.py EXACTLY):
 *   B1  isError=true denial path / V1 denial text ("" otherwise)
 *   B2  health report extracted / V2 report JSON or "" when unavailable
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"

#define CUA_DOC_OK 0
#define CUA_DOC_UNAVAILABLE 1
#define CUA_DOC_PROTOCOL_ERR 2

int cua_doctor_extract_health_report(const json_t *result, json_t **out,
                                     char **out_msg);
char *json_serialize(const json_t *v);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s extract <result-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[2048];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        WCUP_BEGIN("extract_health_report");

        json_t *result = NULL;
        if (strcmp(buf, "-") != 0) {
            char *err = NULL;
            result = json_parse(buf, &err);
            free(err);
        }
        int is_obj = result && result->type == JSON_OBJECT;
        if (!is_obj) {
            /* protocol-error rung: non-object response */
            WCUP_BRANCH(0);
            char cup[32];
            WCUP_VALSTR("");
            WCUP_BRANCH(0);
            WCUP_VALSTR("");
            WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup);
            WCUP_END("extract_health_report");
            if (result) json_free(result);
            continue;
        }

        json_t *report = NULL;
        char *msg = NULL;
        int rc = cua_doctor_extract_health_report(result, &report, &msg);

        const json_t *iserr = json_obj_get(result, "isError");
        int is_error = iserr && iserr->type == JSON_BOOL && iserr->bool_val;
        WCUP_BRANCH(is_error);                     /* B1 */
        char denial[1024];
        snprintf(denial, sizeof denial, "%s", is_error && msg ? msg : "");
        WCUP_VALSTR(denial);                       /* V1 */
        WCUP_BRANCH(rc == CUA_DOC_OK);             /* B2 */
        char repbuf[1536];
        repbuf[0] = '\0';
        if (rc == CUA_DOC_OK && report) {
            char *ser = json_serialize(report);
            snprintf(repbuf, sizeof repbuf, "%s", ser ? ser : "");
            free(ser);
        }
        WCUP_VALSTR(repbuf);                       /* V2 */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("extract_health_report");
        if (msg) free(msg);
        if (report) json_free(report);
        if (result) json_free(result);
    }
    return 0;
}
