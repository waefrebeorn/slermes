/*
 * noc_w21_thumbnail_f864e8.h -- public C API for the port of
 *   tools/bot_desktop/thumbnail.py (wave 21 batch 5).
 *
 * One JPEG grab of the profile's Xvnc display, served in the Screen hero as a
 * `data:image/jpeg;base64,...` URL. Everything the module decides is here: the
 * DISPLAY gate, the sandbox branch, the launcher gate, the thumbnail size the
 * image is scaled to, and the data URL itself. The three seams are the runtime
 * probes (published_env / sandbox_screen_running / _owned_sandbox_env /
 * _launcher_pid), the grab (PIL ImageGrab / sandbox_host.grab_jpeg) and the
 * JPEG encoder -- the caller supplies the raw bytes of both.
 */
#ifndef NOC_W21_THUMBNAIL_F864E8_H
#define NOC_W21_THUMBNAIL_F864E8_H

#include <stddef.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* thumbnail.THUMB_MAX */
#define W21_THUMBNAIL_F864E8_THUMB_MAX_W 960
#define W21_THUMBNAIL_F864E8_THUMB_MAX_H 600

/* PoP: w21_thumbnail_f864e8_thumbnail_size @ tools/bot_desktop/thumbnail.py:thumbnail_data_url
 * `PIL.Image.thumbnail(size)` on a width x height image (preserve aspect, never
 * upscale, per-side floor/ceil pick closest to the aspect). Returns
 * [width, height]. */
json_t *w21_thumbnail_f864e8_thumbnail_size(long long width, long long height,
                                            long long max_width, long long max_height);

/* PoP: w21_thumbnail_f864e8_data_url @ tools/bot_desktop/thumbnail.py:thumbnail_data_url
 * `"data:image/jpeg;base64," + base64.b64encode(jpeg).decode("ascii")`, or null
 * for empty bytes. Caller frees. */
char *w21_thumbnail_f864e8_data_url(const unsigned char *bytes, size_t len);

/* PoP: w21_thumbnail_f864e8_thumbnail_data_url @ tools/bot_desktop/thumbnail.py:thumbnail_data_url
 * The whole flow. `kase` carries the seams:
 *   {"env": {...}, "sandbox_running": bool, "owned_sandbox": bool,
 *    "sandbox_jpeg_hex": str|null, "launcher_pid": num|null,
 *    "image": {"width": n, "height": n}|null, "jpeg_hex": str|null,
 *    "max_size": [w, h], "quality": n}
 * Returns {"status":"none"} or
 *   {"status":"data", "value": "...", "branch": "sandbox"|"grab", "size": [w,h]}. */
json_t *w21_thumbnail_f864e8_thumbnail_data_url(const json_t *kase);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_THUMBNAIL_F864E8_H */
