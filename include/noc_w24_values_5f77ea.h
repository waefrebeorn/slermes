/*
 * noc_w24_values_5f77ea.h - C surface for upstream
 * plugins/config_backends/remote/values.py (gate w24_values_5f77ea).
 *
 * The client-side value rules of the config-plane wire contract: YAML-derived
 * Python values -> JSON values (§4.1/§4.2, whole document before the diff),
 * and the §14 rule that a secret-shaped key may never carry a literal.  The
 * vendored config-secret-keys.json is a constant table here, so no file is
 * opened; the plane's own re-check on write is the seam.
 *
 * Harness encoding: a Python mapping whose keys are not all strings cannot be
 * written as a JSON object, so ``{"$map": [[key, value], ...]}`` stands for it
 * (insertion order preserved).  Every other value uses its plain JSON form.
 */
#ifndef NOC_W24_VALUES_5F77EA_H
#define NOC_W24_VALUES_5F77EA_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W24_VALUES_5F77EA_SECRET_KEYS 15
#define W24_VALUES_5F77EA_SECRET_SUFFIXES 6
#define W24_VALUES_5F77EA_NON_SECRET_SUFFIXES 6
#define W24_VALUES_5F77EA_ENV_KEYS 31
#define W24_VALUES_5F77EA_ENV_SUFFIXES 3
#define W24_VALUES_5F77EA_ENV_PREFIXES 1
#define W24_VALUES_5F77EA_NON_SECRET_PATHS 3
#define W24_VALUES_5F77EA_MAX_DEPTH 32

/* req: {"value": <any>, "path": [<segment>, ...]} -> {"ok": true, "value": v}
 * or {"ok": false, "error": "<ConfigValueError text>"}.  None/str/bool pass
 * through, an int outside +/-(2^53-1) becomes a decimal string, floats pass,
 * lists convert element-wise, mappings convert their keys (int keys become
 * their decimal string, colliding keys are an error). */
json_t *w24_values_5f77ea_to_wire(const json_t *req);

/* req: {"key": <any>, "path": [...]} -> {"ok": true, "value": "<string key>"}
 * or {"ok": false, "error": "..."}.  _wire_key: a str key as-is, an int key as
 * its decimal string, anything else (bool included) is an error. */
json_t *w24_values_5f77ea_wire_key(const json_t *req);

/* segments: [<str>, ...] -> bool.  §14.2 on decoded segments (list indices
 * already dropped).  "" -> false (Python would raise IndexError). */
json_t *w24_values_5f77ea_is_secret_path(const json_t *segments);

/* req: {"path": [...], "value": <any>} -> the first path that holds a secret
 * literal, or null.  §14.3: lists pass their own path down; a bool, None, ""
 * or a ${PLACEHOLDER} value is not a literal. */
json_t *w24_values_5f77ea_secret_literal_path(const json_t *req);

/* MAX_SAFE_INTEGER and the §14 tables, as an object. */
json_t *w24_values_5f77ea_constants(void);

#ifdef __cplusplus
}
#endif
#endif
