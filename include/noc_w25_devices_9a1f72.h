/*
 * noc_w25_devices_9a1f72.h - C surface for upstream
 * hermes_cli/local_runtime/devices.py (gate w25_devices_9a1f72).
 *
 * Which accelerators the installed llama.cpp build reports.  Running the
 * probe is a seam: loading a GPU driver can crash or hang, so upstream forks a
 * disposable child with a deadline and reads its stdout.  What is ported here
 * is the part that is a pure function of that child's result: the disposition
 * of its exit status / exception, the JSON decode, and the device filter that
 * keeps discrete and integrated GPUs with a positive total and a description.
 */
#ifndef NOC_W25_DEVICES_9A1F72_H
#define NOC_W25_DEVICES_9A1F72_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* probe_devices(engine_dir, backend) with the child's outcome fixed:
 * request: {"stdout": <str>, "returncode": <int>,
 *           "raise": "none" | "timeout" | "oserror"}
 * -> the GPU/iGPU rows of the decoded list, [] on any failure. */
json_t *w25_devices_9a1f72_probe_devices(const json_t *request);

/* _library(directory, name): "<directory>/<name>.dll" on nt, else
 * "<directory>/lib<name>.so".  Separators are normalised to '/'. */
json_t *w25_devices_9a1f72_library(const json_t *request);

/* The ggml_backend_dev_type values the filter admits. */
json_t *w25_devices_9a1f72_constants(void);

#ifdef __cplusplus
}
#endif
#endif
