/*
 * chat_composer.c — Message input with autocomplete for C11 desktop app
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

#include "chat_composer.h"
#include "hermes_core_types.h"
#include "clipboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Built-in slash commands ────────────────────────────────────────────── */

typedef struct {
    slash_cmd_t cmd;
    const char *name;
    const char *description;
} slash_cmd_entry_t;

static const slash_cmd_entry_t slash_commands[] = {
    { SLASH_HELP,     "help",     "Show available commands" },
    { SLASH_CLEAR,    "clear",    "Clear the current conversation" },
    { SLASH_MODEL,    "model",    "Switch or list models" },
    { SLASH_SETTINGS, "settings", "Open settings" },
    { SLASH_NEW,      "new",      "Start a new conversation" },
    { SLASH_UNDO,     "undo",     "Undo last action" },
    { SLASH_REDO,     "redo",     "Redo last undone action" },
    { SLASH_COPY,     "copy",     "Copy last response to clipboard" },
    { SLASH_PASTE,    "paste",    "Paste from clipboard" },
    { SLASH_SEARCH,   "search",   "Search conversation history" },
    { SLASH_NONE, NULL, NULL }
};

/* ── Autocomplete: built-in suggestions ─────────────────────────────────── */

typedef struct {
    const char *trigger;  /* prefix that triggers this suggestion */
    const char *text;     /* suggestion text */
    const char *desc;     /* description */
} builtin_suggestion_t;

static const builtin_suggestion_t builtin_suggestions[] = {
    { "/",          "/help",        "Show available commands" },
    { "/",          "/clear",       "Clear conversation" },
    { "/",          "/model",       "Switch model" },
    { "/",          "/new",         "New conversation" },
    { "/",          "/settings",    "Open settings" },
    { "/",          "/search",      "Search history" },
    { "help",       "/help",        "Show available commands" },
    { "clear",      "/clear",       "Clear conversation" },
    { "model",      "/model",       "Switch model" },
    { "new",        "/new",         "New conversation" },
    { NULL, NULL, NULL }
};

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

/* PoP: composer_create @ apps/desktop/src/app/chat/composer/index.tsx */
composer_t *composer_create(void) {
    composer_t *c = calloc(1, sizeof(composer_t));
    if (!c) {
        fprintf(stderr, "composer_create: calloc failed");
        return NULL;
    }

    c->text[0] = '\0';
    c->length = 0;
    c->cursor_pos = 0;
    c->selection_start = -1;
    c->selection_end = -1;
    c->suggestion_count = 0;
    c->suggestion_selected = -1;
    c->autocomplete_visible = false;
    c->pending_cmd = SLASH_NONE;
    c->cmd_arg[0] = '\0';
    c->history_count = 0;
    c->history_pos = -1;
    c->attachment_count = 0;
    c->focused = false;
    c->multiline = false;
    c->submitting = false;

    return c;
}

void composer_dispose(composer_t *c) {
    if (!c) return;
    /* Free attachment data */
    composer_clear_attachments(c);
    for (int i = 0; i < c->history_count; i++)
        free(c->history[i]);
    free(c);
}

/* ── Text editing ────────────────────────────────────────────────────────── */

/* PoP: composer_input @ apps/desktop/src/app/chat/composer/index.tsx */
void composer_insert(composer_t *c, const char *text) {
    if (!c || !text || !*text) return;

    /* Delete selection first */
    if (c->selection_start >= 0 && c->selection_end > c->selection_start) {
        composer_delete_selection(c);
    }

    size_t insert_len = strlen(text);
    if (c->length + insert_len >= COMPOSER_MAX_TEXT) {
        insert_len = COMPOSER_MAX_TEXT - c->length - 1;
    }
    if (insert_len == 0) return;

    /* Shift text after cursor */
    memmove(c->text + c->cursor_pos + insert_len,
            c->text + c->cursor_pos,
            c->length - c->cursor_pos + 1);
    memcpy(c->text + c->cursor_pos, text, insert_len);
    c->length += (int)insert_len;
    c->cursor_pos += (int)insert_len;
}

