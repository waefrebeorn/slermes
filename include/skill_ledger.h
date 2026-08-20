/*
 * skill_ledger.h — Per-mutation skill audit ledger + single-edit rollback.
 * Port of Python tools/skill_ledger.py (388 lines).
 *
 * Opaque struct + minimal includes. The ledger is TELEMETRY, NOT A GATE:
 * a ledger failure must never block the mutation it describes. The one
 * deliberate exception is rollback_entry, which FAILS CLOSED when its own
 * pre-rollback safety capture fails.
 *
 * Every public symbol is a faithful 1:1 port of the Python function with the
 * same name (name-parity mandate). Functions that never raise/never block the
 * mutation return NULL on failure; rollback_entry returns (ok, message).
 */

#ifndef SKILL_LEDGER_H
#define SKILL_LEDGER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Actor constants (mirror Python ACTOR_* / _VALID_ACTORS). */
#define SKILL_LEDGER_ACTOR_CURATOR "curator"
#define SKILL_LEDGER_ACTOR_AGENT   "agent"
#define SKILL_LEDGER_ACTOR_USER    "user"

/* A captured {path, sha256} manifest item. Caller frees via
 * skill_ledger_manifest_free. */
typedef struct {
    char **paths;
    char **sha256s;
    size_t count;
} skill_ledger_manifest_t;

/* A single ledger entry (parsed). Caller frees via skill_ledger_entry_free. */
typedef struct {
    char  *id;
    char  *ts;
    char  *actor;
    char  *action;
    char  *skill;
    char  *evidence_json;      /* raw evidence object serialized, or "" */
    skill_ledger_manifest_t before;
    skill_ledger_manifest_t after;
} skill_ledger_entry_t;

/* ── Actor override (mirrors Python contextvars _actor_override) ─────────── */

/* Bind an explicit actor for subsequent ledger records in this thread.
 * Returns a token; callers must reset_ledger_actor(token) when done. */
int set_ledger_actor(const char *actor);

/* Reset the actor override to its prior value. */
void reset_ledger_actor(int token);

/* Best-effort actor derivation: explicit override, else background-review
 * provenance signal, else agent. */
const char *derive_actor(void);

/* ── Paths + config gate ─────────────────────────────────────────────────── */

/* Returns the ledger path: ~/.hermes/skills/.curator_ledger.jsonl
 * (static buffer, do not free). */
const char *ledger_path(void);

/* Returns the blobs dir: ~/.hermes/.curator_backups/blobs
 * (static buffer, do not free). */
const char *blobs_dir(void);

/* Config gate skills.ledger (default True). */
bool ledger_enabled(void);

/* ── Blob store (content-addressed, deduped) ─────────────────────────────── */

/* Write data to the blob store keyed by its sha256. Dedupes. Returns a
 * malloc'd hex digest (caller frees), or NULL on failure. */
char *store_blob(const unsigned char *data, size_t len);

/* Return malloc'd blob content (caller frees), or NULL when missing/invalid. */
unsigned char *read_blob(const char *sha256, size_t *out_len);

/* Capture {path, sha256} for every file under root (recursively), storing
 * each file's content as a blob. Returns NULL when root is NULL / missing.
 * Caller frees via skill_ledger_manifest_free. Raises nothing. */
skill_ledger_manifest_t *snapshot_paths(const char *root);

/* ── Append + read ────────────────────────────────────────────────────────── */

/* Append one ledger entry. Returns a malloc'd entry id (caller frees), or
 * NULL when the ledger is disabled or the write failed (never raises). */
char *append_entry(const char *action, const char *skill,
                   const skill_ledger_manifest_t *before,
                   const char *before_json,
                   const skill_ledger_manifest_t *after,
                   const char *after_json,
                   const char *actor, const char *evidence_json);

/* One-stop hook for mutation call sites. NEVER raises and never blocks the
 * mutation. Returns a malloc'd entry id (caller frees) or NULL. */
char *record_mutation(const char *action, const char *skill,
                      const char *before_root, const char *before_json,
                      const char *after_root, const char *actor,
                      const char *evidence_json);

/* Best-effort pre-mutation capture. Returns NULL on failure or when the
 * ledger is disabled. Free via skill_ledger_manifest_free. */
skill_ledger_manifest_t *capture_before(const char *root);

/* Read the ledger, newest first. Malformed lines are skipped. Returns a
 * malloc'd NULL-terminated array of entries (caller frees each + the array). */
skill_ledger_entry_t **list_entries(const char *skill, int limit, size_t *out_count);

/* Fetch a single entry by id. Returns NULL when not found. Free via
 * skill_ledger_entry_free. */
skill_ledger_entry_t *get_entry(const char *entry_id);

/* ── Single-edit rollback ─────────────────────────────────────────────────── */

/* Restore the before-state of a single mutation. Fail-closed. */
bool rollback_entry(const char *entry_id, char *out_msg, size_t msg_len);

/* ── Free helpers ─────────────────────────────────────────────────────────── */
void skill_ledger_manifest_free(skill_ledger_manifest_t *m);
/* Free only the inner path/sha arrays (for manifests embedded inline in
 * another struct). Does not free the owning struct. */
void skill_ledger_manifest_free_arrays(skill_ledger_manifest_t *m);
void skill_ledger_entry_free(skill_ledger_entry_t *e);
void skill_ledger_entries_free(skill_ledger_entry_t **arr, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* SKILL_LEDGER_H */
