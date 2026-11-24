/*
 * noc_w33_servers_a4580e.h - C surface for upstream agent/lsp/servers.py
 * (gate w33_servers_a4580e).
 *
 * The pure decision core of the LSP server registry: the per-language server
 * descriptor table and its capability flags, the didOpen languageId map, the
 * file->server match (extension or extensionless basename, compound
 * extensions first), the lsp.servers custom-entry normaliser, the
 * marker-driven root decision, the untrusted-workspace allow-list, and the
 * positional command assembly.  Every process/file probe (shutil.which,
 * os.path.exists, nearest_root walking the tree, pm.env_for, auto-install,
 * logging) is a seam owned by the caller; this header owns only what the
 * values decide.
 */
#ifndef NOC_W33_SERVERS_A4580E_H
#define NOC_W33_SERVERS_A4580E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* path -> lower-cased extension (compound ones first), or the full basename
 * when the file has no extension ("Dockerfile"). */
json_t *w33_servers_a4580e_ext_or_basename(const char *path);

/* (path, a custom server's own language_id or NULL/"") -> didOpen languageId,
 * falling back to LANGUAGE_BY_EXT and then "plaintext". */
json_t *w33_servers_a4580e_language_id(const char *path, const char *srv_language_id);

/* LANGUAGE_BY_EXT as an object: extension -> languageId. */
json_t *w33_servers_a4580e_language_table(void);

/* ServerDef.matches: is ext_or_basename(path) one of `extensions`? */
json_t *w33_servers_a4580e_matches(const char *path, const json_t *extensions);

/* find_server_for_file over (custom_servers(cfg) + SERVERS): the first
 * descriptor whose extensions claim the path, or null. */
json_t *w33_servers_a4580e_find_server(const char *path, const json_t *custom_cfg);

/* The built-in registry, in source order, as descriptor objects. */
json_t *w33_servers_a4580e_registry(void);

/* custom_servers(): lsp.servers entries that declare extensions and name no
 * built-in server, normalised to descriptors (malformed entries are skipped). */
json_t *w33_servers_a4580e_custom_servers(const json_t *servers_cfg);

/* The _markers_root/_root_or_workspace decision over the two nearest_root
 * probes (the walk itself is the seam):
 * req = {markers, found, found_plain, workspace, excludes} -> root or null. */
json_t *w33_servers_a4580e_root_decision(const json_t *req);

/* server_id in UNTRUSTED_SAFE_SERVERS. */
json_t *w33_servers_a4580e_untrusted_safe(const char *server_id);

/* _simple_spawn's positional assembly with the binary lookup resolved:
 * req = {bin, args, trusted, base_init, untrusted_init, seed} ->
 * {command, init, seed} or null when the binary did not resolve. */
json_t *w33_servers_a4580e_simple_spawn(const json_t *req);

/* _custom_spawn's positional assembly with the PATH lookup resolved:
 * req = {bin, command} -> [bin, *command[1:]] or null. */
json_t *w33_servers_a4580e_custom_argv(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
