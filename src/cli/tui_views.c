/*
 * tui_views.c - TUI modal views.
 *
 * Split from tui_fullscreen.c (soul.md: self-contained modules). Each view
 * renders one modal pane. No file-scope state of its own.
 */
#include "tui_views.h"
#include "hermes_cli.h"
#include "hermes_agent.h"
#include "hermes_core_types.h"
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
#include "tui_state.h"


/* ==================================================================
 *  P193: TOOL FEED — live tool call display
 * ================================================================== */

/* ── Shelf group classifier ───────────────────────────── */
/* Derive a shelf group name from a tool name (e.g. "read_file" → "📂 File Ops") */ const char *tui_tool_shelf(const char *tool_name) {
    if (!tool_name || !*tool_name) return "⚙ Tools";
    /* Known tool name prefixes mapped to shelf groups */
    struct { const char *prefix; const char *shelf; } map[] = {
        {"read_",      "📂 File Ops"},
        {"write_",     "📂 File Ops"},
        {"edit_",      "📂 File Ops"},
        {"delete_",    "📂 File Ops"},
        {"file_",      "📂 File Ops"},
        {"append_",    "📂 File Ops"},
        {"create_",    "📂 File Ops"},
        {"mkdir_",     "📂 File Ops"},
        {"ls_",        "📂 File Ops"},
        {"mv_",        "📂 File Ops"},
        {"cp_",        "📂 File Ops"},
        {"glob_",      "📂 File Ops"},
        {"search_",    "🔍 Search"},
        {"grep_",      "🔍 Search"},
        {"find_",      "🔍 Search"},
        {"lookup_",    "🔍 Search"},
        {"web_",       "🌐 Web"},
        {"fetch_",     "🌐 Web"},
        {"http_",      "🌐 Web"},
        {"url_",       "🌐 Web"},
        {"bash",       "💻 Shell"},
        {"sh_",        "💻 Shell"},
        {"shell_",     "💻 Shell"},
        {"python_",    "💻 Shell"},
        {"exec_",      "💻 Shell"},
        {"git_",       "🔧 Git"},
        {"npm_",       "🔧 Build"},
        {"make_",      "🔧 Build"},
        {"build_",     "🔧 Build"},
        {"compile_",   "🔧 Build"},
        {"test_",      "🧪 Test"},
        {"run_",       "🧪 Test"},
        {"db_",        "🗄 DB"},
        {"sql_",       "🗄 DB"},
        {"redis_",     "🗄 DB"},
        {"query_",     "🗄 DB"},
        {"memory_",    "🧠 Memory"},
        {"remember_",  "🧠 Memory"},
        {"recall_",    "🧠 Memory"},
        {"image_",     "🖼 Image"},
        {"audio_",     "🎵 Audio"},
        {"tts_",       "🎵 Audio"},
        {"speech_",    "🎵 Audio"},
        {"code_",      "💻 Code"},
        {"think_",     "💡 Think"},
        {"reason_",    "💡 Think"},
        {"plan_",      "💡 Think"},
        {"list_",      "📋 List"},
        {"show_",      "📋 List"},
        {"get_",       "📋 List"},
        {"chat_",      "💬 Chat"},
        {"send_",      "💬 Chat"},
        {"message_",   "💬 Chat"},
        {"notify_",    "💬 Chat"},
        {"kanban_",    "📋 Kanban"},
        {"todo_",      "📋 Kanban"},
        {"task_",      "📋 Kanban"},
        {"board_",     "📋 Kanban"},
        {"mcp_",       "🔌 MCP"},
        {"tool_",      "🔌 Tools"},
        {"plugin_",    "🔌 Tools"},
        {"delegate_",  "🤖 Agent"},
        {"subagent_",  "🤖 Agent"},
        {"agent_",     "🤖 Agent"},
        {"spawn_",     "🤖 Agent"},
        {"fork_",      "🤖 Agent"},
        {"api_",       "🌐 API"},
        {"rest_",      "🌐 API"},
        {"json_",      "🌐 API"},
        {"yaml_",      "🌐 API"},
        {"config_",    "⚙ Config"},
        {"set_",       "⚙ Config"},
        {"toggle_",    "⚙ Config"},
        {"enable_",    "⚙ Config"},
        {"disable_",   "⚙ Config"},
        {"cron_",      "⏰ Cron"},
        {"schedule_",  "⏰ Cron"},
        {"task_",      "⏰ Cron"},
        {NULL, NULL}
    };
    /* Also check if tool name contains certain keywords */
    for (int i = 0; map[i].prefix; i++) {
        if (strncmp(tool_name, map[i].prefix, strlen(map[i].prefix)) == 0)
            return map[i].shelf;
    }
    /* Fallback: first segment before '_' or '.' */
    static char fallback[48];
    const char *sep = strchr(tool_name, '_');
    if (!sep) sep = strchr(tool_name, '.');
    if (sep && (size_t)(sep - tool_name) < sizeof(fallback) - 3) {
        int len = (int)(sep - tool_name);
        memcpy(fallback, tool_name, len);
        fallback[len] = 0;
        /* Capitalise first char */
        if (fallback[0] >= 'a' && fallback[0] <= 'z')
            fallback[0] -= 32;
        return fallback;
    }
    return "⚙ Tools";
}


