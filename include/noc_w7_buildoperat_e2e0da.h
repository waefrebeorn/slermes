#ifndef NOC_W7_BUILDOPERAT_E2E0DA_H
#define NOC_W7_BUILDOPERAT_E2E0DA_H
/*
 * Public C API for the port of pm/build_operations.py (gate w7_buildoperat_e2e0da).
 *
 * These functions model the caller-owned admission/validation decisions of the
 * build operations: argument shapes, tool-vs-state selection, store disjointness
 * and install permission. The fixture injects the already-resolved facts
 * (name shape, StatePackage presence, store overlap, manifest presence,
 * explicit/install permission); post-admission I/O stays in Python.
 * Every function returns {"ok":true} or {"error":<class>,"message":<text>}.
 */
#include "hermes_json.h"

json_t *w7_buildoperat_prepare_tools(const json_t *names, int state_present);
json_t *w7_buildoperat_verified_tools(const json_t *names, int state_present, int facts_present);
json_t *w7_buildoperat_stage_tools(const json_t *names, int overlap, int state_present, int facts_present);
json_t *w7_buildoperat_check_project_lock(int manifest_present, int offline, int explicit);
json_t *w7_buildoperat_build_requirements_environment(int explicit);

#endif /* NOC_W7_BUILDOPERAT_E2E0DA_H */
