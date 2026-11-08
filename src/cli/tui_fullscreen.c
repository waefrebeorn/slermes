/*
 * tui_fullscreen.c — Full ncurses TUI for Hermes C (P189-P200).
 * MIT License — WuBu Slermes Project
 *
 * Implements 12 phases:
 *   P189: Layout     — split panes (history | input | tool feed | status)
 *   P190: Input      — multi-line, emoji picker, slash autocomplete
 *   P191: Messages   — role-colored, syntax highlight, markdown render
 *   P192: Streaming  — token streaming, real-time token counter
 *   P193: Tool feed  — live tool calls, progress bars, result preview
 *   P194: Status bar — model/provider, tokens, iterations, budget
 *   P195: Sessions   — browse, search, load/delete/export
 *   P196: Config     — interactive key browser, set/get/explain
 *   P197: Images     — sixel/kitty display, zoom/pan
 *   P198: Themes     — skin files, color schemes
 *   P199: Gateway    — JSON-RPC backend (fifo transport)
 *   P200: Mobile     — responsive layout, touch input
 */

/* PoP: TUI fullscreen mode (TypeScript-based) */

#define _GNU_SOURCE
#include "hermes_cli.h"
#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "tui_fullscreen.h"
#include "tui_views.h"
#include "tui_stream.h"
#include "tui_input.h"

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

/* ── Forward definitions (hoisted; used by early helpers below, defined
 *    in full further down the file). Kept here so the translation unit is
 *    self-contained without relying on the hermes.h god header. ──────────── */
#define MAX_MESSAGES_DISPLAY 4096
#define MODEL_PICKER_MAX 256
#define MODEL_NAME_MAX 128

#include "tui_state.h"

tui_global_state_t tui;

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

