/*
 * noc_w24_remotecomm_c4bdad.h - C surface for upstream
 * tools/environments/remote_common.py (gate w24_remotecomm_c4bdad).
 *
 * The value layer shared by the non-local terminal backends (docker, ssh, cloud
 * SDKs): which env values are forwarded into a remote shell, which scoped names
 * must be ``unset`` there, the ``unset ... || true`` prefix, the ``bash -c``
 * argv tail, the client-subprocess env merge, and ``shlex.quote`` itself.
 *
 * Seams (the caller owns them, the port takes their results as values):
 *   * ``os.environ`` / ``~/.hermes/.env`` are passed in as objects, never read;
 *   * the passthrough registry, the internal-secret and provider-blocklist
 *     predicates, the global-env predicate and the multiplex flag are passed in
 *     as explicit name sets, since each is a registry/scope lookup upstream;
 *   * ``resolve_passthrough_value`` is modelled as identity.
 * Nothing here spawns a process, opens a socket, reads a file or installs a dep.
 */
#ifndef W24_REMOTECOMM_C4BDAD_H
#define W24_REMOTECOMM_C4BDAD_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* shlex.quote(s): bare when safe, else POSIX single-quoted. */
json_t *w24_remotecomm_c4bdad_shlex_quote(const json_t *s);

/* prepend_unset(cmd, names): "unset <sorted quoted names> 2>/dev/null || true\n<cmd>",
 * or cmd untouched when there is nothing to unset. */
json_t *w24_remotecomm_c4bdad_prepend_unset(const json_t *cmd, const json_t *names);

/* bash_argv(cmd, login) -> ["bash", "-l", "-c", cmd] / ["bash", "-c", cmd]. */
json_t *w24_remotecomm_c4bdad_bash_argv(const json_t *cmd, const json_t *login);

/* client_env_with(values, environ) -> {**environ, **values}, or null when there
 * is nothing to add (inherit). */
json_t *w24_remotecomm_c4bdad_client_env_with(const json_t *values, const json_t *host_env);

/* resolve_passthrough_env with the registry/scope seams as explicit values ->
 * {"exec_env": {...}, "unset": [<sorted names>]}. */
json_t *w24_remotecomm_c4bdad_resolve_passthrough(const json_t *explicit_forward,
                                                  const json_t *passthrough,
                                                  const json_t *env,
                                                  const json_t *hermes_env,
                                                  const json_t *multiplex,
                                                  const json_t *internal_secret,
                                                  const json_t *blocklisted,
                                                  const json_t *global_env);

/* The literal patterns the module matches with. */
json_t *w24_remotecomm_c4bdad_constants(void);

#ifdef __cplusplus
}
#endif
#endif
