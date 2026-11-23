#ifndef NOC_W10_DOCTORREPO_32657C_H
#define NOC_W10_DOCTORREPO_32657C_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_doctorrepo_32657c_check_bool @ hermes_cli/doctor_report.py:check_bool
 * Returns {"ret":bool(cond),"kind":"ok"/"warn"/"fail","text":...,"detail":...}
 * mirroring check_bool's return value and the primitive it dispatches to. */
struct json_t *w10_doctorrepo_32657c_check_bool(const struct json_t *cond, int fail,
                                                const struct json_t *ok, const struct json_t *bad);
/* PoP: w10_doctorrepo_32657c_finding_merge @ hermes_cli/doctor_report.py:Finding.merge
 * Returns the merged Finding {issues, manual_issues, fixed} (f1 extended by f2). */
struct json_t *w10_doctorrepo_32657c_finding_merge(const struct json_t *first, const struct json_t *second);
/* PoP: w10_doctorrepo_32657c_ensure_dir @ hermes_cli/doctor_report.py:ensure_dir
 * Returns {"kind","msg","detail","fixed"} for the check primitive chosen. */
struct json_t *w10_doctorrepo_32657c_ensure_dir(int exists, int should_fix,
        const char *exists_msg, const char *created_msg, const char *missing_msg);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_DOCTORREPO_32657C_H */
