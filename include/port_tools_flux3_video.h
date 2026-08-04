/*
 * port_tools_flux3_video.h — C11 port of pure helpers from
 * tools/flux3_video_tool.py.
 *
 * Ports the deterministic, I/O-free helpers from the FLUX 3 video
 * generation tool: local-path detection, JSON payload classification
 * (poll finished / retry-after / transport error), argument shaping,
 * URL->filename extraction, and the download timeout ceiling.
 *
 * The async HTTP/gateway machinery stays in Python; this header only
 * covers the pure logic that transforms strings/dicts into values.
 *
 * Memory: string-returning functions return malloc'd strings (caller
 * frees) or NULL. All other functions return a value by value.
 */

#ifndef PORT_TOOLS_FLUX3_VIDEO_H
#define PORT_TOOLS_FLUX3_VIDEO_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_t json_t;

/* PoP: _looks_like_local_path @ tools/flux3_video_tool.py:_looks_like_local_path */
/* True for things we should read off disk rather than forward as-is.
 * Mirrors the Python: base64 payloads (>=256 chars of base64 alphabet)
 * are NOT paths; file://, ~, /, ./, ../, Windows drive paths, and \\\\
 * UNC roots ARE. */
bool f3_looks_like_local_path(const char *value);

/* PoP: _display_path @ tools/flux3_video_tool.py:_display_path */
/* A value safe to embed in an error message shown to the model:
 * path unchanged when <= 200 chars, else "first200… (N characters)".
 * Returns a malloc'd string (caller frees). */
char *f3_display_path(const char *path);

/* PoP: _poll_is_finished @ tools/flux3_video_tool.py:_poll_is_finished */
/* True when there is nothing to wait for: unreadable, a refusal
 * ("error" key), or a terminal poll status in the details. */
bool f3_poll_is_finished(const char *raw_json);

/* PoP: _retry_after_seconds @ tools/flux3_video_tool.py:_retry_after_seconds */
/* How long the gateway asked us to wait, when a refusal is a throttle.
 * Reads details.retryAfterSeconds as a positive number; returns -1
 * when absent/invalid (Python returns None). */
double f3_retry_after_seconds(const char *raw_json);

/* PoP: _is_transport_error @ tools/flux3_video_tool.py:_is_transport_error */
/* True when the gateway did not answer (transport_error flag is true). */
bool f3_is_transport_error(const char *raw_json);

/* PoP: _without_media @ tools/flux3_video_tool.py:_without_media */
/* Drop media fields (input_image/input_images/input_video) entirely.
 * input: JSON object string; output: JSON object string (malloc'd). */
char *f3_without_media(const char *args_json);

/* PoP: _submit_args @ tools/flux3_video_tool.py:_submit_args */
/* The wire body: the model's arguments minus Nones, plus our mode.
 * input: JSON object string + mode; output: JSON object string. */
char *f3_submit_args(const char *args_json, const char *mode);

/* PoP: _error @ tools/flux3_video_tool.py:_error */
/* json.dumps({"error": message}, ensure_ascii=False). malloc'd. */
char *f3_error(const char *message);

/* PoP: _filename_from_url @ tools/flux3_video_tool.py:_filename_from_url */
/* Plain filename from a signed URL path: unquoted basename, keep only
 * [A-Za-z0-9._-], strip leading dots, cap at 120 chars; fallback
 * "flux3-video.mp4". malloc'd. */
char *f3_filename_from_url(const char *url);

/* PoP: _delivery_lead_in @ tools/flux3_video_tool.py:_delivery_lead_in */
/* Opens the result text ahead of the gateway's delivery guidance.
 * delivers_as_attachment is computed by the caller (session context).
 * malloc'd string. */
char *f3_delivery_lead_in(const char *target, bool delivers_as_attachment);

/* PoP: _download_read_timeout @ tools/flux3_video_tool.py:_download_read_timeout */
/* What is left of the call for a download, never more than the ceiling:
 * max(0, min(300.0, 240.0 - elapsed - 5.0)). `elapsed` is passed in
 * (monotonic seconds since the call started). */
double f3_download_read_timeout(double elapsed);

/* --- Constants shared with the Python original --- */
#define F3_MIN_BASE64_PAYLOAD_LENGTH 256
#define F3_DOWNLOAD_READ_TIMEOUT_SECONDS 300.0
#define F3_DOWNLOAD_GRACE_SECONDS 5.0
#define F3_CALL_BACKSTOP_SECONDS 240.0
#define F3_MAX_FILENAME_LENGTH 120
#define F3_FALLBACK_FILENAME "flux3-video.mp4"

#ifdef __cplusplus
}
#endif

#endif /* PORT_TOOLS_FLUX3_VIDEO_H */
