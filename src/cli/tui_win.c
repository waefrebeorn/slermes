/*
 * tui_win.c - TUI window / render helpers.
 *
 * Split from tui_fullscreen.c (soul.md: self-contained modules). Layout,
 * window creation / resize, history redraw, markdown rendering, role
 * colours and the small status printers. No file-scope state.
 */
#include "tui_win.h"
#include "tui_eventpub.h"
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
#include "plugin.h"
#include "chat_render.h"
#include "chat_composer.h"
#include "slermes_home.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>



/* ==================================================================
 *  P189: LAYOUT — pane creation, resizing, split logic
 * ================================================================== */

/* Calculate pane dimensions based on layout mode */
static void tui_calculate_layout(void) {
    int rows = tui.rows;
    int cols = tui.cols;

    /* Status bar always at bottom */
    int status_height = 1;
    int status_y = rows - status_height;

    /* Input area */
    int input_height;
    int input_y;

    if (tui.layout_mode == TUI_LAYOUT_MOBILE || tui.layout_mode == TUI_LAYOUT_COMPACT) {
        input_height = 2;           /* compact input */
        input_y = status_y - input_height;
    } else {
        input_height = 4;           /* multi-line input */
        input_y = status_y - input_height;
    }

    /* Tool feed pane (right column, above input) */
    int tool_feed_width;
    int tool_feed_x;
    int tool_feed_height = input_y;
    int tool_feed_y = 0;

    if (tui.layout_mode == TUI_LAYOUT_MOBILE) {
        /* Mobile: tool feed at bottom of history, full width, small */
        tool_feed_width = cols;
        tool_feed_x = 0;
        tool_feed_height = 3;      /* thin */
        tool_feed_y = input_y - tool_feed_height;
        input_y -= tool_feed_height;
    } else if (tui.layout_mode == TUI_LAYOUT_COMPACT) {
        tool_feed_width = 0;        /* hidden in compact */
        tool_feed_x = 0;
        tool_feed_height = 0;
    } else {
        /* Normal: tool feed on right side */
        tool_feed_width = cols / 4;
        if (tool_feed_width < 20) tool_feed_width = 20;
        if (tool_feed_width > 40) tool_feed_width = 40;
        tool_feed_x = cols - tool_feed_width;
        tool_feed_height = input_y;
    }

    /* History pane (everything remaining) */
    int hist_y = 0;
    int hist_height;
    int hist_width;
    int hist_x = 0;

    if (tui.layout_mode == TUI_LAYOUT_MOBILE) {
        hist_width = cols;
        hist_height = tool_feed_y;
    } else if (tui.layout_mode == TUI_LAYOUT_COMPACT) {
        hist_width = cols;
        hist_height = input_y;
    } else {
        hist_width = cols - tool_feed_width;
        hist_height = tool_feed_height;
    }

    /* Store pane dimensions */
    tui.panes[PANE_HISTORY].y = hist_y;
    tui.panes[PANE_HISTORY].x = hist_x;
    tui.panes[PANE_HISTORY].rows = hist_height;
    tui.panes[PANE_HISTORY].cols = hist_width;
    tui.panes[PANE_HISTORY].visible = hist_height > 0 && hist_width > 5;

    tui.panes[PANE_TOOL_FEED].y = tool_feed_y;
    tui.panes[PANE_TOOL_FEED].x = tool_feed_x;
    tui.panes[PANE_TOOL_FEED].rows = tool_feed_height;
    tui.panes[PANE_TOOL_FEED].cols = tool_feed_width;
    tui.panes[PANE_TOOL_FEED].visible = tool_feed_height > 1 && tool_feed_width > 10;

    tui.panes[PANE_INPUT].y = input_y;
    tui.panes[PANE_INPUT].x = 0;
    tui.panes[PANE_INPUT].rows = input_height;
    tui.panes[PANE_INPUT].cols = cols;
    tui.panes[PANE_INPUT].visible = input_height > 0;

    tui.panes[PANE_STATUS].y = status_y;
    tui.panes[PANE_STATUS].x = 0;
    tui.panes[PANE_STATUS].rows = status_height;
    tui.panes[PANE_STATUS].cols = cols;
    tui.panes[PANE_STATUS].visible = true;
}


/* Create/recreate all pane windows */ void tui_create_windows(void) {
    tui_calculate_layout();

    for (int i = 0; i < PANE_COUNT; i++) {
        pane_t *p = &tui.panes[i];
        if (p->win) delwin(p->win);
        p->win = NULL;

        if (!p->visible || p->rows < 1 || p->cols < 1)
            continue;

        p->win = newwin(p->rows, p->cols, p->y, p->x);
        if (p->win) {
            scrollok(p->win, TRUE);
            keypad(p->win, TRUE);
            wrefresh(p->win);
        }
    }
}


