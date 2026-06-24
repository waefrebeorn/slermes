/*
 * chat_composer.h — Message input with autocomplete for C11 desktop app
 *
 * Provides text input buffer, cursor management, autocomplete suggestions,
 * slash command detection/execution, file attachments, and input history.
 *
 * PoP: composer_create       @ apps/desktop/src/app/chat/composer/index.tsx
 * PoP: composer_input        @ apps/desktop/src/app/chat/composer/index.tsx
 * PoP: composer_submit       @ apps/desktop/src/app/chat/composer/index.tsx
 * PoP: composer_autocomplete @ apps/desktop/src/app/chat/composer/index.tsx
 * PoP: composer_slash_cmd    @ apps/desktop/src/app/chat/composer/index.tsx
 * PoP: composer_attach       @ apps/desktop/src/app/chat/composer/index.tsx
 */

#ifndef CHAT_COMPOSER_H
#define CHAT_COMPOSER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define COMPOSER_MAX_TEXT   65536
#define COMPOSER_MAX_SUGGEST 32
#define COMPOSER_MAX_SLASH  64
#define COMPOSER_MAX_HISTORY 256

/* ── Slash command result ───────────────────────────────────────────────── */
typedef enum {
    SLASH_RESULT_OK = 0,       /* Command executed successfully */
    SLASH_RESULT_ERROR,        /* Execution failed */
    SLASH_RESULT_CLEAR,        /* Clear conversation */
    SLASH_RESULT_NEW_SESSION,  /* Start new session */
    SLASH_RESULT_REDIRECT      /* Redirect to another screen */
} slash_result_t;

/* ── Attachment types ───────────────────────────────────────────────────── */
#define COMPOSER_MAX_ATTACHMENTS 16
#define COMPOSER_MAX_ATTACH_PATH 4096
#define COMPOSER_MAX_ATTACH_MIME 64
#define COMPOSER_MAX_ATTACH_DATA (10 * 1024 * 1024)  /* 10MB max */

typedef enum {
    ATTACH_FILE = 0,       /* File path attachment */
    ATTACH_IMAGE,          /* Image (PNG/JPEG) */
    ATTACH_TEXT,           /* Text snippet */
} attachment_type_t;

typedef struct {
    attachment_type_t type;
    char path[COMPOSER_MAX_ATTACH_PATH];    /* File path (for file/image) */
    char mime[COMPOSER_MAX_ATTACH_MIME];    /* MIME type */
    char *data;            /* Inline data (for text snippets or loaded images) */
    size_t data_size;      /* Size of data in bytes */
    char alt_text[512];    /* Alt text / description */
} composer_attachment_t;

/* ── Slash command types ────────────────────────────────────────────────── */
typedef enum {
    SLASH_NONE = 0,
    SLASH_HELP,
    SLASH_CLEAR,
    SLASH_MODEL,
    SLASH_SETTINGS,
    SLASH_NEW,
    SLASH_UNDO,
    SLASH_REDO,
    SLASH_COPY,
    SLASH_PASTE,
    SLASH_SEARCH,
    SLASH_UNKNOWN,
} slash_cmd_t;

/* ── Autocomplete suggestion ────────────────────────────────────────────── */
typedef struct {
    char text[256];
    char description[512];
    int  score;          /* relevance score, higher = better */
} composer_suggestion_t;

/* ── Composer state ─────────────────────────────────────────────────────── */
typedef struct composer_t {
    char   text[COMPOSER_MAX_TEXT];
    int    length;
    int    cursor_pos;       /* byte offset into text */
    int    selection_start;  /* -1 if no selection */
    int    selection_end;

    /* Autocomplete */
    composer_suggestion_t suggestions[COMPOSER_MAX_SUGGEST];
    int    suggestion_count;
    int    suggestion_selected; /* -1 if none selected */
    bool   autocomplete_visible;

    /* Slash command */
    slash_cmd_t pending_cmd;
    char        cmd_arg[COMPOSER_MAX_TEXT];

    /* Input history */
    char   *history[COMPOSER_MAX_HISTORY];
    int     history_count;
    int     history_pos;     /* -1 = current (not navigating) */

    /* Attachments */
    composer_attachment_t attachments[COMPOSER_MAX_ATTACHMENTS];
    int                   attachment_count;

    /* State flags */
    bool   focused;
    bool   multiline;
    bool   submitting;
} composer_t;

/* ── Slash command handler context ──────────────────────────────────────── */
typedef struct slash_context_t {
    composer_t *composer;      /* Active composer */
    void       *app_ctx;       /* Application context (desktop_app_state_t*) */
    char        result_msg[1024]; /* Result message for user display */
} slash_context_t;

/* ── Slash command entry ────────────────────────────────────────────────── */
typedef struct {
    slash_cmd_t      cmd;
    const char      *name;
    const char      *description;
    const char      *usage;
    slash_result_t (*handler)(slash_context_t *ctx, const char *arg);
} slash_cmd_handler_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

/* PoP: composer_create @ apps/desktop/src/app/chat/composer/index.tsx */
composer_t *composer_create(void);
void composer_dispose(composer_t *c);

/* ── Text editing ────────────────────────────────────────────────────────── */