/* Draw session browser overlay */

/* Handle session browser input */

/* Activate session browser — P195 */

/* ==================================================================
 *  P196: CONFIG EDITOR
 * ================================================================== */

/* Initialize config entries */

/* Draw config editor overlay */

/* Handle config editor input */

/* Activate config editor — P196 */
void tui_fullscreen_config_edit(void) {
    tui.modal_mode = MODE_CONFIG_EDIT;
    tui_config_editor_init();
}


/* ==================================================================
 *  P197: IMAGE VIEWER — detect and use sixel/kitty protocol
 * ================================================================== */

/* Detect terminal image protocol support */

/* Display image via sixel escape codes — P197 */

/* Display image via kitty protocol — P197 */

/* P197: Display image — try kitty first, then sixel */

/* P197: Image viewer modal keyboard handler */

/* P197: Draw image viewer modal overlay */

/* ==================================================================
 *  P199: GATEWAY — JSON-RPC backend via FIFO
 * ================================================================== */

/* Draw gateway status dashboard — P199 */ void tui_draw_gateway_status(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " GATEWAY STATUS ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    int y = 2;
    mvwprintw(win, y++, 0, " RPC Status:     %s",
              tui.gateway.state == RPC_CONNECTED ? "Connected" : "Idle");
    mvwprintw(win, y++, 0, " FIFO Path:      %s", RPC_FIFO_PATH);
    mvwprintw(win, y++, 0, " FIFO FD:        %d", tui.gateway.fifo_fd);
    mvwprintw(win, y++, 0, " Read Buffer:    %d/%d bytes",
              tui.gateway.read_pos, (int)sizeof(tui.gateway.read_buf));
    y++;
    mvwprintw(win, y++, 0, " Agent:          %s",
              tui.agent ? "Available" : "Not available");
    if (tui.agent && tui.agent->db) {
        mvwprintw(win, y++, 0, " Session DB:     Connected");
    } else {
        mvwprintw(win, y++, 0, " Session DB:     Not connected");
    }
    y++;
    mvwprintw(win, y++, 0, " Platforms:      19 registered (in binary)");
    mvwprintw(win, y++, 0, " Model:          %s", tui.status.model);
    mvwprintw(win, y++, 0, " Provider:       %s", tui.status.provider);

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}


/* Draw cron job viewer overlay — U07 */ void tui_draw_cron_viewer(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " CRON JOBS ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    int y = 2;
    mvwprintw(win, y++, 0, " Cron Dir:       %s",
              "default");
    mvwprintw(win, y++, 0, " Max Jobs:       %d", 5);
    mvwprintw(win, y++, 0, " Job Timeout:    %ds", 300);
    mvwprintw(win, y++, 0, " Retention:      %d days", 7);
    mvwprintw(win, y++, 0, " Notify Failure: %s", "No");
    mvwprintw(win, y++, 0, " Poll Interval:  %ds", 60);
    y++;
    mvwprintw(win, y++, 0, " Use /cron list or cron CLI to manage jobs");
    mvwprintw(win, y++, 0, " Use hermes cron:add <name> <schedule> <cmd>");

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}