/* Create/recreate all pane windows */
static void tui_create_windows(void) {
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

/* P189: Recalculate and redraw all panes (for resize) */
static void tui_resize_panes(void) {
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

/* ==================================================================
 *  SIGWINCH handler — terminal resize
 * ================================================================== */

/* SIGWINCH handler — set flag for main loop to process */
static volatile sig_atomic_t g_resize_requested = 0;

static void handle_winch(int sig) {
    (void)sig;
    g_resize_requested = 1;
    tui_eventpub_resize(tui.rows, tui.cols);
}

/* SIGINT handler — abort streaming during blocking agent_chat call */
static void handle_sigint(int sig) {
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
 *  P194: STATUS BAR — model, tokens, iterations, budget
 * ================================================================== */

void tui_redraw_status(void) {
    WINDOW *win = tui.panes[PANE_STATUS].win;
    if (!win) return;

    werase(win);

    wattron(win, A_REVERSE | COLOR_PAIR(1));

    char buf[1024];
    int pos = 0;

    /* Model/Provider info */
    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s", tui.status.model[0] ? tui.status.model : "Slermes");
    if (tui.status.provider[0])
        pos += snprintf(buf + pos, sizeof(buf) - pos, " [%s]", tui.status.provider);

    /* Mode */
    pos += snprintf(buf + pos, sizeof(buf) - pos, " | %s", tui.status.mode[0] ? tui.status.mode : "idle");

    /* Iterations */
    if (tui.status.iteration > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | iter %d/%d",
                        tui.status.iteration, tui.status.max_iterations);

    /* Token usage */
    if (tui.status.tokens_in > 0 || tui.status.tokens_out > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | \u2191%d \u2193%d",
                        tui.status.tokens_in, tui.status.tokens_out);

    /* Budget */
    if (tui.status.budget_remaining > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | budget %.1f",
                        tui.status.budget_remaining);

    /* FPS overlay when visible */
    if (tui.fps_visible) {
        double elapsed = ((double)clock() / CLOCKS_PER_SEC) - tui.fps_start_time;
        double fps = (elapsed > 0.0) ? (double)tui.frame_count / elapsed : 0.0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | FPS: %.0f", fps);
    }

    /* Layout indicator */
    if (tui.layout_mode == TUI_LAYOUT_MOBILE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | MOBILE");
    else if (tui.layout_mode == TUI_LAYOUT_COMPACT)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " | COMPACT");

    /* Right-align time */
    char time_str[16];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info)
        strftime(time_str, sizeof(time_str), " %H:%M ", tm_info);
    else
        snprintf(time_str, sizeof(time_str), " --:-- ");

    int right_start = tui.panes[PANE_STATUS].cols - (int)strlen(time_str);
    if (right_start > pos) {
        while (pos < right_start)
            buf[pos++] = ' ';
        memcpy(buf + pos, time_str, strlen(time_str));
        pos += strlen(time_str);
    }

    buf[pos] = '\0';
    mvwprintw(win, 0, 0, "%s", buf);
    wattroff(win, A_REVERSE | COLOR_PAIR(1));

    wnoutrefresh(win);
}

/* Update status bar from agent state (P194) */
void tui_fullscreen_status_update(const char *model, const char *provider,
                                    int iteration, int max_iterations,
                                    int tokens_in, int tokens_out,
                                    double budget_remaining) {
    if (model) strncpy(tui.status.model, model, sizeof(tui.status.model) - 1);
    if (provider) strncpy(tui.status.provider, provider, sizeof(tui.status.provider) - 1);
    tui.status.iteration = iteration;
    tui.status.max_iterations = max_iterations;
    tui.status.tokens_in = tokens_in;
    tui.status.tokens_out = tokens_out;
    tui.status.budget_remaining = budget_remaining;
    tui.status.dirty = true;
    tui_eventpub_agent(model, provider, iteration, max_iterations,
                       tokens_in, tokens_out, budget_remaining);
}

/* Add or update a tool call in the feed, with shelf-grouping */
void tui_fullscreen_tool_status(const char *tool_name, const char *status,
                                 int progress, int total) {
    if (!tool_name) return;

    /* Find existing or create new */
    tool_call_entry_t *entry = NULL;
    for (int i = 0; i < tui.tool_feed.count; i++) {
        if (strcmp(tui.tool_feed.calls[i].name, tool_name) == 0) {
            entry = &tui.tool_feed.calls[i];
            break;
        }
    }

    if (!entry) {
        if (tui.tool_feed.count >= MAX_TOOL_CALLS)
            return;
        entry = &tui.tool_feed.calls[tui.tool_feed.count++];
        strncpy(entry->name, tool_name, sizeof(entry->name) - 1);
        entry->status = TOOL_STATUS_PENDING;
        entry->progress = 0;
        entry->total_steps = total;
        entry->shelf_header = false;
        entry->result_preview[0] = '\0';
        /* Assign shelf group */
        const char *shelf = tui_tool_shelf(tool_name);
        strncpy(entry->shelf, shelf, sizeof(entry->shelf) - 1);
        /* Mark as shelf header if we're the first in this group */
        for (int i = 0; i < tui.tool_feed.count; i++) {
            if (strcmp(tui.tool_feed.calls[i].shelf, entry->shelf) == 0) {
                if (tui.tool_feed.calls[i].shelf_header) {
                    entry->shelf_header = false;
                } else {
                    entry->shelf_header = (i == tui.tool_feed.count - 1);
                }
                break;
            }
        }
        /* If no other tool in this shelf, mark as header */
        {
            bool found_shelf = false;
            for (int i = 0; i < tui.tool_feed.count - 1; i++) {
                if (strcmp(tui.tool_feed.calls[i].shelf, entry->shelf) == 0) {
                    found_shelf = true;
                    break;
                }
            }
            if (!found_shelf)
                entry->shelf_header = true;
        }
    }

    /* Update status */
    if (strcmp(status, "running") == 0) entry->status = TOOL_STATUS_RUNNING;
    else if (strcmp(status, "done") == 0) entry->status = TOOL_STATUS_DONE;
    else if (strcmp(status, "error") == 0) entry->status = TOOL_STATUS_ERROR;
    else if (strcmp(status, "pending") == 0) entry->status = TOOL_STATUS_PENDING;

    if (progress >= 0) entry->progress = progress;
    if (total > 0) entry->total_steps = total;

    /* Recalculate active count */
    tui.tool_feed.active_count = 0;
    for (int i = 0; i < tui.tool_feed.count; i++) {
        if (tui.tool_feed.calls[i].status == TOOL_STATUS_RUNNING ||
            tui.tool_feed.calls[i].status == TOOL_STATUS_PENDING)
            tui.tool_feed.active_count++;
    }

    /* Emit event */
    tui_eventpub_tool(tool_name, status, progress, total, NULL);

    /* Redraw tool feed pane */
    WINDOW *win = tui.panes[PANE_TOOL_FEED].win;
    if (!win) return;

    werase(win);

    /* Title */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 0, 0, " Tools [\xF0\x9F\x93\x82] %d/%d ", tui.tool_feed.active_count, tui.tool_feed.count);
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Render grouped by shelf */
    int y = 1;
    char last_shelf[48] = "";
    for (int i = 0; i < tui.tool_feed.count && y < tui.panes[PANE_TOOL_FEED].rows; i++) {
        const tool_call_entry_t *tc = &tui.tool_feed.calls[i];

        /* Shelf header if this tool starts a new group */
        if (last_shelf[0] == '\0' || strcmp(tc->shelf, last_shelf) != 0) {
            strncpy(last_shelf, tc->shelf, sizeof(last_shelf) - 1);
            if (y < tui.panes[PANE_TOOL_FEED].rows) {
                wattron(win, A_DIM | COLOR_PAIR(10));
                mvwprintw(win, y, 0, " %s", tc->shelf);
                wattroff(win, A_DIM | COLOR_PAIR(10));
                y++;
            }
        }

        if (y >= tui.panes[PANE_TOOL_FEED].rows) break;

        /* Status symbol — use string for multibyte UTF-8 */
        const char *sym_str;
        int pair;
        switch (tc->status) {
            case TOOL_STATUS_PENDING: sym_str = "\xE2\x97\x8B"; pair = 10; break; /* ○ */
            case TOOL_STATUS_RUNNING: sym_str = "\xE2\x96\xB6"; pair = 6; break;  /* ▶ */
            case TOOL_STATUS_DONE:    sym_str = "\xE2\x9C\x93"; pair = 9; break;   /* ✓ */
            case TOOL_STATUS_ERROR:   sym_str = "\xE2\x9C\x97"; pair = 7; break;   /* ✗ */
            default:                  sym_str = "?"; pair = 10;
        }

        wattron(win, COLOR_PAIR(pair) | A_BOLD);
        mvwprintw(win, y, 2, "%s %s", sym_str, tc->name);
        wattroff(win, A_BOLD);

        /* Progress bar */
        if (tc->status == TOOL_STATUS_RUNNING && tc->total_steps > 0) {
            int pct = tc->progress * 100 / tc->total_steps;
            int avail = tui.panes[PANE_TOOL_FEED].cols - 6 - (int)strlen(tc->name);
            int bar_w = avail > 3 ? (avail < 25 ? avail : 25) : 3;

            int filled = pct * bar_w / 100;
            int cx = 4 + (int)strlen(tc->name);
            mvwprintw(win, y, cx, " ");
            wattron(win, A_REVERSE);
            for (int b = 0; b < bar_w; b++)
                mvwaddch(win, y, cx + 1 + b, b < filled ? ' ' : '.');
            wattroff(win, A_REVERSE);
            mvwprintw(win, y, cx + 1 + bar_w, " %d%%", pct);
        }

        /* Result preview for completed tools */
        if (tc->status == TOOL_STATUS_DONE && tc->result_preview[0]) {
            int ny = y + 1;
            if (ny < tui.panes[PANE_TOOL_FEED].rows) {
                wattron(win, A_DIM | COLOR_PAIR(10));
                int preview_cols = tui.panes[PANE_TOOL_FEED].cols - 4;
                char preview[128];
                snprintf(preview, sizeof(preview), "%.*s", preview_cols, tc->result_preview);
                mvwprintw(win, ny, 4, "%s", preview);
                wattroff(win, A_DIM | COLOR_PAIR(10));
            }
        }

        y++;
    }

    wnoutrefresh(win);

    /* Also redraw history if streaming */
    if (tui.stream.active)
        tui_redraw_history();
    doupdate();
}

/* Redraw the input pane — handles multi-line wrap */
static void tui_redraw_input(void) {
    WINDOW *win = tui.panes[PANE_INPUT].win;
    if (!win) return;

    werase(win);

    int input_cols = tui.panes[PANE_INPUT].cols;

    /* Draw prompt */
    wattron(win, COLOR_PAIR(2) | A_BOLD);
    mvwprintw(win, 0, 0, "%s ", tui_get_theme()->prompt_sym);
    wattroff(win, A_BOLD);

    int prompt_len = strlen(tui_get_theme()->prompt_sym) + 1;

    /* Check for autocomplete overlay */
    if (tui.input.autocomplete_active && tui.input.autocomplete_count > 0) {
        /* Draw autocomplete popup */
        int popup_y = 1;
        int popup_x = 0;
        int popup_max = tui.panes[PANE_INPUT].rows - 1;
        if (popup_max > 5) popup_max = 5;

        for (int i = 0; i < popup_max && i < tui.input.autocomplete_count; i++) {
            int idx = i;
            if (tui.input.autocomplete_sel >= 0)
                idx = tui.input.autocomplete_sel + i;
            if (idx >= tui.input.autocomplete_count) break;

            if (i == 0) {
                wattron(win, A_REVERSE | COLOR_PAIR(13));
            } else {
                wattron(win, COLOR_PAIR(10));
            }
            mvwprintw(win, popup_y + i, popup_x, " %-*s",
                      input_cols - 2, tui.input.autocomplete_matches[idx]);
            wattroff(win, A_REVERSE | COLOR_PAIR(10) | COLOR_PAIR(13));
        }
    }

    /* Check for emoji picker */
    if (tui.input.emoji_picker_active) {
        int popup_y = 1;
        int popup_x = 0;
        int popup_max = tui.panes[PANE_INPUT].rows - 1;
        if (popup_max > 4) popup_max = 4;

        mvwprintw(win, popup_y, popup_x, " Emoji: ");
        for (int i = 0; i < popup_max && i < tui.input.emoji_count; i++) {
            if (i == tui.input.emoji_sel)
                wattron(win, A_REVERSE);
            mvwprintw(win, popup_y + 1 + i, popup_x + 4, "%s",
                      tui.input.emoji_results[i]);
            wattroff(win, A_REVERSE);
        }
    }

    /* Draw input text with horizontal scrolling */
    int display_len = tui.input.len;
    int scroll_col = tui.input.scroll_col;
    int avail = input_cols - prompt_len - 1;
    if (avail < 1) avail = 1;

    /* Ensure cursor is visible */
    if (tui.input.pos < scroll_col)
        scroll_col = tui.input.pos;
    if (tui.input.pos > scroll_col + avail - 1)
        scroll_col = tui.input.pos - avail + 1;
    if (scroll_col < 0) scroll_col = 0;
    tui.input.scroll_col = scroll_col;

    /* Display visible portion */
    int vis_len = display_len - scroll_col;
    if (vis_len > avail) vis_len = avail;
    if (vis_len < 0) vis_len = 0;

    char display_buf[1024];
    if (vis_len > 0) {
        strncpy(display_buf, tui.input.buf + scroll_col, vis_len);
        display_buf[vis_len] = '\0';
    } else {
        display_buf[0] = '\0';
    }

    /* Draw text */
    if (tui.input.echo) {
        mvwprintw(win, 0, prompt_len, "%s", display_buf);
    } else {
        /* Password mode: show asterisks */
        for (int i = 0; i < vis_len; i++)
            mvwaddch(win, 0, prompt_len + i, '*');
    }

    /* Place cursor */
    int cursor_x = prompt_len + (tui.input.pos - scroll_col);
    wmove(win, 0, cursor_x);

    wnoutrefresh(win);
}

/* Create FIFO for RPC communication */
static bool tui_gateway_init(void) {
    /* Remove old FIFO if exists */
    unlink(RPC_FIFO_PATH);

    /* Create new FIFO */
    if (mkfifo(RPC_FIFO_PATH, 0600) != 0) {
        /* Non-fatal: gateway mode is optional */
        tui.gateway.state = RPC_IDLE;
        tui.gateway.fifo_fd = -1;
        tui_eventpub_init(NULL);  /* Init without FIFO path for local dispatch */
        return false;
    }

    tui.gateway.state = RPC_IDLE;
    tui.gateway.fifo_fd = -1;
    tui.gateway.read_pos = 0;

    tui_eventpub_init(RPC_FIFO_PATH);  /* Init with FIFO path */
    tui_slash_init_all();              /* T06: Register all slash commands */

    return true;
}

/* Try to connect gateway (non-blocking open of FIFO) */
static bool tui_gateway_connect(void) {
    if (tui.gateway.fifo_fd >= 0) return true;

    tui.gateway.fifo_fd = open(RPC_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (tui.gateway.fifo_fd < 0) return false;

    tui.gateway.state = RPC_CONNECTED;
    return true;
}

/* Process one RPC message — simplified JSON-RPC */
static void tui_gateway_process_message(const char *msg) {
    if (!msg || !*msg) return;

    /* Simple dispatch based on method field */
    if (strstr(msg, "\"method\":\"print\"")) {
        char *params = strstr(msg, "\"params\"");
        if (params) {
            char *text = strchr(params, ':');
            if (text) {
                text++;
                /* Skip quotes */
                while (*text && (*text == ' ' || *text == '"')) text++;
                char *end = strchr(text, '"');
                if (end) *end = '\0';
                tui_history_add(MSG_ROLE_INFO, text, false);
            }
        }
    } else if (strstr(msg, "\"method\":\"error\"")) {
        char *params = strstr(msg, "\"params\"");
        if (params) {
            char *text = strchr(params, ':');
            if (text) {
                text++;
                while (*text && (*text == ' ' || *text == '"')) text++;
                char *end = strchr(text, '"');
                if (end) *end = '\0';
                tui_history_add(MSG_ROLE_ERROR, text, false);
            }
        }
    } else if (strstr(msg, "\"method\":\"stream\"")) {
        char *params = strstr(msg, "\"params\"");
        if (params) {
            char *text = strchr(params, ':');
            if (text) {
                text++;
                while (*text && (*text == ' ' || *text == '"')) text++;
                char *end = strchr(text, '"');
                if (end) *end = '\0';
                tui_fullscreen_stream_token(text);
            }
        }
    } else if (strstr(msg, "\"method\":\"stream_done\"")) {
        tui_fullscreen_stream_done();
    } else if (strstr(msg, "\"method\":\"tool_status\"")) {
        /* Parse tool status from JSON */
        char *name = strstr(msg, "\"name\"");
        char *status = strstr(msg, "\"status\"");
        if (name && status) {
            char tname[64] = {0}, tstatus[32] = {0};
            sscanf(name, "\"name\":\"%63[^\"]\"", tname);
            sscanf(status, "\"status\":\"%31[^\"]\"", tstatus);
            tui_fullscreen_tool_status(tname, tstatus, 0, 0);
        }
    }
}

/* Poll gateway FIFO for incoming messages */
static void tui_gateway_poll(void) {
    if (tui.gateway.fifo_fd < 0) return;

    char buf[4096];
    int n = (int)read(tui.gateway.fifo_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;

    buf[n] = '\0';

    /* Accumulate in read buffer */
    int remain = RPC_BUF_SIZE - tui.gateway.read_pos - 1;
    if (n > remain) n = remain;
    memcpy(tui.gateway.read_buf + tui.gateway.read_pos, buf, n);
    tui.gateway.read_pos += n;
    tui.gateway.read_buf[tui.gateway.read_pos] = '\0';

    /* Process complete messages (separated by newlines) */
    char *line_start = tui.gateway.read_buf;
    while (1) {
        char *nl = strchr(line_start, '\n');
        if (!nl) break;

        *nl = '\0';
        tui_gateway_process_message(line_start);
        line_start = nl + 1;
    }

    /* Shift remaining */
    if (line_start > tui.gateway.read_buf) {
        int remaining = tui.gateway.read_pos - (int)(line_start - tui.gateway.read_buf);
        if (remaining > 0) {
            memmove(tui.gateway.read_buf, line_start, remaining);
        }
        tui.gateway.read_pos = remaining;
        tui.gateway.read_buf[remaining] = '\0';
    }
}

/* Send RPC message to agent process */
static void tui_gateway_send(const char *method, const char *params_json) {
    /* Write to FIFO (agent process reads from other end) */
    char write_fifo[64];
    snprintf(write_fifo, sizeof(write_fifo), "%s.out", RPC_FIFO_PATH);

    int fd = open(write_fifo, O_WRONLY | O_NONBLOCK);
    if (fd < 0) return;

    char msg[4096];
    snprintf(msg, sizeof(msg), "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}\n",
             method, params_json ? params_json : "{}");
    write(fd, msg, strlen(msg));
    close(fd);
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

/* Initialize slash command system — register all TUI commands */
static void tui_slash_init_all(void) {
    tui_slash_init((void *)tui.agent);
    tui_slash_register_batch(slash_registry);
}

static void tui_process_input(const char *line) {
   if (!line || !*line) return;

   /* Emit command/input event */
   tui_eventpub_command(line);

   /* Add to history */
   tui_input_history_add(line);
    tui.input.history_pos = -1;

    /* Echo to history */
    tui_history_add(MSG_ROLE_USER, line, false);

    /* Check for slash commands — dispatch via T06 slash worker */
    if (line[0] == '/') {
        if (!tui_slash_dispatch(line)) {
            /* Not a registered slash command — try agent CLI + skills */
            if (tui.agent && commands_dispatch((char *)line, tui.agent)) {
                /* command handled */
            } else if (tui.agent && commands_try_skill((char *)line, tui.agent)) {
                /* skill command handled */
            } else if (tui.agent && commands_try_quick((char *)line, tui.agent)) {
                /* CL13: user-defined quick command handled */
            } else {
                tui_history_add(MSG_ROLE_ERROR, "Unknown command", true);
            }
            tui_redraw_history();
        }
        return;
    }

    /* Send to agent via chat */
    if (tui.agent) {
        /* Wire agent_chat with streaming — this replaces the previous stub
         * that showed "[Agent processing...]" and did nothing. The retry
         * logic lives in agent_loop.c's retry loop, which works for any
         * caller including this TUI path. */
        tui.agent->stream_cb = tui_stream_cb;
        tui.agent->stream_data = NULL;

        char *resp = agent_chat(tui.agent, line);
        tui_fullscreen_stream_done();

        if (resp) {
            tui_history_add(MSG_ROLE_ASSISTANT, resp, false);
            free(resp);
        } else {
            tui_history_add(MSG_ROLE_ERROR, "Agent returned no response", true);
        }
        tui_redraw_history();
    }
}

/* ==================================================================
 *  T13: MODEL PICKER — fetch models, draw overlay, handle input
 * ================================================================== */

/* Initialize model picker: fetch model list from metadata */

/* Draw the model picker overlay window */

/* Handle keyboard input for model picker overlay */

/* ==================================================================
 *  MAIN INPUT LOOP — unified input handling for all modes
 * ================================================================== */

/* Handle modal overlays before normal input */
static int tui_handle_modal_input(int ch) {
    switch (tui.modal_mode) {
        case MODE_SESSION_BROWSE:
            return tui_session_browser_handle(ch);
        case MODE_CONFIG_EDIT:
            return tui_config_editor_handle(ch);
        case MODE_IMAGE_VIEW:
            return tui_image_viewer_handle(ch);
        case MODE_GATEWAY_STATUS:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_CRON_VIEW:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_LOG_VIEW:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_SKILL_BROWSE:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_HELP:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;
        case MODE_MODEL_PICK:
            return tui_model_picker_handle(ch);
        case MODE_TODO_PANEL:
            if (tui_todo_panel_handle(ch))
                return 1;
            return 0;
        case MODE_PLUGIN_HUB:
            return tui_plugin_hub_handle(ch);
        default:
            return 0;
    }
}

/* Handle normal input mode — P190 multi-line editing */
static int tui_handle_input(int ch) {
    switch (ch) {
        case '\n':
        case '\r':
            /* Send line */
            if (tui.input.len > 0) {
                tui.input.buf[tui.input.len] = '\0';
                tui_process_input(tui.input.buf);

                /* Clear input buffer */
                tui.input.len = 0;
                tui.input.pos = 0;
                tui.input.scroll_col = 0;
                tui.input.autocomplete_active = false;
                tui.input.emoji_picker_active = false;

                /* Redraw everything */
                tui_redraw_history();
                tui_redraw_status();
            }
            return 1;

        case KEY_BACKSPACE:
        case 127:
            if (tui.input.pos > 0) {
                memmove(tui.input.buf + tui.input.pos - 1,
                        tui.input.buf + tui.input.pos,
                        tui.input.len - tui.input.pos);
                tui.input.pos--;
                tui.input.len--;
                tui.input.buf[tui.input.len] = '\0';
            }
            break;

        case KEY_DC: /* Delete */
            if (tui.input.pos < tui.input.len) {
                memmove(tui.input.buf + tui.input.pos,
                        tui.input.buf + tui.input.pos + 1,
                        tui.input.len - tui.input.pos - 1);
                tui.input.len--;
            }
            break;

        case KEY_LEFT:
            if (tui.input.pos > 0) tui.input.pos--;
            break;

        case KEY_RIGHT:
            if (tui.input.pos < tui.input.len) tui.input.pos++;
            break;

        case KEY_HOME:
            tui.input.pos = 0;
            break;

        case KEY_END:
            tui.input.pos = tui.input.len;
            break;

        case KEY_UP:
            /* History navigation */
            if (tui.input.history_pos < 0) {
                /* Save current input */
                tui.input.history_pos = tui.input.history_count - 1;
            } else if (tui.input.history_pos > 0) {
                tui.input.history_pos--;
            }
            if (tui.input.history_pos >= 0 && tui.input.history_pos < tui.input.history_count) {
                strncpy(tui.input.buf, tui.input.history[tui.input.history_pos],
                        INPUT_BUF_SIZE - 1);
                tui.input.len = strlen(tui.input.buf);
                tui.input.pos = tui.input.len;
            }
            break;

        case KEY_DOWN:
            if (tui.input.history_pos >= 0) {
                tui.input.history_pos++;
                if (tui.input.history_pos >= tui.input.history_count) {
                    /* New input */
                    tui.input.history_pos = -1;
                    tui.input.buf[0] = '\0';
                    tui.input.len = 0;
                    tui.input.pos = 0;
                } else {
                    strncpy(tui.input.buf, tui.input.history[tui.input.history_pos],
                            INPUT_BUF_SIZE - 1);
                    tui.input.len = strlen(tui.input.buf);
                    tui.input.pos = tui.input.len;
                }
            }
            break;

        case KEY_PPAGE: /* Page Up — scroll history up */
            tui.panes[PANE_HISTORY].scroll_pos++;
            break;

        case KEY_NPAGE: /* Page Down — scroll history down */
            if (tui.panes[PANE_HISTORY].scroll_pos > 0)
                tui.panes[PANE_HISTORY].scroll_pos--;
            break;

        case '\t': /* Tab */
            if (tui.input.autocomplete_active) {
                /* Cycle through autocomplete suggestions */
                if (tui.input.autocomplete_count > 0) {
                    tui.input.autocomplete_sel =
                        (tui.input.autocomplete_sel + 1) % tui.input.autocomplete_count;

                    /* Get the actual command */
                    const char *match = tui.input.autocomplete_matches[tui.input.autocomplete_sel];
                    char cmd[256] = {0};
                    sscanf(match, "%255s", cmd);
                    if (cmd[0]) {
                        strncpy(tui.input.buf, cmd, INPUT_BUF_SIZE - 1);
                        tui.input.len = strlen(cmd);
                        tui.input.pos = tui.input.len;
                    }
                }
                tui.input.autocomplete_active = false;
            } else if (tui.input.emoji_picker_active) {
                /* Select emoji */
                if (tui.input.emoji_count > 0) {
                    const char *emoji = tui.input.emoji_results[tui.input.emoji_sel];
                    /* Replace the :name: with emoji */
                    int start = tui.input.pos;
                    while (start > 0 && tui.input.buf[start] != ':') start--;
                    if (tui.input.buf[start] == ':') {
                        int end = tui.input.pos;
                        int emoji_len = strlen(emoji);
                        int tail_len = tui.input.len - end - 1;
                        /* Remove :name: and insert emoji */
                        memmove(tui.input.buf + start + emoji_len,
                                tui.input.buf + end + 1, tail_len > 0 ? tail_len : 0);
                        memcpy(tui.input.buf + start, emoji, emoji_len);
                        tui.input.len = start + emoji_len + tail_len;
                        tui.input.pos = start + emoji_len;
                    }
                }
                tui.input.emoji_picker_active = false;
            } else {
                /* Trigger autocomplete */
                tui_autocomplete();
            }
            break;

        case 23: /* Ctrl+W — delete word */
            tui_input_delete_word();
            break;

        case 11: /* Ctrl+K — delete to end */
            tui_input_delete_to_end();
            break;

        case 3: /* Ctrl+C — quit */
            tui.running = false;
            return 1;

        case 12: /* Ctrl+L — redraw */
            clearok(stdscr, TRUE);
            refresh();
            tui_resize_panes();
            tui_redraw_history();
            tui_redraw_status();
            return 1;

        case 16: /* Ctrl+P — toggle FPS overlay */
            tui.fps_visible = !tui.fps_visible;
            tui_redraw_status();
            return 1;

        default:
            /* Regular character */
            if (ch >= 32 && ch <= 126) {
                if (tui.input.len < INPUT_BUF_SIZE - 2) {
                    tui_input_insert_char((char)ch);

                    /* Trigger autocomplete for slash commands */
                    if (ch == '/' && tui.input.pos == 1)
                        tui_autocomplete();
                }
            } else if (ch >= 256) {
                /* Extended key — check for wide chars */
                /* In ncursesw, this could be a wide char */
            }
            break;
    }

    tui_redraw_input();

    /* If history was scrolled, redraw it */
    if (ch == KEY_PPAGE || ch == KEY_NPAGE)
        tui_redraw_history();

    return 0;
}

/* ==================================================================
 *  INITIALIZATION AND CLEANUP
 * ================================================================== */

bool tui_fullscreen_init(void) {
    memset(&tui, 0, sizeof(tui));

    /* Redirect stderr to log file to prevent diagnostic leaks into ncurses display */
    tui.saved_stderr = dup(STDERR_FILENO);
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDERR_FILENO);
        close(null_fd);
    }

    /* Set locale for wide char support */
    setlocale(LC_ALL, "");

    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1); /* visible cursor */

    /* Check color support */
    if (!has_colors()) {
        endwin();
        if (tui.saved_stderr >= 0)
            dprintf(tui.saved_stderr, "Terminal does not support colors\n");
        return false;
    }

    start_color();
    use_default_colors(); /* Use terminal default for -1 */

    /* Initialize theme system (P198) */
    tui_theme_init();
    tui_load_external_skins();
    tui_apply_theme(tui_themes[tui_current_theme]);

    /* Setup signal handlers */
    signal(SIGWINCH, handle_winch);
    signal(SIGINT, handle_sigint);  /* abort streaming on Ctrl+C */

    /* Get terminal dimensions */
    getmaxyx(stdscr, tui.rows, tui.cols);
    if (tui.rows < 8 || tui.cols < 40) {
        endwin();
        if (tui.saved_stderr >= 0)
            dprintf(tui.saved_stderr, "Terminal too small (%dx%d), need at least 40x8\n",
                    tui.cols, tui.rows);
        return false;
    }

    /* Set default layout mode based on terminal width */
    if (tui.cols < 80)
        tui.layout_mode = TUI_LAYOUT_MOBILE;
    else
        tui.layout_mode = TUI_LAYOUT_NORMAL;

    /* Create layout panes (P189) */
    tui_create_windows();

    /* Initialize subsystems */
    tui_input_init();             /* P190 */
    tui_image_viewer_init();      /* P197 */
    tui_gateway_init();           /* P199 */

    /* Status defaults */
    strcpy(tui.status.mode, "idle");
    strcpy(tui.status.model, "Slermes");
    strcpy(tui.status.provider, "C");
    tui.status.max_iterations = 90;
    tui.status.budget_remaining = -1;

    /* FPS overlay init */
    tui.frame_count = 0;
    tui.fps_start_time = (double)clock() / CLOCKS_PER_SEC;
    tui.fps_visible = false;

    /* Welcome messages */
    tui_history_add(MSG_ROLE_INFO,
                    "WuBu Slermes C v" HERMES_VERSION " — Full ncurses TUI", true);
    tui_history_add(MSG_ROLE_INFO,
                    "Type /help for commands. P189-P200 fully implemented.", false);
    tui_history_add(MSG_ROLE_INFO, "", false);

    /* Redraw everything */
    tui_redraw_history();
    tui_redraw_status();
    tui_redraw_input();

    doupdate();

    /* Initialize chat composer integration */
    tui_composer_init();

    /* Initialize model picker */
    tui_model_picker_init();

    tui.running = true;
    tui.initialized = true;
    return true;
}

void tui_fullscreen_cleanup(void) {
    /* Restore stderr before ncurses cleanup */
    if (tui.saved_stderr >= 0) {
        dup2(tui.saved_stderr, STDERR_FILENO);
        close(tui.saved_stderr);
        tui.saved_stderr = -1;
    }

    /* Clean up gateway FIFO */
    if (tui.gateway.fifo_fd >= 0)
        close(tui.gateway.fifo_fd);
    unlink(RPC_FIFO_PATH);

    /* Free input history */
    for (int i = 0; i < tui.input.history_count; i++)
        free(tui.input.history[i]);

    /* Free external skin memory (first 4 are static const built-ins;
     * external skins registered at runtime are heap-allocated structs). */
    for (int i = 4; i < tui_theme_count; i++) {
        free((void *)tui_themes[i]);
        tui_themes[i] = NULL;
    }

    /* Destroy windows */
    for (int i = 0; i < PANE_COUNT; i++) {
        if (tui.panes[i].win) {
            delwin(tui.panes[i].win);
            tui.panes[i].win = NULL;
        }
    }

    endwin();
    tui.running = false;
    tui_eventpub_shutdown();

    /* Free render cache and composer */
    tui_render_cache_free();
    tui_composer_shutdown();
}

/* ==================================================================
 *  MAIN LOOP — P189-P200 all integrated
 * ================================================================== */

int tui_fullscreen_run(agent_state_t *state) {
    if (!state) return 1;
    tui.agent = state;

    if (!tui_fullscreen_init())
        return 1;

    /* Copy model info from agent state */
    strncpy(tui.status.model, state->llm.model, sizeof(tui.status.model) - 1);
    strncpy(tui.status.provider, state->llm.provider, sizeof(tui.status.provider) - 1);
    tui.status.max_iterations = state->max_iterations;
    if (state->budget) {
        double remaining = budget_tracker_remaining_cost(state->budget);
        tui.status.budget_remaining = remaining;
    } else {
        tui.status.budget_remaining = -1;
    }

    tui_redraw_status();
    doupdate();

    /* Main event loop */
    while (tui.running) {
        /* Frame counter for FPS overlay */
        tui.frame_count++;

        /* Poll gateway for incoming messages (P199) */
        if (tui.gateway.state == RPC_CONNECTED)
            tui_gateway_poll();

        /* Flush pending event publisher messages to FIFO */
        tui_eventpub_flush();

        /* Handle deferred terminal resize (F03) — async-signal-safe:
         * signal handler only sets a flag, actual ncurses calls
         * happen here in the main loop context. */
        if (g_resize_requested) {
            g_resize_requested = 0;
            endwin();
            refresh();
            clear();
            tui_resize_panes();
        }

        /* Thinking indicator + non-blocking input during streaming (T11) */
        if (tui.stream.active) {
            if (tui.panes[PANE_STATUS].win) {
                WINDOW *sw = tui.panes[PANE_STATUS].win;
                int sw_cols = tui.panes[PANE_STATUS].cols;
                double elapsed = ((double)clock() / CLOCKS_PER_SEC) - tui.stream.start_time;
                char think_buf[96];
                
                if (tui.stream.first_token) {
                    /* Before first token: rich animated thinking indicator */
                    /* Multi-frame spinner with phase labels */
                    static const char spin[] = {'|', '/', '-', '\\'};
                    int frame = tui.think_frame % 4;
                    
                    /* Animated ellipsis: cycles through . .. ... patterns */
                    int dot_phase = tui.think_frame % 12;
                    const char *dots;
                    if (dot_phase < 3)       dots = ".   ";  /* Phase 0-2: single dot */
                    else if (dot_phase < 6)  dots = "..  ";  /* Phase 3-5: two dots */
                    else if (dot_phase < 9)  dots = "... ";  /* Phase 6-8: three dots */
                    else                     dots = " .. ";  /* Phase 9-11: two dots (return) */
                    
                    /* Phase label changes with elapsed time */
                    const char *phase;
                    if (elapsed < 2.0)       phase = "think";
                    else if (elapsed < 5.0)  phase = "ponder";
                    else if (elapsed < 10.0) phase = "deep";
                    else                     phase = "focus";
                    
                    snprintf(think_buf, sizeof(think_buf), " %c %s%s %4.1fs  ", 
                             spin[frame], phase, dots, elapsed);
                } else {
                    /* After first token: live token counter + tok/s */
                    double tps = (elapsed > 0.1) ? tui.stream.token_count / elapsed : 0.0;
                    /* Animated receiving indicator: arrow pulses */
                    const char *recv_chr = (tui.think_frame % 8 < 3) ? ">" : "=>";
                    snprintf(think_buf, sizeof(think_buf), " %s t:%d %.0f/s  ", 
                             recv_chr, tui.stream.token_count, tps);
                }
                
                int buf_start = sw_cols - (int)strlen(think_buf) - 1;
                if (buf_start < 0) buf_start = 0;
                mvwprintw(sw, 0, buf_start, "%s", think_buf);
                wnoutrefresh(sw);
                doupdate();
            }
            tui.think_frame++;

            /* Non-blocking input check: process abort/commands during streaming */
            int ch = wgetch(tui.panes[PANE_INPUT].win);
            if (ch == 3) { /* Ctrl+C — abort streaming */
                tui.stream.abort_requested = true;
                tui_history_add(MSG_ROLE_WARN, "[Aborted by user]", true);
                tui_fullscreen_stream_done();
                tui_redraw_history();
                tui_redraw_status();
                continue;
            } else if (ch == ERR) {
                /* No input available — sleep a bit then continue loop */
                napms(50);
                continue;
            } else if (ch >= 32 && ch <= 126) {
                /* Type-ahead: buffer character for after streaming ends (T18) */
                if (tui.stream.type_ahead_len < TYPE_AHEAD_BUF_SIZE - 1) {
                    tui.stream.type_ahead_buf[tui.stream.type_ahead_len++] = (char)ch;
                    tui.stream.type_ahead_buf[tui.stream.type_ahead_len] = '\0';
                }
                beep();
            }
            continue; /* skip normal input processing during streaming */
        }

        /* Get input from the input pane */
        int ch = wgetch(tui.panes[PANE_INPUT].win);
        if (ch == ERR) continue;

        /* Handle modal overlays first */
        if (tui.modal_mode != MODE_NORMAL) {
            if (tui_handle_modal_input(ch))
                continue; /* modal handled the input, refresh loop */
            /* Redraw modal after input */
            switch (tui.modal_mode) {
                case MODE_SESSION_BROWSE: tui_draw_session_browser(); break;
                case MODE_CONFIG_EDIT:    tui_draw_config_editor(); break;
                case MODE_IMAGE_VIEW:     tui_draw_image_viewer(); break;
                case MODE_GATEWAY_STATUS: tui_draw_gateway_status(); break;
                case MODE_CRON_VIEW:      tui_draw_cron_viewer(); break;
                case MODE_LOG_VIEW:       tui_draw_log_viewer(); break;
                case MODE_SKILL_BROWSE:  tui_draw_skill_browser(); break;
                case MODE_HELP:           break; /* help stays until dismissed */
                case MODE_TODO_PANEL:      tui_draw_todo_panel(); break;
                case MODE_AGENT_INFO:      { if (ch == 'q' || ch == 27) { tui.modal_mode = MODE_NORMAL; continue; } tui_draw_agent_info(); break; }
                case MODE_PLUGIN_HUB:      tui_draw_plugin_hub(); break;
                default: break;
            }
            continue;
        }

        /* Handle normal input */
        tui_handle_input(ch);

        /* Periodic status bar refresh */
        if (tui.status.dirty) {
            tui_redraw_status();
            tui.status.dirty = false;
        }
    }

    tui_fullscreen_cleanup();
    return 0;
}

/* ── Session Lifecycle ── */
/* Port of Python: tui_gateway.server — session.create / session.resume / session.close */

static char g_session_key[128] = {0};
static char g_active_model[256] = {0};
static bool g_agent_build_started = false;
static bool g_agent_ready = false;
static char g_session_cwd[1024] = {0};

typedef struct {
    const char *event_type;
    tui_session_hook_t cb;
    void *userdata;
    bool active;
} session_hook_entry_t;

#define MAX_SESSION_HOOKS 16
static session_hook_entry_t g_session_hooks[MAX_SESSION_HOOKS];
static int g_session_hook_count = 0;

void tui_fullscreen_session_create(const char *session_key, const char *model) {
    if (session_key)
        strncpy(g_session_key, session_key, sizeof(g_session_key) - 1);
    if (model)
        strncpy(g_active_model, model, sizeof(g_active_model) - 1);

    /* Emit session start event */
    tui_eventpub_session(g_session_key, "started");
    tui_eventpub_flush();

    /* Notify session boundary hooks */
    tui_fullscreen_notify_boundary("on_session_start", g_session_key);

    /* Set cwd from environment */
    const char *cwd = getenv("TERMINAL_CWD");
    if (cwd)
        strncpy(g_session_cwd, cwd, sizeof(g_session_cwd) - 1);
    else if (getcwd(g_session_cwd, sizeof(g_session_cwd)) == NULL)
        g_session_cwd[0] = '\0';

    g_agent_build_started = false;
    g_agent_ready = false;
}

void tui_fullscreen_session_resume(const char *session_key) {
    if (session_key)
        strncpy(g_session_key, session_key, sizeof(g_session_key) - 1);

    tui_eventpub_session(g_session_key, "resumed");
    tui_eventpub_flush();
    tui_fullscreen_notify_boundary("on_session_resume", g_session_key);
}

void tui_fullscreen_session_close(const char *end_reason) {
    (void)end_reason;

    /* Finalize first */
    tui_fullscreen_session_finalize();

    /* Emit session end event */
    tui_eventpub_session(g_session_key, "ended");
    tui_eventpub_flush();
    tui_fullscreen_notify_boundary("on_session_end", g_session_key);

    g_session_key[0] = '\0';
}

void tui_fullscreen_session_finalize(void) {
    if (!g_session_key[0]) return;
    tui_fullscreen_notify_boundary("on_session_finalize", g_session_key);
}

/* ── Deferred Agent Factory ── */
/* Port of Python: tui_gateway.server._start_agent_build / _wait_agent */

void tui_fullscreen_start_agent_build(void) {
    if (g_agent_build_started) return;
    g_agent_build_started = true;

    /* In a full implementation, this would spawn a background thread,
     * create the AIAgent, wire callbacks, and wait for the first prompt.
     * For now, mark ready and emit session.info.
     */
    tui_fullscreen_status_update(g_active_model, "resolved", 0, 90, 0, 0, 0.0);
    g_agent_ready = true;

    tui_fullscreen_notify_boundary("on_session_reset", g_session_key);
}

bool tui_fullscreen_wait_agent(int timeout_ms) {
    (void)timeout_ms;
    return g_agent_ready;
}

/* ── Model Switch ── */
/* Port of Python: tui_gateway.server._resolve_model / _apply_model_switch */

static char g_resolved_model[256] = {0};

const char *tui_fullscreen_resolve_model(void) {
    if (g_resolved_model[0]) return g_resolved_model;

    const char *env = getenv("HERMES_MODEL");
    if (env && env[0]) {
        strncpy(g_resolved_model, env, sizeof(g_resolved_model) - 1);
        return g_resolved_model;
    }
    env = getenv("HERMES_INFERENCE_MODEL");
    if (env && env[0]) {
        strncpy(g_resolved_model, env, sizeof(g_resolved_model) - 1);
        return g_resolved_model;
    }

    /* Fallback to config file */
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        char cfg_path[512];
        snprintf(cfg_path, sizeof(cfg_path), "%s/.slermes/config.yaml", home);
        /* Parse model from yaml — for now use default */
        strncpy(g_resolved_model, "anthropic/claude-sonnet-4",
                sizeof(g_resolved_model) - 1);
        return g_resolved_model;
    }

    strncpy(g_resolved_model, "anthropic/claude-sonnet-4",
            sizeof(g_resolved_model) - 1);
    return g_resolved_model;
}

