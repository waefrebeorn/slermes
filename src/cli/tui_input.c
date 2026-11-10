/*
 * tui_input.c - TUI input-buffer editing and autocomplete helpers.
 *
 * Split from tui_fullscreen.c (soul.md: self-contained modules). Owns the
 * composer buffer editing, emoji/slash completion and the case-insensitive
 * substring scan. No file-scope state of its own.
 */
#include "tui_input.h"
#include "tui_fullscreen.h"
#include "hermes_cli.h"
#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "tui_views.h"
#include "tui_stream.h"
#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "base64.h"
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <wchar.h>
#include <locale.h>
#include "budget_tracker.h"
#include "provider_metadata.h"
#include "hermes_plugin.h"
#include "plugin.h"
#include "tui_eventpub.h"
#include "tui_slash_worker.h"
#include "tui_slash_cmds.h"
#include "chat_render.h"
#include "chat_composer.h"
#include "slermes_home.h"


/* ==================================================================
 *  P190: INPUT — multi-line editing, autocomplete, emoji picker
 * ================================================================== */

/* Initialize input state */ void tui_input_init(void) {
    memset(&tui.input, 0, sizeof(tui.input));
    tui.input.history_pos = -1;
    tui.input.active = true;
}


/* Add line to input history */ void tui_input_history_add(const char *line) {
    if (!line || !*line) return;
    /* Avoid duplicates */
    if (tui.input.history_count > 0 &&
        strcmp(tui.input.history[tui.input.history_count - 1], line) == 0)
        return;

    if (tui.input.history_count >= INPUT_HISTORY_MAX)
        return;

    tui.input.history[tui.input.history_count] = strdup(line);
    tui.input.history_count++;
}


/* Find slash command completions — P190 */
static int tui_find_slash_completions(const char *prefix) {
    int count = 0;
    size_t plen = strlen(prefix);

    for (int i = 0; slash_commands[i].cmd && count < AUTOCOMPLETE_MAX; i++) {
        if (strncmp(slash_commands[i].cmd, prefix, plen) == 0) {
            snprintf(tui.input.autocomplete_matches[count],
                     sizeof(tui.input.autocomplete_matches[0]),
                     "%s  %s", slash_commands[i].cmd, slash_commands[i].desc);
            count++;
        }
    }
    return count;
}


/* Find emoji completions — P190 */
static int tui_find_emoji(const char *prefix) {
    int count = 0;
    size_t plen = strlen(prefix);
    if (plen == 0) return 0;

    for (int i = 0; emoji_list[i][0] && count < EMOJI_MAX; i++) {
        if (strncmp(emoji_list[i][1], prefix, plen) == 0) {
            strncpy(tui.input.emoji_results[count], emoji_list[i][0],
                    sizeof(tui.input.emoji_results[0]) - 1);
            count++;
        }
    }
    return count;
}


/* Delete last word (for Ctrl+W) */ void tui_input_delete_word(void) {
    while (tui.input.pos > 0 && tui.input.buf[tui.input.pos - 1] == ' ')
        tui.input.pos--;
    while (tui.input.pos > 0 && tui.input.buf[tui.input.pos - 1] != ' ')
        tui.input.pos--;
    tui.input.buf[tui.input.pos] = '\0';
    tui.input.len = tui.input.pos;
}


/* Delete to end of line (for Ctrl+K) */ void tui_input_delete_to_end(void) {
    tui.input.buf[tui.input.pos] = '\0';
    tui.input.len = tui.input.pos;
}


/* Perform autocomplete — P190 */ void tui_autocomplete(void) {
    tui.input.autocomplete_active = false;
    tui.input.autocomplete_count = 0;
    tui.input.autocomplete_sel = 0;

    /* Check if we're typing a slash command */
    if (tui.input.buf[0] == '/' && tui.input.pos > 0) {
        char prefix[256];
        strncpy(prefix, tui.input.buf, tui.input.pos);
        prefix[tui.input.pos] = '\0';

        tui.input.autocomplete_count = tui_find_slash_completions(prefix);
        if (tui.input.autocomplete_count > 0) {
            tui.input.autocomplete_active = true;
        }
    }

    /* Check for emoji :name: pattern */
    if (tui.input.len > 0 && tui.input.buf[tui.input.pos - 1] == ':') {
        /* Find start of emoji name */
        int start = tui.input.pos - 1;
        while (start > 0 && tui.input.buf[start] != ':')
            start--;
        if (start < tui.input.pos - 1 && tui.input.buf[start] == ':') {
            char prefix[64];
            int plen = (tui.input.pos - 1) - (start + 1);
            if (plen > 0 && plen < 64) {
                strncpy(prefix, tui.input.buf + start + 1, plen);
                prefix[plen] = '\0';
                tui.input.emoji_count = tui_find_emoji(prefix);
                if (tui.input.emoji_count > 0) {
                    tui.input.emoji_picker_active = true;
                    tui.input.emoji_sel = 0;
                }
            }
        }
    }

    tui_redraw_input();
}


/* Insert a character into the input buffer */ void tui_input_insert_char(char ch) {
    if (tui.input.len >= INPUT_BUF_SIZE - 1) return;

    /* Shift right */
    memmove(tui.input.buf + tui.input.pos + 1,
            tui.input.buf + tui.input.pos,
            tui.input.len - tui.input.pos);
    tui.input.buf[tui.input.pos] = ch;
    tui.input.pos++;
    tui.input.len++;
    tui.input.buf[tui.input.len] = '\0';
}


/* ==================================================================
 *  P195: SESSION BROWSER
 * ================================================================== */

/* Case-insensitive substring match helper */
static bool strstr_lower(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return false;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)needle[0])) {
            if (strncasecmp(haystack, needle, nlen) == 0)
                return true;
        }
        haystack++;
    }
    return false;
}