/* P189: Recalculate and redraw all panes (for resize) */ void tui_resize_panes(void) {
    /* Get new terminal dimensions */
    getmaxyx(stdscr, tui.rows, tui.cols);

    /* Recreate all windows with new layout */
    tui_create_windows();

    /* Redraw all content */
    for (int i = 0; i < PANE_COUNT; i++) {
        if (tui.panes[i].win)
            wnoutrefresh(tui.panes[i].win);
    }
    doupdate();
}


/* SIGINT handler — abort streaming during blocking agent_chat call */ void handle_sigint(int sig) {
    (void)sig;
    if (tui.stream.active)
        tui.stream.abort_requested = true;
}


/* ==================================================================
 *  P191: MESSAGE DISPLAY — rendering messages with role colors
 * ================================================================== */

/* Get color pair for role */
static int tui_role_color(msg_role_t role) {
    switch (role) {
        case MSG_ROLE_USER:      return 3;
        case MSG_ROLE_ASSISTANT: return 4;
        case MSG_ROLE_SYSTEM:    return 5;
        case MSG_ROLE_TOOL:      return 6;
        case MSG_ROLE_ERROR:     return 7;
        case MSG_ROLE_WARN:      return 8;
        case MSG_ROLE_INFO:      return 10;
        default:                 return 4;
    }
}


/* Add a line to message history */
void tui_history_add(msg_role_t role, const char *text, bool bold) {
    display_line_t *line = &tui.history.lines[tui.history.head];
    strncpy(line->text, text ? text : "", MAX_LINE_LENGTH - 1);
    line->role = role;
    line->color_pair = tui_role_color(role);
    line->bold = bold;
    line->dim = false;
    line->code_block = false;

    tui.history.head = (tui.history.head + 1) % MAX_MESSAGES_DISPLAY;
    if (tui.history.count < MAX_MESSAGES_DISPLAY)
        tui.history.count++;

    /* Emit event */
    tui_eventpub_message((int)role, text ? text : "", bold);
}


/* Write a formatted line to a window with role coloring */
static void tui_wprint_role(WINDOW *win, msg_role_t role, const char *text,
                             bool bold, bool dim) {
    if (!win) return;

    int pair = tui_role_color(role);

    if (bold) wattron(win, A_BOLD);
    if (dim)  wattron(win, A_DIM);
    wattron(win, COLOR_PAIR(pair));

    wprintw(win, "%s", text);

    wattroff(win, COLOR_PAIR(pair));
    if (bold) wattroff(win, A_BOLD);
    if (dim)  wattroff(win, A_DIM);
}


/* Simple markdown-like rendering — P191 */
static void tui_render_markdown(WINDOW *win, const char *text, msg_role_t role) {
    if (!win || !text) return;

    int base_pair = tui_role_color(role);
    int hl_pair = 9;  /* highlight pair */
    int dim_pair = 10;

    /* State machine for inline formatting */
    bool in_bold = false;
    bool in_code = false;
    bool in_header = false;
    int col = 0;

    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            /* End formatting at newline */
            if (in_bold) { wattroff(win, A_BOLD); in_bold = false; }
            /* Flush header style */
            if (in_header) {
                wattroff(win, A_BOLD);
                in_header = false;
            }
            waddch(win, '\n');
            col = 0;
            /* Check for header at start of line */
            if (p[1] == '#') in_header = true;
            continue;
        }

        /* Detect markdown patterns */
        if (col == 0 && *p == '#' && role != MSG_ROLE_ERROR) {
            in_header = true;
            wattron(win, A_BOLD | COLOR_PAIR(base_pair));
            continue;
        }
        if (in_header && *p == '#') continue; /* skip extra # */
        if (in_header && *p == ' ') continue; /* skip space after # */

        if (*p == '`' && !in_bold) {
            if (in_code) {
                wattroff(win, COLOR_PAIR(hl_pair));
                in_code = false;
            } else {
                wattron(win, COLOR_PAIR(hl_pair));
                in_code = true;
            }
            continue;
        }

        /* Bold: **text** */
        if (*p == '*' && *(p+1) == '*' && col > 0) {
            if (in_bold) {
                wattroff(win, A_BOLD);
                in_bold = false;
            } else {
                wattron(win, A_BOLD);
                in_bold = true;
            }
            p++; /* skip second * */
            continue;
        }

        /* Code block lines (indented) */
        if (col == 0 && *p == ' ' && role != MSG_ROLE_ERROR) {
            wattron(win, COLOR_PAIR(dim_pair));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(dim_pair));
            col++;
            continue;
        }

        /* Write character */
        if (in_header) {
            wattron(win, A_BOLD | COLOR_PAIR(base_pair));
            waddch(win, *p);
            wattroff(win, A_BOLD);
        } else if (in_code) {
            wattron(win, COLOR_PAIR(hl_pair));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(hl_pair));
        } else {
            waddch(win, *p);
        }
        col++;
    }

    if (in_bold) wattroff(win, A_BOLD);
    if (in_code) wattroff(win, COLOR_PAIR(hl_pair));
}


