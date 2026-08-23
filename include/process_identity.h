/*
 * process_identity.h — C11 port of hermes_cli/process_identity.py
 *
 * Spawn tags (HERMES_SPAWN), the machine-wide spawn ledger
 * (spawn-ledger.json), and the Windows job-object self-attach. All of it is
 * best-effort and fail-safe; a corrupt ledger is quarantined, never
 * rewritten blind.
 */
#ifndef SLERMES_PROCESS_IDENTITY_H
#define SLERMES_PROCESS_IDENTITY_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROCESS_IDENTITY_SPAWN_ENV_VAR "HERMES_SPAWN"
#define PROCESS_IDENTITY_LEDGER_FILENAME "spawn-ledger.json"

extern const char *const PROCESS_IDENTITY_REAPABLE_PURPOSES[];

/* install_id @ hermes_cli/process_identity.py:install_id — stable 12-hex id
 * for THIS install derived from its canonical lowercase path. project_root
 * NULL → the running install's root. Caller frees. */
char *process_identity_install_id(const char *project_root);

/* _own_create_time @ ...:_own_create_time — process start as seconds since
 * epoch (from /proc self starttime); returns 0 on failure. */
double process_identity_own_create_time(void);

/* SpawnTag @ ...:SpawnTag */
typedef struct {
    char install[16];
    char purpose[128];
    long spawner_pid;
    int has_spawner_create;
    double spawner_create;
} process_identity_spawn_tag_t;

/* build_spawn_tag @ ...:build_spawn_tag — malloc'd "v1:<install>:<purpose>:
 * <pid>:<create|->" tag. Caller frees. */
char *process_identity_build_spawn_tag(const char *purpose,
                                       const char *project_root);

/* spawn_env @ ...:spawn_env — {"HERMES_SPAWN": tag} fragment. Caller frees.
 * out_var/out_val point into the returned buffer semantics: both are
 * malloc'd strings, caller frees each. */
void process_identity_spawn_env(const char *purpose, const char *project_root,
                                char **out_var, char **out_val);

/* parse_spawn_tag @ ...:parse_spawn_tag — 0 on success, -1 malformed. */
int process_identity_parse_spawn_tag(const char *raw,
                                     process_identity_spawn_tag_t *out);

/* _ledger_path @ ...:_ledger_path — machine-root ledger path. Caller frees. */
char *process_identity_ledger_path(void);

/* _read_ledger @ ...:_read_ledger — entries list (caller frees) for empty/
 * missing; NULL with *corrupt=1 for CORRUPT (never silently an empty roster). */
json_t *process_identity_read_ledger(const char *path, int *corrupt);

/* _quarantine_ledger @ ...:_quarantine_ledger — move aside to ".corrupt". */
void process_identity_quarantine_ledger(const char *path);

/* register_self @ ...:register_self — record this process in the machine
 * spawn ledger (dead entries pruned). Returns 1 on success. */
int process_identity_register_self(const char *purpose,
                                   const char *project_root);

/* ledger_entries @ ...:ledger_entries — live-verified entries for THIS
 * install (JSON array, caller frees). Corrupt → quarantined + empty array. */
json_t *process_identity_ledger_entries(const char *project_root);

/* spawner_is_dead @ ...:spawner_is_dead — 1 provably gone, 0 alive,
 * -1 unprovable / no spawner recorded. */
int process_identity_spawner_is_dead(json_t *entry);

/* attach_self_to_kill_on_close_job @ ...:attach_self_to_kill_on_close_job —
 * Windows-only; POSIX builds faithfully return 0 (not Windows). */
int process_identity_attach_self_to_kill_on_close_job(void);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PROCESS_IDENTITY_H */
