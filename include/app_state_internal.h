/*
 * app_state_internal.h — Private internal definitions for app_state
 *
 * This header contains the full struct definitions needed by modules
 * that directly manipulate app_state internals. Do NOT include this
 * in public headers — only in .c files that need direct access.
 */

#ifndef APP_STATE_INTERNAL_H
#define APP_STATE_INTERNAL_H

#include "app_state.h"
#include "app_session_entry.h"
#include "gui_core.h"
#include "slermes_home.h"
#include "sqlite3.h"
#include <stdbool.h>
#include <stddef.h>

/* ══════════════════════════════════════════════════════════════════════
 * Internal Data Structures (full definitions)
 * ══════════════════════════════════════════════════════════════════════ */

struct message_entry {
    char role[32];
    char content[65536];
    long timestamp;
};

struct nav_item {
    const char *icon;
    const char *label;
    int view_id;
};

/* Full app_state struct definition */
struct app_state {
    sqlite3    *db;
    char        state_db_path[512];
    char        latest_model[128];
    bool        running;

    app_session_entry_t sessions[MAX_SESSIONS];
    int                 session_count;
    int                 selected_session;

    message_entry_t messages[MAX_MESSAGES];
    int             message_count;

    int  selected_nav;
    int  current_view;
    char current_view_name[64];

    char skill_names[MAX_ITEMS][128];
    int  skill_count;
    char profile_names[MAX_ITEMS][64];
    int  profile_count;
    char cron_names[MAX_ITEMS][128];
    int  cron_count;

    /* Hover for every interactive element */
    int hover_nav, hover_session, hover_tool;
    int hover_newchat, hover_pill, hover_profile, hover_search;
    bool mouse_in_sidebar, mouse_in_chat;
    bool mouse_in_titlebar, mouse_in_statusbar;
    bool composer_hover;

    /* Bubble position tracking for hover-detection */
    int bubble_y[MAX_MESSAGES], bubble_h[MAX_MESSAGES];
    int hover_message;
    int hover_action; /* 0=copy, 1=edit, -1=none */

    int  sidebar_scroll, sidebar_content_h;
    int  chat_scroll, chat_content_h;

    int  total_sessions;
    int  total_messages;

    /* Section expand state */
    bool sessions_expanded;
    bool nav_expanded;

    /* Sidebar search */
    char search_query[64];
    int  search_query_len;
    bool search_active;

    /* Theme */
    bool dark_mode;

    /* Sidebar collapsed state (titlebar toggle) */
    bool sidebar_collapsed;

    /* Haptics muted (titlebar toggle) */
    bool haptics_muted;

    gc_window_t *win;
    gc_theme_t   theme;

    /* Composer state */
    char        composer_buf[4096];
    int         composer_pos;
    bool        composer_focused;
    bool        api_busy;
    char        api_status[128];

    /* Session pinning */
    int  pinned_session_ids[MAX_SESSIONS];
    int  pinned_session_count;

    /* Model picker overlay */
    bool        show_model_picker;
    int         model_picker_hover;
    int         model_picker_scroll;

    /* Scroll-to-bottom button */
    bool        show_scroll_button;
    bool        scroll_button_hover;

    /* Session header */
    bool        show_session_menu;
    int         session_menu_hover; /* 0=pin, 1=delete, 2=archive */

    /* Load more */
    bool        sessions_loaded_all;
    int         sessions_page;

    /* Gateway status */
    bool        gateway_connected;
    char        gateway_status_text[64];

    /* Session export/import */
    char        export_path[512];
    bool        show_export_dialog;
    bool        show_import_dialog;
    char        import_path[512];
    int         import_path_len;

    /* Notification toast */
    char        toast_msg[256];
    int         toast_time;

    /* Petdex */
    bool        pet_active;
    int         pet_type;
    int         pet_frame;
    int         pet_frame_tick;
    float       pet_x, pet_y, pet_vx, pet_vy;
    float       pet_scale;
    bool        pet_show_gallery;
    int         pet_count;
    int         pet_selected;
    char        pet_names[16][32];
    long        frame_count;

    /* Voice */
    bool        voice_active;
    bool        voice_recording;
    int         voice_tts_pending;

    /* Command palette */
    bool        show_command_palette;
    char        command_palette_query[128];
    int         command_palette_query_len;
    int         command_palette_selected;
    int         command_palette_result_count;

    /* Notification count */
    int         notification_count;
    char        notification_history[32][256];
    bool        show_notifications;

    /* Side-by-side preview */
    bool        show_preview;
    char        preview_title[256];
    char        preview_content[65536];

    /* Image paste overlay */
    bool        image_paste_active;
    char        image_paste_path[512];
    char        image_paste_data[1048576];
    int         image_paste_data_len;
    bool        image_paste_is_base64;

    /* Settings / config */
    bool        yolo_active;
};

#endif /* APP_STATE_INTERNAL_H */