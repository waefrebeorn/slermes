#ifndef APP_STATE_H
#define APP_STATE_H

#include "gui_core.h"
#include "slermes_home.h"
#include "app_session_entry.h"
#include <stdbool.h>
#include <stddef.h>

/* ══════════════════════════════════════════════════════════════════════
 * Opaque Types — forward declarations only in header
 * ══════════════════════════════════════════════════════════════════════ */

typedef struct app_state app_state_t;
typedef struct message_entry message_entry_t;
typedef struct nav_item nav_item_t;

/* ══════════════════════════════════════════════════════════════════════
 * Public Constants
 * ══════════════════════════════════════════════════════════════════════ */

#define MAX_SESSIONS     200
#define MAX_MESSAGES     500
#define MAX_ITEMS        200

/* Hit test types */
#define HIT_NONE         -1
#define HIT_NAV          0
#define HIT_SESSION      1
#define HIT_TITLEBAR     2
#define HIT_STATUSBAR    3
#define HIT_CHAT         4
#define HIT_TOOL         5
#define HIT_COMPOSER     6
#define HIT_NEWCHAT      7
#define HIT_PILL         8
#define HIT_PROFILE      9
#define HIT_SEARCH       10
#define HIT_SESSIONS_HDR 11
#define HIT_NAV_HDR      12
#define HIT_MESSAGE_COPY 13
#define HIT_MESSAGE_EDIT 14
#define HIT_SCROLL_BOTTOM 15
#define HIT_SESSION_HDR  16
#define HIT_SESSION_PIN  17
#define HIT_SESSION_DEL  18
#define HIT_SESSION_ARCH 19
#define HIT_LOAD_MORE    20
#define HIT_MODEL_PICKER 21

/* Layout constants */
#define TITLEBAR_H       34
#define STATUSBAR_H      20
#define SIDEBAR_W        237
#define ITEM_H           30
#define SEARCH_H         28
#define SECTION_H        16
#define PADDING          16
#define BUBBLE_GAP       6
#define BUBBLE_RAD       6
#define COMPOSER_H       44
#define PILL_H           20
#define SCROLLBAR_W      6

/* ══════════════════════════════════════════════════════════════════════
 * API
 * ══════════════════════════════════════════════════════════════════════ */

/* Create/destroy */
app_state_t *app_state_create(void);
void app_state_destroy(app_state_t *app);

/* Getters for layout */
int app_win_w(app_state_t *app);
int app_win_h(app_state_t *app);
int app_sidebar_w(app_state_t *app);
int app_chat_x(app_state_t *app);
int app_chat_w(app_state_t *app);
int app_chat_y(app_state_t *app);
int app_chat_h(app_state_t *app);
int app_sidebar_h(app_state_t *app);

/* Database */
int app_db_open(app_state_t *app);
void app_db_close(app_state_t *app);
void app_load_sessions(app_state_t *app);
void app_load_messages(app_state_t *app, int idx);
void app_load_skills(app_state_t *app);
void app_load_profiles(app_state_t *app);
void app_load_cron(app_state_t *app);
void app_load_stats(app_state_t *app);

/* Session management */
int app_selected_session(app_state_t *app);
void app_set_selected_session(app_state_t *app, int idx);
int app_session_count(app_state_t *app);
app_session_entry_t *app_get_session(app_state_t *app, int idx);

/* Navigation */
int app_selected_nav(app_state_t *app);
void app_set_selected_nav(app_state_t *app, int idx);
int app_current_view(app_state_t *app);
void app_set_current_view(app_state_t *app, int view);
const char *app_current_view_name(app_state_t *app);
void app_set_current_view_name(app_state_t *app, const char *name);

/* Theme */
void app_toggle_theme(app_state_t *app);

/* Sidebar */
void app_toggle_sidebar(app_state_t *app);
bool app_sidebar_collapsed(app_state_t *app);
void app_toggle_sessions_expanded(app_state_t *app);
bool app_sessions_expanded(app_state_t *app);
void app_toggle_nav_expanded(app_state_t *app);
bool app_nav_expanded(app_state_t *app);

/* Search */
void app_set_search_query(app_state_t *app, const char *query, int len);
const char *app_search_query(app_state_t *app);
int app_search_query_len(app_state_t *app);
bool app_search_active(app_state_t *app);
void app_set_search_active(app_state_t *app, bool active);

/* Composer */
const char *app_composer_buf(app_state_t *app);
int app_composer_pos(app_state_t *app);
void app_set_composer_buf(app_state_t *app, const char *buf);
void app_set_composer_pos(app_state_t *app, int pos);
bool app_composer_focused(app_state_t *app);
void app_set_composer_focused(app_state_t *app, bool focused);

/* API state */
bool app_api_busy(app_state_t *app);
void app_set_api_busy(app_state_t *app, bool busy);
const char *app_api_status(app_state_t *app);
void app_set_api_status(app_state_t *app, const char *status);

