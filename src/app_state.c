/*
 * app_state.c — Application State Management
 *
 * Opaque implementation of the app_state_t struct.
 * All internal state is hidden behind accessor functions.
 */

#define _GNU_SOURCE
#include "app_state_internal.h"
#include "session_db.h"
#include "gui_core.h"
#include "slermes_home.h"
#include "sqlite3.h"
#include "libhttp/http.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

/* ══════════════════════════════════════════════════════════════════════
 * Navigation items (static data)
 * ══════════════════════════════════════════════════════════════════════ */

static nav_item_t nav_items[] = {
    {"\xe2\x97\x8b", "Chat",        0},
    {"\xe2\x96\xb6", "Cmd Center",  1},
    {"\xe2\x9c\xa6", "Skills",      2},
    {"\xe2\x9d\x90", "Artifacts",   3},
    {"\xe2\x8c\x9a", "Cron",        4},
    {"\xe2\x99\xa0", "Profiles",    5},
    {"\xe2\x99\x9f", "Agents",      6},
    {"\xe2\x87\x84", "Messaging",   7},
    {"\xf0\x9f\x93\x81", "Files",   8},
    {"\xf0\x9f\x93\xab", "Snippets", 9},
    {NULL, NULL, 0}
};

/* ══════════════════════════════════════════════════════════════════════
 * Implementation
 * ══════════════════════════════════════════════════════════════════════ */

app_state_t *app_state_create(void) {
    app_state_t *app = calloc(1, sizeof(app_state_t));
    if (!app) return NULL;
    
    app->running = true;
    app->sessions_expanded = true;
    app->nav_expanded = true;
    app->dark_mode = true;
    app->sidebar_collapsed = false;
    app->haptics_muted = false;
    app->composer_pos = 0;
    app->composer_focused = false;
    app->api_busy = false;
    app->pinned_session_count = 0;
    app->show_model_picker = false;
    app->model_picker_hover = -1;
    app->model_picker_scroll = 0;
    app->show_scroll_button = false;
    app->scroll_button_hover = false;
    app->show_session_menu = false;
    app->session_menu_hover = -1;
    app->sessions_loaded_all = false;
    app->sessions_page = 0;
    app->gateway_connected = false;
    app->show_export_dialog = false;
    app->show_import_dialog = false;
    app->import_path_len = 0;
    app->toast_time = 0;
    app->pet_active = false;
    app->pet_type = 0;
    app->pet_frame = 0;
    app->pet_frame_tick = 0;
    app->pet_x = 0.0f;
    app->pet_y = 0.0f;
    app->pet_vx = 0.0f;
    app->pet_vy = 0.0f;
    app->pet_show_gallery = false;
    app->pet_count = 0;
    app->pet_selected = 0;
    app->pet_scale = 0.33f;
    app->frame_count = 0;
    app->voice_active = false;
    app->voice_recording = false;
    app->voice_tts_pending = 0;
    app->show_command_palette = false;
    app->command_palette_query_len = 0;
    app->command_palette_selected = 0;
    app->command_palette_result_count = 0;
    app->notification_count = 0;
    app->show_notifications = false;
    app->show_preview = false;
    app->image_paste_active = false;
    app->image_paste_data_len = 0;
    app->image_paste_is_base64 = false;
    app->yolo_active = false;
    app->win = NULL;
    app->theme = gc_theme_dark;
    
    return app;
}

void app_state_destroy(app_state_t *app) {
    if (!app) return;
    if (app->db) {
        sqlite3_close(app->db);
        app->db = NULL;
    }
    free(app);
}

/* Layout getters */
int app_win_w(app_state_t *app) { return app ? gc_window_w(app->win) : 0; }
int app_win_h(app_state_t *app) { return app ? gc_window_h(app->win) : 0; }
int app_sidebar_w(app_state_t *app) { return app && app->sidebar_collapsed ? 48 : SIDEBAR_W; }
int app_chat_x(app_state_t *app) { return app ? app_sidebar_w(app) : 0; }
int app_chat_w(app_state_t *app) { return app ? app_win_w(app) - app_sidebar_w(app) : 0; }
int app_chat_y(app_state_t *app) { return TITLEBAR_H; }
int app_chat_h(app_state_t *app) { return app ? app_win_h(app) - TITLEBAR_H - STATUSBAR_H : 0; }
int app_sidebar_h(app_state_t *app) { return app ? app_win_h(app) - TITLEBAR_H - STATUSBAR_H : 0; }

/* Database — these are thin delegators to session_db.c (the canonical DB
 * module), which owns the shared app_state_t sqlite accessors. Kept for the
 * public app_* API surface (app_state.h); all real work lives in session_db.c. */