bool composer_backspace(composer_t *c) {
    if (!c || c->cursor_pos <= 0) return false;

    if (c->selection_start >= 0 && c->selection_end > c->selection_start) {
        composer_delete_selection(c);
        return true;
    }

    memmove(c->text + c->cursor_pos - 1,
            c->text + c->cursor_pos,
            c->length - c->cursor_pos + 1);
    c->cursor_pos--;
    c->length--;
    return true;
}

bool composer_delete(composer_t *c) {
    if (!c || c->cursor_pos >= c->length) return false;

    memmove(c->text + c->cursor_pos,
            c->text + c->cursor_pos + 1,
            c->length - c->cursor_pos);
    c->length--;
    return true;
}

/* PoP: move_cursor @ tools/computer_use/cua_backend.py:move_cursor */
void composer_move_cursor(composer_t *c, int delta) {
    if (!c) return;
    int new_pos = c->cursor_pos + delta;
    if (new_pos < 0) new_pos = 0;
    if (new_pos > c->length) new_pos = c->length;
    c->cursor_pos = new_pos;
}

void composer_set_cursor(composer_t *c, int pos) {
    if (!c) return;
    if (pos < 0) pos = 0;
    if (pos > c->length) pos = c->length;
    c->cursor_pos = pos;
}

void composer_select_all(composer_t *c) {
    if (!c) return;
    c->selection_start = 0;
    c->selection_end = c->length;
}

void composer_clear_selection(composer_t *c) {
    if (!c) return;
    c->selection_start = -1;
    c->selection_end = -1;
}

char *composer_get_selection(const composer_t *c) {
    if (!c || c->selection_start < 0 || c->selection_end <= c->selection_start)
        return NULL;
    int len = c->selection_end - c->selection_start;
    char *sel = malloc(len + 1);
    strncpy(sel, c->text + c->selection_start, len);
    sel[len] = '\0';
    return sel;
}

void composer_delete_selection(composer_t *c) {
    if (!c || c->selection_start < 0 || c->selection_end <= c->selection_start)
        return;
    int len = c->selection_end - c->selection_start;
    memmove(c->text + c->selection_start,
            c->text + c->selection_end,
            c->length - c->selection_end + 1);
    c->length -= len;
    c->cursor_pos = c->selection_start;
    c->selection_start = -1;
    c->selection_end = -1;
}

void composer_clear(composer_t *c) {
    if (!c) return;
    c->text[0] = '\0';
    c->length = 0;
    c->cursor_pos = 0;
    c->selection_start = -1;
    c->selection_end = -1;
    c->pending_cmd = SLASH_NONE;
    c->cmd_arg[0] = '\0';
}

const char *composer_get_text(const composer_t *c) {
    return c ? c->text : "";
}

int composer_get_length(const composer_t *c) {
    return c ? c->length : 0;
}

/* ── Submission ──────────────────────────────────────────────────────────── */

/* PoP: composer_submit @ apps/desktop/src/app/chat/composer/index.tsx */
char *composer_submit(composer_t *c) {
    if (!c || c->length == 0) return NULL;

    char *text = malloc(c->length + 1);
    strcpy(text, c->text);

    /* Push to history */
    composer_history_push(c);

    /* Clear composer */
    composer_clear(c);

    return text;
}

/* ── Autocomplete ────────────────────────────────────────────────────────── */

/* PoP: composer_autocomplete @ apps/desktop/src/app/chat/composer/index.tsx */
void composer_update_suggestions(composer_t *c) {
    if (!c) return;

    c->suggestion_count = 0;
    c->suggestion_selected = -1;
    c->autocomplete_visible = false;

    /* Get word at cursor */
    int word_start = c->cursor_pos;
    while (word_start > 0 && !isspace((unsigned char)c->text[word_start - 1]))
        word_start--;
    int word_len = c->cursor_pos - word_start;
    if (word_len <= 0) return;

    char word[256];
    if (word_len >= (int)sizeof(word)) word_len = (int)sizeof(word) - 1;
    strncpy(word, c->text + word_start, word_len);
    word[word_len] = '\0';

    /* Match against built-in suggestions */
    for (int i = 0; builtin_suggestions[i].trigger && c->suggestion_count < COMPOSER_MAX_SUGGEST; i++) {
        if (strncmp(word, builtin_suggestions[i].trigger, strlen(builtin_suggestions[i].trigger)) == 0 ||
            strncmp(word, builtin_suggestions[i].text, word_len) == 0) {
            composer_suggestion_t *s = &c->suggestions[c->suggestion_count++];
            strncpy(s->text, builtin_suggestions[i].text, sizeof(s->text) - 1);
            strncpy(s->description, builtin_suggestions[i].desc, sizeof(s->description) - 1);
            s->score = 100 - c->suggestion_count;
        }
    }

    if (c->suggestion_count > 0)
        c->autocomplete_visible = true;
}

