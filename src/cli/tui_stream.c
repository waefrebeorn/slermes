/*
 * tui_stream.c - TUI streaming-render helpers.
 *
 * Split from tui_fullscreen.c (soul.md: self-contained modules). Buffers
 * streamed assistant tokens and flushes them to the history pane. No
 * file-scope state of its own.
 */
#include "tui_stream.h"
#include "hermes_cli.h"
#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "tui_views.h"
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
 *  P192: STREAMING — token display and counter
 * ================================================================== */

/* Start streaming session */
static void tui_stream_start(void) {
    tui.stream.active = true;
    tui.stream.abort_requested = false;
    tui.stream.current_pos = 0;
    tui.stream.current_line[0] = '\0';
    tui.stream.token_count = 0;
    tui.stream.start_time = (double)clock() / CLOCKS_PER_SEC;
    tui.stream.first_token = true;
    tui.stream.bytes_received = 0;
    tui.stream.tokens_per_sec = 0.0;
    tui.stream.type_ahead_len = 0;
    tui.stream.type_ahead_buf[0] = '\0';
    tui.think_frame = 0;

    /* Set input window to non-blocking during entire streaming */
    if (tui.panes[PANE_INPUT].win)
        nodelay(tui.panes[PANE_INPUT].win, TRUE);

    strcpy(tui.status.mode, "stream");
}


/* End streaming session */
static void tui_stream_finish(void) {
    if (!tui.stream.active) return;

    /* Flush remaining content */
    if (tui.stream.current_pos > 0) {
        tui.stream.current_line[tui.stream.current_pos] = '\0';
        tui_history_add(MSG_ROLE_ASSISTANT, tui.stream.current_line, false);
        tui.stream.current_pos = 0;
    }

    tui.stream.active = false;
    tui.stream.first_token = false;
    strcpy(tui.status.mode, "chat");

    /* Restore blocking input */
    if (tui.panes[PANE_INPUT].win)
        nodelay(tui.panes[PANE_INPUT].win, FALSE);

    /* Inject type-ahead buffer captured during streaming (T18) */
    if (tui.stream.type_ahead_len > 0) {
        int inject_len = tui.stream.type_ahead_len;
        if (tui.input.len + inject_len < INPUT_BUF_SIZE - 1) {
            memcpy(tui.input.buf + tui.input.len, tui.stream.type_ahead_buf, inject_len);
            tui.input.len += inject_len;
            tui.input.buf[tui.input.len] = '\0';
            tui.input.pos = tui.input.len;
        }
        tui.stream.type_ahead_len = 0;
        tui.stream.type_ahead_buf[0] = '\0';
    }

    /* Update token rate */
    double elapsed = ((double)clock() / CLOCKS_PER_SEC) - tui.stream.start_time;
    if (elapsed > 0)
        tui.stream.tokens_per_sec = tui.stream.token_count / elapsed;

    tui_redraw_history();
    tui_redraw_status();
}


/* Write a streaming token to the display */
void tui_fullscreen_stream_token(const char *token) {
    if (!token || !*token) return;

    if (!tui.stream.active)
        tui_stream_start();

    tui.stream.token_count++;
    tui.stream.bytes_received += strlen(token);
    tui.stream.last_token_time = (double)clock() / CLOCKS_PER_SEC;

    /* Emit event */
    tui_eventpub_stream_token(token, tui.stream.token_count);

    /* Add to current streaming line */
    int tlen = strlen(token);
    if (tui.stream.current_pos + tlen < MAX_LINE_LENGTH - 1) {
        memcpy(tui.stream.current_line + tui.stream.current_pos, token, tlen);
        tui.stream.current_pos += tlen;
        tui.stream.current_line[tui.stream.current_pos] = '\0';
    }

    /* Check for newlines — commit line on each newline */
    if (strchr(token, '\n')) {
        /* Commit completed lines */
        char *save = strdup(tui.stream.current_line);
        if (save) {
            /* Remove trailing newline for storage */
            size_t slen = strlen(save);
            if (slen > 0 && save[slen-1] == '\n') save[slen-1] = '\0';
            tui_history_add(MSG_ROLE_ASSISTANT, save, false);
            free(save);
        }
        tui.stream.current_pos = 0;
        tui.stream.current_line[0] = '\0';
    }

    /* Display latest content in history pane */
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (win) {
        /* Show streaming content at bottom */
        int max_rows = tui.panes[PANE_HISTORY].rows - 1;
        int y = max_rows; /* last line */

        /* Clear line and write streaming content */
        mvwprintw(win, y, 0, "%-*s", tui.panes[PANE_HISTORY].cols - 1, " ");
        mvwprintw(win, y, 0, "%s", tui.stream.current_line);

        /* Show token counter in dim */
        if (tui.stream.token_count > 0 && tui.stream.token_count % 50 == 0) {
            /* Brief counter flash */
            wattron(win, A_DIM | COLOR_PAIR(10));
            mvwprintw(win, 0, tui.panes[PANE_HISTORY].cols - 15, " [%d tok]", tui.stream.token_count);
            wattroff(win, A_DIM | COLOR_PAIR(10));
        }

        wnoutrefresh(win);

        /* Update status bar with token count */
        tui_fullscreen_status_update(NULL, NULL, 0, 0, 0, tui.stream.token_count, 0);

        /* Redraw input and status */
        wnoutrefresh(tui.panes[PANE_INPUT].win);
        tui_redraw_status();
        doupdate();
    }
}


void tui_fullscreen_stream_done(void) {
    tui_stream_finish();
    tui_eventpub_stream_done();
}


/* ==================================================================
 *  COMMAND DISPATCH — handle slash commands and agent chat
 * ================================================================== */

/* Stream callback adapter — tui_fullscreen_stream_token signature
* doesn't match llm_token_cb_t (no userdata, void return).
* This wrapper bridges the gap so agent_loop can stream tokens
* into the TUI display during agent_chat(). */
/* Check abort flag in streaming callback — allows signal/thread abort */ int tui_stream_cb(const char *token, void *userdata) {
   (void)userdata;
   if (tui.stream.abort_requested)
       return 1; /* abort streaming */
   tui_fullscreen_stream_token(token);
   return 0; /* continue streaming */
}