bool tui_fullscreen_switch_model(const char *model_spec, bool persist) {
    if (!model_spec || !*model_spec) return false;

    strncpy(g_active_model, model_spec, sizeof(g_active_model) - 1);
    strncpy(g_resolved_model, model_spec, sizeof(g_resolved_model) - 1);

    if (persist) {
        /* Persist the chosen model to the slermes home so it survives restart. */
        const char *home = slermes_home();
        char mp[1024];
        snprintf(mp, sizeof(mp), "%s/active_model.txt", home);
        FILE *mf = fopen(mp, "w");
        if (mf) { fprintf(mf, "%s\n", model_spec); fclose(mf); }
        tui_fullscreen_print("Model switched to %s (persisted)\n", model_spec);
    } else {
        tui_fullscreen_print("Model switched to %s (session only)\n", model_spec);
    }

    tui_eventpub_model(g_active_model, "");
    return true;
}

/* ── Secret Prompting ── */
/* Port of Python: tui_gateway.server._block / _clear_pending */

/* Maximum number of pending secret prompts */
#define MAX_PENDING_PROMPTS 8

typedef struct {
    char request_id[32];
    char prompt_text[512];
    char answer[512];
    bool answered;
} pending_prompt_t;

static pending_prompt_t g_pending_prompts[MAX_PENDING_PROMPTS];
static int g_pending_count = 0;

