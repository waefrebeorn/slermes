/*
 * noc_w24_paths_616c0c.h - C surface for upstream
 * plugins/config_backends/remote/paths.py (gate w24_paths_616c0c).
 *
 * The dotted-path grammar of the config plane wire contract: escaping,
 * segment-wise prefix matching and the §3.1 limits.  Every refusal is a
 * value ({"ok": false, "error": "<str(PathError)>"}); nothing here touches
 * the plane, a socket or a config file.
 */
#ifndef W24_PATHS_616C0C_H
#define W24_PATHS_616C0C_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* §3.1 limits. */
#define W24_PATHS_616C0C_MAX_SEGMENTS 32
#define W24_PATHS_616C0C_MAX_SEGMENT_BYTES 256
#define W24_PATHS_616C0C_MAX_ENCODED_BYTES 2048

/* segments: [str] -> {"ok": true, "value": str} | {"ok": false, "error": str} */
json_t *w24_paths_616c0c_encode(const json_t *segments);

/* encoded: str -> {"ok": true, "value": [str]} | {"ok": false, "error": str}
 * (the error is str(PathError) from the ported decoder/validator). */
json_t *w24_paths_616c0c_decode(const json_t *encoded);

/* req: {"segments": [str], "encoded": str|null} -> {"ok": true} | error object.
 * An explicit "encoded" is used for both the displayed path and the byte limit,
 * exactly as the Python validator does. */
json_t *w24_paths_616c0c_validate(const json_t *req);

/* Segment-wise prefix test: len(lock) <= len(path) and path[:len(lock)] == lock. */
json_t *w24_paths_616c0c_covers(const json_t *lock, const json_t *path);

/* locks: [[str[] lock, any level], ...] -> {"lock": [...], "level": any} | null */
json_t *w24_paths_616c0c_covering_lock(const json_t *locks, const json_t *path);

/* A CLI dotted key: decode, else split on every ".". -> {"ok": true, "value": [...]} */
json_t *w24_paths_616c0c_from_dotted(const json_t *dotted);

/* MAX_SEGMENTS / MAX_SEGMENT_BYTES / MAX_ENCODED_BYTES as an object. */
json_t *w24_paths_616c0c_constants(void);

#ifdef __cplusplus
}
#endif
#endif
