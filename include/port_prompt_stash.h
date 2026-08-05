/* port_prompt_stash.h — C11 port of hermes_cli/prompt_stash.py
 *
 * Ctrl+S prompt stash: a pure, in-memory state machine for the classic CLI
 * composer. Newest-first stack of parked drafts with a browse-panel cursor.
 *
 * Faithful-port notes:
 *  - No I/O, no prompt_toolkit. time.monotonic is injectable via a clock fn.
 *  - `images` are kept opaque (List[Any]) — modelled as a json_t* array so the
 *    C port can round-trip an arbitrary value; tests mostly use empty lists.
 *  - All mutations preserve newest-first ordering (index 0 = most recent).
 */

#ifndef PORT_PROMPT_STASH_H
#define PORT_PROMPT_STASH_H

#include <stdbool.h>
#include <stddef.h>
#include "../lib/libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROMPT_STASH_PREVIEW_WIDTH 60
#define PROMPT_STASH_MAX_ITEMS      20

/* Outcomes of a single Ctrl+S press (resolve_ctrl_s). */
typedef enum {
    CTRL_S_NOOP = 0,
    CTRL_S_STASHED,
    CTRL_S_RESTORED,
    CTRL_S_OPEN_PANEL,
    CTRL_S_CLOSE_PANEL
} ctrl_s_action_t;

/* Collapse a multi-line draft into one preview line (build_preview). Caller frees. */
char *prompt_stash_build_preview(const char *text, int width);

/* One parked draft. */
typedef struct stash_entry {
    char      *text;        /* owned */
    json_t    *images;      /* owned json_t* array; NULL = empty */
    double     stashed_at;  /* monotonic clock value */
    char      *preview;     /* owned */
} stash_entry_t;

/* The stash. */
typedef struct prompt_stash prompt_stash_t;

/* Injectable clock: returns a monotonic float (seconds). */
typedef double (*prompt_stash_clock_fn)(void);

prompt_stash_t *prompt_stash_new(int max_items, prompt_stash_clock_fn clock);
void prompt_stash_free(prompt_stash_t *ps);

/* -- queries -- */
size_t prompt_stash_len(const prompt_stash_t *ps);
bool   prompt_stash_bool(const prompt_stash_t *ps);          /* __bool__ */
size_t prompt_stash_count(const prompt_stash_t *ps);         /* len(self) */
/* Newest-first copy of entries (caller frees via prompt_stash_free_entries). */
stash_entry_t **prompt_stash_items(const prompt_stash_t *ps, size_t *out_n);
void   prompt_stash_free_entries(stash_entry_t **items, size_t n);
/* Entries as JSON array of dicts (as_dict). Caller frees with json_free. */
json_t *prompt_stash_panel_rows(const prompt_stash_t *ps);
/* StashEntry.as_dict — single entry as a JSON dict. Caller frees with json_free. */
json_t *prompt_stash_entry_as_dict(const stash_entry_t *e);
/* Status-bar indicator ("", "📌 2", or "📌 2 ▲"). Caller frees. */
char   *prompt_stash_indicator(const prompt_stash_t *ps);
/* Composer placeholder hint. Caller frees. */
char   *prompt_stash_placeholder_hint(const prompt_stash_t *ps);

/* -- mutators -- */
/* stash: push (text, images). images may be NULL (treated as empty).
 * Returns false (no-op) for a blank buffer with no images. The C port takes
 * ownership of `images` (a json_t* array) when provided. */
bool prompt_stash_stash(prompt_stash_t *ps, const char *text, json_t *images);
/* pop(index) -> malloc'd (text, images_json) or NULL. images_json is owned by
 * caller (json_free). text is owned (free). */
bool prompt_stash_pop(prompt_stash_t *ps, int index,
                      char **out_text, json_t **out_images);
/* peek(index) -> borrowed entry or NULL. */
const stash_entry_t *prompt_stash_peek(const prompt_stash_t *ps, int index);
void prompt_stash_clear(prompt_stash_t *ps);

/* -- panel state -- */
bool prompt_stash_open_panel(prompt_stash_t *ps);
void prompt_stash_close_panel(prompt_stash_t *ps);
int  prompt_stash_move_cursor(prompt_stash_t *ps, int delta);
bool prompt_stash_delete_at_cursor(prompt_stash_t *ps);
/* restore_at_cursor -> malloc'd (text, images_json) or NULL. */
bool prompt_stash_restore_at_cursor(prompt_stash_t *ps,
                                    char **out_text, json_t **out_images);

/* -- gesture -- */
/* resolve_ctrl_s(stash, buffer_text, images).
 * action is set to one of CTRL_S_*. If ACTION_RESTORED, out_text/out_images
 * are malloc'd (text owned, images_json owned). Otherwise they are NULL.
 * `images` may be NULL. Returns the action. */
ctrl_s_action_t prompt_stash_resolve_ctrl_s(prompt_stash_t *ps,
                                            const char *buffer_text,
                                            json_t *images,
                                            char **out_text,
                                            json_t **out_images);

#ifdef __cplusplus
}
#endif

#endif /* PORT_PROMPT_STASH_H */