/* Messages */
int app_message_count(app_state_t *app);
message_entry_t *app_get_message(app_state_t *app, int idx);

/* Hover state */
void app_update_hover(app_state_t *app, int mx, int my);
int app_hover_message(app_state_t *app);
void app_set_hover_message(app_state_t *app, int idx);
int app_hover_action(app_state_t *app);
void app_set_hover_action(app_state_t *app, int action);

/* Scroll */
int app_sidebar_scroll(app_state_t *app);
void app_set_sidebar_scroll(app_state_t *app, int scroll);
int app_chat_scroll(app_state_t *app);
void app_set_chat_scroll(app_state_t *app, int scroll);
int app_sidebar_content_h(app_state_t *app);
void app_set_sidebar_content_h(app_state_t *app, int h);
int app_chat_content_h(app_state_t *app);
void app_set_chat_content_h(app_state_t *app, int h);

/* Model picker */
bool app_show_model_picker(app_state_t *app);
void app_set_show_model_picker(app_state_t *app, bool show);
int app_model_picker_hover(app_state_t *app);
void app_set_model_picker_hover(app_state_t *app, int idx);
int app_model_picker_scroll(app_state_t *app);
void app_set_model_picker_scroll(app_state_t *app, int scroll);

/* Scroll to bottom button */
bool app_show_scroll_button(app_state_t *app);
void app_set_show_scroll_button(app_state_t *app, bool show);
bool app_scroll_button_hover(app_state_t *app);
void app_set_scroll_button_hover(app_state_t *app, bool hover);

/* Session menu */
bool app_show_session_menu(app_state_t *app);
void app_set_show_session_menu(app_state_t *app, bool show);
int app_session_menu_hover(app_state_t *app);
void app_set_session_menu_hover(app_state_t *app, int idx);

/* Load more */
bool app_sessions_loaded_all(app_state_t *app);
void app_set_sessions_loaded_all(app_state_t *app, bool loaded);
int app_sessions_page(app_state_t *app);
void app_set_sessions_page(app_state_t *app, int page);

/* Gateway status */
bool app_gateway_connected(app_state_t *app);
void app_set_gateway_connected(app_state_t *app, bool connected);
const char *app_gateway_status_text(app_state_t *app);
void app_set_gateway_status_text(app_state_t *app, const char *text);

/* Toast */
const char *app_toast_msg(app_state_t *app);
int app_toast_time(app_state_t *app);
void app_set_toast(app_state_t *app, const char *msg, int time);

/* Pet */
bool app_pet_active(app_state_t *app);
int app_pet_type(app_state_t *app);
int app_pet_frame(app_state_t *app);
int app_pet_frame_tick(app_state_t *app);
float app_pet_x(app_state_t *app);
float app_pet_y(app_state_t *app);
float app_pet_vx(app_state_t *app);
float app_pet_vy(app_state_t *app);
float app_pet_scale(app_state_t *app);
void app_set_pet_x(app_state_t *app, float x);
void app_set_pet_y(app_state_t *app, float y);
void app_set_pet_scale(app_state_t *app, float scale);
bool app_pet_show_gallery(app_state_t *app);
void app_set_pet_show_gallery(app_state_t *app, bool show);
int app_pet_selected(app_state_t *app);
int app_pet_count(app_state_t *app);
const char *app_pet_name(app_state_t *app, int idx);

/* Voice */
bool app_voice_active(app_state_t *app);
bool app_voice_recording(app_state_t *app);
int app_voice_tts_pending(app_state_t *app);

/* Command palette */
bool app_show_command_palette(app_state_t *app);
const char *app_command_palette_query(app_state_t *app);
int app_command_palette_query_len(app_state_t *app);
int app_command_palette_selected(app_state_t *app);
int app_command_palette_result_count(app_state_t *app);

/* Notifications */
int app_notification_count(app_state_t *app);
const char *app_notification_history(app_state_t *app, int idx);
bool app_show_notifications(app_state_t *app);
void app_set_show_notifications(app_state_t *app, bool show);

/* Preview */
bool app_show_preview(app_state_t *app);
const char *app_preview_title(app_state_t *app);
const char *app_preview_content(app_state_t *app);

/* Image paste */
bool app_image_paste_active(app_state_t *app);
const char *app_image_paste_path(app_state_t *app);
const char *app_image_paste_data(app_state_t *app);
int app_image_paste_data_len(app_state_t *app);
bool app_image_paste_is_base64(app_state_t *app);

/* Settings */
bool app_yolo_active(app_state_t *app);

/* Stats */
int app_total_sessions(app_state_t *app);
int app_total_messages(app_state_t *app);

/* Window access */
gc_window_t *app_get_window(app_state_t *app);
void app_set_window(app_state_t *app, gc_window_t *win);
const gc_theme_t *app_get_theme(app_state_t *app);

/* Navigation items */
const nav_item_t *app_nav_items(void);
int app_nav_item_count(void);

#endif /* APP_STATE_H */