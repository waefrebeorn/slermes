/*
 * memory_store.h — public API for the bounded curated memory store.
 * Faithful C port of tools/memory_tool.py (MemoryStore + load_on_disk_store).
 *
 * Self-contained, opaque struct, minimal includes. Two stores:
 *   - "memory"  -> <HERMES_HOME>/memories/MEMORY.md
 *   - "user"    -> <HERMES_HOME>/memories/USER.md
 * Entries are separated by "\n§\n" (ENTRY_DELIMITER), may be multiline.
 *
 * Mid-session writes update files on disk immediately (durable) but the
 * "system prompt snapshot" is frozen at load time (kept stable here as a
 * captured snapshot string). We do NOT mutate the snapshot mid-session.
 *
 * Threat scanning is INJECTABLE (memory_store_set_threat_scanner) so this
 * module stays self-contained — the caller wires the shared threat-pattern
 * library (mirrors Python's tools.threat_patterns import). Default: no scan.
 */

#ifndef HERMES_MEMORY_STORE_H
#define HERMES_MEMORY_STORE_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque memory store. */
typedef struct memory_store_t memory_store_t;

/* Threat scanner: returns a non-NULL, malloc'd error string if `content`
 * is blocked (caller frees), or NULL if allowed. May be NULL (no scan). */
typedef char *(*memory_threat_scanner_t)(const char *content);

/* Create a store with the given char limits (0 => built-in defaults). */
memory_store_t *memory_store_new(int memory_char_limit, int user_char_limit);

/* Free a store. */
void memory_store_free(memory_store_t *store);

/* Set the injectable threat scanner (may be NULL). */
void memory_store_set_threat_scanner(memory_store_t *store,
                                     memory_threat_scanner_t scanner);

/* Load entries from <mem_dir>/MEMORY.md and <mem_dir>/USER.md. Creates the
 * directory if missing. Captures the frozen system-prompt snapshot. */
void memory_store_load(memory_store_t *store, const char *mem_dir);

/* Snapshot accessor (frozen at load). Returns the rendered block for `target`
 * ("memory"/"user"), or NULL if empty. Caller does NOT free (points into store). */
const char *memory_store_snapshot(memory_store_t *store, const char *target);

/* ---- mutation results ------------------------------------------------
 * Each returns a malloc'd JSON string the caller frees. Shape mirrors
 * Python's dict dumps: {"success":bool,"done":bool,"target":...,"usage":...,
 * "entry_count":n,["message":...]|["error":...],["staged":...]}. */

char *memory_store_add(memory_store_t *store, const char *target,
                       const char *content);
char *memory_store_replace(memory_store_t *store, const char *target,
                           const char *old_text, const char *new_content);
char *memory_store_remove(memory_store_t *store, const char *target,
                          const char *old_text);

/* Apply a batch of {action, content?, old_text?} ops atomically against the
 * final budget. `ops` is a JSON array node (json_node_t*). All-or-nothing. */
char *memory_store_apply_batch(memory_store_t *store, const char *target,
                               const json_node_t *ops);

/* Usage string for a target, e.g. "37% — 815/2,200 chars". Caller frees. */
char *memory_store_usage(memory_store_t *store, const char *target);

/* Char counts. */
int memory_store_char_count(memory_store_t *store, const char *target);
int memory_store_char_limit(memory_store_t *store, const char *target);
int memory_store_entry_count(memory_store_t *store, const char *target);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MEMORY_STORE_H */