int app_db_open(app_state_t *app) { return session_db_open(app); }
void app_db_close(app_state_t *app) { session_db_close(app); }
void app_load_sessions(app_state_t *app) { session_db_load_sessions(app); }
void app_load_messages(app_state_t *app, int idx) { session_db_load_messages(app, idx); }
void app_load_skills(app_state_t *app) { session_db_load_skills(app); }
void app_load_profiles(app_state_t *app) { session_db_load_profiles(app); }
void app_load_cron(app_state_t *app) { session_db_load_cron(app); }
void app_load_stats(app_state_t *app) { session_db_load_stats(app); }

/* Session management */
int app_selected_session(app_state_t *app) { return app ? app->selected_session : -1; }
void app_set_selected_session(app_state_t *app, int idx) { if (app) app->selected_session = idx; }
int app_session_count(app_state_t *app) { return app ? app->session_count : 0; }
app_session_entry_t *app_get_session(app_state_t *app, int idx) {
    if (!app || idx < 0 || idx >= app->session_count) return NULL;
    return &app->sessions[idx];
}

/* Navigation */
int app_selected_nav(app_state_t *app) { return app ? app->selected_nav : 0; }
void app_set_selected_nav(app_state_t *app, int idx) { if (app) app->selected_nav = idx; }
int app_current_view(app_state_t *app) { return app ? app->current_view : 0; }
void app_set_current_view(app_state_t *app, int view) { if (app) app->current_view = view; }
const char *app_current_view_name(app_state_t *app) { return app ? app->current_view_name : ""; }
void app_set_current_view_name(app_state_t *app, const char *name) { 
    if (app && name) strncpy(app->current_view_name, name, sizeof(app->current_view_name)-1); 
}

/* Theme */
void app_toggle_theme(app_state_t *app) {
    if (!app) return;
    app->dark_mode = !app->dark_mode;
    app->theme = app->dark_mode ? gc_theme_dark : gc_theme_light;
    if (app->win) gc_set_theme(app->win, &app->theme);
}

/* Sidebar */
void app_toggle_sidebar(app_state_t *app) { if (app) app->sidebar_collapsed = !app->sidebar_collapsed; }
bool app_sidebar_collapsed(app_state_t *app) { return app ? app->sidebar_collapsed : false; }
void app_toggle_sessions_expanded(app_state_t *app) { if (app) app->sessions_expanded = !app->sessions_expanded; }
bool app_sessions_expanded(app_state_t *app) { return app ? app->sessions_expanded : true; }
void app_toggle_nav_expanded(app_state_t *app) { if (app) app->nav_expanded = !app->nav_expanded; }
bool app_nav_expanded(app_state_t *app) { return app ? app->nav_expanded : true; }

/* Search */
void app_set_search_query(app_state_t *app, const char *query, int len) {
    if (!app || !query) return;
    if (len >= 64) len = 63;
    memcpy(app->search_query, query, len);
    app->search_query[len] = '\0';
    app->search_query_len = len;
}
const char *app_search_query(app_state_t *app) { return app ? app->search_query : ""; }
int app_search_query_len(app_state_t *app) { return app ? app->search_query_len : 0; }
bool app_search_active(app_state_t *app) { return app ? app->search_active : false; }
void app_set_search_active(app_state_t *app, bool active) { if (app) app->search_active = active; }

/* Composer */
const char *app_composer_buf(app_state_t *app) { return app ? app->composer_buf : ""; }
int app_composer_pos(app_state_t *app) { return app ? app->composer_pos : 0; }
void app_set_composer_buf(app_state_t *app, const char *buf) {
    if (!app || !buf) return;
    strncpy(app->composer_buf, buf, sizeof(app->composer_buf)-1);
}
void app_set_composer_pos(app_state_t *app, int pos) { if (app) app->composer_pos = pos; }
bool app_composer_focused(app_state_t *app) { return app ? app->composer_focused : false; }
void app_set_composer_focused(app_state_t *app, bool focused) { if (app) app->composer_focused = focused; }

/* API state */
bool app_api_busy(app_state_t *app) { return app ? app->api_busy : false; }
void app_set_api_busy(app_state_t *app, bool busy) { if (app) app->api_busy = busy; }
const char *app_api_status(app_state_t *app) { return app ? app->api_status : ""; }
void app_set_api_status(app_state_t *app, const char *status) {
    if (app && status) strncpy(app->api_status, status, sizeof(app->api_status)-1);
}

/* Messages */
int app_message_count(app_state_t *app) { return app ? app->message_count : 0; }
message_entry_t *app_get_message(app_state_t *app, int idx) {
    if (!app || idx < 0 || idx >= app->message_count) return NULL;
    return &app->messages[idx];
}

