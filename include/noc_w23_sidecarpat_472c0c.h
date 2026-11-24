/*
 * noc_w23_sidecarpat_472c0c.h - C surface for upstream
 * plugins/platforms/photon/sidecar_paths.py (gate w23_sidecarpat_472c0c).
 *
 * Decides where the Photon sidecar runs from: env override, writable source
 * tree, read-only tree with baked current deps, or a mirror under
 * $HERMES_HOME/photon/sidecar.
 *
 * Seams (injected by the harness, never decided here): the PHOTON_SIDECAR_DIR
 * env value, the writability probe result, the node_modules presence, the
 * lockfile-vs-install-marker freshness, the hermes home, the mirror mkdir
 * outcome, which source files exist, which mirror files exist and whether a
 * source/mirror pair already compares equal.
 */
#ifndef NOC_W23_SIDECARPAT_472C0C_H
#define NOC_W23_SIDECARPAT_472C0C_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SOURCE_SIDECAR_DIR: Path(__file__).parent / "sidecar" in the quarry. */
#define W23_SIDECARPAT_472C0C_SOURCE_SIDECAR_DIR \
    "C:/Users/eman5/hermes-agent/plugins/platforms/photon/sidecar"

/* _MIRROR_FILES, in copy order; node_modules is deliberately absent. */
size_t w23_sidecarpat_472c0c_mirror_file_count(void);
const char *w23_sidecarpat_472c0c_mirror_file(size_t index);

/* str(Path(x)) on Windows: "/" -> "\", runs collapsed, trailing separator
 * dropped, empty -> ".".  Caller frees. */
char *w23_sidecarpat_472c0c_path_norm(const char *path);

/* world keys: source_dir (null -> default), override, writable, node_modules,
 * lock_newer, hermes_home, mirror_fail, src_files[], dst_files[], same.
 * Returns {"dir": <str>, "copies": [<name>...]}. */
json_t *w23_sidecarpat_472c0c_resolve(const json_t *world);

/* world keys: cached (null -> resolve), plus the resolve keys.  Caller frees. */
char *w23_sidecarpat_472c0c_sidecar_dir(const json_t *world);

/* world keys: log (the persisted _NPM_ERROR_LOG, may be null), sidecar_dir
 * (the memoized _SIDECAR_DIR, may be null), plus the resolve keys.
 * Caller frees. */
char *w23_sidecarpat_472c0c_npm_error_log(const json_t *world);

/* world key touch_ok: the create-then-unlink probe. */
int w23_sidecarpat_472c0c_dir_writable(const json_t *world);

/* str(SOURCE_SIDECAR_DIR).  Caller frees. */
char *w23_sidecarpat_472c0c_source_default(void);

#ifdef __cplusplus
}
#endif
#endif
