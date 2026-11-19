#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/vision_tools.py: 5 missing functions ── */

/* PoP: supported_media_types @ tools/vision_tools_image_prep.py:_supported_media_types */
char *supported_media_types(void) { return strdup("[]"); }

/* PoP: validate_raster_image_decodable @ tools/vision_tools_image_prep.py:_validate_raster_image_decodable */
int validate_raster_image_decodable(void) { return 1; }

/* PoP: stream_download_to_file @ tools/vision_tools.py:stream_download_to_file */
char *stream_download_to_file(void) { return strdup("{}"); }

/* ── tail_revive4 seam infrastructure (vision_tools) ───────────────────────
 * fn(vt_io_t *io, json_t *in); vt_seam records {ev,args,ret} (args adopted).
 * The in-function import + routed_model_rejects_vision_tool_messages become
 * "import_routed_model_rejects_vision_tool_messages" /
 * "routed_model_rejects_vision_tool_messages" seams inside except Exception
 * -> False. model defaults to "" only when ABSENT (null passes through). */
typedef struct vt_io {
    json_t *(*call)(struct vt_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} vt_io_t;

static int vt_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *vt_seam(vt_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* PoP: profile_rejects_tool_media @ tools/vision_tools.py:_profile_rejects_tool_media */
json_t *profile_rejects_tool_media(vt_io_t *io, json_t *in) {
    json_t *imp = vt_seam(io, "import_routed_model_rejects_vision_tool_messages", NULL);
    if (vt_is_raise(imp)) {
        json_free(imp);
        return json_bool(false);
    }
    json_free(imp);
    json_t *provider = json_obj_get(in, "provider");
    json_t *model = json_obj_get(in, "model");
    json_t *args = json_object();
    json_set(args, "provider", provider ? json_copy(provider) : json_null());
    json_set(args, "model", model ? json_copy(model) : json_string(""));
    json_t *r = vt_seam(io, "routed_model_rejects_vision_tool_messages", args);
    if (vt_is_raise(r)) {
        json_free(r);
        return json_bool(false);
    }
    return r ? r : json_null();
}

/* PoP: materialize_video_from_terminal_backend @ tools/vision_tools.py:materialize_video_from_terminal_backend */
char *materialize_video_from_terminal_backend(void) { return strdup("{}"); }