/* Draw log viewer overlay — U08 */ void tui_draw_log_viewer(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " LOG VIEWER ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    char log_dir[512] = "";
    /* Look for logs in ~/.slermes/ */
    const char *lh = getenv("SLERMES_HOME");
    if (!lh) lh = getenv("HOME");
    if (lh) snprintf(log_dir, sizeof(log_dir), "%s/.slermes", lh);
    else snprintf(log_dir, sizeof(log_dir), "~/.slermes");

    int y = 2;
    mvwprintw(win, y++, 0, " Log Dir:  %s", log_dir);
    mvwprintw(win, y++, 0, " Files:    agent.log, errors.log, gateway.log");
    y++;

    /* Try to show tail of agent.log */
    char log_path[512 + 32];
    snprintf(log_path, sizeof(log_path), "%s/agent.log", log_dir);
    FILE *f = fopen(log_path, "r");
    if (f) {
        /* Seek near end and read last N lines */
        char buf[2048];
        int lines_shown = 0;
        int max_lines = w_rows - y - 2;

        /* Go to end, read backwards for lines */
        fseek(f, 0, SEEK_END);
        long pos = ftell(f);
        long read_start = pos - (max_lines * 80); /* estimate ~80 chars per line */
        if (read_start < 0) read_start = 0;
        fseek(f, read_start, SEEK_SET);

        /* Read from estimated position */
        while (fgets(buf, sizeof(buf), f) && lines_shown < max_lines) {
            /* Strip newline */
            size_t blen = strlen(buf);
            if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
            if (blen > 0 && buf[blen-2] == '\r') buf[blen-2] = '\0';

            /* Truncate long lines to fit terminal width */
            if ((int)strlen(buf) >= w_cols - 1)
                buf[w_cols - 4] = '\0';

            mvwprintw(win, y, 0, " %s", buf);
            y++;
            lines_shown++;
        }
        fclose(f);

        if (lines_shown == 0) {
            mvwprintw(win, y++, 0, " (empty log)");
        }
    } else {
        mvwprintw(win, y++, 0, " (no agent.log found)");
    }

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}


/* Draw skill browser overlay — U03 */

/* ==================================================================
 *  T15: TODO PANEL — kanban task board overlay
 * ================================================================== */

/* Initialize todo panel — fetch tasks from kanban */

/* Draw todo panel overlay */

/* Handle todo panel input */

