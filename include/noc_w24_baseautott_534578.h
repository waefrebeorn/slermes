/*
 * noc_w24_baseautott_534578.h - C surface for upstream
 * gateway/platforms/base_auto_tts.py (gate w24_baseautott_534578).
 *
 * The post-handler auto-TTS decision: whether synthesis is attempted at all,
 * which output path is requested, and which of the tool's reported files
 * survive the existence filter.  The runner's secret scope, the TTS tool and
 * the filesystem are seams: the scope is a callable-or-not flag, the tool's
 * JSON answer is an input, and existence is the case's "existing" list.
 */
#ifndef W24_BASEAUTOTT_534578_H
#define W24_BASEAUTOTT_534578_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The failure text raised (and logged) when markdown cleanup leaves nothing. */
#define W24_BASEAUTOTT_534578_EMPTY_TEXT "Empty text after markdown cleanup"

/* {"requirements": bool, "clean_text": <any>, "output_path": <str>,
 *  "tts": {"success": <any>, "file_paths": <any>, "file_path": <any>},
 *  "existing": [<str>, ...]}
 * -> {"paths": [...], "requested_path": <str|null>, "error": <str|null>}
 *
 * Mirrors synthesize_auto_tts + _synthesize: no requirements or an empty
 * cleanup answer means no attempt (and no requested path); otherwise the
 * requested path is output_path and the kept paths are the tool's reported
 * paths that are truthy and exist, with "file_paths" falsy falling back to
 * [file_path].  A non-string path is the TypeError seam: no paths are kept. */
json_t *w24_baseautott_534578_synthesize(const json_t *args);

/* {"resolver": bool, "source": <any>} -> "nullcontext" when the runner's
 * resolver is not callable or source is None (identity, not falsiness),
 * otherwise "resolve". */
json_t *w24_baseautott_534578_profile_scope(const json_t *args);

/* The module's literal table. */
json_t *w24_baseautott_534578_constants(void);

#ifdef __cplusplus
}
#endif
#endif