/* Hover state */
/* app_update_hover is implemented in event_handling.c, where hit_test lives. */
int app_hover_message(app_state_t *app) { return app ? app->hover_message : -1; }
void app_set_hover_message(app_state_t *app, int idx) { if (app) app->hover_message = idx; }
int app_hover_action(app_state_t *app) { return app ? app->hover_action : -1; }
void app_set_hover_action(app_state_t *app, int action) { if (app) app->hover_action = action; }

/* Scroll */
int app_sidebar_scroll(app_state_t *app) { return app ? app->sidebar_scroll : 0; }
void app_set_sidebar_scroll(app_state_t *app, int scroll) { if (app) app->sidebar_scroll = scroll; }
int app_chat_scroll(app_state_t *app) { return app ? app->chat_scroll : 0; }
void app_set_chat_scroll(app_state_t *app, int scroll) { if (app) app->chat_scroll = scroll; }
int app_sidebar_content_h(app_state_t *app) { return app ? app->sidebar_content_h : 0; }
void app_set_sidebar_content_h(app_state_t *app, int h) { if (app) app->sidebar_content_h = h; }
int app_chat_content_h(app_state_t *app) { return app ? app->chat_content_h : 0; }
void app_set_chat_content_h(app_state_t *app, int h) { if (app) app->chat_content_h = h; }

/* Model picker */
bool app_show_model_picker(app_state_t *app) { return app ? app->show_model_picker : false; }
void app_set_show_model_picker(app_state_t *app, bool show) { if (app) app->show_model_picker = show; }
int app_model_picker_hover(app_state_t *app) { return app ? app->model_picker_hover : -1; }
void app_set_model_picker_hover(app_state_t *app, int idx) { if (app) app->model_picker_hover = idx; }
int app_model_picker_scroll(app_state_t *app) { return app ? app->model_picker_scroll : 0; }
void app_set_model_picker_scroll(app_state_t *app, int scroll) { if (app) app->model_picker_scroll = scroll; }

/* Scroll to bottom button */
bool app_show_scroll_button(app_state_t *app) { return app ? app->show_scroll_button : false; }
void app_set_show_scroll_button(app_state_t *app, bool show) { if (app) app->show_scroll_button = show; }
bool app_scroll_button_hover(app_state_t *app) { return app ? app->scroll_button_hover : false; }
void app_set_scroll_button_hover(app_state_t *app, bool hover) { if (app) app->scroll_button_hover = hover; }

/* Session menu */
bool app_show_session_menu(app_state_t *app) { return app ? app->show_session_menu : false; }
void app_set_show_session_menu(app_state_t *app, bool show) { if (app) app->show_session_menu = show; }
int app_session_menu_hover(app_state_t *app) { return app ? app->session_menu_hover : -1; }
void app_set_session_menu_hover(app_state_t *app, int idx) { if (app) app->session_menu_hover = idx; }

/* Load more */
bool app_sessions_loaded_all(app_state_t *app) { return app ? app->sessions_loaded_all : false; }
void app_set_sessions_loaded_all(app_state_t *app, bool loaded) { if (app) app->sessions_loaded_all = loaded; }
int app_sessions_page(app_state_t *app) { return app ? app->sessions_page : 0; }
void app_set_sessions_page(app_state_t *app, int page) { if (app) app->sessions_page = page; }

/* Gateway status */
bool app_gateway_connected(app_state_t *app) { return app ? app->gateway_connected : false; }
void app_set_gateway_connected(app_state_t *app, bool connected) { if (app) app->gateway_connected = connected; }
const char *app_gateway_status_text(app_state_t *app) { return app ? app->gateway_status_text : ""; }
void app_set_gateway_status_text(app_state_t *app, const char *text) {
    if (app && text) strncpy(app->gateway_status_text, text, sizeof(app->gateway_status_text)-1);
}

/* Toast */
const char *app_toast_msg(app_state_t *app) { return app ? app->toast_msg : ""; }
int app_toast_time(app_state_t *app) { return app ? app->toast_time : 0; }
void app_set_toast(app_state_t *app, const char *msg, int time) {
    if (!app || !msg) return;
    strncpy(app->toast_msg, msg, sizeof(app->toast_msg)-1);
    app->toast_time = time;
}