int composer_get_suggestions(const composer_t *c,
                             const composer_suggestion_t **out,
                             int max_count) {
    if (!c || !out || max_count <= 0) return 0;
    int count = c->suggestion_count < max_count ? c->suggestion_count : max_count;
    *out = c->suggestions;
    return count;
}

void composer_suggestion_next(composer_t *c) {
    if (!c || c->suggestion_count <= 0) return;
    c->suggestion_selected++;
    if (c->suggestion_selected >= c->suggestion_count)
        c->suggestion_selected = 0;
}

void composer_suggestion_prev(composer_t *c) {
    if (!c || c->suggestion_count <= 0) return;
    c->suggestion_selected--;
    if (c->suggestion_selected < 0)
        c->suggestion_selected = c->suggestion_count - 1;
}

bool composer_apply_suggestion(composer_t *c) {
    if (!c || c->suggestion_selected < 0 || c->suggestion_selected >= c->suggestion_count)
        return false;

    /* Replace word at cursor with suggestion */
    int word_start = c->cursor_pos;
    while (word_start > 0 && !isspace((unsigned char)c->text[word_start - 1]))
        word_start--;

    /* Delete current word */
    int word_len = c->cursor_pos - word_start;
    if (word_len > 0) {
        memmove(c->text + word_start, c->text + c->cursor_pos, c->length - c->cursor_pos + 1);
        c->length -= word_len;
        c->cursor_pos = word_start;
    }

    /* Insert suggestion */
    composer_insert(c, c->suggestions[c->suggestion_selected].text);
    c->autocomplete_visible = false;
    c->suggestion_selected = -1;
    return true;
}

void composer_hide_autocomplete(composer_t *c) {
    if (c) {
        c->autocomplete_visible = false;
        c->suggestion_selected = -1;
    }
}

/* ── Slash commands ──────────────────────────────────────────────────────── */

/* PoP: composer_slash_cmd @ apps/desktop/src/app/chat/composer/index.tsx */
slash_cmd_t composer_detect_slash(const composer_t *c) {
    if (!c || c->length <= 0 || c->text[0] != '/') return SLASH_NONE;

    /* Extract command name */
    char cmd[64];
    int i = 1;
    while (i < (int)sizeof(cmd) - 1 && i < c->length && !isspace((unsigned char)c->text[i])) {
        cmd[i - 1] = c->text[i];
        i++;
    }
    cmd[i - 1] = '\0';

    return slash_cmd_from_string(cmd);
}

const char *composer_slash_arg(const composer_t *c) {
    if (!c || c->length <= 0 || c->text[0] != '/') return NULL;

    int i = 1;
    while (i < c->length && !isspace((unsigned char)c->text[i])) i++;
    while (i < c->length && isspace((unsigned char)c->text[i])) i++;

    if (i >= c->length) return NULL;
    return c->text + i;
}

const char *slash_cmd_name(slash_cmd_t cmd) {
    for (int i = 0; slash_commands[i].name; i++) {
        if (slash_commands[i].cmd == cmd) return slash_commands[i].name;
    }
    return NULL;
}

slash_cmd_t slash_cmd_from_string(const char *name) {
    if (!name) return SLASH_NONE;
    for (int i = 0; slash_commands[i].name; i++) {
        if (strcasecmp(name, slash_commands[i].name) == 0)
            return slash_commands[i].cmd;
    }
    return SLASH_UNKNOWN;
}

/* ── Attachments ─────────────────────────────────────────────────────────── */

