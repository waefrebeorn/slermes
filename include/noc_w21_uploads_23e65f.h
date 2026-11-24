#ifndef NOC_W21_UPLOADS_23E65F_H
#define NOC_W21_UPLOADS_23E65F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/web_routers/uploads.py (gate w21_uploads_23e65f).
 *
 * POST /api/chat/file-upload: the byte cap, the 413 wording, the staged-file
 * receipt and the optional descriptor. The spool itself (FastAPI's UploadFile)
 * and the profile-home writer stay upstream; the port decides the request. */

/* PoP: w21_uploads_23e65f_upload @ hermes_cli/web_routers/uploads.py:upload_chat_file
 * `upload_chat_file(file=..., profile=...)` as a value:
 *   case = {"size": <int bytes>,          # file.file.seek(0, SEEK_END)
 *           "filename": "..."|null,       # file.filename
 *           "profile": "..."|null,        # the ?profile= query value
 *           "path": "...",                # publish_staged_upload() answer
 *           "descriptor": {...}|null}     # staged_upload_descriptor() answer
 * Answer: {"ok":true,"path":...,"size":...,"published_name":...,
 *          ["staged_upload": {...}]}     # the descriptor only when truthy
 *       | {"error":{"status_code":413,"detail":"File is too large; cap is N MiB"}}. */
json_t *w21_uploads_23e65f_upload(const json_t *case_);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_UPLOADS_23E65F_H */
