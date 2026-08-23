/* gate: security_audit_osv_severity_from_record — drives the REAL
 * src/hermes_cli/security_audit.c::security_osv_severity_from_record
 * (PoP: hermes_cli/security_audit.py::_osv_severity_from_record).
 *
 * Input: the OSV record as a JSON string (argv-safe, no spaces needed).
 *
 * Coarse rung ladder around the real C call (must match twin.py EXACTLY):
 *   B1 record is an object
 *   B2 database_specific.severity is a nonempty string / B3 in severity order
 *   B4 affected is an array; per entry: B(entry obj) B(eco obj) B(sev str)
 *      B(in order); loop tick each iteration
 *   V(result tier)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include "json.h"
#include "security_audit.h"

static int in_severity_order_local(const char *s) {
    return (strcmp(s, "UNKNOWN") == 0 || strcmp(s, "LOW") == 0 ||
            strcmp(s, "MODERATE") == 0 || strcmp(s, "MEDIUM") == 0 ||
            strcmp(s, "HIGH") == 0 || strcmp(s, "CRITICAL") == 0);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_osv_severity <record_json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("osv_severity_from_record");
        const char *rec_s = argv[i];
        json_t *record = json_parse(rec_s, NULL);

        WCUP_BRANCH(record && record->type == JSON_OBJECT);        /* B1 */
        if (!record || record->type != JSON_OBJECT) goto emit;

        {
            const json_t *db = json_obj_get(record, "database_specific");
            WCUP_BRANCH(db && db->type == JSON_OBJECT);            /* B2 */
            if (db && db->type == JSON_OBJECT) {
                const json_t *raw = json_obj_get((json_t *)db, "severity");
                WCUP_BRANCH(raw && raw->type == JSON_STRING &&
                            raw->str_val && raw->str_val[0]);      /* B3 */
                if (raw && raw->type == JSON_STRING && raw->str_val && raw->str_val[0]) {
                    /* uppercase+strip like both impls */
                    char up[64]; size_t k = 0;
                    for (; raw->str_val[k] && k < sizeof(up)-1; k++)
                        up[k] = (char)((raw->str_val[k] >= 'a' && raw->str_val[k] <= 'z')
                                       ? raw->str_val[k] - 32 : raw->str_val[k]);
                    up[k] = '\0';
                    /* trim spaces/tabs */
                    char *s = up; while (*s==' '||*s=='\t') s++;
                    size_t l = strlen(s);
                    while (l && (s[l-1]==' '||s[l-1]=='\t')) s[--l]='\0';
                    WCUP_VALSTR(s);
                    WCUP_BRANCH(in_severity_order_local(s));       /* B4 */
                    if (in_severity_order_local(s)) goto emit;
                }
            }
        }

        {
            const json_t *aff = json_obj_get(record, "affected");
            WCUP_BRANCH(aff && aff->type == JSON_ARRAY);           /* B5 */
            if (aff && aff->type == JSON_ARRAY) {
                size_t n = json_len((json_t *)aff);
                for (size_t e = 0; e < n; e++) {
                    WCUP_LOOP_TICK();
                    json_t *entry = json_get((json_t *)aff, e);
                    WCUP_BRANCH(entry && entry->type == JSON_OBJECT);
                    if (!entry || entry->type != JSON_OBJECT) continue;
                    const json_t *eco = json_obj_get(entry, "ecosystem_specific");
                    WCUP_BRANCH(eco && eco->type == JSON_OBJECT);
                    if (!eco || eco->type != JSON_OBJECT) continue;
                    const json_t *sev = json_obj_get((json_t *)eco, "severity");
                    WCUP_BRANCH(sev && sev->type == JSON_STRING &&
                                sev->str_val && sev->str_val[0]);
                    if (!(sev && sev->type == JSON_STRING && sev->str_val && sev->str_val[0])) continue;
                    char up[64]; size_t k = 0;
                    for (; sev->str_val[k] && k < sizeof(up)-1; k++)
                        up[k] = (char)((sev->str_val[k] >= 'a' && sev->str_val[k] <= 'z')
                                       ? sev->str_val[k] - 32 : sev->str_val[k]);
                    up[k] = '\0';
                    char *s = up; while (*s==' '||*s=='\t') s++;
                    size_t l = strlen(s);
                    while (l && (s[l-1]==' '||s[l-1]=='\t')) s[--l]='\0';
                    WCUP_VALSTR(s);
                    WCUP_BRANCH(in_severity_order_local(s));
                    if (in_severity_order_local(s)) goto emit;
                }
            }
        }
emit:;
        char out[64];
        security_osv_severity_from_record(record, out, sizeof out);
        WCUP_VALSTR(out);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("osv_severity_from_record");
        if (record) json_free(record);
    }
    return 0;
}
