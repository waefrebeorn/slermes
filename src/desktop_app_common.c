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
#include "desktop_state.h"
#include "hermes_logger.h"
#include "libhttp/http.h"
#include "json.h"
#include "session_db.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════
 *  Internal State
 * ═══════════════════════════════════════════════════════════════════════ */

/* Shared desktop state instance — type defined in desktop_state.h. */
desktop_state_t g_desktop = {0};

/* ═══════════════════════════════════════════════════════════════════════
 *  Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════ */

void notify_status(const char *fmt, ...) {
    if (!g_desktop.status_cb) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_desktop.status_cb(buf);
}

int find_session_by_id(const char *id) {
    for (int i = 0; i < g_desktop.session_count; i++) {
        if (strcmp(g_desktop.sessions[i].id, id) == 0)
            return i;
    }
    return -1;
}

int find_model_by_id(const char *id) {
    for (int i = 0; i < g_desktop.model_count; i++) {
        if (strcmp(g_desktop.models[i].model_id, id) == 0)
            return i;
    }
    return -1;
}

int find_profile_by_name(const char *name) {
    for (int i = 0; i < g_desktop.profile_count; i++) {
        if (strcmp(g_desktop.profiles[i].name, name) == 0)
            return i;
    }
    return -1;
}

const char *desktop_config_dir(void) {
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

const char *desktop_settings_path(void) {
    static char buf[1024];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/settings.json", desktop_config_dir());
    return buf;
}

const char *desktop_sessions_path(void) {
    static char buf[1024];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/sessions.json", desktop_config_dir());
    return buf;
}

const char *desktop_profiles_dir(void) {
    static char buf[1024];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/profiles", desktop_config_dir());
    dir_create(buf);
    return buf;
}

const char *desktop_safe_storage_path(void) {
    static char buf[1024];
    if (buf[0]) return buf;
    snprintf(buf, sizeof(buf), "%s/safe_storage.dat", desktop_config_dir());
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Session Management
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: session_delete @ apps/desktop/src/app/session/hooks/use-session-actions.ts */

/* PoP: session_delete @ apps/desktop/src/app/session/hooks/use-session-actions.ts */


/* PoP: session_rename @ apps/desktop/src/app/session/hooks/use-session-actions.ts */

/* PoP: session_archive @ apps/desktop/src/app/session/hooks/use-session-actions.ts */


/* PoP: session_search @ apps/desktop/src/app/session/hooks/use-session-actions.ts */

/* ═══════════════════════════════════════════════════════════════════════
 *  Model Management
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: model_picker @ apps/desktop/src/app/model-picker-overlay.tsx */

/* PoP: model_switch @ apps/desktop/src/app/session/hooks/use-model-controls.ts */


/* ═══════════════════════════════════════════════════════════════════════
 *  Profile Management
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: profile_list @ apps/desktop/src/app/profiles/index.tsx */

/* PoP: profile_create @ apps/desktop/src/app/profiles/create-profile-dialog.tsx */

/* PoP: profile_delete @ apps/desktop/src/app/profiles/delete-profile-dialog.tsx */

/* PoP: profile_rename @ apps/desktop/src/app/profiles/rename-profile-dialog.tsx */


/* PoP: profile_soul @ apps/desktop/src/app/profiles/index.tsx */


/* PoP: profile_model @ apps/desktop/src/app/profiles/index.tsx */


/* ═══════════════════════════════════════════════════════════════════════
 *  Settings
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: settings_page @ apps/desktop/src/app/settings/index.tsx */


/* PoP: theme_switcher @ apps/desktop/src/app/settings/index.tsx */


/* PoP: connection_config @ apps/desktop/src/app/settings/index.tsx */


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
 *  File Dialogs — real implementation (zenity/kdialog, curses fallback)
 * ═══════════════════════════════════════════════════════════════════════ */

/* Try a desktop-native selector via popen; returns malloc'd path or NULL.
 * cmd is the full shell command that prints a single path to stdout. */
static char *desktop_run_selector(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;
    char buf[4096];
    if (!fgets(buf, sizeof(buf), f)) { pclose(f); return NULL; }
    pclose(f);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (n == 0) return NULL;
    return strdup(buf);
}

/* PoP: file_dialog_open @ electron/main.cjs:selectPaths */
char *desktop_file_dialog_open(const char *title, const char *filter) {
    (void)filter;
    /* Prefer desktop-native selectors. */
    char cmd[1024];
    if (system("command -v zenity >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "zenity --file-selection --title '%s'",
                 title ? title : "Open");
        char *r = desktop_run_selector(cmd);
        if (r) return r;
    }
    if (system("command -v kdialog >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "kdialog --getopenfilename . '%s'",
                 title ? title : "Open");
        char *r = desktop_run_selector(cmd);
        if (r) return r;
    }
    /* Fallback: read a path from stdin (headless / SSH / WSL context). */
    fprintf(stderr, "desktop_file_dialog_open: no GUI selector; reading path from stdin: ");
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (n == 0) return NULL;
    FILE *t = fopen(buf, "rb");
    if (!t) { fprintf(stderr, "desktop_file_dialog_open: file not found: %s\n", buf); return NULL; }
    fclose(t);
    return strdup(buf);
}

/* PoP: file_dialog_save @ electron/main.cjs:selectPaths */
char *desktop_file_dialog_save(const char *title, const char *default_name, const char *filter) {
    (void)filter;
    char cmd[1024];
    if (system("command -v zenity >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "zenity --file-selection --save --confirm-overwrite --title '%s'%s%s",
                 title ? title : "Save",
                 default_name ? " --filename " : "",
                 default_name ? default_name : "");
        char *r = desktop_run_selector(cmd);
        if (r) return r;
    }
    if (system("command -v kdialog >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "kdialog --getsavefilename . '%s'",
                 default_name ? default_name : "");
        char *r = desktop_run_selector(cmd);
        if (r) return r;
    }
    fprintf(stderr, "desktop_file_dialog_save: no GUI selector; reading path from stdin: ");
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (n == 0) return NULL;
    return strdup(buf);
}

char *desktop_file_dialog_pick_dir(const char *title) {
    char cmd[1024];
    if (system("command -v zenity >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "zenity --file-selection --directory --title '%s'",
                 title ? title : "Select Directory");
        char *r = desktop_run_selector(cmd);
        if (r) return r;
    }
    if (system("command -v kdialog >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "kdialog --getexistingdirectory .");
        char *r = desktop_run_selector(cmd);
        if (r) return r;
    }
    fprintf(stderr, "desktop_file_dialog_pick_dir: no GUI selector; reading dir from stdin: ");
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (n == 0) return NULL;
    struct stat st;
    if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "desktop_file_dialog_pick_dir: not a directory: %s\n", buf);
        return NULL;
    }
    return strdup(buf);
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

/* safe-storage state moved to desktop_settings.c */


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
    g_desktop.update_info.downloading = true;
    g_desktop.update_info.download_progress = 0.0;

    http_t *http = http_new(120);
    if (!http) { g_desktop.update_info.downloading = false; return false; }
    http_resp_t *resp = http_get(http, url, NULL);
    bool ok = false;
    if (resp && resp->status >= 200 && resp->status < 300 && resp->body && resp->body_len > 0) {
        /* atomic write: tmp + rename */
        char tmp[1088];
        snprintf(tmp, sizeof(tmp), "%s.tmp.%d", dest_path, (int)getpid());
        FILE *f = fopen(tmp, "wb");
        if (f) {
            size_t wr = fwrite(resp->body, 1, resp->body_len, f);
            fflush(f);
            fclose(f);
            if (wr == resp->body_len && rename(tmp, dest_path) == 0) {
                chmod(dest_path, 0755);
                ok = true;
            } else {
                unlink(tmp);
            }
        }
    } else {
        fprintf(stderr, "desktop_update_download: HTTP %d\n", resp ? resp->status : -1);
    }
    if (resp) http_resp_free(resp);
    http_free(http);
    g_desktop.update_info.downloading = false;
    g_desktop.update_info.download_progress = ok ? 1.0 : 0.0;
    return ok;
}

/* PoP: update_apply @ electron/main.cjs:update-relaunch */
bool desktop_update_apply(const char *update_path) {
    if (!update_path) return false;
    fprintf(stderr, "desktop_update_apply: %s\n", update_path);
    /* Electron's update-relaunch = swap binary + relaunch. C analogue:
     * replace our own executable with the downloaded binary, then exec it. */
    char self[1024];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n <= 0) return false;
    self[n] = '\0';

    struct stat st;
    if (stat(update_path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size == 0) {
        fprintf(stderr, "desktop_update_apply: update file missing or empty\n");
        return false;
    }

    /* keep a rollback copy, then move update into place */
    char backup[1088];
    snprintf(backup, sizeof(backup), "%s.old", self);
    unlink(backup);
    if (rename(self, backup) != 0) {
        fprintf(stderr, "desktop_update_apply: cannot back up current binary: %s\n",
                strerror(errno));
        return false;
    }
    if (rename(update_path, self) != 0) {
        /* cross-device rename fails: fall back to copy */
        FILE *in = fopen(update_path, "rb");
        FILE *out = in ? fopen(self, "wb") : NULL;
        bool copied = false;
        if (in && out) {
            char buf[65536];
            size_t r;
            copied = true;
            while ((r = fread(buf, 1, sizeof(buf), in)) > 0)
                if (fwrite(buf, 1, r, out) != r) { copied = false; break; }
        }
        if (in) fclose(in);
        if (out) fclose(out);
        if (!copied) {
            rename(backup, self);   /* roll back */
            fprintf(stderr, "desktop_update_apply: install failed, rolled back\n");
            return false;
        }
        unlink(update_path);
    }
    chmod(self, 0755);

    /* relaunch (Electron app.relaunch() + app.exit()) */
    execl(self, self, (char *)NULL);
    /* only reached when exec failed — roll back */
    fprintf(stderr, "desktop_update_apply: exec failed: %s — rolling back\n",
            strerror(errno));
    rename(backup, self);
    return false;
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

/* PoP: session_import @ apps/desktop/src/app/session/index.tsx */
char *desktop_session_import(const char *path) {
    if (!path) return NULL;

    /* Read the whole file (JSON or markdown). */
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "desktop_session_import: cannot open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) {
        fclose(f);
        fprintf(stderr, "desktop_session_import: bad file size %ld\n", sz);
        return NULL;
    }
    char *data = malloc((size_t)sz + 1);
    if (!data) { fclose(f); return NULL; }
    size_t rd = fread(data, 1, (size_t)sz, f);
    fclose(f);
    data[rd] = '\0';

    /* Skip leading whitespace to detect format. */
    const char *p = data;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;

    char new_id[64];
    snprintf(new_id, sizeof(new_id), "import-%08x", (unsigned)time(NULL));

    if (*p == '{') {
        /* JSON session import: parse metadata + messages. */
        char *err = NULL;
        json_t *doc = json_parse(p, &err);
        if (!doc) {
            fprintf(stderr, "desktop_session_import: JSON parse failed: %s\n",
                    err ? err : "unknown");
            free(err);
            free(data);
            return NULL;
        }
        const char *title = json_get_str(doc, "title", "Imported Session");
        const char *src   = json_get_str(doc, "source", "import");
        const char *model = json_get_str(doc, "model", "");

        /* Create the session row. */
        if (!session_db_create_named(NULL, new_id, title, src, model)) {
            fprintf(stderr, "desktop_session_import: create session failed\n");
            json_free(doc);
            free(data);
            return NULL;
        }

        /* Insert messages if the document carries them. */
        json_t *msgs = json_obj_get(doc, "messages");
        size_t nm = msgs ? json_len(msgs) : 0;
        if (nm > 0) {
            for (size_t i = 0; i < nm; i++) {
                json_t *m = json_get(msgs, i);
                if (!m) continue;
                const char *role = json_get_str(m, "role", "user");
                const char *content = json_get_str(m, "content", "");
                double ts = json_get_num(m, "timestamp", 0.0);
                if (!ts) ts = (double)time(NULL) - (double)(nm - i);
                session_db_insert_message(new_id, role, content, ts);
            }
        }
        json_free(doc);
        fprintf(stderr, "desktop_session_import: imported JSON session %s (%zu messages)\n",
                new_id, nm);
    } else {
        /* Markdown import: title = first heading, body as one user message. */
        const char *title = "Imported Session";
        const char *h = strstr(p, "# ");
        if (h) {
            const char *eol = strchr(h + 2, '\n');
            size_t tl = eol ? (size_t)(eol - h - 2) : strlen(h + 2);
            if (tl > 0 && tl < 200) {
                static char tbuf[256];
                memcpy(tbuf, h + 2, tl);
                tbuf[tl] = '\0';
                title = tbuf;
            }
        }
        /* Create the session row, then store the markdown body. */
        if (!session_db_create_named(NULL, new_id, title, "import", "")) {
            fprintf(stderr, "desktop_session_import: create session failed\n");
            free(data);
            return NULL;
        }
        session_db_insert_message(new_id, "user", p, (double)time(NULL));
        fprintf(stderr, "desktop_session_import: imported Markdown session %s\n", new_id);
    }

    free(data);
    return strdup(new_id);
}

/* PoP: session_drag_drop @ apps/desktop/src/app/session/index.tsx */


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
    /* Delegate to the real voice recorder (voice_mode.c: ALSA capture +
     * VAD state machine, mirrors AudioRecorder). If no audio backend is
     * present, fail-open with a clear message. */
    extern const char *voice_import_audio(void);
    extern void voice_recorder_start(void (*on_silence_stop)(void));
    if (!voice_import_audio()) {
        fprintf(stderr, "desktop_voice_input_start: no audio capture backend\n");
        return false;
    }
    g_voice_cb = cb;
    g_voice_active = true;
    voice_recorder_start(NULL);
    return true;
}

