#ifndef NOC_W9_UPDATESTAGE_53DF9D_H
#define NOC_W9_UPDATESTAGE_53DF9D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_updatestage_53df9d_ui_present_in_log @ hermes_cli/update_stage.py:_ui_present_in_log
 * Last-40-lines tail join, then the two substring checks.  Returns a JSON bool. */
struct json_t *w9_updatestage_53df9d_ui_present_in_log(const char *log_text);

/* PoP: w9_updatestage_53df9d_status_file @ hermes_cli/update_stage.py:status_file
 * Injectable-root seam: `exported` is the value of HERMES_UPDATE_STATUS_FILE
 * (already read from the environment by the caller), `marker_result` is the
 * path that _status_from_marker() would return ("" == None).  Matches the
 * Python .strip()/truthiness logic.  Returns a JSON string or null. */
struct json_t *w9_updatestage_53df9d_status_file(const char *exported,
                                                 const char *marker_result);

/* PoP: w9_updatestage_53df9d_status_from_marker @ hermes_cli/update_stage.py:_status_from_marker
 * Seam for the marker-derived status path: `marker_text` is the utf-8-sig
 * decoded marker content, `candidate_exists` is Path.is_file() on the derived
 * ${TMPDIR}/hermes-update-status.<pid>.  Returns the JSON basename
 * "hermes-update-status.<pid>" (the environment-dependent temp directory is
 * the injected seam), or JSON null. */
struct json_t *w9_updatestage_53df9d_status_from_marker(const char *marker_text,
                                                       int candidate_exists);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_UPDATESTAGE_53DF9D_H */
