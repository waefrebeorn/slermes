/*
 * port_hermes_state.h — C11 port of pure helpers from hermes_state.py.
 *
 * Ports deterministic, I/O-free helpers: SHA-256 system prompt hashing,
 * disk-full error classification, and journal mode resolution.
 */

#ifndef PORT_HERMES_STATE_H
#define PORT_HERMES_STATE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _system_prompt_hash @ hermes_state.py:_system_prompt_hash */
/* SHA-256 hex digest of the system prompt. Returns a malloc'd 65-char
 * hex string (caller frees), or NULL on allocation failure. */
char *hs_system_prompt_hash(const char *system_prompt);

/* PoP: is_disk_full_error @ hermes_state.py:is_disk_full_error */
/* True when *exc* (or a stringified error) is a disk-full / ENOSPC failure.
 * Covers OSError ENOSPC, SQLite "database or disk is full", and
 * plain English/errno strings. NULL returns false. */
bool hs_is_disk_full_error(const char *exc);

/* PoP: resolve_journal_mode @ hermes_state.py:resolve_journal_mode */
/* Return the configured journal mode ("wal" or "delete").
 * Reads database.journal_mode from config.yaml; invalid or missing
 * values fail safely to "wal". Returns a malloc'd string (caller frees). */
char *hs_resolve_journal_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_STATE_H */