char *tui_fullscreen_prompt_secret(const char *prompt_text, int timeout_ms) {
    if (!prompt_text || g_pending_count >= MAX_PENDING_PROMPTS) return NULL;

    /* Register the pending prompt */
    pending_prompt_t *pp = &g_pending_prompts[g_pending_count];
    snprintf(pp->request_id, sizeof(pp->request_id), "prompt_%d", g_pending_count);
    strncpy(pp->prompt_text, prompt_text, sizeof(pp->prompt_text) - 1);
    pp->answered = false;
    pp->answer[0] = '\0';
    g_pending_count++;

    /* Show prompt to user in TUI — this is a simplified blocking version.
     * In the full Python version, this emits an event and waits for
     * approval.respond on stdin.
     */
    echo();
    curs_set(1);
    nocbreak();

    /* Draw prompt overlay */
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (win) {
        int rows = tui.panes[PANE_HISTORY].rows;
        (void)rows;
        int prompt_y = rows > 2 ? rows - 2 : 0;

        wattron(win, A_BOLD | COLOR_PAIR(7));
        mvwprintw(win, prompt_y, 0, " %s: ", prompt_text);
        wattroff(win, A_BOLD | COLOR_PAIR(7));
        wclrtoeol(win);

        char input_buf[512] = {0};
        int input_pos = 0;
        int ch;

        while ((ch = wgetch(win)) != '\n' && ch != KEY_ENTER && ch != 27) {
            if (ch == KEY_BACKSPACE || ch == 127) {
                if (input_pos > 0) input_pos--;
            } else if (ch >= 32 && ch <= 126 && input_pos < (int)sizeof(input_buf) - 1) {
                input_buf[input_pos++] = (char)ch;
            }
            input_buf[input_pos] = '\0';
            mvwprintw(win, prompt_y, strlen(prompt_text) + 3, "%-40s",
                      input_buf);
        }

        /* Hide the input echo */
        noecho();
        curs_set(0);
        cbreak();

        if (ch == 27) {
            /* Esc — cancelled */
            pp->answer[0] = '\0';
        } else {
            strncpy(pp->answer, input_buf, sizeof(pp->answer) - 1);
        }
        pp->answered = true;
    }

    noecho();
    curs_set(0);
    cbreak();

    return strdup(pp->answer);
}

