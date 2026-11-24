/*
 * noc_w27_adapter_74f618_p4.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p4).
 *
 * Gate 4: inbound file classification (audio cache extension, voice-clip
 * heuristic, image/audio/video/document kind) and the user-facing attachment
 * diagnostics built from a Slack API error response.
 */
#ifndef NOC_W27_ADAPTER_74F618_P4_H
#define NOC_W27_ADAPTER_74F618_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"file_obj": obj|null, "mimetype": str?} -> str */
json_t *w27_p4_resolve_audio_ext(const json_t *req);

/* req: {"file_obj": obj|null} -> bool */
json_t *w27_p4_is_voice_clip(const json_t *req);

/* req: {"file_obj": obj|null} -> str */
json_t *w27_p4_attachment_label(const json_t *req);

/* req: {"file_obj": obj|null, "mimetype": str?} -> str */
json_t *w27_p4_file_kind(const json_t *req);

/* req: {"response": obj|null, "file_obj": obj|null} -> str|null */
json_t *w27_p4_describe_api_error(const json_t *req);

/* req: {"response": obj|null, "message": str?, "file_obj": obj|null} -> str|null */
json_t *w27_p4_describe_download_failure(const json_t *req);

/* req: {"status_code": int|null, "message": str?, "response_error": str?} -> bool */
json_t *w27_p4_retryable_upload_error(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P4_H */
