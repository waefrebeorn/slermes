/*
 * port_update_pause_record.h -- pure helpers ported from
 * hermes_cli/update_pause_record.py (the durable record of the gateways and
 * services `hermes update` paused on Windows).
 *
 * Only the network-free, subprocess-free slice is here. Everything that needs
 * a kernel mutex, git, psutil or the filesystem's atomic-replace semantics is
 * deliberately left to the caller.
 */
#ifndef PORT_UPDATE_PAUSE_RECORD_H
#define PORT_UPDATE_PAUSE_RECORD_H

#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stand-in for update_lock.incarnation_live(pid, ct): 1 live, 0 not live,
 * -1 unknown. `update_pause_record.identity_is_live` treats only an explicit
 * "not live" as dead, so the caller's unknown and live collapse together. */
typedef int (*upr_incarnation_fn)(long long pid, const char *ct);

/* _install_key -- sha256(normcase(root))[:12]. `normcase` is Windows
 * lowercasing plus '/' -> '\'; `root` NULL means the install root itself,
 * which the caller supplies. Writes 12 chars + NUL. */
void upr_install_key(const char *root, char *out, size_t outsz);

/* _digest -- sha256 of the file's bytes, hex. NULL when it cannot be read
 * (Python returns None so a deleted file reads the same way twice). */
char *upr_digest_file(const char *path);

/* _has_work -- whether a token still owes anything. */
int upr_has_work(const json_t *token);

/* _without -- drop the profiles / unmapped / posix_units entries whose pid is
 * in `pids`. Mutates and returns `token`. */
json_t *upr_without(json_t *token, const char *const *pids, size_t n);

/* _accepted_path -- <marker>.accepted (same directory, name suffixed). */
void upr_accepted_path(const char *path, char *out, size_t outsz);

/* _retired_path -- Path.with_suffix(".retired"). */
void upr_retired_path(const char *path, char *out, size_t outsz);

/* identity_is_live -- the update marker's incarnation rule: only an explicit
 * "not live" verdict counts as dead. */
int upr_identity_is_live(const json_t *ident, upr_incarnation_fn fn);

/* _live_pids -- the identities of `token` that are still live, as a JSON array
 * of pid strings. Caller owns the result. */
json_t *upr_live_pids(const json_t *token, upr_incarnation_fn fn);

#ifdef __cplusplus
}
#endif

#endif /* PORT_UPDATE_PAUSE_RECORD_H */
