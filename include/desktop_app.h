/*
 * desktop_app.h — Expanded C11 Desktop Application Framework for Slermes Agent
 *
 * Cross-platform desktop app: session management, model picker, profiles,
 * settings, notifications, file dialogs, and all P0/P1 desktop parity features.
 *
 * This is the expanded header — the original desktop_app.h is preserved
 * for backward compatibility. New code uses this expanded API.
 */

#ifndef DESKTOP_APP_H
#define DESKTOP_APP_H

#include "window.h"
#include "window_compositor.h"
#include "slermes_pty.h"
#include "terminal.h"
#include "chat_render.h"
#include "chat_composer.h"
#include "gateway_client.h"
#include "clipboard.h"
#include "file_ops.h"
#include "gateway_probe.h"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define DESKTOP_APP_NAME    "Slermes Agent"
#define DESKTOP_APP_VERSION "1.0.0-c11"
#define DESKTOP_MAX_SESSIONS 256
#define DESKTOP_MAX_PROFILES 64
#define DESKTOP_MAX_MODELS   128
#define DESKTOP_MAX_SETTINGS 512
#define DESKTOP_MAX_NOTIFICATIONS 64

/* ═══════════════════════════════════════════════════════════════════════
 *  Session Management
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    char   id[64];
    char   title[256];
    char   last_message[512];
    time_t updated_at;
    time_t started_at;
    bool   is_active;
    bool   is_archived;
    bool   is_pinned;
    char   model[256];
    char   provider[64];
    int    message_count;
    int    input_tokens;
    int    output_tokens;
} desktop_session_t;

typedef void (*desktop_session_cb)(const desktop_session_t *sessions, int count);

/* Session lifecycle */
int  desktop_session_create(const char *title, const char *model, const char *provider);
bool desktop_session_delete(const char *id, bool confirm);
bool desktop_session_select(const char *id);
bool desktop_session_rename(const char *id, const char *new_title);
bool desktop_session_archive(const char *id);
bool desktop_session_unarchive(const char *id);
bool desktop_session_pin(const char *id, bool pinned);
int  desktop_session_list(desktop_session_t *out, int max_count, bool include_archived);
int  desktop_session_count(void);
int  desktop_session_search(const char *query, desktop_session_t *out, int max_count);

/* ═══════════════════════════════════════════════════════════════════════
 *  Model Management
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    char   model_id[256];
    char   provider[64];
    char   display_name[256];
    bool   available;
    bool   is_active;
    int    context_length;
    int    max_output_tokens;
    double cost_per_1m_input;
    double cost_per_1m_output;
} desktop_model_t;

/* Model lifecycle */
int  desktop_model_list(desktop_model_t *out, int max_count);
bool desktop_model_select(const char *model_id);
bool desktop_model_refresh(void);
const char *desktop_model_active_id(void);
const desktop_model_t *desktop_model_active(void);
const desktop_model_t *desktop_model_find(const char *model_id);

/* ═══════════════════════════════════════════════════════════════════════
 *  Profile Management
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: profile_scope @ apps/desktop/src/app/profile/index.tsx */
typedef enum {
    PROFILE_SCOPE_LOCAL = 0,
    PROFILE_SCOPE_WORKSPACE,
    PROFILE_SCOPE_GLOBAL,
} profile_scope_t;

typedef struct {
    char   name[128];
    char   path[1024];
    bool   is_default;
    bool   has_env;
    int    skill_count;
    char   model[256];
    char   provider[64];
    char   soul[65536];  /* SOUL.md content */
    profile_scope_t scope; /* LOCAL / WORKSPACE / GLOBAL */
} desktop_profile_t;

/* Profile lifecycle */
int  desktop_profile_list(desktop_profile_t *out, int max_count);
bool desktop_profile_create(const char *name, const char *clone_from);
bool desktop_profile_delete(const char *name, bool confirm);
bool desktop_profile_rename(const char *old_name, const char *new_name);
bool desktop_profile_select(const char *name);
bool desktop_profile_set_soul(const char *name, const char *soul_content);
bool desktop_profile_get_soul(const char *name, char *out, size_t out_size);
bool desktop_profile_set_model(const char *name, const char *model_id);
const desktop_profile_t *desktop_profile_active(void);
const desktop_profile_t *desktop_profile_find(const char *name);

