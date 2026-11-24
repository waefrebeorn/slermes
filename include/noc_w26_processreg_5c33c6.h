/*
 * noc_w26_processreg_5c33c6.h - C surface for upstream
 * tools/process_registry_output_log.py (gate w26_processreg_5c33c6).
 *
 * The value core of a worker's file-backed process output: where a profile
 * keeps those logs, which checkpointed path may be adopted, the exit-status
 * sidecar path, the shell prefix that records the status there, and the drain
 * budgets.  Every seam stays in the caller: the worker test, the directory
 * creation, the glob/stat retirement sweep, the descriptor and the tail loop.
 */
#ifndef NOC_W26_PROCESSREG_5C33C6_H
#define NOC_W26_PROCESSREG_5C33C6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* hermes_home: str -> str(Path(hermes_home) / "logs" / "process-output"). */
json_t *w26_processreg_5c33c6_output_dir(const json_t *hermes_home);

/* path, output_dir: str|null -> str(path) when path names a ".log" directly in
 * output_dir, else "". */
json_t *w26_processreg_5c33c6_adoptable_log(const json_t *path, const json_t *output_dir);

/* log: str|null -> str(Path(log).with_suffix(".exit")), null when the path has
 * no name (Path raises). */
json_t *w26_processreg_5c33c6_exit_path(const json_t *log);

/* log, command: the command prefixed so the shell records "$?" beside the log. */
json_t *w26_processreg_5c33c6_record_exit_command(const json_t *log, const json_t *command);

/* _TAIL_SECONDS / _FINAL_DRAIN_MAX_BYTES, as an object. */
json_t *w26_processreg_5c33c6_constants(void);

#ifdef __cplusplus
}
#endif
#endif
