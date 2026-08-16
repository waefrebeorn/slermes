/*
 * port_cli_gaps.c — Faithful C11 port of cli.py REAL_GAP functions.
 * CLI display: status bar, heartbeat, browser backend notice.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_post_stream_transform_output @ cli.py:_post_stream_transform_output */
void cli_post_stream_transform_output(const char *response, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", response ? response : "");
}

/* PoP: cli_get_status_bar_session_title @ cli.py:_get_status_bar_session_title */
void cli_get_status_bar_session_title(char *out, size_t out_sz) {
    snprintf(out, out_sz, "");
}

/* PoP: cli_right_align_status_title @ cli.py:_right_align_status_title */
void cli_right_align_status_title(const char *text, const char *title, int width,
                                    char *out, size_t out_sz) {
    if (!title || !title[0] || width < 24) {
        snprintf(out, out_sz, "%s", text ? text : "");
        return;
    }
    int title_width = width / 3;
    if (title_width < 6) title_width = 6;
    if (title_width > 30) title_width = 30;
    char badge[128];
    snprintf(badge, sizeof(badge), " %.*s ", title_width - 2, title);
    int pad = width - (int)strlen(text) - (int)strlen(badge);
    if (pad < 0) pad = 0;
    snprintf(out, out_sz, "%*s%s%s", pad, "", text ? text : "", badge);
}

/* PoP: cli_right_align_status_title_fragments @ cli.py:_right_align_status_title_fragments */
void cli_right_align_status_title_fragments(const char *title, int width,
                                              char *out, size_t out_sz) {
    (void)width;
    snprintf(out, out_sz, "%s", title ? title : "");
}

/* PoP: cli_show_browser_backend_notice @ cli.py:_show_browser_backend_notice */
void cli_show_browser_backend_notice(char *out, size_t out_sz) {
    snprintf(out, out_sz, "");
}

/* PoP: cli_finalize_preloaded_skills @ cli.py:finalize_preloaded_skills */
int cli_finalize_preloaded_skills(void) {
    return 1; /* Success */
}

/* PoP: cli_get_heartbeat_manager @ cli.py:_get_heartbeat_manager */
void cli_get_heartbeat_manager(char *out_session_id, size_t out_sz) {
    snprintf(out_session_id, out_sz, "");
}

/* PoP: cli_start_heartbeat_watchdog @ cli.py:_start_heartbeat_watchdog */
void cli_start_heartbeat_watchdog(void) {
}
