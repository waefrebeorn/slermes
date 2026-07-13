#ifndef DESKTOP_STATE_H
#define DESKTOP_STATE_H
#include "desktop_app.h"
typedef struct {
    desktop_session_t sessions[DESKTOP_MAX_SESSIONS];
    int session_count; int active_session;
    desktop_model_t models[DESKTOP_MAX_MODELS];
    int model_count; int active_model;
    desktop_profile_t profiles[DESKTOP_MAX_PROFILES];
    int profile_count; int active_profile;
    desktop_setting_t settings[DESKTOP_MAX_SETTINGS];
    int setting_count; desktop_theme_t theme;
    desktop_notification_t notifications[DESKTOP_MAX_NOTIFICATIONS];
    int notification_count;
    char gateway_url[1024]; char gateway_token[2048]; bool connected;
    char auth_ticket[2048]; bool auth_valid;
    desktop_update_info_t update_info;
    bool running; void (*status_cb)(const char *status);
#ifdef _WIN32
    HANDLE lock_handle;
#else
    int lock_fd;
#endif
    bool session_dnd_enabled;
} desktop_state_t;
extern desktop_state_t g_desktop;
int find_session_by_id(const char *id);
int find_model_by_id(const char *id);
int find_profile_by_name(const char *name);
desktop_setting_t *find_setting(const char *key);
void notify_status(const char *fmt, ...);
extern const char *desktop_settings_path(void);
extern const char *desktop_sessions_path(void);
extern const char *desktop_profiles_dir(void);
extern const char *desktop_safe_storage_path(void);
extern const char *desktop_config_dir(void);
extern bool dir_create(const char *path);
#endif