/* PoP: composer_attach_file @ apps/desktop/src/app/chat/composer/index.tsx */
int composer_attach_file(composer_t *c, const char *path, const char *mime) {
    if (!c || !path || c->attachment_count >= COMPOSER_MAX_ATTACHMENTS)
        return -1;

    /* Validate path length */
    if (strlen(path) >= COMPOSER_MAX_ATTACH_PATH) {
        fprintf(stderr, "composer_attach_file: path too long (%s)", path);
        return -1;
    }

    /* Check file size — reject if too large */
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "composer_attach_file: cannot open %s", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fclose(f);
    if (fsize > COMPOSER_MAX_ATTACH_DATA) {
        fprintf(stderr, "composer_attach_file: file too large (%ld bytes)", fsize);
        return -1;
    }

    composer_attachment_t *att = &c->attachments[c->attachment_count];
    att->type = ATTACH_FILE;
    strncpy(att->path, path, COMPOSER_MAX_ATTACH_PATH - 1);
    if (mime) {
        strncpy(att->mime, mime, COMPOSER_MAX_ATTACH_MIME - 1);
    } else {
        strncpy(att->mime, "application/octet-stream", COMPOSER_MAX_ATTACH_MIME - 1);
    }
    att->data = NULL;
    att->data_size = 0;
    /* Use filename as alt text */
    const char *basename = strrchr(path, '/');
    if (!basename) basename = path;
    else basename++;
    strncpy(att->alt_text, basename, sizeof(att->alt_text) - 1);

    return c->attachment_count++;
}

/* PoP: composer_attach_image @ apps/desktop/src/app/chat/composer/index.tsx */
int composer_attach_image(composer_t *c, const char *image_path, const char *alt_text) {
    if (!c || !image_path || c->attachment_count >= COMPOSER_MAX_ATTACHMENTS)
        return -1;

    if (strlen(image_path) >= COMPOSER_MAX_ATTACH_PATH) {
        fprintf(stderr, "composer_attach_image: path too long");
        return -1;
    }

    /* Verify it's a supported image format by extension */
    const char *ext = strrchr(image_path, '.');
    if (!ext || (strcasecmp(ext, ".png") != 0 && strcasecmp(ext, ".jpg") != 0 &&
                 strcasecmp(ext, ".jpeg") != 0 && strcasecmp(ext, ".webp") != 0 &&
                 strcasecmp(ext, ".gif") != 0)) {
        fprintf(stderr, "composer_attach_image: unsupported format (%s)", ext ? ext : "(none)");
        return -1;
    }

    composer_attachment_t *att = &c->attachments[c->attachment_count];
    att->type = ATTACH_IMAGE;
    strncpy(att->path, image_path, COMPOSER_MAX_ATTACH_PATH - 1);
    /* Detect MIME from extension */
    if (strcasecmp(ext, ".png") == 0)
        strncpy(att->mime, "image/png", COMPOSER_MAX_ATTACH_MIME - 1);
    else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        strncpy(att->mime, "image/jpeg", COMPOSER_MAX_ATTACH_MIME - 1);
    else if (strcasecmp(ext, ".webp") == 0)
        strncpy(att->mime, "image/webp", COMPOSER_MAX_ATTACH_MIME - 1);
    else if (strcasecmp(ext, ".gif") == 0)
        strncpy(att->mime, "image/gif", COMPOSER_MAX_ATTACH_MIME - 1);
    att->data = NULL;
    att->data_size = 0;
    if (alt_text) {
        strncpy(att->alt_text, alt_text, sizeof(att->alt_text) - 1);
    } else {
        strncpy(att->alt_text, "image", sizeof(att->alt_text) - 1);
    }

    return c->attachment_count++;
}

/* PoP: composer_attach_text @ apps/desktop/src/app/chat/composer/index.tsx */
int composer_attach_text(composer_t *c, const char *text, const char *alt_text) {
    if (!c || !text || c->attachment_count >= COMPOSER_MAX_ATTACHMENTS)
        return -1;

    size_t text_len = strlen(text);
    if (text_len > (size_t)COMPOSER_MAX_ATTACH_DATA) {
        fprintf(stderr, "composer_attach_text: text too long (%zu)", text_len);
        return -1;
    }

    composer_attachment_t *att = &c->attachments[c->attachment_count];
    att->type = ATTACH_TEXT;
    att->path[0] = '\0';
    strncpy(att->mime, "text/plain", COMPOSER_MAX_ATTACH_MIME - 1);
    att->data = malloc(text_len + 1);
    if (!att->data) {
        fprintf(stderr, "composer_attach_text: malloc failed");
        return -1;
    }
    memcpy(att->data, text, text_len);
    att->data[text_len] = '\0';
    att->data_size = text_len;
    if (alt_text) {
        strncpy(att->alt_text, alt_text, sizeof(att->alt_text) - 1);
    } else {
        /* Truncate for description */
        size_t alen = text_len < 64 ? text_len : 64;
        strncpy(att->alt_text, text, alen);
        att->alt_text[alen] = '\0';
        if (alen < text_len) strcat(att->alt_text, "...");
    }

    return c->attachment_count++;
}

