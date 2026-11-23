#ifndef NOC_W12_STAGEDUPLO_DF301A_H
#define NOC_W12_STAGEDUPLO_DF301A_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_stageduplo_df301a_safe_filename @ hermes_cli/staged_uploads.py:_safe_filename
 * The shared writer/reader filename rule (the _SAFE_FILENAME regex + stem /
 * suffix split + length clamps).  NULL or "" input means "attachment". */
void w12_stageduplo_df301a_safe_filename(const char *value, char *out, size_t cap);

/* PoP: w12_stageduplo_df301a_parse_staged_upload @ hermes_cli/staged_uploads.py:parse_staged_upload
 * install_id is an injected seam for get_install_id().  Returns
 * {"home":...,"path":...} on success or {"error":"<str(ValueError)>"}. */
struct json_t *w12_stageduplo_df301a_parse_staged_upload(const char *install_id,
                                                        const struct json_t *descriptor);

/* PoP: w12_stageduplo_df301a_staged_upload_descriptor @ hermes_cli/staged_uploads.py:staged_upload_descriptor
 * install_id is injected; returns the receipt object or NULL (no identity). */
struct json_t *w12_stageduplo_df301a_staged_upload_descriptor(const char *install_id,
                                                             const char *path,
                                                             const char *profile_home);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_STAGEDUPLO_DF301A_H */
