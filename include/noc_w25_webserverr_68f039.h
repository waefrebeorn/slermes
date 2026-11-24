/*
 * noc_w25_webserverr_68f039.h - C surface for upstream
 * hermes_cli/web_server_remote_media.py (gate w25_webserverr_68f039).
 *
 * Inline audio/video served out of a profile's SSH workspace: the one byte
 * range a media element asks for, read as a slice through the workspace
 * adapter.  The adapter call itself (``SshWorkspaceFs.stream_file``) and the
 * ASGI request are seams; what is ported here is the range arithmetic and the
 * response envelope those values decide.
 */
#ifndef NOC_W25_WEBSERVERR_68F039_H
#define NOC_W25_WEBSERVERR_68F039_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _MAX_POSITION_DIGITS: more digits than any served file can need. */
#define W25_WEBSERVERR_68F039_MAX_POSITION_DIGITS 19
/* _PAST_ANY_FILE = 10 ** _MAX_POSITION_DIGITS */
#define W25_WEBSERVERR_68F039_PAST_ANY_FILE 10000000000000000000.0

/* digits: a run of ASCII digits (extra leading zeros allowed) ->
 * the byte position, or _PAST_ANY_FILE when the number has more than
 * _MAX_POSITION_DIGITS significant digits. */
json_t *w25_webserverr_68f039_position(const char *digits);

/* byte_range(header, size) -> null (serve the whole file), or
 * {"first": n, "last": n} for the inclusive span, or
 * {"unsatisfiable": size} for the 416 HTTPException the upstream raises. */
json_t *w25_webserverr_68f039_byte_range(const json_t *header, long long size);

/* response(request, target, size, media_type) ->
 * {"status": 200|206|416,
 *  "headers": {"accept-ranges", "content-disposition", "content-length",
 *              "x-content-type-options", "content-range"(206 only),
 *              "content-type"},
 *  "read": null | "full" | {"offset": n, "length": n}}
 * where "read" is the workspace adapter call the body would make. */
json_t *w25_webserverr_68f039_response(const json_t *request, const char *target,
                                       long long size, const char *media_type);

/* The module constants, as an object. */
json_t *w25_webserverr_68f039_constants(void);

#ifdef __cplusplus
}
#endif
#endif
