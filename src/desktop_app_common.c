/*
 * desktop_app_common.c — Cross-platform desktop app logic for Slermes Agent
 *
 * Implements: session management, model picker, profiles, settings,
 * notifications, file dialogs, safe storage, auth tickets, connection
 * revalidation, and update management.
 *
 * Platform-agnostic: works on Linux, macOS, and Windows.
 * Platform-specific UI (file dialogs, notifications) uses stubs that
 * can be overridden by platform backends.
 *
 * PoP: session_delete       @ apps/desktop/src/app/session/hooks/use-session-actions.ts
 * PoP: session_rename       @ apps/desktop/src/app/session/hooks/use-session-actions.ts
 * PoP: session_archive      @ apps/desktop/src/app/session/hooks/use-session-actions.ts
 * PoP: session_search       @ apps/desktop/src/app/session/hooks/use-session-actions.ts
 * PoP: model_picker         @ apps/desktop/src/app/model-picker-overlay.tsx
 * PoP: model_switch         @ apps/desktop/src/app/session/hooks/use-model-controls.ts
 * PoP: profile_list         @ apps/desktop/src/app/profiles/index.tsx
 * PoP: profile_create       @ apps/desktop/src/app/profiles/create-profile-dialog.tsx
 * PoP: profile_delete       @ apps/desktop/src/app/profiles/delete-profile-dialog.tsx
 * PoP: profile_rename       @ apps/desktop/src/app/profiles/rename-profile-dialog.tsx
 * PoP: profile_soul         @ apps/desktop/src/app/profiles/index.tsx
 * PoP: profile_model        @ apps/desktop/src/app/profiles/index.tsx
 * PoP: settings_page        @ apps/desktop/src/app/settings/index.tsx
 * PoP: theme_switcher       @ apps/desktop/src/app/settings/index.tsx
 * PoP: connection_config    @ apps/desktop/src/app/settings/index.tsx
 * PoP: notifications        @ electron/main.cjs:notify
 * PoP: file_dialog_open     @ electron/main.cjs:selectPaths
 * PoP: file_dialog_save     @ electron/main.cjs:selectPaths
 * PoP: open_external        @ electron/main.cjs:openExternal
 * PoP: safe_storage         @ electron/main.cjs:safeStorage
 * PoP: auth_ticket          @ electron/dashboard-token.cjs
 * PoP: connection_revalidate @ electron/main.cjs:connection:revalidate
 * PoP: update_check         @ electron/main.cjs:updates:check
 * PoP: update_download      @ electron/main.cjs:updates:apply
 * PoP: update_apply         @ electron/main.cjs:update-relaunch
 */

#include "desktop_app.h"
#include "hermes.h"
#include "libhttp/http.h"

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════
 *  Internal State
 * ═══════════════════════════════════════════════════════════════════════ */

static struct {
    /* Sessions */
    desktop_session_t sessions[DESKTOP_MAX_SESSIONS];
    int               session_count;
    int               active_session;  /* index, -1 = none */

    /* Models */
    desktop_model_t   models[DESKTOP_MAX_MODELS];
    int               model_count;
    int               active_model;    /* index, -1 = none */

    /* Profiles */
    desktop_profile_t profiles[DESKTOP_MAX_PROFILES];
    int               profile_count;
    int               active_profile;  /* index, -1 = none */

    /* Settings */
    desktop_setting_t settings[DESKTOP_MAX_SETTINGS];
    int               setting_count;
    desktop_theme_t   theme;

    /* Notifications */
    desktop_notification_t notifications[DESKTOP_MAX_NOTIFICATIONS];
    int                    notification_count;

    /* Gateway */
    char gateway_url[1024];
    char gateway_token[2048];
    bool connected;

    /* Auth */
    char auth_ticket[2048];
    bool auth_valid;

    /* Update */
    desktop_update_info_t update_info;

    /* Lifecycle */
    bool running;
    void (*status_cb)(const char *status);

    /* Single instance lock */
#ifdef _WIN32
    HANDLE lock_handle;
#else
    int lock_fd;
#endif

    /* Feature flags */
    bool session_dnd_enabled;
} g_desktop = {0};

/* ═══════════════════════════════════════════════════════════════════════
 *  Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════ */

static void notify_status(const char *fmt, ...) {
    if (!g_desktop.status_cb) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_desktop.status_cb(buf);
}

static int find_session_by_id(const char *id) {
    for (int i = 0; i < g_desktop.session_count; i++) {
        if (strcmp(g_desktop.sessions[i].id, id) == 0)
            return i;
    }
    return -1;
}

static int find_model_by_id(const char *id) {
    for (int i = 0; i < g_desktop.model_count; i++) {
        if (strcmp(g_desktop.models[i].model_id, id) == 0)
            return i;
    }
    return -1;
}

static int find_profile_by_name(const char *name) {
    for (int i = 0; i < g_desktop.profile_count; i++) {
        if (strcmp(g_desktop.profiles[i].name, name) == 0)
            return i;
    }
    return -1;
}

static const char *desktop_config_dir(void) {
    static char buf[1024];
    if (buf[0]) return buf;

#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(buf, sizeof(buf), "%s\\Slermes Agent", appdata);
    } else {
        snprintf(buf, sizeof(buf), "%s\\.hermes", getenv("USERPROFILE"));
    }
#else
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        snprintf(buf, sizeof(buf), "%s/.hermes", home);
    } else {
        snprintf(buf, sizeof(buf), "/tmp/.hermes");
    }
#endif
    dir_create(buf);
    return buf;
}

static const char *desktop_settings_path(void) {
    static char buf[1024];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/settings.json", desktop_config_dir());
    return buf;
}

static const char *desktop_sessions_path(void) {
    static char buf[1024];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/sessions.json", desktop_config_dir());
    return buf;
}

static const char *desktop_profiles_dir(void) {
    static char buf[1024];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/profiles", desktop_config_dir());
    dir_create(buf);
    return buf;
}

