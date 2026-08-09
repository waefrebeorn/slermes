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



/* PoP: _submit_args @ tools/flux3_video_tool.py:_submit_args */
json_t *flux3_submit_args(const char *mode, json_t *args);

/* PoP: _shared_submit_properties @ tools/flux3_video_tool.py:_shared_submit_properties */
json_t *flux3_shared_submit_properties(void);

/* PoP: _endpoints @ tools/flux3_video_tool.py:_endpoints */
json_t *flux3_endpoints(void);

/* PoP: _warm_nous_token @ tools/flux3_video_tool.py:_warm_nous_token */
void flux3_warm_nous_token(void);

/* PoP: _has_nous_credential @ tools/flux3_video_tool.py:_has_nous_credential */
bool flux3_has_nous_credential(void);

/* PoP: check_bfl_requirements @ tools/flux3_video_tool.py:check_bfl_requirements */
bool flux3_check_bfl_requirements(void);

/* PoP: _delivers_as_an_attachment @ tools/flux3_video_tool.py:_delivers_as_an_attachment */
bool flux3_delivers_as_an_attachment(void);

/* PoP: _default_directory @ tools/flux3_video_tool.py:_default_directory */
char *flux3_default_directory(void);

/* PoP: _resolve_destination @ tools/flux3_video_tool.py:_resolve_destination */
char *flux3_resolve_destination(const char *save_to, const char *filename);

/* ── Gateway I/O ── */
#define F3_SIGN_IN_MESSAGE \
    "BFL video generation needs a Nous Portal sign-in. " \
    "Ask the user to run `hermes model` and sign in to Nous, then retry."

#define F3_TRANSPORT_READ_TIMEOUT_SECONDS  180.0
#define F3_TRANSPORT_CONNECT_TIMEOUT_SECONDS 10.0
#define F3_POLL_BUDGET_SECONDS  180.0
#define F3_POLL_GAP_SECONDS    5.0
#define F3_POLL_WAIT_SLICE_SECONDS 1.0
#define F3_POLL_READ_TIMEOUT_SECONDS 60.0
#define F3_MAX_CONSECUTIVE_TRANSPORT_ERRORS 3
#define F3_CALL_BACKSTOP_SECONDS 240.0
#define F3_DOWNLOAD_READ_TIMEOUT_SECONDS 300.0
#define F3_DOWNLOAD_CONNECT_TIMEOUT_SECONDS 15.0
#define F3_DOWNLOAD_GRACE_SECONDS 5.0
#define F3_MIN_PLAUSIBLE_VIDEO_BYTES_VAL (64 * 1024)
#define F3_MAX_FILENAME_ATTEMPTS_VAL 50

/* PoP: _call_gateway @ tools/flux3_video_tool.py:_call_gateway */
char *f3_call_gateway(const char *method, const char *url,
                      const char *json_body, double read_timeout);

/* PoP: _wait_between_looks @ tools/flux3_video_tool.py:_wait_between_looks */
bool f3_wait_between_looks(double seconds);

/* PoP: _prepare_media @ tools/flux3_video_tool.py:_prepare_media */
char *f3_prepare_media(const char *args_json, const char *task_id);

/* PoP: _deliver_media @ tools/flux3_video_tool.py:_deliver_media */
char *f3_deliver_media(const char *value, const char *permitted_json, const char *task_id);

/* PoP: _save_if_ready @ tools/flux3_video_tool.py:_save_if_ready */
char *f3_save_if_ready(const char *raw, const char *save_to, double started);

/* PoP: _download_video @ tools/flux3_video_tool.py:_download_video */
char *f3_download_video(const char *url, const char *save_to, double started);

/* PoP: _submit @ tools/flux3_video_tool.py:_submit */
char *f3_submit(const char *mode, const char *args_json);

/* PoP: _handle_text_to_video @ tools/flux3_video_tool.py:_handle_text_to_video */
char *f3_handle_text_to_video(const char *args_json, const char *task_id);

/* PoP: _handle_image_to_video @ tools/flux3_video_tool.py:_handle_image_to_video */
char *f3_handle_image_to_video(const char *args_json, const char *task_id);

/* PoP: _handle_keyframes_to_video @ tools/flux3_video_tool.py:_handle_keyframes_to_video */
char *f3_handle_keyframes_to_video(const char *args_json, const char *task_id);

/* PoP: _handle_video_continuation @ tools/flux3_video_tool.py:_handle_video_continuation */
char *f3_handle_video_continuation(const char *args_json, const char *task_id);

/* PoP: _poll_until_done @ tools/flux3_video_tool.py:_poll_until_done */
char *f3_poll_until_done(const char *url, const char *save_to, double started);

/* PoP: _handle_get_result @ tools/flux3_video_tool.py:_handle_get_result */
char *f3_handle_get_result(const char *args_json);

/* PoP: _handle_prompting_guide @ tools/flux3_video_tool.py:_handle_prompting_guide */
const char *f3_handle_prompting_guide(void);

#endif /* FLUX3_VIDEO_TOOL_H */