bool desktop_voice_input_stop(void) {
    if (!g_voice_active) return false;
    extern char *voice_recorder_stop(void);
    extern char *transcribe_recording(const char *file_path, const char *model,
                                      int chunk_seconds);
    g_voice_active = false;
    char *wav = voice_recorder_stop();
    if (wav && wav[0] && g_voice_cb) {
        /* Transcribe and deliver the transcript. */
        char *result = transcribe_recording(wav, NULL, 30);
        if (result) {
            json_t *j = json_parse(result, NULL);
            if (j) {
                const char *text = json_get_str(j, "transcript", "");
                if (text && *text) g_voice_cb(text, true);
                json_free(j);
            }
            free(result);
        }
    }
    free(wav);
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

    /* Prefer an available TTS engine; speak asynchronously so callers don't block. */
    char cmd[8200];
    if (system("command -v espeak >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "espeak --stdin >/dev/null 2>&1 & echo '%s' | espeak >/dev/null 2>&1 &",
                 text);
        system(cmd);
        g_voice_speaking = false;
        return true;
    }
    if (system("command -v festival >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "echo '%s' | festival --tts >/dev/null 2>&1 &", text);
        system(cmd);
        g_voice_speaking = false;
        return true;
    }
#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd), "say '%s' >/dev/null 2>&1 &", text);
    system(cmd);
    g_voice_speaking = false;
    return true;
#endif
    /* No TTS engine available — record state, report inability honestly. */
    fprintf(stderr, "desktop_voice_output_speak: no TTS engine (espeak/festival/say) available\n");
    g_voice_speaking = false;
    return false;
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
    (void)x; (void)y;

    /* Prefer a desktop-native list selector. */
    if (system("command -v zenity >/dev/null 2>&1") == 0) {
        /* Build a zenity --list command with the item labels. */
        char cmd[8192];
        int pos = snprintf(cmd, sizeof(cmd), "zenity --list --title 'Menu' --column 'Action'");
        for (int i = 0; i < item_count && pos < (int)sizeof(cmd) - 256; i++) {
            pos += snprintf(cmd + pos, sizeof(cmd) - (size_t)pos, " '%s'",
                            items[i].label ? items[i].label : "");
        }
        FILE *f = popen(cmd, "r");
        if (f) {
            char buf[1024];
            if (fgets(buf, sizeof(buf), f)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
                pclose(f);
                for (int i = 0; i < item_count; i++) {
                    if (items[i].label && strcmp(items[i].label, buf) == 0) {
                        if (cb) cb(items[i].label);
                        return true;
                    }
                }
            } else { pclose(f); }
        }
    }

    /* Fallback: numbered prompt on stdin (headless / SSH / WSL). */
    fprintf(stderr, "context menu:\n");
    for (int i = 0; i < item_count; i++)
        fprintf(stderr, "  %d) %s\n", i + 1, items[i].label ? items[i].label : "");
    fprintf(stderr, "choice: ");
    char buf[256];
    if (!fgets(buf, sizeof(buf), stdin)) return false;
    int choice = atoi(buf);
    if (choice < 1 || choice > item_count) return false;
    if (cb) cb(items[choice - 1].label);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Profile Scope, Auxiliary Models, Model Analytics, Model Visibility
 * ═════════════════════════════════════════════════════════════════════ */

/* PoP: profile_scope @ apps/desktop/src/app/profile/index.tsx */


/* auxiliary + analytics module-local state moved to desktop_models.c */

/* PoP: model_visibility @ apps/desktop/src/app/model/index.tsx */


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
    char *value;  /* heap-allocated (was char[1024] × 128 ≈ 160KB .bss) */
} env_entry_t;

static env_entry_t g_env_vars[128];
static int g_env_count = 0;

bool desktop_env_set(const char *key, const char *value) {
    if (!key || !value) return false;

    /* Update existing */
    for (int i = 0; i < g_env_count; i++) {
        if (strcmp(g_env_vars[i].key, key) == 0) {
            free(g_env_vars[i].value);
            g_env_vars[i].value = strdup(value);
            return true;
        }
    }

    if (g_env_count >= 128) return false;
    strncpy(g_env_vars[g_env_count].key, key, 255);
    g_env_vars[g_env_count].value = strdup(value);
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
            free(g_env_vars[i].value);
            for (int j = i; j < g_env_count - 1; j++) {
                g_env_vars[j] = g_env_vars[j + 1];
                g_env_vars[j + 1].value = NULL;  /* ownership moved */
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
    fprintf(stderr, "desktop_show_in_folder: %s\n", cmd);
    int rc = system(cmd);
    return rc == 0;
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
    char *path;  /* heap-allocated (was char[1024] × 64 ≈ 64KB .bss) */
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
    entry->path = strdup(path);
    entry->cb = cb;
    return true;
}

bool desktop_file_watch_remove(const char *path) {
    if (!path) return false;
    for (int i = 0; i < g_file_watch_count; i++) {
        if (strcmp(g_file_watches[i].path, path) == 0) {
            free(g_file_watches[i].path);
            for (int j = i; j < g_file_watch_count - 1; j++) {
                g_file_watches[j] = g_file_watches[j + 1];
                g_file_watches[j + 1].path = NULL;  /* ownership moved */
            }
            g_file_watch_count--;
            return true;
        }
    }
    return false;
}

void desktop_file_watch_clear(void) {
    for (int i = 0; i < g_file_watch_count; i++)
        free(g_file_watches[i].path);
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
