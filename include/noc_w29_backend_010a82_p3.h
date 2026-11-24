/*
 * noc_w29_backend_010a82_p3.h - C surface for upstream
 * plugins/config_backends/remote/backend.py (gate w29_backend_010a82_p3).
 *
 * P3 covers the write intent and the PATCH body: the key-level edit a save
 * means (§15.2 steps 1-5), its re-application to the current document, lock and
 * secret-literal validation, and the writerConfigVersion stamp.  The plane's
 * response and the CAS retry stay seams; this is the payload decision.
 */
#ifndef NOC_W29_BACKEND_010A82_P3_H
#define NOC_W29_BACKEND_010A82_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"server_config": obj, "st_base": obj|null, "read_base": obj|null,
 * "private": bool, "document": obj|null, "set": {dotted: any},
 * "unset": [dotted], "locks": [[path, level], ...],
 * "pinned_keys": [dotted], "refused_prefixes": [dotted]} ->
 * {"ok": true, "sets": [[path, value], ...], "unsets": [[...]],
 * "pinned": [encoded]} | {"ok": false, "error": str, "code": str,
 * "message": str}. */
json_t *w29_backend_010a82_intent(const json_t *req);

/* req: {"server_config": obj, "st_base": obj|null,
 * "sets": [[path, value], ...], "unsets": [[...]], "pinned": [[path], ...],
 * "locks": [[path, level], ...],
 * "profile_writer": {"kind":"null"|"unknown"|"int","value":int},
 * "latest": int} -> {"ok": true, "body": obj|null, "sets": [[path, value]],
 * "unsets": [[...]]} | error object. */
json_t *w29_backend_010a82_patch_body(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
