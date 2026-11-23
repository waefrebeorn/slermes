#ifndef NOC_W7_SURFACESWIT_4B9E48_H
#define NOC_W7_SURFACESWIT_4B9E48_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RUNTIME_ENVIRONMENT_HEADING / RUNTIME_ENVIRONMENT_END from
 * agent/prompt_builder.py, mirrored here as the boundary literals. */

/* PoP: w7_surfaceswit_4b9e48_split_runtime_boundary @ agent/surface_switch.py:split_runtime_boundary
 * Returns [identity, runtime_marker, runtime]. Caller frees. */
json_t *w7_surfaceswit_4b9e48_split_runtime_boundary(const char *prompt);

/* PoP: w7_surfaceswit_4b9e48_runtime_host_value @ agent/surface_switch.py:runtime_host_value
 * "Label: value" from the first three host lines after the "User home
 * directory:" line; "" when absent. Returns a malloc'd string; caller frees. */
char *w7_surfaceswit_4b9e48_runtime_host_value(const char *prompt, const char *label);

/* PoP: w7_surfaceswit_4b9e48_identity_line_value @ agent/surface_switch.py:identity_line_value
 * Last "Label: value" line in the trailing paragraph of the identity block.
 * Returns a malloc'd string; caller frees. */
char *w7_surfaceswit_4b9e48_identity_line_value(const char *prompt, const char *label);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_SURFACESWIT_4B9E48_H */
