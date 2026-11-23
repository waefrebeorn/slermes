#ifndef NOC_W7_MEDIA_2C731E_H
#define NOC_W7_MEDIA_2C731E_H
/*
 * Public C API for the port of session_rules/media.py (gate w7_media_2c731e).
 * Every exported symbol is prefixed with the gate short name.
 *
 * Set-valued Python results are returned as JSON arrays in sorted order so a
 * differential harness compares them deterministically.
 */
#include "hermes_json.h"

/* hosted_document_references(request_id, payload, media_root) */
json_t *w7_media_hosted_document_references(const char *request_id,
        const json_t *payload, const char *media_root);

/* admission_media_references(payload) */
json_t *w7_media_admission_media_references(const json_t *payload);

/* archive_refs_in_text(*texts) — texts is a JSON array of strings */
json_t *w7_media_archive_refs_in_text(const json_t *texts);

/* retirement_candidates_in_text(*texts) */
json_t *w7_media_retirement_candidates_in_text(const json_t *texts);

/* spillover_names_in_text(*texts) */
json_t *w7_media_spillover_names_in_text(const json_t *texts);

#endif /* NOC_W7_MEDIA_2C731E_H */