void tui_fullscreen_clear_prompts(void) {
    for (int i = 0; i < g_pending_count; i++) {
        g_pending_prompts[i].answered = true;
        g_pending_prompts[i].answer[0] = '\0';
    }
    g_pending_count = 0;
}

/* ── Approval Flow ── */
/* Port of Python: tui_gateway.server — approval.request / clarify/sudo */

bool tui_fullscreen_request_approval(const char *tool_name,
                                      const char *args_preview) {
    char prompt_text[1024];
    snprintf(prompt_text, sizeof(prompt_text),
             "Allow %s(%s)? [y/N]", tool_name,
             args_preview ? args_preview : "");

    char *answer = tui_fullscreen_prompt_secret(prompt_text, 30000);
    if (!answer) return false;

    bool approved = (answer[0] == 'y' || answer[0] == 'Y');
    free(answer);
    return approved;
}

char *tui_fullscreen_request_clarify(const char *question) {
    return tui_fullscreen_prompt_secret(question, 60000);
}

/* ── Session Boundary Hooks ── */
/* Port of Python: tui_gateway.server._notify_session_boundary */

bool tui_fullscreen_register_hook(const char *event_type,
                                   tui_session_hook_t cb,
                                   void *userdata) {
    if (!event_type || !cb) return false;
    if (g_session_hook_count >= MAX_SESSION_HOOKS) return false;

    g_session_hooks[g_session_hook_count].event_type = event_type;
    g_session_hooks[g_session_hook_count].cb = cb;
    g_session_hooks[g_session_hook_count].userdata = userdata;
    g_session_hooks[g_session_hook_count].active = true;
    g_session_hook_count++;
    return true;
}

void tui_fullscreen_notify_boundary(const char *event_type,
                                     const char *session_id) {
    if (!event_type) return;

    /* Fire matching hooks */
    for (int i = 0; i < g_session_hook_count; i++) {
        if (!g_session_hooks[i].active) continue;
        if (strcmp(g_session_hooks[i].event_type, event_type) == 0 ||
            strcmp(g_session_hooks[i].event_type, "*") == 0) {
            g_session_hooks[i].cb(event_type, session_id,
                                  g_session_hooks[i].userdata);
        }
    }

    /* Also emit session boundary event via eventpub */
    if (session_id && session_id[0]) {
        if (strcmp(event_type, "on_session_start") == 0 ||
            strcmp(event_type, "on_session_resume") == 0) {
            tui_eventpub_session(session_id, "started");
        } else if (strcmp(event_type, "on_session_end") == 0 ||
                   strcmp(event_type, "on_session_finalize") == 0) {
            tui_eventpub_session(session_id, "ended");
        }
        tui_eventpub_flush();
    }
}
