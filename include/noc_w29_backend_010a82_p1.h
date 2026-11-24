/*
 * noc_w29_backend_010a82_p1.h - C surface for upstream
 * plugins/config_backends/remote/backend.py (gate w29_backend_010a82_p1).
 *
 * P1 covers the module's constant budget, the wire-response decision helpers
 * and the capability/diagnostic surface: what a fetched body is accepted as,
 * which plane refusals are the caller's value, how a refusal is turned into an
 * exception, which env names this backend owns, and how it describes itself.
 * Every HTTP call, log handler and filesystem probe is a fixture input here.
 */
#ifndef NOC_W29_BACKEND_010A82_P1_H
#define NOC_W29_BACKEND_010A82_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* POLL_SECONDS / MIN_POLL_SECONDS / BOOT_RETRY_DELAYS / MAX_RETRY_WAIT,
 * _MAX_PREPARE, _MAX_POSTPROCESS and the _VALUE_REFUSALS table, as an object. */
json_t *w29_backend_010a82_constants(void);

/* max(POLL_SECONDS, MIN_POLL_SECONDS). */
json_t *w29_backend_010a82_poll_interval(void);

/* req: {"default_config": {...}} -> the _config_version int, else 1. */
json_t *w29_backend_010a82_latest_config_version(const json_t *req);

/* (config is a dict) and (profileVersion is an int) and (locks is a list). */
json_t *w29_backend_010a82_effective_ok(const json_t *body);

/* {"known": bool, "value": ...}: the profile level's writerConfigVersion, with
 * known=false standing for the _UNKNOWN sentinel. */
json_t *w29_backend_010a82_profile_writer(const json_t *body);

/* req: {"status": int, "error": str} -> bool: is that refusal about the value. */
json_t *w29_backend_010a82_value_refusal(const json_t *req);

/* resp: {"status": int, "body": {...}} -> {"error": "ConfigLockedError" |
 * "ConfigValueError" | "ConfigWriteError", "code": str, "message": str,
 * "path": str|null, "locked_by": str|null}. */
json_t *w29_backend_010a82_write_error(const json_t *resp);

/* req: {"note": str} -> {"level": "debug"|"warning", "message": str} (what
 * _log_notes would log, and at which level). */
json_t *w29_backend_010a82_note_level(const json_t *req);

/* supports_file_tooling / honors_managed_config / protected_env_names /
 * deployment_env_names / bootstrap_env_names, as an object of sorted lists. */
json_t *w29_backend_010a82_capabilities(void);

/* req: {"managed": str|null} -> the doctor_warnings tuple as a list of
 * [text, fix, detail] triples ([] when no managed file exists). */
json_t *w29_backend_010a82_doctor_warnings(const json_t *req);

/* req: {"profile": str} -> "Remote Config (profile <repr> level)".
 * Seam: _profile_for(home) is the input, the home is never inspected. */
json_t *w29_backend_010a82_destination(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
