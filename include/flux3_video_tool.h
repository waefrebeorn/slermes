/*
 * flux3_video_tool.h — Port of tools/flux3_video_tool.py (pure helpers).
 */
#ifndef FLUX3_VIDEO_TOOL_H
#define FLUX3_VIDEO_TOOL_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#define FLUX3_MAX_FILENAME_ATTEMPTS 9
#define FLUX3_MIN_PLAUSIBLE_VIDEO_BYTES 1024
#define FLUX3_MAX_FILENAME_LEN 120

/* PoP: _looks_like_local_path @ tools/flux3_video_tool.py:_looks_like_local_path */
bool flux3_looks_like_local_path(const char *value);

/* PoP: _display_path @ tools/flux3_video_tool.py:_display_path */
char *flux3_display_path(const char *path);

/* PoP: _filename_from_url @ tools/flux3_video_tool.py:_filename_from_url */
char *flux3_filename_from_url(const char *url);

/* PoP: _free_path @ tools/flux3_video_tool.py:_free_path */
char *flux3_free_path(const char *directory, const char *name);

/* PoP: _is_transport_error @ tools/flux3_video_tool.py:_is_transport_error */
bool flux3_is_transport_error(const char *raw);

/* PoP: _poll_is_finished @ tools/flux3_video_tool.py:_poll_is_finished */
bool flux3_poll_is_finished(const char *raw);

/* PoP: _retry_after_seconds @ tools/flux3_video_tool.py:_retry_after_seconds */
double flux3_retry_after_seconds(const char *raw);

/* PoP: _still_generating @ tools/flux3_video_tool.py:_still_generating */
char *flux3_still_generating(const char *job_id);

/* PoP: _without_media @ tools/flux3_video_tool.py:_without_media */
json_t *flux3_without_media(json_t *args);

/* PoP: _submit_args @ tools/flux3_video_tool.py:_submit_args */
json_t *flux3_submit_args(const char *mode, json_t *args);

/* PoP: _shared_submit_properties @ tools/flux3_video_tool.py:_shared_submit_properties */
json_t *flux3_shared_submit_properties(void);

/* PoP: _resolve_destination @ tools/flux3_video_tool.py:_resolve_destination */
char *flux3_resolve_destination(const char *save_to, const char *filename);

#endif /* FLUX3_VIDEO_TOOL_H */