/* Pet */
bool app_pet_active(app_state_t *app) { return app ? app->pet_active : false; }
int app_pet_type(app_state_t *app) { return app ? app->pet_type : 0; }
int app_pet_frame(app_state_t *app) { return app ? app->pet_frame : 0; }
int app_pet_frame_tick(app_state_t *app) { return app ? app->pet_frame_tick : 0; }
float app_pet_x(app_state_t *app) { return app ? app->pet_x : 0.0f; }
float app_pet_y(app_state_t *app) { return app ? app->pet_y : 0.0f; }
float app_pet_vx(app_state_t *app) { return app ? app->pet_vx : 0.0f; }
float app_pet_vy(app_state_t *app) { return app ? app->pet_vy : 0.0f; }
float app_pet_scale(app_state_t *app) { return app ? app->pet_scale : 0.33f; }
void app_set_pet_x(app_state_t *app, float x) { if (app) app->pet_x = x; }
void app_set_pet_y(app_state_t *app, float y) { if (app) app->pet_y = y; }
/* PoP: app_set_pet_scale @ hermes_cli/pets.py:set_pet_scale */
void app_set_pet_scale(app_state_t *app, float scale) { if (app) app->pet_scale = scale; }
bool app_pet_show_gallery(app_state_t *app) { return app ? app->pet_show_gallery : false; }
void app_set_pet_show_gallery(app_state_t *app, bool show) { if (app) app->pet_show_gallery = show; }
int app_pet_selected(app_state_t *app) { return app ? app->pet_selected : 0; }
int app_pet_count(app_state_t *app) { return app ? app->pet_count : 0; }
const char *app_pet_name(app_state_t *app, int idx) {
    if (!app || idx < 0 || idx >= app->pet_count) return "";
    return app->pet_names[idx];
}

/* Voice */
bool app_voice_active(app_state_t *app) { return app ? app->voice_active : false; }
bool app_voice_recording(app_state_t *app) { return app ? app->voice_recording : false; }
int app_voice_tts_pending(app_state_t *app) { return app ? app->voice_tts_pending : 0; }

/* Command palette */
bool app_show_command_palette(app_state_t *app) { return app ? app->show_command_palette : false; }
const char *app_command_palette_query(app_state_t *app) { return app ? app->command_palette_query : ""; }
int app_command_palette_query_len(app_state_t *app) { return app ? app->command_palette_query_len : 0; }
int app_command_palette_selected(app_state_t *app) { return app ? app->command_palette_selected : 0; }
int app_command_palette_result_count(app_state_t *app) { return app ? app->command_palette_result_count : 0; }

/* Notifications */
int app_notification_count(app_state_t *app) { return app ? app->notification_count : 0; }
const char *app_notification_history(app_state_t *app, int idx) {
    if (!app || idx < 0 || idx >= app->notification_count) return "";
    return app->notification_history[idx];
}
bool app_show_notifications(app_state_t *app) { return app ? app->show_notifications : false; }
void app_set_show_notifications(app_state_t *app, bool show) { if (app) app->show_notifications = show; }

/* Preview */
bool app_show_preview(app_state_t *app) { return app ? app->show_preview : false; }
const char *app_preview_title(app_state_t *app) { return app ? app->preview_title : ""; }
const char *app_preview_content(app_state_t *app) { return app ? app->preview_content : ""; }
void app_set_show_preview(app_state_t *app, bool show) { if (app) app->show_preview = show; }
void app_set_preview(app_state_t *app, const char *title, const char *content) {
    if (!app) return;
    if (title) snprintf(app->preview_title, sizeof(app->preview_title), "%s", title);
    else app->preview_title[0] = '\0';
    if (content) snprintf(app->preview_content, sizeof(app->preview_content), "%s", content);
    else app->preview_content[0] = '\0';
}

/* Image paste */
bool app_image_paste_active(app_state_t *app) { return app ? app->image_paste_active : false; }
const char *app_image_paste_path(app_state_t *app) { return app ? app->image_paste_path : ""; }
const char *app_image_paste_data(app_state_t *app) { return app ? app->image_paste_data : ""; }
int app_image_paste_data_len(app_state_t *app) { return app ? app->image_paste_data_len : 0; }
bool app_image_paste_is_base64(app_state_t *app) { return app ? app->image_paste_is_base64 : false; }

/* Settings */
bool app_yolo_active(app_state_t *app) { return app ? app->yolo_active : false; }

/* Window access */
gc_window_t *app_get_window(app_state_t *app) { return app ? app->win : NULL; }
void app_set_window(app_state_t *app, gc_window_t *win) { if (app) app->win = win; }
const gc_theme_t *app_get_theme(app_state_t *app) { return app ? &app->theme : NULL; }

/* Navigation items */
const nav_item_t *app_nav_items(void) { return nav_items; }
int app_nav_item_count(void) { 
    int count = 0;
    while (nav_items[count].label) count++;
    return count;
}