/* Redraw the history pane from the message buffer */
void tui_redraw_history(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    /* Calculate visible lines */
    int max_rows = tui.panes[PANE_HISTORY].rows - 1;
    if (max_rows < 1) return;

    int start = tui.history.count - max_rows - tui.panes[PANE_HISTORY].scroll_pos;
    if (start < 0) start = 0;

    /* Draw header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 0, 0, " Messages (%d) ", tui.history.count);
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Draw visible lines */
    int y = 1;
    for (int i = start; i < tui.history.count && y < tui.panes[PANE_HISTORY].rows; i++) {
        /* Map i through ring buffer */
        int ring_idx;
        if (tui.history.count < MAX_MESSAGES_DISPLAY) {
            /* Simple case: linear */
            ring_idx = i;
        } else {
            /* Ring buffer: oldest = head, newest = (head - 1) % MAX */
            ring_idx = (tui.history.head - tui.history.count + i) % MAX_MESSAGES_DISPLAY;
            if (ring_idx < 0) ring_idx += MAX_MESSAGES_DISPLAY;
        }

        const display_line_t *line = &tui.history.lines[ring_idx];

        /* Role prefix */
        const char *prefix = "";
        switch (line->role) {
            case MSG_ROLE_USER:      prefix = "You: "; break;
            case MSG_ROLE_ASSISTANT: prefix = "AI:  "; break;
            case MSG_ROLE_SYSTEM:    prefix = "Sys: "; break;
            case MSG_ROLE_TOOL:      prefix = "  -> "; break;
            case MSG_ROLE_ERROR:     prefix = "Err: "; break;
            case MSG_ROLE_WARN:      prefix = "Wrn: "; break;
            case MSG_ROLE_INFO:      prefix = ""; break;
        }

        wmove(win, y, 0);

        /* Print prefix in role color */
        tui_wprint_role(win, line->role, prefix, true, false);

        /* Print message text with markdown rendering */
        int prefix_len = strlen(prefix);
        /* wmove past prefix */
        wmove(win, y, prefix_len);

        if (line->bold) wattron(win, A_BOLD);
        tui_render_markdown(win, line->text, line->role);
        if (line->bold) wattroff(win, A_BOLD);

        y++;
    }

    wnoutrefresh(win);
}


/* ==================================================================
 *  GENERAL DISPLAY FUNCTIONS (public API)
 * ================================================================== */

void tui_fullscreen_print(const char *fmt, ...) {
    if (!tui.running) {
        /* Before TUI initialized, fall back to printf */
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
        return;
    }

    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    tui_history_add(MSG_ROLE_ASSISTANT, buf, false);
    tui_redraw_history();

    /* Redraw input */
    if (tui.panes[PANE_INPUT].win) {
        wnoutrefresh(tui.panes[PANE_INPUT].win);
    }
    doupdate();
}


void tui_fullscreen_error(const char *fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (!tui.running) {
        fprintf(stderr, "Error: %s\n", buf);
        return;
    }

    tui_history_add(MSG_ROLE_ERROR, buf, true);
    tui_redraw_history();
    if (tui.panes[PANE_INPUT].win)
        wnoutrefresh(tui.panes[PANE_INPUT].win);
    doupdate();
}


void tui_fullscreen_warn(const char *fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (!tui.running) {
        printf("Warning: %s\n", buf);
        return;
    }

    tui_history_add(MSG_ROLE_WARN, buf, false);
    tui_redraw_history();
    if (tui.panes[PANE_INPUT].win)
        wnoutrefresh(tui.panes[PANE_INPUT].win);
    doupdate();
}


/* OSC52 clipboard copy — writes text to system clipboard via terminal escape. */
void tui_fullscreen_clipboard_copy(const char *text) {
    if (!text || !*text) return;
    size_t len = strlen(text);
    char *b64 = base64_encode((const unsigned char *)text, len);
    if (!b64) return;
    /* OSC52: ESC ] 52 ; c ; <base64> ST */
    printf("\x1b]52;c;%s\x1b\\", b64);
    fflush(stdout);
    free(b64);
}


/* Display help hints — hotkeys and common commands overlay */
void tui_fullscreen_help_hints(void) {
    static const char *hints[] = {
        "/help — full command list",
        "/clear — new session",
        "Ctrl+C — interrupt",
        "Ctrl+L — clear screen",
        "↑↓ — history",
        "Tab — complete",
        NULL
    };
    if (!tui.running) {
        printf("Help hints:\n");
        for (int i = 0; hints[i]; i++) printf("  %s\n", hints[i]);
        return;
    }
    wattron(tui.panes[PANE_TOOL_FEED].win, A_DIM);
    for (int i = 0; hints[i]; i++) {
        mvwaddstr(tui.panes[PANE_TOOL_FEED].win, i + 1, 2, hints[i]);
    }
    wattroff(tui.panes[PANE_TOOL_FEED].win, A_DIM);
    wnoutrefresh(tui.panes[PANE_TOOL_FEED].win);
    doupdate();
}