/* PoP: composer_input @ apps/desktop/src/app/chat/composer/index.tsx */
/* Insert text at cursor position. */
void composer_insert(composer_t *c, const char *text);

/* Delete character before cursor (backspace). Returns true if deleted. */
bool composer_backspace(composer_t *c);

/* Delete character after cursor (delete key). Returns true if deleted. */
bool composer_delete(composer_t *c);

/* Move cursor. delta: positive = right, negative = left. */
void composer_move_cursor(composer_t *c, int delta);

/* Move cursor to position. */
void composer_set_cursor(composer_t *c, int pos);

/* Select all text. */
void composer_select_all(composer_t *c);

/* Clear selection. */
void composer_clear_selection(composer_t *c);

/* Get selected text (caller must free). Returns NULL if no selection. */
char *composer_get_selection(const composer_t *c);

/* Delete selected text. */
void composer_delete_selection(composer_t *c);

/* Clear all text. */
void composer_clear(composer_t *c);

/* Get the current text. */
const char *composer_get_text(const composer_t *c);
int composer_get_length(const composer_t *c);

/* ── Submission ──────────────────────────────────────────────────────────── */

/* PoP: composer_submit @ apps/desktop/src/app/chat/composer/index.tsx */
/* Get text and clear composer. Returns text (caller must free). */
char *composer_submit(composer_t *c);

/* ── Autocomplete ────────────────────────────────────────────────────────── */

/* PoP: composer_autocomplete @ apps/desktop/src/app/chat/composer/index.tsx */
/* Update autocomplete suggestions based on current text. */
void composer_update_suggestions(composer_t *c);

/* Get current suggestions. */
int composer_get_suggestions(const composer_t *c,
                             const composer_suggestion_t **out,
                             int max_count);

/* Select next/previous suggestion. */
void composer_suggestion_next(composer_t *c);
void composer_suggestion_prev(composer_t *c);

/* Apply the currently selected suggestion. Returns true if applied. */
bool composer_apply_suggestion(composer_t *c);

/* Hide autocomplete. */
void composer_hide_autocomplete(composer_t *c);

/* ── Slash commands ──────────────────────────────────────────────────────── */

/* PoP: composer_slash_cmd @ apps/desktop/src/app/chat/composer/index.tsx */
/* Detect slash command at current text. Returns SLASH_NONE if not a command. */
slash_cmd_t composer_detect_slash(const composer_t *c);

/* Get the argument part of a slash command (text after the command). */
const char *composer_slash_arg(const composer_t *c);

/* Built-in slash command names. */
const char *slash_cmd_name(slash_cmd_t cmd);
slash_cmd_t slash_cmd_from_string(const char *name);

/* ── Attachments ─────────────────────────────────────────────────────────── */

/* PoP: composer_attach_file @ apps/desktop/src/app/chat/composer/index.tsx */
/* Attach a file by path. Returns attachment index, or -1 on failure. */
int composer_attach_file(composer_t *c, const char *path, const char *mime);

/* PoP: composer_attach_image @ apps/desktop/src/app/chat/composer/index.tsx */
/* Attach an image from file path (PNG/JPEG). Returns index, or -1 on failure. */
int composer_attach_image(composer_t *c, const char *image_path, const char *alt_text);

/* PoP: composer_attach_text @ apps/desktop/src/app/chat/composer/index.tsx */
/* Attach a text snippet. Returns index, or -1 on failure. */
int composer_attach_text(composer_t *c, const char *text, const char *alt_text);

/* PoP: composer_get_attachments @ apps/desktop/src/app/chat/composer/index.tsx */
/* Get current attachment count. */
int composer_get_attachments(const composer_t *c);

/* PoP: composer_get_attachment @ apps/desktop/src/app/chat/composer/index.tsx */
/* Get attachment at index. Returns NULL if out of bounds. */
const composer_attachment_t *composer_get_attachment(const composer_t *c, int idx);

/* PoP: composer_clear_attachments @ apps/desktop/src/app/chat/composer/index.tsx */
/* Remove all attachments and free their data. */
void composer_clear_attachments(composer_t *c);

/* PoP: composer_remove_attachment @ apps/desktop/src/app/chat/composer/index.tsx */
/* Remove attachment at index. */
bool composer_remove_attachment(composer_t *c, int idx);

/* ── Slash command execution ─────────────────────────────────────────────── */

/* PoP: composer_execute_slash @ apps/desktop/src/app/chat/composer/index.tsx */
/* Execute a slash command. Returns result code. */
slash_result_t composer_execute_slash(composer_t *c, slash_context_t *ctx);

/* PoP: composer_submit_with_slash @ apps/desktop/src/app/chat/composer/index.tsx */
/* Submit text — if it starts with "/", execute as slash command. */
slash_result_t composer_submit_with_slash(composer_t *c, slash_context_t *ctx,
                                          char **out_text);

/* ── History ─────────────────────────────────────────────────────────────── */

/* Push current text to history (before submission). */
void composer_history_push(composer_t *c);

/* Navigate history. Returns true if history entry loaded. */
bool composer_history_prev(composer_t *c);
bool composer_history_next(composer_t *c);

#ifdef __cplusplus
}
#endif

#endif /* CHAT_COMPOSER_H */
