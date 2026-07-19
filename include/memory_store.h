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

/* Threat scanner: returns a non-NULL, malloc'd error string if `content`
 * is blocked (caller frees), or NULL if allowed. May be NULL (no scan). */
typedef char *(*memory_threat_scanner_t)(const char *content);

/* Write-gate decision returned by a memory_store_write_gate_t callback.
 * Mirrors Python's write_approval GateDecision: allow / blocked / staged. */
typedef struct {
    int allow;     /* 1 => proceed with the real write */
    int blocked;   /* 1 => hard-blocked, do not write */
    int staged;    /* 1 => staged for approval (write not performed now) */
    char *message; /* malloc'd human message (caller frees) */
    char *pending_id; /* malloc'd staged record id (caller frees) or NULL */
} memory_write_gate_decision_t;

/* Write-gate callback. `detail` is a malloc'd inline summary/detail string the
 * caller builds (and frees); the callback returns a fully-populated decision
 * (it malloc's message/pending_id). Returns a decision with allow=1 when the
 * write should proceed. May be NULL (fail-open, no gate). */
typedef memory_write_gate_decision_t (*memory_store_write_gate_t)(
    const char *target, const char *detail);

/* Set the injectable write gate (may be NULL). Default: NULL (fail-open). */
void memory_store_set_write_gate(memory_store_t *store,
                                 memory_store_write_gate_t gate);

/* Free a gate decision's malloc'd fields (not the struct itself). */
void memory_store_free_gate_decision(memory_write_gate_decision_t *d);

/* ---- the memory_tool handler ----------------------------------------
 * Faithful port of tools/memory_tool.py: memory_tool / _missing_old_text_error
 * / apply_memory_pending / check_memory_requirements. Returns a malloc'd JSON
 * string (caller frees). Two shapes:
 *   - single op: action + (content / old_text)
 *   - batch:     operations = JSON array of {action, content?, old_text?}
 * The write gate (if set) is evaluated before the real store write; on
 * block/stage it returns the gate result and does NOT mutate the store. */

/* Single-op or batch dispatch. `operations` may be NULL (single-op shape). */
char *memory_tool_run(memory_store_t *store, const char *action,
                      const char *target, const char *content,
                      const char *old_text, const json_node_t *operations);

/* Replay a staged write (bypasses the gate). `payload` is a JSON object with
 * action/target/content/old_text (or action="batch" + operations). Returns the
 * store's result dict as a malloc'd JSON string (caller frees). */
char *memory_tool_apply_pending(memory_store_t *store, const json_node_t *payload);

/* Always available (no external requirements). */
int memory_tool_available(void);

/* Build the recoverable "old_text required" error (current entry inventory +
 * retry instruction). Returns malloc'd JSON (caller frees). */
char *memory_tool_missing_old_text_error(memory_store_t *store,
                                         const char *target, const char *action);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MEMORY_STORE_H */