/* ==================================================================
 *  T14: AGENT INFO — display current agent configuration
 * ================================================================== */ void tui_draw_agent_info(void) {
    if (!tui.agent) {
        tui_history_add(MSG_ROLE_WARN, "No agent state available", false);
        tui.modal_mode = MODE_NORMAL;
        return;
    }

    /* Calculate overlay dimensions */
    int rows = tui.rows;
    int cols = tui.cols;
    int overlay_h = 16;
    int overlay_w = (cols > 52) ? 50 : cols - 2;
    int start_y = (rows - overlay_h) / 2;
    int start_x = (cols - overlay_w) / 2;
    if (start_y < 0) start_y = 0;
    if (start_x < 0) start_x = 0;

    /* Create overlay window */
    WINDOW *win = newwin(overlay_h, overlay_w, start_y, start_x);
    if (!win) return;
    keypad(win, TRUE);

    /* Draw border and title */
    box(win, 0, 0);
    mvwprintw(win, 0, 2, "[ Agent Info ]");

    /* Gather agent info */
    agent_state_t *a = tui.agent;

    mvwprintw(win, 2, 2, "Model:      %s", a->llm.model[0] ? a->llm.model : "(none)");
    mvwprintw(win, 3, 2, "Provider:   %s", a->llm.provider[0] ? a->llm.provider : "(none)");
    mvwprintw(win, 4, 2, "Session ID: %s", a->session_id);

    mvwprintw(win, 6, 2, "Iterations: %d / %d", a->iteration_count, a->max_iterations);
    mvwprintw(win, 7, 2, "Messages:   %zu", a->message_count);
    mvwprintw(win, 8, 2, "Tools:      %zu registered", a->tools.count);

    /* Token stats */
    mvwprintw(win, 10, 2, "Input Tokens:  %d", a->session_input_tokens);
    mvwprintw(win, 11, 2, "Output Tokens: %d", a->session_output_tokens);
    mvwprintw(win, 12, 2, "Total Tokens:  %d", a->session_total_tokens);

    /* Budget */
    if (a->budget) {
        double remaining = budget_tracker_remaining_cost(a->budget);
        if (remaining >= 0)
            mvwprintw(win, 13, 2, "Budget:     $%.4f remaining", remaining);
        else
            mvwprintw(win, 13, 2, "Budget:     unlimited");
    } else {
        mvwprintw(win, 13, 2, "Budget:     (none)");
    }

    mvwprintw(win, 14, 2, "JSON Mode:  %s", a->llm.json_mode ? "on" : "off");

    /* Footer */
    mvwprintw(win, overlay_h - 2, 2, "Press q or ESC to close");

    wnoutrefresh(win);
    doupdate();
    delwin(win);
}
 void tui_draw_skill_browser(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;
    (void)w_cols;

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " SKILL BROWSER ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    /* Determine skill directory */
    const char *slermes_home = getenv("SLERMES_HOME");
    if (!slermes_home) slermes_home = getenv("HOME");
    char skill_dir[512];
    if (slermes_home) {
        snprintf(skill_dir, sizeof(skill_dir), "%s/.slermes/skills", slermes_home);
    } else {
        snprintf(skill_dir, sizeof(skill_dir), "~/.slermes/skills");
    }

    int y = 2;
    mvwprintw(win, y++, 0, " Skills Dir: %s", skill_dir);
    y++;

    /* Scan directory for skill files */
    DIR *d = opendir(skill_dir);
    if (d) {
        struct dirent *entry;
        int count = 0;
        while ((entry = readdir(d)) != NULL && y < w_rows - 3) {
            /* Skip hidden and non-directory entries */
            if (entry->d_name[0] == '.') continue;
            if (!hermes_dirent_is_dir(skill_dir, entry->d_name) && !hermes_dirent_is_file(skill_dir, entry->d_name)) continue;
            /* Only show .md files and directories */
            size_t nlen = strlen(entry->d_name);
            if (hermes_dirent_is_file(skill_dir, entry->d_name) && (nlen < 3 || strcmp(entry->d_name + nlen - 3, ".md") != 0))
                continue;

            char display[256];
            if (hermes_dirent_is_dir(skill_dir, entry->d_name)) {
                snprintf(display, sizeof(display), " [%s]", entry->d_name);
            } else {
                snprintf(display, sizeof(display), "   %s", entry->d_name);
            }
            mvwprintw(win, y, 0, " %-60s", display);
            y++;
            count++;
        }
        closedir(d);
        if (count == 0) {
            mvwprintw(win, y++, 0, " (no skills found)");
        }
    } else {
        mvwprintw(win, y++, 0, " (cannot access skills directory)");
    }

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}


/* ==================================================================
 *  HELP OVERLAY (P190)
 * ================================================================== */

static void tui_draw_help(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " HERMES TUI HELP ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    int y = 2;
    mvwprintw(win, y++, 0, " Navigation:");
    mvwprintw(win, y++, 0, "   Tab           Switch between panes");
    mvwprintw(win, y++, 0, "   PgUp/PgDn     Scroll history pane");
    mvwprintw(win, y++, 0, "   Ctrl+C        Abort streaming / Quit (at prompt)");
    mvwprintw(win, y++, 0, "   Ctrl+L        Redraw screen");
    mvwprintw(win, y++, 0, "   Ctrl+P        Toggle FPS overlay");
    y++;
    mvwprintw(win, y++, 0, " Input:");
    mvwprintw(win, y++, 0, "   Enter         Send message");
    mvwprintw(win, y++, 0, "   Ctrl+W        Delete word");
    mvwprintw(win, y++, 0, "   Ctrl+K        Delete to end of line");
    mvwprintw(win, y++, 0, "   Tab           Autocomplete slash commands");
    mvwprintw(win, y++, 0, "   Up/Down       History navigation");
    y++;
    mvwprintw(win, y++, 0, " Slash Commands:");
    const slash_cmd_entry_t **all_cmds = tui_slash_get_all();
    for (int i = 0; all_cmds[i] != NULL; i++) {
        mvwprintw(win, y++, 0, "   %-15s %s %s",
                  all_cmds[i]->cmd,
                  all_cmds[i]->desc,
                  all_cmds[i]->args);
    }

    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, tui.panes[PANE_HISTORY].rows - 1, 0, " Press any key to close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}