/* ═══════════════════════════════════════════════════════════════════════
 *  Settings
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum {
    SETTING_STRING = 0,
    SETTING_INT,
    SETTING_BOOL,
    SETTING_DOUBLE,
} setting_type_t;

typedef struct {
    char          key[256];
    setting_type_t type;
    union {
        char   s[1024];
        int    i;
        bool   b;
        double d;
    } value;
} desktop_setting_t;

/* Settings API */
bool desktop_settings_get(const char *key, char *value, size_t value_size);
bool desktop_settings_set(const char *key, const char *value);
bool desktop_settings_get_int(const char *key, int *value);
bool desktop_settings_set_int(const char *key, int value);
bool desktop_settings_get_bool(const char *key, bool *value);
bool desktop_settings_set_bool(const char *key, bool value);
int  desktop_settings_list(desktop_setting_t *out, int max_count);
bool desktop_settings_load(const char *path);
bool desktop_settings_save(const char *path);

/* Theme */
typedef enum {
    THEME_SYSTEM = 0,
    THEME_DARK,
    THEME_LIGHT,
} desktop_theme_t;

desktop_theme_t desktop_settings_get_theme(void);
bool desktop_settings_set_theme(desktop_theme_t theme);

/* Connection config */
bool desktop_settings_get_gateway_url(char *url, size_t url_size);
bool desktop_settings_set_gateway_url(const char *url);

