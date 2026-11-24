#ifndef NOC_W16_UPDATECOMP_C7D826_H
#define NOC_W16_UPDATECOMP_C7D826_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_updatecomp_c7d826_exit_status @ hermes_cli/update_completion.py:_exit_status */
double w16_updatecomp_c7d826_exit_status(double code);

/* PoP: w16_updatecomp_c7d826_owed_user_action @ hermes_cli/update_completion.py:_owed_user_action */
struct json_t *w16_updatecomp_c7d826_owed_user_action(const struct json_t *request);

/* PoP: w16_updatecomp_c7d826_write_bootstrap_result @ hermes_cli/update_completion.py:_write_bootstrap_result
 * Returns the dict the Python helper writes to result_path (file I/O is the seam). */
struct json_t *w16_updatecomp_c7d826_write_bootstrap_result(const struct json_t *request,
                                                            double code,
                                                            const struct json_t *receipt);

/* PoP: w16_updatecomp_c7d826_report_desktop_build_owed @ hermes_cli/update_completion.py:_report_desktop_build_owed
 * Returns the lines the Python helper prints. */
struct json_t *w16_updatecomp_c7d826_report_desktop_build_owed(const struct json_t *request,
                                                               const struct json_t *record,
                                                               const char *reason);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_UPDATECOMP_C7D826_H */