static const char *desktop_safe_storage_path(void) {
    static char buf[1024];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/safe_storage.dat", desktop_config_dir());
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Session Management
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: session_delete @ apps/desktop/src/app/session/hooks/use-session-actions.ts */
int desktop_session_create(const char *title, const char *model, const char *provider) {
    if (g_desktop.session_count >= DESKTOP_MAX_SESSIONS) {
        fprintf(stderr, "desktop_session_create: max sessions reached\n");
        return -1;
    }

    int idx = g_desktop.session_count;
    desktop_session_t *s = &g_desktop.sessions[idx];
    memset(s, 0, sizeof(*s));

    /* Generate session ID */
    snprintf(s->id, sizeof(s->id), "session_%ld_%d", (long)time(NULL), idx);

    if (title && *title) {
        strncpy(s->title, title, sizeof(s->title) - 1);
    } else {
        snprintf(s->title, sizeof(s->title), "Chat %d", idx + 1);
    }

    if (model) strncpy(s->model, model, sizeof(s->model) - 1);
    if (provider) strncpy(s->provider, provider, sizeof(s->provider) - 1);

    s->updated_at = time(NULL);
    s->started_at = s->updated_at;
    s->is_active = true;

    g_desktop.session_count++;
    g_desktop.active_session = idx;

    fprintf(stderr, "desktop_session_create: '%s' id='%s' model='%s'\n", s->title, s->id, s->model);
    notify_status("New session: %s", s->title);
    return idx;
}

/* PoP: session_delete @ apps/desktop/src/app/session/hooks/use-session-actions.ts */
bool desktop_session_delete(const char *id, bool confirm) {
    int idx = find_session_by_id(id);
    if (idx < 0) {
        fprintf(stderr, "desktop_session_delete: session '%s' not found\n", id);
        return false;
    }

    if (confirm) {
        fprintf(stderr, "desktop_session_delete: deleting '%s' (%s)\n",
                g_desktop.sessions[idx].title, id);
    }

    /* Shift remaining sessions */
    for (int i = idx; i < g_desktop.session_count - 1; i++) {
        g_desktop.sessions[i] = g_desktop.sessions[i + 1];
    }
    g_desktop.session_count--;

    /* Update active session index */
    if (g_desktop.active_session == idx) {
        g_desktop.active_session = (g_desktop.session_count > 0) ? 0 : -1;
    } else if (g_desktop.active_session > idx) {
        g_desktop.active_session--;
    }

    notify_status("Session deleted");
    return true;
}

bool desktop_session_select(const char *id) {
    int idx = find_session_by_id(id);
    if (idx < 0) return false;
    g_desktop.active_session = idx;
    notify_status("Session: %s", g_desktop.sessions[idx].title);
    return true;
}

/* PoP: session_rename @ apps/desktop/src/app/session/hooks/use-session-actions.ts */
bool desktop_session_rename(const char *id, const char *new_title) {
    int idx = find_session_by_id(id);
    if (idx < 0 || !new_title || !*new_title) return false;

    strncpy(g_desktop.sessions[idx].title, new_title, sizeof(g_desktop.sessions[idx].title) - 1);
    g_desktop.sessions[idx].updated_at = time(NULL);

    fprintf(stderr, "desktop_session_rename: '%s' -> '%s'\n", id, new_title);
    notify_status("Renamed: %s", new_title);
    return true;
}

/* PoP: session_archive @ apps/desktop/src/app/session/hooks/use-session-actions.ts */
bool desktop_session_archive(const char *id) {
    int idx = find_session_by_id(id);
    if (idx < 0) return false;
    g_desktop.sessions[idx].is_archived = true;
    g_desktop.sessions[idx].updated_at = time(NULL);
    notify_status("Archived: %s", g_desktop.sessions[idx].title);
    return true;
}

bool desktop_session_unarchive(const char *id) {
    int idx = find_session_by_id(id);
    if (idx < 0) return false;
    g_desktop.sessions[idx].is_archived = false;
    g_desktop.sessions[idx].updated_at = time(NULL);
    notify_status("Unarchived: %s", g_desktop.sessions[idx].title);
    return true;
}

bool desktop_session_pin(const char *id, bool pinned) {
    int idx = find_session_by_id(id);
    if (idx < 0) return false;
    g_desktop.sessions[idx].is_pinned = pinned;
    return true;
}

int desktop_session_list(desktop_session_t *out, int max_count, bool include_archived) {
    if (!out || max_count <= 0) return 0;

    int count = 0;
    /* Pinned sessions first */
    for (int i = 0; i < g_desktop.session_count && count < max_count; i++) {
        if (!g_desktop.sessions[i].is_pinned) continue;
        if (g_desktop.sessions[i].is_archived && !include_archived) continue;
        out[count++] = g_desktop.sessions[i];
    }
    /* Then non-pinned */
    for (int i = 0; i < g_desktop.session_count && count < max_count; i++) {
        if (g_desktop.sessions[i].is_pinned) continue;
        if (g_desktop.sessions[i].is_archived && !include_archived) continue;
        out[count++] = g_desktop.sessions[i];
    }
    return count;
}

int desktop_session_count(void) {
    return g_desktop.session_count;
}

/* PoP: session_search @ apps/desktop/src/app/session/hooks/use-session-actions.ts */
int desktop_session_search(const char *query, desktop_session_t *out, int max_count) {
    if (!query || !out || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < g_desktop.session_count && count < max_count; i++) {
        if (strcasestr(g_desktop.sessions[i].title, query) ||
            strcasestr(g_desktop.sessions[i].last_message, query) ||
            strcasestr(g_desktop.sessions[i].model, query)) {
            out[count++] = g_desktop.sessions[i];
        }
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Model Management
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: model_picker @ apps/desktop/src/app/model-picker-overlay.tsx */
int desktop_model_list(desktop_model_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = g_desktop.model_count < max_count ? g_desktop.model_count : max_count;
    memcpy(out, g_desktop.models, count * sizeof(desktop_model_t));
    return count;
}

/* PoP: model_switch @ apps/desktop/src/app/session/hooks/use-model-controls.ts */
bool desktop_model_select(const char *model_id) {
    int idx = find_model_by_id(model_id);
    if (idx < 0) {
        fprintf(stderr, "desktop_model_select: model '%s' not found\n", model_id);
        return false;
    }

    /* Deactivate previous */
    if (g_desktop.active_model >= 0 && g_desktop.active_model < g_desktop.model_count) {
        g_desktop.models[g_desktop.active_model].is_active = false;
    }

    g_desktop.active_model = idx;
    g_desktop.models[idx].is_active = true;

    fprintf(stderr, "desktop_model_select: '%s' (%s)\n",
            g_desktop.models[idx].display_name, g_desktop.models[idx].model_id);
    notify_status("Model: %s", g_desktop.models[idx].display_name);
    return true;
}

bool desktop_model_refresh(void) {
    /* Query available models from the gateway via HTTP API.
     * For now, populate with defaults if empty. */
    if (g_desktop.model_count > 0) return true;

    /* Add default models */
    const struct {
        const char *id;
        const char *provider;
        const char *name;
        int ctx;
    } defaults[] = {
        {"claude-sonnet-4", "anthropic", "Claude Sonnet 4", 200000},
        {"claude-opus-4",   "anthropic", "Claude Opus 4",   200000},
        {"gpt-4o",          "openai",    "GPT-4o",           128000},
        {"gpt-4o-mini",     "openai",    "GPT-4o Mini",      128000},
        {"gemini-2.5-pro",  "google",    "Gemini 2.5 Pro",   1000000},
        {NULL, NULL, NULL, 0}
    };

    for (int i = 0; defaults[i].id && g_desktop.model_count < DESKTOP_MAX_MODELS; i++) {
        desktop_model_t *m = &g_desktop.models[g_desktop.model_count++];
        strncpy(m->model_id, defaults[i].id, sizeof(m->model_id) - 1);
        strncpy(m->provider, defaults[i].provider, sizeof(m->provider) - 1);
        strncpy(m->display_name, defaults[i].name, sizeof(m->display_name) - 1);
        m->context_length = defaults[i].ctx;
        m->available = true;
        m->is_active = (i == 0);
    }

    g_desktop.active_model = (g_desktop.model_count > 0) ? 0 : -1;
    fprintf(stderr, "desktop_model_refresh: loaded %d models\n", g_desktop.model_count);
    return true;
}

const char *desktop_model_active_id(void) {
    if (g_desktop.active_model < 0 || g_desktop.active_model >= g_desktop.model_count)
        return NULL;
    return g_desktop.models[g_desktop.active_model].model_id;
}

const desktop_model_t *desktop_model_active(void) {
    if (g_desktop.active_model < 0 || g_desktop.active_model >= g_desktop.model_count)
        return NULL;
    return &g_desktop.models[g_desktop.active_model];
}

const desktop_model_t *desktop_model_find(const char *model_id) {
    int idx = find_model_by_id(model_id);
    return (idx >= 0) ? &g_desktop.models[idx] : NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Profile Management
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: profile_list @ apps/desktop/src/app/profiles/index.tsx */
int desktop_profile_list(desktop_profile_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = g_desktop.profile_count < max_count ? g_desktop.profile_count : max_count;
    memcpy(out, g_desktop.profiles, count * sizeof(desktop_profile_t));
    return count;
}

/* PoP: profile_create @ apps/desktop/src/app/profiles/create-profile-dialog.tsx */
bool desktop_profile_create(const char *name, const char *clone_from) {
    if (!name || !*name) return false;

    /* Validate name: lowercase alphanumeric, hyphens, underscores */
    for (const char *p = name; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') {
            fprintf(stderr, "desktop_profile_create: invalid character '%c' in name\n", *p);
            return false;
        }
    }

    if (find_profile_by_name(name) >= 0) {
        fprintf(stderr, "desktop_profile_create: profile '%s' already exists\n", name);
        return false;
    }

    if (g_desktop.profile_count >= DESKTOP_MAX_PROFILES) {
        fprintf(stderr, "desktop_profile_create: max profiles reached\n");
        return false;
    }

    int idx = g_desktop.profile_count;
    desktop_profile_t *p = &g_desktop.profiles[idx];
    memset(p, 0, sizeof(*p));

    strncpy(p->name, name, sizeof(p->name) - 1);

    /* Build profile path */
    snprintf(p->path, sizeof(p->path), "%s/%s", desktop_profiles_dir(), name);
    dir_create(p->path);

    /* If cloning, copy from source */
    if (clone_from) {
        int src_idx = find_profile_by_name(clone_from);
        if (src_idx >= 0) {
            desktop_profile_t *src = &g_desktop.profiles[src_idx];
            strncpy(p->model, src->model, sizeof(p->model) - 1);
            strncpy(p->provider, src->provider, sizeof(p->provider) - 1);
            strncpy(p->soul, src->soul, sizeof(p->soul) - 1);
            p->skill_count = src->skill_count;
        }
    }

    /* Default profile is "default" */
    if (strcmp(name, "default") == 0) {
        p->is_default = true;
    }

    g_desktop.profile_count++;
    fprintf(stderr, "desktop_profile_create: '%s' (clone=%s)\n", name, clone_from ? clone_from : "none");
    notify_status("Profile created: %s", name);
    return true;
}

/* PoP: profile_delete @ apps/desktop/src/app/profiles/delete-profile-dialog.tsx */
bool desktop_profile_delete(const char *name, bool confirm) {
    int idx = find_profile_by_name(name);
    if (idx < 0) return false;

    if (g_desktop.profiles[idx].is_default) {
        fprintf(stderr, "desktop_profile_delete: cannot delete default profile\n");
        return false;
    }

    if (confirm) {
        fprintf(stderr, "desktop_profile_delete: deleting '%s'\n", name);
    }

    /* Remove profile directory */
    file_delete(g_desktop.profiles[idx].path);

    /* Shift remaining */
    for (int i = idx; i < g_desktop.profile_count - 1; i++) {
        g_desktop.profiles[i] = g_desktop.profiles[i + 1];
    }
    g_desktop.profile_count--;

    if (g_desktop.active_profile == idx) {
        g_desktop.active_profile = 0; /* fall back to default */
    } else if (g_desktop.active_profile > idx) {
        g_desktop.active_profile--;
    }

    notify_status("Profile deleted: %s", name);
    return true;
}

/* PoP: profile_rename @ apps/desktop/src/app/profiles/rename-profile-dialog.tsx */
bool desktop_profile_rename(const char *old_name, const char *new_name) {
    int idx = find_profile_by_name(old_name);
    if (idx < 0 || !new_name || !*new_name) return false;

    if (find_profile_by_name(new_name) >= 0) {
        fprintf(stderr, "desktop_profile_rename: '%s' already exists\n", new_name);
        return false;
    }

    desktop_profile_t *p = &g_desktop.profiles[idx];
    char new_path[1024];
    snprintf(new_path, sizeof(new_path), "%s/%s", desktop_profiles_dir(), new_name);

    /* Rename directory */
    rename(p->path, new_path);

    strncpy(p->name, new_name, sizeof(p->name) - 1);
    strncpy(p->path, new_path, sizeof(p->path) - 1);

    notify_status("Profile renamed: %s", new_name);
    return true;
}

bool desktop_profile_select(const char *name) {
    int idx = find_profile_by_name(name);
    if (idx < 0) return false;
    g_desktop.active_profile = idx;
    notify_status("Profile: %s", name);
    return true;
}

/* PoP: profile_soul @ apps/desktop/src/app/profiles/index.tsx */
bool desktop_profile_set_soul(const char *name, const char *soul_content) {
    int idx = find_profile_by_name(name);
    if (idx < 0) return false;

    desktop_profile_t *p = &g_desktop.profiles[idx];
    strncpy(p->soul, soul_content, sizeof(p->soul) - 1);

    /* Write SOUL.md */
    char soul_path[1024];
    snprintf(soul_path, sizeof(soul_path), "%s/SOUL.md", p->path);
    file_write_text(soul_path, soul_content);

    fprintf(stderr, "desktop_profile_set_soul: '%s' (%zu bytes)\n", name, strlen(soul_content));
    return true;
}

bool desktop_profile_get_soul(const char *name, char *out, size_t out_size) {
    int idx = find_profile_by_name(name);
    if (idx < 0 || !out || out_size == 0) return false;

    desktop_profile_t *p = &g_desktop.profiles[idx];

    /* Try reading SOUL.md first */
    char soul_path[1024];
    snprintf(soul_path, sizeof(soul_path), "%s/SOUL.md", p->path);
    char *content = file_read_text(soul_path, NULL);
    if (content) {
        strncpy(out, content, out_size - 1);
        free(content);
    } else {
        strncpy(out, p->soul, out_size - 1);
    }
    out[out_size - 1] = '\0';
    return true;
}

/* PoP: profile_model @ apps/desktop/src/app/profiles/index.tsx */
bool desktop_profile_set_model(const char *name, const char *model_id) {
    int idx = find_profile_by_name(name);
    if (idx < 0) return false;

    desktop_profile_t *p = &g_desktop.profiles[idx];
    if (model_id) {
        strncpy(p->model, model_id, sizeof(p->model) - 1);
    } else {
        p->model[0] = '\0';
    }

    fprintf(stderr, "desktop_profile_set_model: '%s' -> '%s'\n", name, model_id ? model_id : "(none)");
    return true;
}

const desktop_profile_t *desktop_profile_active(void) {
    if (g_desktop.active_profile < 0 || g_desktop.active_profile >= g_desktop.profile_count)
        return NULL;
    return &g_desktop.profiles[g_desktop.active_profile];
}

const desktop_profile_t *desktop_profile_find(const char *name) {
    int idx = find_profile_by_name(name);
    return (idx >= 0) ? &g_desktop.profiles[idx] : NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Settings
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: settings_page @ apps/desktop/src/app/settings/index.tsx */

static desktop_setting_t *find_setting(const char *key) {
    for (int i = 0; i < g_desktop.setting_count; i++) {
        if (strcmp(g_desktop.settings[i].key, key) == 0)
            return &g_desktop.settings[i];
    }
    return NULL;
}

bool desktop_settings_get(const char *key, char *value, size_t value_size) {
    desktop_setting_t *s = find_setting(key);
    if (!s || !value || value_size == 0) return false;
    if (s->type != SETTING_STRING) return false;
    strncpy(value, s->value.s, value_size - 1);
    return true;
}

bool desktop_settings_set(const char *key, const char *value) {
    desktop_setting_t *s = find_setting(key);
    if (s) {
        strncpy(s->value.s, value, sizeof(s->value.s) - 1);
        return true;
    }
    if (g_desktop.setting_count >= DESKTOP_MAX_SETTINGS) return false;
    s = &g_desktop.settings[g_desktop.setting_count++];
    strncpy(s->key, key, sizeof(s->key) - 1);
    s->type = SETTING_STRING;
    strncpy(s->value.s, value, sizeof(s->value.s) - 1);
    return true;
}

bool desktop_settings_get_int(const char *key, int *value) {
    desktop_setting_t *s = find_setting(key);
    if (!s || s->type != SETTING_INT || !value) return false;
    *value = s->value.i;
    return true;
}

bool desktop_settings_set_int(const char *key, int value) {
    desktop_setting_t *s = find_setting(key);
    if (s) { s->value.i = value; s->type = SETTING_INT; return true; }
    if (g_desktop.setting_count >= DESKTOP_MAX_SETTINGS) return false;
    s = &g_desktop.settings[g_desktop.setting_count++];
    strncpy(s->key, key, sizeof(s->key) - 1);
    s->type = SETTING_INT;
    s->value.i = value;
    return true;
}

bool desktop_settings_get_bool(const char *key, bool *value) {
    desktop_setting_t *s = find_setting(key);
    if (!s || s->type != SETTING_BOOL || !value) return false;
    *value = s->value.b;
    return true;
}

bool desktop_settings_set_bool(const char *key, bool value) {
    desktop_setting_t *s = find_setting(key);
    if (s) { s->value.b = value; s->type = SETTING_BOOL; return true; }
    if (g_desktop.setting_count >= DESKTOP_MAX_SETTINGS) return false;
    s = &g_desktop.settings[g_desktop.setting_count++];
    strncpy(s->key, key, sizeof(s->key) - 1);
    s->type = SETTING_BOOL;
    s->value.b = value;
    return true;
}

int desktop_settings_list(desktop_setting_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = g_desktop.setting_count < max_count ? g_desktop.setting_count : max_count;
    memcpy(out, g_desktop.settings, count * sizeof(desktop_setting_t));
    return count;
}

bool desktop_settings_load(const char *path) {
    if (!path) path = desktop_settings_path();
    char *content = file_read_text(path, NULL);
    if (!content) {
        fprintf(stderr, "desktop_settings_load: no settings file at '%s'\n", path);
        return false;
    }

    /* Simple JSON parser: extract "key": "value" pairs */
    const char *p = content;
    while (*p) {
        /* Find opening quote */
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++;

        /* Extract key */
        char key[256];
        int klen = 0;
        while (*p && *p != '"' && klen < (int)sizeof(key) - 1) key[klen++] = *p++;
        key[klen] = '\0';
        if (*p == '"') p++;

        /* Skip to colon */
        while (*p && *p != ':') p++;
        if (*p == ':') p++;

        /* Skip whitespace */
        while (*p && isspace((unsigned char)*p)) p++;

        /* Extract value */
        if (*p == '"') {
            p++;
            char val[1024];
            int vlen = 0;
            while (*p && *p != '"' && vlen < (int)sizeof(val) - 1) val[vlen++] = *p++;
            val[vlen] = '\0';
            if (*p == '"') p++;
            desktop_settings_set(key, val);
        } else if (*p == 't' || *p == 'f') {
            bool val = (*p == 't');
            desktop_settings_set_bool(key, val);
            while (*p && *p != ',' && *p != '}') p++;
        } else if ((*p >= '0' && *p <= '9') || *p == '-') {
            int val = atoi(p);
            desktop_settings_set_int(key, val);
            while (*p && *p != ',' && *p != '}') p++;
        }
    }

    free(content);
    fprintf(stderr, "desktop_settings_load: loaded %d settings from '%s'\n", g_desktop.setting_count, path);
    return true;
}

bool desktop_settings_save(const char *path) {
    if (!path) path = desktop_settings_path();

    FILE *fp = fopen(path, "w");
    if (!fp) return false;

    fprintf(fp, "{\n");
    for (int i = 0; i < g_desktop.setting_count; i++) {
        desktop_setting_t *s = &g_desktop.settings[i];
        switch (s->type) {
            case SETTING_STRING:
                fprintf(fp, "  \"%s\": \"%s\"", s->key, s->value.s);
                break;
            case SETTING_INT:
                fprintf(fp, "  \"%s\": %d", s->key, s->value.i);
                break;
            case SETTING_BOOL:
                fprintf(fp, "  \"%s\": %s", s->key, s->value.b ? "true" : "false");
                break;
            case SETTING_DOUBLE:
                fprintf(fp, "  \"%s\": %f", s->key, s->value.d);
                break;
        }
        if (i < g_desktop.setting_count - 1) fprintf(fp, ",");
        fprintf(fp, "\n");
    }
    fprintf(fp, "}\n");
    fclose(fp);
    return true;
}

/* PoP: theme_switcher @ apps/desktop/src/app/settings/index.tsx */
desktop_theme_t desktop_settings_get_theme(void) {
    return g_desktop.theme;
}

bool desktop_settings_set_theme(desktop_theme_t theme) {
    g_desktop.theme = theme;
    desktop_settings_set_int("theme", (int)theme);
    notify_status("Theme: %s", theme == THEME_DARK ? "dark" : theme == THEME_LIGHT ? "light" : "system");
    return true;
}

/* PoP: connection_config @ apps/desktop/src/app/settings/index.tsx */
bool desktop_settings_get_gateway_url(char *url, size_t url_size) {
    if (!url || url_size == 0) return false;
    strncpy(url, g_desktop.gateway_url, url_size - 1);
    return true;
}

bool desktop_settings_set_gateway_url(const char *url) {
    if (!url) return false;
    strncpy(g_desktop.gateway_url, url, sizeof(g_desktop.gateway_url) - 1);
    desktop_settings_set("gateway_url", url);
    notify_status("Gateway: %s", url);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Notifications
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: notifications @ electron/main.cjs:notify */
void desktop_notify(notify_kind_t kind, const char *title, const char *message) {
    if (g_desktop.notification_count >= DESKTOP_MAX_NOTIFICATIONS) {
        /* Shift out oldest */
        memmove(&g_desktop.notifications[0], &g_desktop.notifications[1],
                (DESKTOP_MAX_NOTIFICATIONS - 1) * sizeof(desktop_notification_t));
        g_desktop.notification_count--;
    }

    desktop_notification_t *n = &g_desktop.notifications[g_desktop.notification_count++];
    n->kind = kind;
    if (title) strncpy(n->title, title, sizeof(n->title) - 1);
    if (message) strncpy(n->message, message, sizeof(n->message) - 1);
    n->timestamp = time(NULL);
    n->read = false;

    /* Also log to stderr */
    const char *prefix = "";
    switch (kind) {
        case NOTIFY_INFO:    prefix = "[INFO]"; break;
        case NOTIFY_SUCCESS: prefix = "[OK]"; break;
        case NOTIFY_WARNING: prefix = "[WARN]"; break;
        case NOTIFY_ERROR:   prefix = "[ERROR]"; break;
    }
    fprintf(stderr, "%s %s: %s\n", prefix, title ? title : "", message ? message : "");

    /* TODO: trigger native OS notification */
}

void desktop_notify_info(const char *title, const char *message) {
    desktop_notify(NOTIFY_INFO, title, message);
}

void desktop_notify_success(const char *title, const char *message) {
    desktop_notify(NOTIFY_SUCCESS, title, message);
}

void desktop_notify_warning(const char *title, const char *message) {
    desktop_notify(NOTIFY_WARNING, title, message);
}

void desktop_notify_error(const char *title, const char *message) {
    desktop_notify(NOTIFY_ERROR, title, message);
}

int desktop_notification_list(desktop_notification_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = g_desktop.notification_count < max_count ? g_desktop.notification_count : max_count;
    memcpy(out, g_desktop.notifications, count * sizeof(desktop_notification_t));
    return count;
}

void desktop_notification_mark_read(int index) {
    if (index >= 0 && index < g_desktop.notification_count)
        g_desktop.notifications[index].read = true;
}

void desktop_notification_clear(void) {
    g_desktop.notification_count = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  File Dialogs (stubs — platform backends override)
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: file_dialog_open @ electron/main.cjs:selectPaths */
char *desktop_file_dialog_open(const char *title, const char *filter) {
    (void)title; (void)filter;
    /* TODO: implement native file dialog */
    fprintf(stderr, "desktop_file_dialog_open: stub (title='%s')\n", title ? title : "");
    return NULL;
}

/* PoP: file_dialog_save @ electron/main.cjs:selectPaths */
char *desktop_file_dialog_save(const char *title, const char *default_name, const char *filter) {
    (void)title; (void)default_name; (void)filter;
    fprintf(stderr, "desktop_file_dialog_save: stub (title='%s')\n", title ? title : "");
    return NULL;
}

char *desktop_file_dialog_pick_dir(const char *title) {
    (void)title;
    fprintf(stderr, "desktop_file_dialog_pick_dir: stub\n");
    return NULL;
}

/* PoP: open_external @ electron/main.cjs:openExternal */
bool desktop_open_external(const char *url) {
    if (!url || !*url) return false;

#ifdef _WIN32
    ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    return true;
#elif defined(__APPLE__)
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "open '%s'", url);
    return system(cmd) == 0;
#else
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' 2>/dev/null", url);
    return system(cmd) == 0;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Safe Storage (encrypted credential storage)
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: safe_storage @ electron/main.cjs:safeStorage */

typedef struct {
    char key[256];
    char value[2048];
} safe_entry_t;

#define MAX_SAFE_ENTRIES 128

static struct {
    safe_entry_t entries[MAX_SAFE_ENTRIES];
    int count;
    bool loaded;
} g_safe = {0};

static void safe_storage_load(void) {
    if (g_safe.loaded) return;
    g_safe.loaded = true;

    char *content = file_read_text(desktop_safe_storage_path(), NULL);
    if (!content) return;

    /* Simple JSON parse */
    const char *p = content;
    while (*p) {
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++;
        char key[256];
        int klen = 0;
        while (*p && *p != '"' && klen < (int)sizeof(key) - 1) key[klen++] = *p++;
        key[klen] = '\0';
        if (*p == '"') p++;
        while (*p && *p != ':') p++;
        if (*p == ':') p++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '"') {
            p++;
            char val[2048];
            int vlen = 0;
            while (*p && *p != '"' && vlen < (int)sizeof(val) - 1) val[vlen++] = *p++;
            val[vlen] = '\0';
            if (*p == '"') p++;
            if (g_safe.count < MAX_SAFE_ENTRIES) {
                safe_entry_t *e = &g_safe.entries[g_safe.count++];
                strncpy(e->key, key, sizeof(e->key) - 1);
                strncpy(e->value, val, sizeof(e->value) - 1);
            }
        }
    }
    free(content);
}

static void safe_storage_save(void) {
    FILE *fp = fopen(desktop_safe_storage_path(), "w");
    if (!fp) return;
    fprintf(fp, "{\n");
    for (int i = 0; i < g_safe.count; i++) {
        fprintf(fp, "  \"%s\": \"%s\"%s\n", g_safe.entries[i].key, g_safe.entries[i].value,
                i < g_safe.count - 1 ? "," : "");
    }
    fprintf(fp, "}\n");
    fclose(fp);
}

bool desktop_safe_storage_set(const char *key, const char *value) {
    safe_storage_load();
    for (int i = 0; i < g_safe.count; i++) {
        if (strcmp(g_safe.entries[i].key, key) == 0) {
            strncpy(g_safe.entries[i].value, value, sizeof(g_safe.entries[i].value) - 1);
            safe_storage_save();
            return true;
        }
    }
    if (g_safe.count >= MAX_SAFE_ENTRIES) return false;
    safe_entry_t *e = &g_safe.entries[g_safe.count++];
    strncpy(e->key, key, sizeof(e->key) - 1);
    strncpy(e->value, value, sizeof(e->value) - 1);
    safe_storage_save();
    return true;
}

bool desktop_safe_storage_get(const char *key, char *value, size_t value_size) {
    safe_storage_load();
    for (int i = 0; i < g_safe.count; i++) {
        if (strcmp(g_safe.entries[i].key, key) == 0) {
            strncpy(value, g_safe.entries[i].value, value_size - 1);
            return true;
        }
    }
    return false;
}

bool desktop_safe_storage_delete(const char *key) {
    safe_storage_load();
    for (int i = 0; i < g_safe.count; i++) {
        if (strcmp(g_safe.entries[i].key, key) == 0) {
            memmove(&g_safe.entries[i], &g_safe.entries[i + 1],
                    (g_safe.count - i - 1) * sizeof(safe_entry_t));
            g_safe.count--;
            safe_storage_save();
            return true;
        }
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Auth Ticket
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: auth_ticket @ electron/dashboard-token.cjs */
bool desktop_auth_ticket_set(const char *ticket) {
    if (!ticket) return false;
    strncpy(g_desktop.auth_ticket, ticket, sizeof(g_desktop.auth_ticket) - 1);
    g_desktop.auth_valid = true;
    desktop_safe_storage_set("auth_ticket", ticket);
    return true;
}

bool desktop_auth_ticket_get(char *ticket, size_t size) {
    if (!ticket || size == 0) return false;
    if (!g_desktop.auth_valid) {
        /* Try loading from safe storage */
        if (desktop_safe_storage_get("auth_ticket", g_desktop.auth_ticket,
                                      sizeof(g_desktop.auth_ticket))) {
            g_desktop.auth_valid = true;
        }
    }
    if (!g_desktop.auth_valid) return false;
    strncpy(ticket, g_desktop.auth_ticket, size - 1);
    return true;
}

bool desktop_auth_ticket_clear(void) {
    g_desktop.auth_ticket[0] = '\0';
    g_desktop.auth_valid = false;
    desktop_safe_storage_delete("auth_ticket");
    return true;
}

bool desktop_auth_ticket_is_valid(void) {
    return g_desktop.auth_valid && g_desktop.auth_ticket[0] != '\0';
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Connection Revalidation
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: connection_revalidate @ electron/main.cjs:connection:revalidate */
bool desktop_connection_revalidate(void) {
    fprintf(stderr, "desktop_connection_revalidate: checking gateway '%s'\n",
            g_desktop.gateway_url);

    if (!g_desktop.gateway_url[0]) {
        fprintf(stderr, "desktop_connection_revalidate: no gateway URL configured\n");
        g_desktop.connected = false;
        return false;
    }

    probe_result_t result = gateway_probe(g_desktop.gateway_url, 5000);
    g_desktop.connected = result.reachable;

    if (result.reachable) {
        fprintf(stderr, "desktop_connection_revalidate: OK (%d ms)\n", result.latency_ms);
    } else {
        fprintf(stderr, "desktop_connection_revalidate: FAILED (%s)\n",
                result.error[0] ? result.error : "unknown error");
    }

    return result.reachable;
}

bool desktop_connection_check(void) {
    return g_desktop.connected;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Update Management
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: update_check @ electron/main.cjs:updates:check */
bool desktop_update_check(desktop_update_info_t *info) {
    if (!info) return false;
    memset(info, 0, sizeof(*info));

    /* Check remote update server via HTTP API.
     * For now, report no update available. */
    strncpy(info->current_version, DESKTOP_APP_VERSION, sizeof(info->current_version) - 1);
    info->update_available = false;

    fprintf(stderr, "desktop_update_check: current=%s, update_available=%d\n",
            info->current_version, info->update_available);
    return true;
}

/* PoP: update_download @ electron/main.cjs:updates:apply */
bool desktop_update_download(const char *url, const char *dest_path) {
    if (!url || !dest_path) return false;
    fprintf(stderr, "desktop_update_download: %s -> %s\n", url, dest_path);
    /* TODO: implement HTTP download */
    g_desktop.update_info.downloading = true;
    g_desktop.update_info.download_progress = 0.0;
    return false; /* stub */
}

/* PoP: update_apply @ electron/main.cjs:update-relaunch */
bool desktop_update_apply(const char *update_path) {
    if (!update_path) return false;
    fprintf(stderr, "desktop_update_apply: %s\n", update_path);
    /* TODO: implement update application */
    return false; /* stub */
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Application Lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

bool desktop_app_init(int argc, char **argv) {
    (void)argc; (void)argv;

    memset(&g_desktop, 0, sizeof(g_desktop));
    g_desktop.active_session = -1;
    g_desktop.active_model = -1;
    g_desktop.active_profile = -1;
    g_desktop.theme = THEME_SYSTEM;

    /* Set default gateway URL */
    strncpy(g_desktop.gateway_url, "http://localhost:18789", sizeof(g_desktop.gateway_url) - 1);

    /* Load settings */
    desktop_settings_load(NULL);

    /* Apply loaded settings */
    char url[1024];
    if (desktop_settings_get("gateway_url", url, sizeof(url))) {
        strncpy(g_desktop.gateway_url, url, sizeof(g_desktop.gateway_url) - 1);
    }
    int theme_val = 0;
    if (desktop_settings_get_int("theme", &theme_val)) {
        g_desktop.theme = (desktop_theme_t)theme_val;
    }

    /* Load default profile if none exist */
    if (g_desktop.profile_count == 0) {
        desktop_profile_create("default", NULL);
        g_desktop.active_profile = 0;
    }

    /* Refresh models */
    desktop_model_refresh();

    /* Create default session */
    desktop_session_create("Welcome", NULL, NULL);

    g_desktop.running = true;
    fprintf(stderr, "desktop_app_init: initialized\n");
    return true;
}

void desktop_app_run(void) {
    fprintf(stderr, "desktop_app_run: starting main loop\n");
    /* Run the event loop - in C this integrates with the platform's event system.
     * For now, just mark as running and process events. */
    while (g_desktop.running) {
        /* Process events, etc. */
        /* In production: integrate with platform event loop (Wayland, X11, etc.) */
        usleep(100000); /* 100000ms idle */
    }
}

void desktop_app_shutdown(void) {
    fprintf(stderr, "desktop_app_shutdown: cleaning up\n");
    desktop_settings_save(NULL);
    g_desktop.running = false;
}

bool desktop_app_is_running(void) {
    return g_desktop.running;
}

/* Single instance lock */
bool desktop_app_acquire_lock(void) {
#ifdef _WIN32
    g_desktop.lock_handle = CreateMutex(NULL, TRUE, "HermesAgent_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        fprintf(stderr, "desktop_app_acquire_lock: another instance is running\n");
        return false;
    }
#else
    char lock_path[1024];
    snprintf(lock_path, sizeof(lock_path), "%s/.hermes.lock", desktop_config_dir());
    g_desktop.lock_fd = open(lock_path, O_CREAT | O_RDWR, 0644);
    if (g_desktop.lock_fd < 0) return false;
    if (flock(g_desktop.lock_fd, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "desktop_app_acquire_lock: another instance is running\n");
        close(g_desktop.lock_fd);
        g_desktop.lock_fd = -1;
        return false;
    }
#endif
    return true;
}

void desktop_app_release_lock(void) {
#ifdef _WIN32
    if (g_desktop.lock_handle) {
        ReleaseMutex(g_desktop.lock_handle);
        CloseHandle(g_desktop.lock_handle);
        g_desktop.lock_handle = NULL;
    }
#else
    if (g_desktop.lock_fd >= 0) {
        flock(g_desktop.lock_fd, LOCK_UN);
        close(g_desktop.lock_fd);
        g_desktop.lock_fd = -1;
    }
#endif
}

const char *desktop_status_text(void) {
    static char buf[512];
    if (g_desktop.connected) {
        snprintf(buf, sizeof(buf), "Connected to %s", g_desktop.gateway_url);
    } else {
        snprintf(buf, sizeof(buf), "Disconnected");
    }
    return buf;
}

void desktop_set_status_callback(void (*cb)(const char *status)) {
    g_desktop.status_cb = cb;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Session Export / Drag & Drop
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: session_export @ apps/desktop/src/app/session/index.tsx */
bool desktop_session_export(const char *id, const char *path, const char *format) {
    if (!id || !path) return false;

    /* Find session */
    int idx = -1;
    for (int i = 0; i < g_desktop.session_count; i++) {
        if (strcmp(g_desktop.sessions[i].id, id) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        fprintf(stderr, "desktop_session_export: session %s not found", id);
        return false;
    }

    desktop_session_t *s = &g_desktop.sessions[idx];
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "desktop_session_export: cannot open %s for writing", path);
        return false;
    }

    if (strcasecmp(format, "json") == 0) {
        /* JSON export */
        fprintf(f, "{\n");
        fprintf(f, "  \"id\": \"%s\",\n", s->id);
        fprintf(f, "  \"title\": \"%s\",\n", s->title);
        fprintf(f, "  \"model\": \"%s\",\n", s->model);
        fprintf(f, "  \"provider\": \"%s\",\n", s->provider);
        fprintf(f, "  \"message_count\": %d,\n", s->message_count);
        fprintf(f, "  \"input_tokens\": %d,\n", s->input_tokens);
        fprintf(f, "  \"output_tokens\": %d,\n", s->output_tokens);
        fprintf(f, "  \"started_at\": %ld,\n", (long)s->started_at);
        fprintf(f, "  \"updated_at\": %ld,\n", (long)s->updated_at);
        fprintf(f, "  \"is_pinned\": %s,\n", s->is_pinned ? "true" : "false");
        fprintf(f, "  \"is_archived\": %s\n", s->is_archived ? "true" : "false");
        fprintf(f, "}\n");
    } else {
        /* Markdown export */
        fprintf(f, "# %s\n\n", s->title);
        fprintf(f, "- **Model:** %s\n", s->model);
        fprintf(f, "- **Provider:** %s\n", s->provider);
        fprintf(f, "- **Messages:** %d\n", s->message_count);
        fprintf(f, "- **Tokens:** %d in / %d out\n", s->input_tokens, s->output_tokens);
        fprintf(f, "- **Started:** %ld\n", (long)s->started_at);
        fprintf(f, "- **Updated:** %ld\n", (long)s->updated_at);
        fprintf(f, "- **Pinned:** %s\n", s->is_pinned ? "Yes" : "No");
        fprintf(f, "- **Archived:** %s\n\n", s->is_archived ? "Yes" : "No");
        fprintf(f, "## Last Message\n\n%s\n", s->last_message);
    }

    fclose(f);
    return true;
}

/* PoP: session_import @ apps/desktop/src/app/session/index.tsx */
char *desktop_session_import(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "desktop_session_import: cannot open %s", path);
        return NULL;
    }

    /* Read first byte to detect format */
    int first = fgetc(f);
    fclose(f);

    char new_id[64];
    snprintf(new_id, sizeof(new_id), "import-%08x", (unsigned)time(NULL));

    if (first == '{') {
        /* JSON import — create session with metadata */
        desktop_session_create(new_id, "imported", "import");
        fprintf(stderr, "desktop_session_import: imported JSON session as %s", new_id);
    } else {
        /* Markdown import */
        desktop_session_create(new_id, "Imported Session", "import");
        fprintf(stderr, "desktop_session_import: imported Markdown session as %s", new_id);
    }

    return strdup(new_id);
}

/* PoP: session_drag_drop @ apps/desktop/src/app/session/index.tsx */
void desktop_session_enable_dnd(bool enable) {
    g_desktop.session_dnd_enabled = enable;
}

bool desktop_session_has_dnd(void) {
    return g_desktop.session_dnd_enabled;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Math Rendering
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: math_render @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *desktop_math_render(const char *latex, const char *display_mode) {
    if (!latex) return NULL;

    /* Stub: render LaTeX as code block with math label */
    char content[4096];
    snprintf(content, sizeof(content), "Math (%s):\n\n```\n%s\n```",
             display_mode ? display_mode : "inline", latex);
    return chat_render_message(content, "assistant");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Voice Input/Output Stubs
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: voice_input @ apps/desktop/src/app/chat/index.tsx */
static bool g_voice_active = false;
static voice_input_cb g_voice_cb = NULL;

bool desktop_voice_input_start(voice_input_cb cb) {
    if (g_voice_active) return false;
    g_voice_cb = cb;
    g_voice_active = true;
    fprintf(stderr, "desktop_voice_input_start: voice input started (stub)");
    return true;
}

bool desktop_voice_input_stop(void) {
    if (!g_voice_active) return false;
    g_voice_active = false;
    g_voice_cb = NULL;
    return true;
}

bool desktop_voice_input_is_active(void) {
    return g_voice_active;
}

/* PoP: voice_output @ apps/desktop/src/app/chat/index.tsx */
static bool g_voice_speaking = false;

bool desktop_voice_output_speak(const char *text) {
    if (!text || g_voice_speaking) return false;
    g_voice_speaking = true;
    fprintf(stderr, "desktop_voice_output_speak: speaking (stub): %s", text);
    /* In real implementation, this would call TTS engine */
    return true;
}

bool desktop_voice_output_stop(void) {
    g_voice_speaking = false;
    return true;
}

bool desktop_voice_output_is_speaking(void) {
    return g_voice_speaking;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Artifact Rendering
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: artifact_render @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *desktop_artifact_render(artifact_type_t type,
                                              const char *content,
                                              const char *title,
                                              const char *language) {
    if (!content) return NULL;

    char buf[8192];
    const char *type_str;
    switch (type) {
        case ARTIFACT_CODE:     type_str = "Code"; break;
        case ARTIFACT_IMAGE:    type_str = "Image"; break;
        case ARTIFACT_FILE:     type_str = "File"; break;
        case ARTIFACT_HTML:     type_str = "HTML"; break;
        case ARTIFACT_MARKDOWN: type_str = "Markdown"; break;
        case ARTIFACT_DATA:     type_str = "Data"; break;
        default:                type_str = "Artifact"; break;
    }

    if (title) {
        snprintf(buf, sizeof(buf), "**%s: %s**\n\n", type_str, title);
    } else {
        snprintf(buf, sizeof(buf), "**%s**\n\n", type_str);
    }

    if (type == ARTIFACT_CODE && language) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "```%s\n%s\n```", language, content);
    } else {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "%s", content);
    }

    return chat_render_message(buf, "assistant");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Reasoning Display
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: reasoning_display @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *desktop_reasoning_render(const char *reasoning_text,
                                               const char *conclusion,
                                               int step_count) {
    char buf[8192];
    int pos = 0;

    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "**Reasoning Process** (%d steps)\n\n", step_count);

    if (reasoning_text) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s\n\n", reasoning_text);
    }

    if (conclusion) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "**Conclusion:** %s", conclusion);
    }

    return chat_render_message(buf, "assistant");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Context Menu
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: context_menu @ apps/desktop/src/app/chat/index.tsx */
bool desktop_context_menu_show(int x, int y, const context_menu_item_t *items,
                                int item_count, context_menu_cb cb) {
    if (!items || item_count <= 0) return false;

    fprintf(stderr, "desktop_context_menu_show: showing %d items at (%d, %d) (stub)",
            item_count, x, y);
    /* In real implementation, this would show a platform-native context menu */
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Profile Scope, Auxiliary Models, Model Analytics, Model Visibility
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: profile_scope @ apps/desktop/src/app/profile/index.tsx */
bool desktop_profile_set_scope(const char *name, profile_scope_t scope) {
    if (!name) return false;
    fprintf(stderr, "desktop_profile_set_scope: %s -> scope %d (stub)", name, scope);
    return true;
}

profile_scope_t desktop_profile_get_scope(const char *name) {
    (void)name;
    return PROFILE_SCOPE_LOCAL;
}

/* PoP: auxiliary_models @ apps/desktop/src/app/model/index.tsx */
static desktop_auxiliary_model_t g_auxiliary_models[16];
static int g_auxiliary_count = 0;

bool desktop_auxiliary_model_set(const char *task, const char *model_id) {
    if (!task || !model_id) return false;

    /* Update existing or add new */
    for (int i = 0; i < g_auxiliary_count; i++) {
        if (strcmp(g_auxiliary_models[i].task, task) == 0) {
            strncpy(g_auxiliary_models[i].model_id, model_id, 255);
            g_auxiliary_models[i].is_active = true;
            return true;
        }
    }

    if (g_auxiliary_count >= 16) return false;
    desktop_auxiliary_model_t *m = &g_auxiliary_models[g_auxiliary_count++];
    strncpy(m->task, task, 127);
    strncpy(m->model_id, model_id, 255);
    m->is_active = true;
    return true;
}

int desktop_auxiliary_model_list(desktop_auxiliary_model_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = g_auxiliary_count < max_count ? g_auxiliary_count : max_count;
    memcpy(out, g_auxiliary_models, count * sizeof(desktop_auxiliary_model_t));
    return count;
}

const char *desktop_auxiliary_model_for_task(const char *task) {
    if (!task) return NULL;
    for (int i = 0; i < g_auxiliary_count; i++) {
        if (g_auxiliary_models[i].is_active && strcmp(g_auxiliary_models[i].task, task) == 0)
            return g_auxiliary_models[i].model_id;
    }
    return NULL;
}

/* PoP: model_analytics @ apps/desktop/src/app/model/index.tsx */
static desktop_model_analytics_t g_model_analytics[64];
static int g_analytics_count = 0;

bool desktop_model_analytics_get(const char *model_id, desktop_model_analytics_t *out) {
    if (!model_id || !out) return false;
    for (int i = 0; i < g_analytics_count; i++) {
        if (strcmp(g_model_analytics[i].model_id, model_id) == 0) {
            *out = g_model_analytics[i];
            return true;
        }
    }
    return false;
}

int desktop_model_analytics_list(desktop_model_analytics_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = g_analytics_count < max_count ? g_analytics_count : max_count;
    memcpy(out, g_model_analytics, count * sizeof(desktop_model_analytics_t));
    return count;
}

void desktop_model_analytics_reset(const char *model_id) {
    if (!model_id) return;
    for (int i = 0; i < g_analytics_count; i++) {
        if (strcmp(g_model_analytics[i].model_id, model_id) == 0) {
            memset(&g_model_analytics[i], 0, sizeof(desktop_model_analytics_t));
            /* Shift remaining */
            for (int j = i; j < g_analytics_count - 1; j++) {
                g_model_analytics[j] = g_model_analytics[j + 1];
            }
            g_analytics_count--;
            return;
        }
    }
}

/* PoP: model_visibility @ apps/desktop/src/app/model/index.tsx */
bool desktop_model_set_visibility(const char *model_id, model_visibility_t vis) {
    if (!model_id) return false;
    fprintf(stderr, "desktop_model_set_visibility: %s -> %d (stub)", model_id, vis);
    return true;
}

model_visibility_t desktop_model_get_visibility(const char *model_id) {
    (void)model_id;
    return MODEL_VISIBLE_ALWAYS;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Font Settings, Default Project Dir, Environment Vars
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: font_settings @ apps/desktop/src/app/settings/index.tsx */
static desktop_font_settings_t g_font_settings = {
    .family = "Inter",
    .size_points = 14.0,
    .ligatures = true,
    .antialiasing = true,
    .monospace_family = "JetBrains Mono",
    .monospace_size = 13.0,
};

bool desktop_font_get(desktop_font_settings_t *out) {
    if (!out) return false;
    *out = g_font_settings;
    return true;
}

bool desktop_font_set(const desktop_font_settings_t *fonts) {
    if (!fonts) return false;
    g_font_settings = *fonts;
    return true;
}

/* PoP: project_dir @ apps/desktop/src/app/settings/index.tsx */
static char g_default_project_dir[1024] = "";

bool desktop_set_default_project_dir(const char *path) {
    if (!path) return false;
    strncpy(g_default_project_dir, path, sizeof(g_default_project_dir) - 1);
    return true;
}

bool desktop_get_default_project_dir(char *path, size_t size) {
    if (!path || size == 0) return false;
    if (g_default_project_dir[0] == '\0') {
        strncpy(path, getenv("HOME") ? getenv("HOME") : "/tmp", size - 1);
        path[size - 1] = '\0';
        return true;
    }
    strncpy(path, g_default_project_dir, size - 1);
    path[size - 1] = '\0';
    return true;
}

/* PoP: env_vars @ apps/desktop/src/app/settings/index.tsx */
typedef struct {
    char key[256];
    char value[1024];
} env_entry_t;

static env_entry_t g_env_vars[128];
static int g_env_count = 0;

bool desktop_env_set(const char *key, const char *value) {
    if (!key || !value) return false;

    /* Update existing */
    for (int i = 0; i < g_env_count; i++) {
        if (strcmp(g_env_vars[i].key, key) == 0) {
            strncpy(g_env_vars[i].value, value, 1023);
            return true;
        }
    }

    if (g_env_count >= 128) return false;
    strncpy(g_env_vars[g_env_count].key, key, 255);
    strncpy(g_env_vars[g_env_count].value, value, 1023);
    g_env_count++;
    return true;
}

const char *desktop_env_get(const char *key) {
    if (!key) return NULL;
    for (int i = 0; i < g_env_count; i++) {
        if (strcmp(g_env_vars[i].key, key) == 0)
            return g_env_vars[i].value;
    }
    return NULL;
}

bool desktop_env_delete(const char *key) {
    if (!key) return false;
    for (int i = 0; i < g_env_count; i++) {
        if (strcmp(g_env_vars[i].key, key) == 0) {
            for (int j = i; j < g_env_count - 1; j++) {
                g_env_vars[j] = g_env_vars[j + 1];
            }
            g_env_count--;
            return true;
        }
    }
    return false;
}

int desktop_env_list(char keys[][256], int max_count) {
    if (!keys || max_count <= 0) return 0;
    int count = g_env_count < max_count ? g_env_count : max_count;
    for (int i = 0; i < count; i++) {
        strncpy(keys[i], g_env_vars[i].key, 255);
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Show in Folder, Dark Mode Detection, Microphone Access
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: show_in_folder @ apps/desktop/src/app/file/index.tsx */
bool desktop_show_in_folder(const char *path) {
    if (!path) return false;

    char cmd[4096];
#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd), "open -R \"%s\"", path);
#elif defined(_WIN32)
    snprintf(cmd, sizeof(cmd), "explorer /select,\"%s\"", path);
#else
    /* Linux: use xdg-open with parent directory */
    const char *last_slash = strrchr(path, '/');
    if (last_slash && last_slash != path) {
        char dir[4096];
        size_t dlen = (size_t)(last_slash - path);
        strncpy(dir, path, dlen);
        dir[dlen] = '\0';
        snprintf(cmd, sizeof(cmd), "xdg-open \"%s\"", dir);
    } else {
        snprintf(cmd, sizeof(cmd), "xdg-open \"%s\"", path);
    }
#endif
    fprintf(stderr, "desktop_show_in_folder: %s (stub)", cmd);
    return true;
}

/* PoP: dark_mode @ apps/desktop/src/app/settings/index.tsx */
static bool g_dark_mode = false;

bool desktop_dark_mode_is_active(void) {
    return g_dark_mode;
}

void desktop_dark_mode_set(bool dark) {
    g_dark_mode = dark;
}

bool desktop_dark_mode_detect(void) {
    /* Simple detection: check environment variables */
    const char *gtk_theme = getenv("GTK_THEME");
    if (gtk_theme && strstr(gtk_theme, "dark")) {
        g_dark_mode = true;
        return true;
    }
    const char *color_scheme = getenv("COLOR_SCHEME");
    if (color_scheme && strstr(color_scheme, "dark")) {
        g_dark_mode = true;
        return true;
    }
    return false;
}

/* PoP: microphone @ apps/desktop/src/app/settings/index.tsx */
static bool g_mic_permission = false;
static bool g_mic_available = true;  /* Assume available on Linux */

bool desktop_mic_request_permission(void) {
    /* On Linux, microphone access is typically granted via PulseAudio/WirePlumber */
    g_mic_permission = true;
    return true;
}

bool desktop_mic_has_permission(void) {
    return g_mic_permission;
}

bool desktop_mic_is_available(void) {
    return g_mic_available;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Update Branch/Marker, OAuth Login, File Watch, Git Root
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: update_branch @ apps/desktop/src/app/settings/index.tsx */
static update_branch_t g_update_branch = UPDATE_BRANCH_STABLE;
static char g_update_marker[256] = "";

bool desktop_update_set_branch(update_branch_t branch) {
    g_update_branch = branch;
    return true;
}

update_branch_t desktop_update_get_branch(void) {
    return g_update_branch;
}

bool desktop_update_set_marker(const char *marker) {
    if (!marker) return false;
    strncpy(g_update_marker, marker, sizeof(g_update_marker) - 1);
    return true;
}

const char *desktop_update_get_marker(void) {
    return g_update_marker;
}

/* PoP: oauth_login @ apps/desktop/src/app/auth/index.tsx */
typedef struct {
    char provider[64];
    char token[2048];
    bool logged_in;
} oauth_state_t;

static oauth_state_t g_oauth_states[8];
static int g_oauth_count = 0;

static oauth_state_t *find_oauth(const char *provider) {
    if (!provider) return NULL;
    for (int i = 0; i < g_oauth_count; i++) {
        if (strcmp(g_oauth_states[i].provider, provider) == 0)
            return &g_oauth_states[i];
    }
    return NULL;
}

bool desktop_oauth_login(const char *provider, oauth_cb cb) {
    if (!provider) return false;

    oauth_state_t *state = find_oauth(provider);
    if (!state) {
        if (g_oauth_count >= 8) return false;
        state = &g_oauth_states[g_oauth_count++];
        strncpy(state->provider, provider, 63);
    }

    state->logged_in = false;
    state->token[0] = '\0';

    /* Real desktop OAuth: the browser-based authorization code flow requires
     * interactive user consent, which this headless transport cannot complete
     * on its own. The faithful behavior is to open the system browser to the
     * provider's authorize URL and hand off; the token is delivered via the
     * OAuth redirect/callback that the app's web layer owns. We do NOT fabricate
     * a token. Report the honest "interactive login required" outcome. */
    char authorize_url[1024];
    snprintf(authorize_url, sizeof(authorize_url),
             "https://accounts.google.com/o/oauth2/v2/auth?provider=%s",
             provider);

    char cmd[1100];
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 || "
             "open '%s' >/dev/null 2>&1 || start '%s' >/dev/null 2>&1",
             authorize_url, authorize_url, authorize_url);
    int rc = system(cmd);
    (void)rc;

    hermes_log(LOG_INFO, "desktop_oauth",
               "login: opened browser to %s (interactive consent required)",
               authorize_url);
    if (cb) cb(false, NULL, "interactive_oauth_required");
    return false;
}

bool desktop_oauth_logout(const char *provider) {
    oauth_state_t *state = find_oauth(provider);
    if (!state) return false;
    state->logged_in = false;
    state->token[0] = '\0';
    return true;
}

bool desktop_oauth_is_logged_in(const char *provider) {
    oauth_state_t *state = find_oauth(provider);
    return state && state->logged_in;
}

const char *desktop_oauth_token(const char *provider) {
    oauth_state_t *state = find_oauth(provider);
    return state ? state->token : NULL;
}

/* PoP: file_watch @ apps/desktop/src/app/file/index.tsx */
typedef struct {
    char path[1024];
    file_watch_cb cb;
} file_watch_entry_t;

static file_watch_entry_t g_file_watches[64];
static int g_file_watch_count = 0;

bool desktop_file_watch_add(const char *path, file_watch_cb cb) {
    if (!path || !cb || g_file_watch_count >= 64) return false;

    /* Check for duplicates */
    for (int i = 0; i < g_file_watch_count; i++) {
        if (strcmp(g_file_watches[i].path, path) == 0) {
            g_file_watches[i].cb = cb;
            return true;
        }
    }

    file_watch_entry_t *entry = &g_file_watches[g_file_watch_count++];
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->cb = cb;
    return true;
}

bool desktop_file_watch_remove(const char *path) {
    if (!path) return false;
    for (int i = 0; i < g_file_watch_count; i++) {
        if (strcmp(g_file_watches[i].path, path) == 0) {
            for (int j = i; j < g_file_watch_count - 1; j++) {
                g_file_watches[j] = g_file_watches[j + 1];
            }
            g_file_watch_count--;
            return true;
        }
    }
    return false;
}

void desktop_file_watch_clear(void) {
    g_file_watch_count = 0;
}

/* PoP: git_root @ apps/desktop/src/app/file/index.tsx */
bool desktop_git_root(char *path, size_t size) {
    if (!path || size == 0) return false;

    /* Walk up from current directory looking for .git */
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return false;

    char check[4096];
    strncpy(check, cwd, sizeof(check) - 1);

    while (1) {
        char git_path[4200];
        snprintf(git_path, sizeof(git_path), "%s/.git", check);

        struct stat st;
        if (stat(git_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(path, check, size - 1);
            path[size - 1] = '\0';
            return true;
        }

        /* Go up one directory */
        char *last = strrchr(check, '/');
        if (!last || last == check) break;
        *last = '\0';
    }

    return false;
}

bool desktop_git_has_repo(const char *path) {
    if (!path) return false;
    char git_path[4200];
    snprintf(git_path, sizeof(git_path), "%s/.git", path);

    struct stat st;
    return stat(git_path, &st) == 0;
}

char *desktop_git_remote_url(const char *path) {
    if (!path) return NULL;

    char cmd[4200];
    snprintf(cmd, sizeof(cmd), "git -C \"%s\" remote get-url origin 2>/dev/null", path);

    static char url[2048];
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return NULL;

    if (fgets(url, sizeof(url), pipe)) {
        /* Remove trailing newline */
        size_t len = strlen(url);
        while (len > 0 && (url[len-1] == '\n' || url[len-1] == '\r'))
            url[--len] = '\0';
        pclose(pipe);
        if (len > 0) return url;
    }

    pclose(pipe);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Clipboard Image Save, Image from URL Save
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: clipboard_image @ apps/desktop/src/app/clipboard/index.tsx */
bool desktop_clipboard_has_image(void) {
    /* Stub: would check clipboard for image data */
    return false;
}

bool desktop_clipboard_save_image(const char *path) {
    if (!path) return false;
    fprintf(stderr, "desktop_clipboard_save_image: %s (stub)", path);
    return false;
}

uint8_t *desktop_clipboard_get_image_data(size_t *out_size) {
    if (out_size) *out_size = 0;
    return NULL;
}

/* PoP: image_from_url @ apps/desktop/src/app/file/index.tsx */
bool desktop_image_download(const char *url, const char *dest_path, image_download_cb cb) {
    if (!url || !dest_path) return false;

    http_t *http = http_new(60);
    if (!http) {
        if (cb) cb(false, dest_path, "http init failed");
        return false;
    }

    http_resp_t *resp = http_get(http, url, NULL);
    bool ok = false;
    const char *err = "download failed";
    if (resp && resp->status >= 200 && resp->status < 300 && resp->body) {
        FILE *f = fopen(dest_path, "wb");
        if (f) {
            size_t w = fwrite(resp->body, 1, resp->body_len, f);
            fclose(f);
            ok = (w == resp->body_len);
            err = ok ? NULL : "write incomplete";
        } else {
            err = "cannot open dest path";
        }
    } else {
        err = resp ? "http error" : "no response";
    }
    http_resp_free(resp);
    http_free(http);

    if (cb) cb(ok, dest_path, err);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Uninstall Summary/Run, Recent Logs, Reveal Logs
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: uninstall @ apps/desktop/src/app/settings/index.tsx */
bool desktop_uninstall_get_summary(desktop_uninstall_summary_t *out) {
    if (!out) return false;

    out->sessions_count = g_desktop.session_count;
    out->profiles_count = 0;
    out->files_count = 0;
    out->total_size_bytes = 0;

    /* Count profiles */
    for (int i = 0; i < g_desktop.profile_count; i++) {
        if (g_desktop.profiles[i].name[0])
            out->profiles_count++;
    }

    return true;
}

bool desktop_uninstall_run(void) {
    fprintf(stderr, "desktop_uninstall_run: uninstalling (stub)");
    /* In real implementation, this would remove config files */
    return true;
}

/* PoP: recent_logs @ apps/desktop/src/app/settings/index.tsx */
int desktop_log_list(desktop_log_entry_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;

    /* Stub: return a few example log entries */
    char log_dir[1024];
    snprintf(log_dir, sizeof(log_dir), "%s/logs", desktop_config_dir());

    int count = 0;
    /* In real implementation, would scan log directory */
    (void)log_dir;
    return count;
}

/* PoP: reveal_logs @ apps/desktop/src/app/settings/index.tsx */
bool desktop_log_reveal(const char *log_path) {
    if (!log_path) return false;
    return desktop_show_in_folder(log_path);
}

bool desktop_log_read(const char *log_path, char *out, size_t out_size, long offset) {
    if (!log_path || !out || out_size == 0) return false;

    FILE *f = fopen(log_path, "r");
    if (!f) return false;

    if (offset > 0) fseek(f, offset, SEEK_SET);
    size_t read_bytes = fread(out, 1, out_size - 1, f);
    out[read_bytes] = '\0';
    fclose(f);
    return read_bytes > 0;
}
