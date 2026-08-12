/*
 * cron_notepad.h — Per-job durable KV scratchpad (port of cron/notepad.py).
 *
 * One SQLite key/value store per cron job at ~/.slermes/cron/notepad.db,
 * carrying state across scheduled wake-ups (cursors, watermarks, watchlists).
 * Size caps: 16 KiB per value, 64 KiB per job.
 */
#ifndef CRON_NOTEPAD_H
#define CRON_NOTEPAD_H

#include <stdbool.h>

#include "hermes_json.h"   /* json_t */

/* PoP: set_note @ cron/notepad.py:set_note */
/* Upsert one key (coerced via str()). Returns a JSON object
 * {job_id,key,value,updated_at} or NULL+err on size-cap violation. */
json_t *cron_notepad_set_note(const char *job_id, const char *key,
                              const char *value, char **err);

/* PoP: _validate @ cron/notepad.py:_validate */
int notepad_validate_note(const char *job_id, const char *key,
                          const char *value, char **err);

/* PoP: get_note @ cron/notepad.py:get_note */
/* Returns malloc'd value, or NULL when no entry exists. */
char *cron_notepad_get_note(const char *job_id, const char *key);

/* PoP: delete_note @ cron/notepad.py:delete_note */
bool cron_notepad_delete_note(const char *job_id, const char *key);

/* PoP: list_notes @ cron/notepad.py:list_notes */
/* Returns a JSON array of {job_id,key,value,updated_at} (caller json_free). */
json_t *cron_notepad_list_notes(const char *job_id);

/* PoP: clear_notepad @ cron/notepad.py:clear_notepad */
/* Delete every key for one job. No-op (returns 0) when no db file yet. */
int cron_notepad_clear_notepad(const char *job_id);

/* PoP: render_notepad_section @ cron/notepad.py:render_notepad_section */
/* Render a job's notepad as a prompt section, or "" when empty/unavailable. */
char *cron_notepad_render_notepad_section(const char *job_id);

#endif /* CRON_NOTEPAD_H */
