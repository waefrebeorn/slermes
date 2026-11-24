/*
 * uploads.c -- real C11 port of the upload decision of upstream Hermes
 * hermes_cli/web_routers/uploads.py.
 *
 * PoP: w21_uploads_23e65f_upload @ hermes_cli/web_routers/uploads.py:upload_chat_file
 *
 * FastAPI's spooled UploadFile, the profile-home resolver and the staged-file
 * writer are transport seams; the cap check, the 413 wording, the receipt and
 * the optional descriptor are the port's.
 */
#include "hermes_json.h"
#include "noc_w21_uploads_23e65f.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* hermes_constants.WEBAPP_ATTACHMENT_MAX_BYTES */
#define UP_MAX_UPLOAD_BYTES 16777216

static int up_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

json_t *w21_uploads_23e65f_upload(const json_t *case_) {
    json_t *size_v, *filename_v, *descriptor;
    double total;
    json_t *out;

    if (!case_ || case_->type != JSON_OBJECT) case_ = json_object();
    size_v = json_obj_get(case_, "size");
    total = (size_v && size_v->type == JSON_NUMBER) ? size_v->num_val : 0.0;

    if (total > (double)UP_MAX_UPLOAD_BYTES) {
        json_t *err = json_object();
        json_t *error = json_object();
        char detail[128];
        /* cap_mib = _MAX_UPLOAD_BYTES // (1024 * 1024) */
        snprintf(detail, sizeof(detail), "File is too large; cap is %d MiB",
                 UP_MAX_UPLOAD_BYTES / (1024 * 1024));
        json_set(error, "status_code", json_number(413));
        json_set(error, "detail", json_string(detail));
        json_set(err, "error", error);
        return err;
    }

    out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "path", json_string(json_get_str(case_, "path", "")));
    json_set(out, "size", json_number(total));
    filename_v = json_obj_get(case_, "filename");
    if (filename_v && filename_v->type == JSON_STRING) {
        /* publish_staged_upload(..., file.filename or "attachment") */
        json_set(out, "published_name",
                 json_string(up_truthy(filename_v) ? filename_v->str_val : "attachment"));
    } else {
        json_set(out, "published_name", json_string("attachment"));
    }
    descriptor = json_obj_get(case_, "descriptor");
    if (up_truthy(descriptor)) {
        json_set(out, "staged_upload", json_copy(descriptor));
    }
    return out;
}