/* PoP: composer_get_attachments @ apps/desktop/src/app/chat/composer/index.tsx */
int composer_get_attachments(const composer_t *c) {
    return c ? c->attachment_count : 0;
}

/* PoP: composer_get_attachment @ apps/desktop/src/app/chat/composer/index.tsx */
const composer_attachment_t *composer_get_attachment(const composer_t *c, int idx) {
    if (!c || idx < 0 || idx >= c->attachment_count) return NULL;
    return &c->attachments[idx];
}

/* PoP: composer_clear_attachments @ apps/desktop/src/app/chat/composer/index.tsx */
void composer_clear_attachments(composer_t *c) {
    if (!c) return;
    for (int i = 0; i < c->attachment_count; i++) {
        free(c->attachments[i].data);
        c->attachments[i].data = NULL;
    }
    c->attachment_count = 0;
}

/* PoP: composer_remove_attachment @ apps/desktop/src/app/chat/composer/index.tsx */
bool composer_remove_attachment(composer_t *c, int idx) {
    if (!c || idx < 0 || idx >= c->attachment_count) return false;

    free(c->attachments[idx].data);
    /* Shift remaining attachments */
    for (int i = idx; i < c->attachment_count - 1; i++) {
        c->attachments[i] = c->attachments[i + 1];
    }
    c->attachment_count--;
    return true;
}

/* ── Slash command execution ─────────────────────────────────────────────── */

static slash_result_t slash_help_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx) return SLASH_RESULT_ERROR;

    /* Build help text */
    int pos = 0;
    pos += snprintf(ctx->result_msg + pos, sizeof(ctx->result_msg) - pos,
                    "**Available Commands:**\n\n");
    for (int i = 0; slash_commands[i].name; i++) {
        pos += snprintf(ctx->result_msg + pos, sizeof(ctx->result_msg) - pos,
                        "- `/%s` — %s\n",
                        slash_commands[i].name,
                        slash_commands[i].description);
    }
    pos += snprintf(ctx->result_msg + pos, sizeof(ctx->result_msg) - pos,
                    "\nType `/<command> <args>` to execute.");
    return SLASH_RESULT_OK;
}

static slash_result_t slash_clear_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx || !ctx->composer) return SLASH_RESULT_ERROR;
    snprintf(ctx->result_msg, sizeof(ctx->result_msg),
             "Conversation cleared.");
    return SLASH_RESULT_CLEAR;
}

static slash_result_t slash_model_handler(slash_context_t *ctx, const char *arg) {
    if (!ctx) return SLASH_RESULT_ERROR;
    if (arg && *arg) {
        snprintf(ctx->result_msg, sizeof(ctx->result_msg),
                 "Model switched to: %s", arg);
        return SLASH_RESULT_OK;
    }
    snprintf(ctx->result_msg, sizeof(ctx->result_msg),
             "Use `/model <model-id>` to switch. Current model shown in status bar.");
    return SLASH_RESULT_OK;
}

static slash_result_t slash_settings_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx) return SLASH_RESULT_ERROR;
    snprintf(ctx->result_msg, sizeof(ctx->result_msg),
             "Settings panel opened.");
    return SLASH_RESULT_REDIRECT;
}

static slash_result_t slash_new_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx) return SLASH_RESULT_ERROR;
    snprintf(ctx->result_msg, sizeof(ctx->result_msg),
             "New conversation started.");
    return SLASH_RESULT_NEW_SESSION;
}

static slash_result_t slash_undo_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx || !ctx->composer) return SLASH_RESULT_ERROR;
    composer_history_prev(ctx->composer);
    snprintf(ctx->result_msg, sizeof(ctx->result_msg), "Undone.");
    return SLASH_RESULT_OK;
}

