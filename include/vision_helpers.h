/*
 * vision_helpers.h — Public surface for the vision/image helpers in
 * src/tools/vision.c (faithful C11 port of tools/vision_tools.py).
 *
 * Everything declared here is implemented in src/tools/vision.c. These are
 * the small, dependency-light helpers the battleground scanner classifies
 * as REAL_GAP — exposed so callers and tests can use them directly.
 */
#ifndef VISION_HELPERS_H
#define VISION_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Detect image MIME type from raw bytes (first 12 bytes). Returns one of:
 *   "image/png", "image/jpeg", "image/gif", "image/webp", "image/bmp"
 * Returns "application/octet-stream" when not recognized. */
const char *vision_detect_image_mime_from_bytes(const unsigned char *buf,
                                                size_t len);

/* Detect image format name from raw bytes (lowercase: "png", "jpeg", ...).
 * Returns NULL when unrecognized. */
const char *vision_detect_image_format_from_bytes(const unsigned char *buf,
                                                  size_t len);

/* Detect image format from file path (extension first, falls back to magic
 * bytes). Returns NULL when unrecognized. */
const char *vision_detect_image_format_from_path(const char *path);

/* True if the file has a known image extension OR begins with an image
 * magic-bytes signature. */
bool vision_validate_image_path(const char *path);

/* Extract image dimensions from raw header bytes. Returns malloc'd
 * "WxH" string or NULL when unrecognized. */
char *vision_extract_dimensions_from_bytes(const unsigned char *buf, size_t n);

/* Extract image dimensions from a local file. Returns malloc'd "WxH"
 * string or NULL when unrecognized. */
char *vision_extract_dimensions_from_path(const char *path);

/* Encode a local image file as a "data:<mime>;base64,<data>" URL.
 * Caller must free() the result. */
char *vision_image_to_base64_data_url(const char *path);

/* True if `error_text` indicates an image/payload size limit hit. */
bool vision_is_image_size_error(const char *error_text);

/* True if the URL/path refers to a "shape" that the runtime can ingest
 * (http(s)://... or data:... or a local path). */
bool vision_url_shape_ok(const char *url);

/* Try to fetch `url` (http(s)://...) into a local temp file with a
 * download timeout. Returns the local path (caller frees) on success,
 * NULL on failure. */
char *vision_download_image(const char *url, int timeout_sec);

/* Resize a local image to fit within (max_dim, max_dim). Uses ImageMagick
 * `convert` if available, otherwise no-op. Returns malloc'd local path
 * on success (caller frees), NULL otherwise. */
char *vision_resize_image_for_vision(const char *path, int max_dim);

/* True if a local path's dimensions exceed max_dim on either axis. */
bool vision_image_exceeds_dimension(const char *path, int max_dim);

/* Convert a local SVG file to a PNG with the same basename.
 * Uses `rsvg-convert` or `inkscape` if available; NULL otherwise. */
char *vision_rasterize_svg_to_png(const char *svg_path);

/* Normalize a local image (auto-resize, auto-convert to PNG) so the
 * vision pipeline can ingest it. Returns malloc'd path (caller frees). */
char *vision_normalize_to_supported_image(const char *path);

/* Resolve the download timeout (seconds) from HERMES_VISION_DOWNLOAD_TIMEOUT,
 * default 30. */
int vision_resolve_download_timeout(void);

/* Number of CPU workers for parallel image encoding.
 * Reads HERMES_VISION_CPU_WORKERS; defaults to min(4, nproc). */
int vision_resolve_cpu_workers(void);

/* Detect the number of host CPUs (online). */
int vision_detect_host_cpus(void);

/* True if an error string indicates a retryable download failure. */
bool vision_is_retryable_download_error(const char *error_text);

/* Check for image-processing tools (convert, identify, ffmpeg, rsvg-convert).
 * Returns a malloc'd JSON string with availability and overall readiness.
 * Caller must free. */
char *vision_check_requirements(void);

#ifdef __cplusplus
}
#endif

#endif /* VISION_HELPERS_H */
