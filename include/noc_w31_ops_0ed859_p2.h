/*
 * noc_w31_ops_0ed859_p2.h - C surface for the OPS PAYLOAD / ARGV DERIVATION
 * partition of upstream hermes_cli/web_routers/ops.py (gate w31_ops_0ed859_p2).
 *
 * The credential-pool row the dashboard renders (secret redacted), the safe
 * upload filename for an imported archive, the shell-hook body + event guard,
 * the argv a spawned ``hermes import`` receives, the memory-reset plan, and the
 * pairing approve dispatch (request id vs one-time code).
 *
 * Seams: the credential pool on disk, agent.redact's console colour decision,
 * the filesystem (upload/stat), config.yaml, and every subprocess spawn.  Every
 * refusal returns as data -- {"ok","status","detail"} -- instead of raising.
 */
#ifndef NOC_W31_OPS_0ED859_P2_H
#define NOC_W31_OPS_0ED859_P2_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {entry, index} -> the redacted pool row:
 * {index, id, label, auth_type, source, priority, last_status, request_count,
 *  token_preview, has_refresh}.  ``index`` is 1-based, as CredentialPool.remove_index(). */
json_t *w31_ops_0ed859_p2_pool_entry_summary(const json_t *req);

/* Upload filename -> Path(name).name, non-[A-Za-z0-9._-] runs -> '-',
 * strip(".-"), empty -> "backup.zip", then ".zip" appended when absent.
 * ``nt`` selects Windows separator/drive handling (Path's behaviour on this host). */
void w31_ops_0ed859_p2_safe_backup_upload_name(const char *filename, int nt, char *out, size_t cap);

/* {event, command} -> {ok, status, detail, event?, command?} with both stripped;
 * 400 "event and command are required" when either is empty. */
json_t *w31_ops_0ed859_p2_hook_body_fields(const json_t *req);

/* {event, command, valid_hooks} -> the same verdict, plus the VALID_HOOKS guard:
 * an unknown event is 400 "Unknown event '<e>'. Valid: <sorted, joined>".
 * A null/absent valid_hooks means the registry was unavailable -> no check. */
json_t *w31_ops_0ed859_p2_hook_event_verdict(const json_t *req);

/* {archive, force, keep_host, messaging_off} -> ["import", archive, flags...]:
 * --force (force or keep_host), --keep-host (keep_host), --messaging-off (keep_host and messaging_off). */
json_t *w31_ops_0ed859_p2_spawn_import_argv(const json_t *req);

/* {target, memory_exists, user_exists} -> {ok, status, detail, target?, deleted?}:
 * target in {all, memory, user} else 400; ``deleted`` lists the builtin files
 * the reset would unlink (MEMORY.md / USER.md) that the fixture says exist. */
json_t *w31_ops_0ed859_p2_memory_reset_target(const json_t *req);

/* {platform, request_id, code} -> {ok, status, detail, platform?, target?,
 * by_request_id?, approve_arg?}.  request_id wins over code by truthiness; a
 * 16-hex target (or any request_id) takes the request path, else the code path
 * approves target.upper().  A blank platform or target is 400. */
json_t *w31_ops_0ed859_p2_pairing_approve_target(const json_t *req);

/* {backup_dir, resolved, resolve_error, is_file, nt} -> {ok, status, detail}:
 * the _dashboard_backup_path chain -- resolve(strict=True) missing -> 404
 * "Backup not found"; any other resolve failure -> 400 "Invalid backup path";
 * a target outside the profile's backups directory -> 403; not a file -> 404.
 * ``resolved`` is the fixture's stat answer (null when it does not resolve). */
json_t *w31_ops_0ed859_p2_backup_download_verdict(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