static slash_result_t slash_redo_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx || !ctx->composer) return SLASH_RESULT_ERROR;
    composer_history_next(ctx->composer);
    snprintf(ctx->result_msg, sizeof(ctx->result_msg), "Redone.");
    return SLASH_RESULT_OK;
}

static slash_result_t slash_copy_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx || !ctx->composer) return SLASH_RESULT_ERROR;
    /* Copy the composer text (or last response) to the system clipboard. */
    const char *text = composer_get_text(ctx->composer);
    if (text && text[0]) {
        if (clipboard_write_text(text)) {
            snprintf(ctx->result_msg, sizeof(ctx->result_msg),
                     "Copied to clipboard.");
            return SLASH_RESULT_OK;
        }
        snprintf(ctx->result_msg, sizeof(ctx->result_msg),
                 "Copy failed — no clipboard tool.");
        return SLASH_RESULT_OK;
    }
    snprintf(ctx->result_msg, sizeof(ctx->result_msg),
             "Nothing to copy — composer is empty.");
    return SLASH_RESULT_OK;
}

static slash_result_t slash_paste_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx || !ctx->composer) return SLASH_RESULT_ERROR;
    /* Read the system clipboard and insert it at the cursor. */
    char *text = clipboard_read_text();
    if (text && text[0]) {
        composer_insert(ctx->composer, text);
        free(text);
        snprintf(ctx->result_msg, sizeof(ctx->result_msg),
                 "Pasted from clipboard.");
        return SLASH_RESULT_OK;
    }
    free(text);
    snprintf(ctx->result_msg, sizeof(ctx->result_msg),
             "Clipboard is empty or unavailable.");
    return SLASH_RESULT_OK;
}

static slash_result_t slash_search_handler(slash_context_t *ctx, const char *arg) {
    if (!ctx) return SLASH_RESULT_ERROR;
    if (arg && *arg) {
        snprintf(ctx->result_msg, sizeof(ctx->result_msg),
                 "Searching for: %s ...", arg);
    } else {
        snprintf(ctx->result_msg, sizeof(ctx->result_msg),
                 "Use `/search <query>` to search conversation history.");
    }
    return SLASH_RESULT_OK;
}

static slash_result_t slash_profile_handler(slash_context_t *ctx, const char *arg) {
    if (!ctx) return SLASH_RESULT_ERROR;
    if (arg && *arg) {
        snprintf(ctx->result_msg, sizeof(ctx->result_msg),
                 "Switched to profile: %s", arg);
    } else {
        snprintf(ctx->result_msg, sizeof(ctx->result_msg),
                 "Use `/profile <name>` to switch profile.");
    }
    return SLASH_RESULT_OK;
}

static slash_result_t slash_archive_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx) return SLASH_RESULT_ERROR;
    snprintf(ctx->result_msg, sizeof(ctx->result_msg),
             "Session archived.");
    return SLASH_RESULT_OK;
}

static slash_result_t slash_pin_handler(slash_context_t *ctx, const char *arg) {
    (void)arg;
    if (!ctx) return SLASH_RESULT_ERROR;
    snprintf(ctx->result_msg, sizeof(ctx->result_msg),
             "Session pinned.");
    return SLASH_RESULT_OK;
}

/* Command handler table */
static const slash_cmd_handler_t slash_handlers[] = {
    { SLASH_HELP,     "help",     "Show available commands",  "/help",         slash_help_handler },
    { SLASH_CLEAR,    "clear",    "Clear conversation",        "/clear",        slash_clear_handler },
    { SLASH_MODEL,    "model",    "Switch model",             "/model <id>",   slash_model_handler },
    { SLASH_SETTINGS, "settings", "Open settings",            "/settings",     slash_settings_handler },
    { SLASH_NEW,      "new",      "New conversation",          "/new",          slash_new_handler },
    { SLASH_UNDO,     "undo",     "Undo last action",          "/undo",         slash_undo_handler },
    { SLASH_REDO,     "redo",     "Redo last undone action",   "/redo",         slash_redo_handler },
    { SLASH_COPY,     "copy",     "Copy last response",        "/copy",         slash_copy_handler },
    { SLASH_PASTE,    "paste",    "Paste from clipboard",      "/paste",        slash_paste_handler },
    { SLASH_SEARCH,   "search",   "Search history",            "/search <q>",   slash_search_handler },
    { SLASH_MODEL,    "profile",  "Switch profile",            "/profile <n>",  slash_profile_handler },
    { SLASH_NONE,     "archive",  "Archive session",           "/archive",      slash_archive_handler },
    { SLASH_NONE,     "pin",      "Pin session",               "/pin",          slash_pin_handler },
    { SLASH_NONE, NULL, NULL, NULL, NULL }
};