/* ═══════════════════════════════════════════════════════════════════════
 *  Notifications
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum {
    NOTIFY_INFO = 0,
    NOTIFY_SUCCESS,
    NOTIFY_WARNING,
    NOTIFY_ERROR,
} notify_kind_t;

typedef struct {
    notify_kind_t kind;
    char          title[256];
    char          message[1024];
    time_t        timestamp;
    bool          read;
} desktop_notification_t;

/* Notification API */
void desktop_notify(notify_kind_t kind, const char *title, const char *message);
void desktop_notify_info(const char *title, const char *message);
void desktop_notify_success(const char *title, const char *message);
void desktop_notify_warning(const char *title, const char *message);
void desktop_notify_error(const char *title, const char *message);
int  desktop_notification_list(desktop_notification_t *out, int max_count);
void desktop_notification_mark_read(int index);
void desktop_notification_clear(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  File Dialogs
 * ═══════════════════════════════════════════════════════════════════════ */

/* Open file dialog — returns allocated path (caller must free), or NULL. */
char *desktop_file_dialog_open(const char *title, const char *filter);

/* Save file dialog — returns allocated path (caller must free), or NULL. */
char *desktop_file_dialog_save(const char *title, const char *default_name, const char *filter);

/* Directory picker — returns allocated path (caller must free), or NULL. */
char *desktop_file_dialog_pick_dir(const char *title);

/* Open external URL in system browser */
bool desktop_open_external(const char *url);

/* ═══════════════════════════════════════════════════════════════════════
 *  Safe Storage (encrypted credential storage)
 * ═══════════════════════════════════════════════════════════════════════ */

bool desktop_safe_storage_set(const char *key, const char *value);
bool desktop_safe_storage_get(const char *key, char *value, size_t value_size);
bool desktop_safe_storage_delete(const char *key);

/* ═══════════════════════════════════════════════════════════════════════
 *  Auth Ticket (dashboard token management)
 * ═══════════════════════════════════════════════════════════════════════ */

bool desktop_auth_ticket_set(const char *ticket);
bool desktop_auth_ticket_get(char *ticket, size_t size);
bool desktop_auth_ticket_clear(void);
bool desktop_auth_ticket_is_valid(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  Connection Revalidation
 * ═══════════════════════════════════════════════════════════════════════ */

bool desktop_connection_revalidate(void);
bool desktop_connection_check(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  Update Management
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool   update_available;
    char   current_version[64];
    char   latest_version[64];
    char   download_url[1024];
    bool   downloading;
    double download_progress;
} desktop_update_info_t;

bool desktop_update_check(desktop_update_info_t *info);
bool desktop_update_download(const char *url, const char *dest_path);
bool desktop_update_apply(const char *update_path);

/* ═══════════════════════════════════════════════════════════════════════
 *  Application Lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

bool desktop_app_init(int argc, char **argv);
void desktop_app_run(void);
void desktop_app_shutdown(void);
bool desktop_app_is_running(void);

/* Single instance lock */
bool desktop_app_acquire_lock(void);
void desktop_app_release_lock(void);

/* Status */
const char *desktop_status_text(void);
void desktop_set_status_callback(void (*cb)(const char *status));

/* ═══════════════════════════════════════════════════════════════════════
 *  Session Export / Drag & Drop
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: session_export @ apps/desktop/src/app/session/index.tsx */
/* Export session to file (JSON/Markdown). Returns true on success. */
bool desktop_session_export(const char *id, const char *path, const char *format);

/* PoP: session_import @ apps/desktop/src/app/session/index.tsx */
/* Import session from file. Returns session ID or NULL. */
char *desktop_session_import(const char *path);

/* PoP: session_drag_drop @ apps/desktop/src/app/session/index.tsx */
/* Enable drag & drop reorder of sessions. */
void desktop_session_enable_dnd(bool enable);
bool desktop_session_has_dnd(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  Math Rendering
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: math_render @ apps/desktop/src/app/chat/index.tsx */
/* Render LaTeX/math expression. Returns rendered message. */
chat_rendered_msg_t *desktop_math_render(const char *latex, const char *display_mode);

/* ═══════════════════════════════════════════════════════════════════════
 *  Voice Input/Output Stubs
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: voice_input @ apps/desktop/src/app/chat/index.tsx */
typedef void (*voice_input_cb)(const char *transcript, bool is_final);

bool desktop_voice_input_start(voice_input_cb cb);
bool desktop_voice_input_stop(void);
bool desktop_voice_input_is_active(void);

/* PoP: voice_output @ apps/desktop/src/app/chat/index.tsx */
bool desktop_voice_output_speak(const char *text);
bool desktop_voice_output_stop(void);
bool desktop_voice_output_is_speaking(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  Artifact Rendering
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: artifact_render @ apps/desktop/src/app/chat/index.tsx */
/* Render an artifact (code output, file preview, etc.) */
typedef enum {
    ARTIFACT_CODE = 0,
    ARTIFACT_IMAGE,
    ARTIFACT_FILE,
    ARTIFACT_HTML,
    ARTIFACT_MARKDOWN,
    ARTIFACT_DATA,
} artifact_type_t;

chat_rendered_msg_t *desktop_artifact_render(artifact_type_t type,
                                              const char *content,
                                              const char *title,
                                              const char *language);

/* ═══════════════════════════════════════════════════════════════════════
 *  Reasoning Display
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: reasoning_display @ apps/desktop/src/app/chat/index.tsx */
/* Display model reasoning steps. */
chat_rendered_msg_t *desktop_reasoning_render(const char *reasoning_text,
                                               const char *conclusion,
                                               int step_count);

/* ═══════════════════════════════════════════════════════════════════════
 *  Context Menu
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: context_menu @ apps/desktop/src/app/chat/index.tsx */
typedef struct {
    const char *label;
    const char *action;
    bool        enabled;
    bool        separator;
    const char *shortcut;
} context_menu_item_t;

typedef void (*context_menu_cb)(const char *action);

bool desktop_context_menu_show(int x, int y, const context_menu_item_t *items,
                                int item_count, context_menu_cb cb);

/* ═══════════════════════════════════════════════════════════════════════
 *  Profile Scope, Auxiliary Models, Model Analytics, Model Visibility
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: profile_scope @ apps/desktop/src/app/profile/index.tsx */
bool desktop_profile_set_scope(const char *name, profile_scope_t scope);
profile_scope_t desktop_profile_get_scope(const char *name);

/* PoP: auxiliary_models @ apps/desktop/src/app/model/index.tsx */
/* Auxiliary models for specific tasks (summarization, etc.) */
typedef struct {
    char   model_id[256];
    char   task[128];       /* e.g., "summarize", "translate" */
    bool   is_active;
} desktop_auxiliary_model_t;

bool desktop_auxiliary_model_set(const char *task, const char *model_id);
int  desktop_auxiliary_model_list(desktop_auxiliary_model_t *out, int max_count);
const char *desktop_auxiliary_model_for_task(const char *task);

/* PoP: model_analytics @ apps/desktop/src/app/model/index.tsx */
typedef struct {
    char   model_id[256];
    int    total_calls;
    int    total_input_tokens;
    int    total_output_tokens;
    double total_cost_ms;
    time_t last_used;
} desktop_model_analytics_t;

bool desktop_model_analytics_get(const char *model_id, desktop_model_analytics_t *out);
int  desktop_model_analytics_list(desktop_model_analytics_t *out, int max_count);
void desktop_model_analytics_reset(const char *model_id);

/* PoP: model_visibility @ apps/desktop/src/app/model/index.tsx */
typedef enum {
    MODEL_VISIBLE_ALWAYS = 0,
    MODEL_VISIBLE_ADVANCED,
    MODEL_VISIBLE_NEVER,
} model_visibility_t;

bool desktop_model_set_visibility(const char *model_id, model_visibility_t vis);
model_visibility_t desktop_model_get_visibility(const char *model_id);

/* ═══════════════════════════════════════════════════════════════════════
 *  Font Settings, Default Project Dir, Environment Vars
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: font_settings @ apps/desktop/src/app/settings/index.tsx */
typedef struct {
    char   family[128];
    double size_points;
    bool   ligatures;
    bool   antialiasing;
    char   monospace_family[128];
    double monospace_size;
} desktop_font_settings_t;

bool desktop_font_get(desktop_font_settings_t *out);
bool desktop_font_set(const desktop_font_settings_t *fonts);

/* PoP: project_dir @ apps/desktop/src/app/settings/index.tsx */
bool desktop_set_default_project_dir(const char *path);
bool desktop_get_default_project_dir(char *path, size_t size);

/* PoP: env_vars @ apps/desktop/src/app/settings/index.tsx */
bool desktop_env_set(const char *key, const char *value);
const char *desktop_env_get(const char *key);
bool desktop_env_delete(const char *key);
int  desktop_env_list(char keys[][256], int max_count);

/* ═══════════════════════════════════════════════════════════════════════
 *  Show in Folder, Dark Mode Detection, Microphone Access
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: show_in_folder @ apps/desktop/src/app/file/index.tsx */
bool desktop_show_in_folder(const char *path);

/* PoP: dark_mode @ apps/desktop/src/app/settings/index.tsx */
bool desktop_dark_mode_is_active(void);
void desktop_dark_mode_set(bool dark);
bool desktop_dark_mode_detect(void);

/* PoP: microphone @ apps/desktop/src/app/settings/index.tsx */
bool desktop_mic_request_permission(void);
bool desktop_mic_has_permission(void);
bool desktop_mic_is_available(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  Update Branch/Marker, OAuth Login, File Watch, Git Root
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: update_branch @ apps/desktop/src/app/settings/index.tsx */
typedef enum {
    UPDATE_BRANCH_STABLE = 0,
    UPDATE_BRANCH_BETA,
    UPDATE_BRANCH_DEV,
    UPDATE_BRANCH_NIGHTLY,
} update_branch_t;

bool desktop_update_set_branch(update_branch_t branch);
update_branch_t desktop_update_get_branch(void);
bool desktop_update_set_marker(const char *marker);
const char *desktop_update_get_marker(void);

/* PoP: oauth_login @ apps/desktop/src/app/auth/index.tsx */
typedef void (*oauth_cb)(bool success, const char *token, const char *error);

bool desktop_oauth_login(const char *provider, oauth_cb cb);
bool desktop_oauth_logout(const char *provider);
bool desktop_oauth_is_logged_in(const char *provider);
const char *desktop_oauth_token(const char *provider);

/* PoP: file_watch @ apps/desktop/src/app/file/index.tsx */
typedef void (*file_watch_cb)(const char *path, const char *event);

bool desktop_file_watch_add(const char *path, file_watch_cb cb);
bool desktop_file_watch_remove(const char *path);
void desktop_file_watch_clear(void);

/* PoP: git_root @ apps/desktop/src/app/file/index.tsx */
bool desktop_git_root(char *path, size_t size);
bool desktop_git_has_repo(const char *path);
char *desktop_git_remote_url(const char *path);

/* ═══════════════════════════════════════════════════════════════════════
 *  Clipboard Image Save, Image from URL Save
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: clipboard_image @ apps/desktop/src/app/clipboard/index.tsx */
bool desktop_clipboard_has_image(void);
bool desktop_clipboard_save_image(const char *path);
uint8_t *desktop_clipboard_get_image_data(size_t *out_size);

/* PoP: image_from_url @ apps/desktop/src/app/file/index.tsx */
typedef void (*image_download_cb)(bool success, const char *path, const char *error);

bool desktop_image_download(const char *url, const char *dest_path, image_download_cb cb);

/* ═══════════════════════════════════════════════════════════════════════
 *  Uninstall Summary/Run, Recent Logs, Reveal Logs
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: uninstall @ apps/desktop/src/app/settings/index.tsx */
typedef struct {
    int  sessions_count;
    int  profiles_count;
    int  files_count;
    long total_size_bytes;
} desktop_uninstall_summary_t;

bool desktop_uninstall_get_summary(desktop_uninstall_summary_t *out);
bool desktop_uninstall_run(void);

/* PoP: recent_logs @ apps/desktop/src/app/settings/index.tsx */
typedef struct {
    char   path[1024];
    char   name[256];
    time_t modified_at;
    long   size_bytes;
} desktop_log_entry_t;

int  desktop_log_list(desktop_log_entry_t *out, int max_count);

/* PoP: reveal_logs @ apps/desktop/src/app/settings/index.tsx */
bool desktop_log_reveal(const char *log_path);
bool desktop_log_read(const char *log_path, char *out, size_t out_size, long offset);

#ifdef __cplusplus
}
#endif

#endif /* DESKTOP_APP_H */
