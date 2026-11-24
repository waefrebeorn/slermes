/*
 * noc_w23_core_082b31.h - C surface for upstream
 * hermes_platform/resolver/core.py (gate w23_core_082b31).
 *
 * Result/context types shared by every resolver, plus passive command location:
 * locate_command reads file metadata only, recording every candidate in probe
 * order and deriving the command/source/present/found views from them.
 *
 * Seams (injected by the harness, never decided here): the ambient process
 * facts a passive probe would read -- os.name/sep/altsep/pathsep, the env
 * mapping used for PATHEXT and $VAR/~/ expansion, the PATH probe (shutil.which)
 * and the "is a file" / "is executable" answers.
 */
#ifndef NOC_W23_CORE_082B31_H
#define NOC_W23_CORE_082B31_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LookupContext.effective_path(): the explicit path when the caller gave a
 * string, JSON null otherwise (ABSENT and None both read as None). */
json_t *w23_core_082b31_effective_path(const json_t *ctx);

/* Resolution views over a supplied kind + candidate list.
 * req: kind, candidates[{value,source,present}] ->
 *      {kind, candidates, command[], source, present[], found}. */
json_t *w23_core_082b31_resolution(const json_t *req);

/* locate_command(name, ctx, known_dirs=...) over the injected probe.
 * req: name, ctx{path?,pathext?}, known_dirs[], os_name, sep, altsep, pathsep,
 *      env{}, which{probe->hit}, files[], xok[].
 * Returns {kind, candidates[{value,source,present}], command[], source,
 * present[], found}. */
json_t *w23_core_082b31_locate(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