/* PoP: composer_execute_slash @ apps/desktop/src/app/chat/composer/index.tsx */
slash_result_t composer_execute_slash(composer_t *c, slash_context_t *ctx) {
    if (!c || c->length <= 0 || c->text[0] != '/') return SLASH_RESULT_ERROR;

    /* Extract command name */
    char cmd_name[64];
    int i = 1;
    while (i < (int)sizeof(cmd_name) - 1 && i < c->length &&
           !isspace((unsigned char)c->text[i])) {
        cmd_name[i - 1] = c->text[i];
        i++;
    }
    cmd_name[i - 1] = '\0';

    /* Extract argument */
    const char *arg = c->text + i;
    while (*arg == ' ') arg++;
    if (*arg == '\0') arg = NULL;

    /* Look up handler */
    for (int j = 0; slash_handlers[j].name; j++) {
        if (strcasecmp(cmd_name, slash_handlers[j].name) == 0) {
            slash_context_t local_ctx;
            if (ctx) {
                local_ctx = *ctx;
            } else {
                memset(&local_ctx, 0, sizeof(local_ctx));
            }
            local_ctx.composer = c;
            return slash_handlers[j].handler(&local_ctx, arg);
        }
    }

    fprintf(stderr, "composer_execute_slash: unknown command /%s", cmd_name);
    return SLASH_RESULT_ERROR;
}

/* PoP: composer_submit_with_slash @ apps/desktop/src/app/chat/composer/index.tsx */
slash_result_t composer_submit_with_slash(composer_t *c, slash_context_t *ctx,
                                          char **out_text) {
    if (!c || c->length == 0) {
        if (out_text) *out_text = NULL;
        return SLASH_RESULT_OK;
    }

    /* Check if it's a slash command */
    if (c->text[0] == '/') {
        slash_result_t result = composer_execute_slash(c, ctx);
        /* Clear the composer after executing a slash command */
        composer_clear(c);
        if (out_text) *out_text = NULL;
        return result;
    }

    /* Regular message submission */
    char *text = malloc(c->length + 1);
    if (!text) return SLASH_RESULT_ERROR;
    strcpy(text, c->text);

    /* Push to history */
    composer_history_push(c);

    /* Clear composer */
    composer_clear(c);

    if (out_text) *out_text = text;
    else free(text);

    return SLASH_RESULT_OK;
}

/* ── History ─────────────────────────────────────────────────────────────── */

void composer_history_push(composer_t *c) {
    if (!c || c->length == 0) return;

    if (c->history_count >= COMPOSER_MAX_HISTORY) {
        free(c->history[0]);
        memmove(c->history, c->history + 1, (COMPOSER_MAX_HISTORY - 1) * sizeof(char *));
        c->history_count--;
    }

    c->history[c->history_count++] = strdup(c->text);
    c->history_pos = -1;
}

bool composer_history_prev(composer_t *c) {
    if (!c || c->history_count == 0) return false;

    if (c->history_pos == -1) {
        /* Save current text */
        c->history_pos = c->history_count - 1;
    } else if (c->history_pos > 0) {
        c->history_pos--;
    } else {
        return false;
    }

    strncpy(c->text, c->history[c->history_pos], COMPOSER_MAX_TEXT - 1);
    c->length = (int)strlen(c->text);
    c->cursor_pos = c->length;
    return true;
}

bool composer_history_next(composer_t *c) {
    if (!c || c->history_pos < 0) return false;

    c->history_pos++;
    if (c->history_pos >= c->history_count) {
        c->history_pos = -1;
        composer_clear(c);
        return true;
    }

    strncpy(c->text, c->history[c->history_pos], COMPOSER_MAX_TEXT - 1);
    c->length = (int)strlen(c->text);
    c->cursor_pos = c->length;
    return true;
}
