/*
 * desktop_sessions.c — concern module extracted from desktop_app_common.c.
 * Self-contained, operates on shared g_desktop (desktop_state.h), C11.
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "desktop_state.h"

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

bool desktop_session_rename(const char *id, const char *new_title) {
    int idx = find_session_by_id(id);
    if (idx < 0 || !new_title || !*new_title) return false;

    strncpy(g_desktop.sessions[idx].title, new_title, sizeof(g_desktop.sessions[idx].title) - 1);
    g_desktop.sessions[idx].updated_at = time(NULL);

    fprintf(stderr, "desktop_session_rename: '%s' -> '%s'\n", id, new_title);
    notify_status("Renamed: %s", new_title);
    return true;
}

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

void desktop_session_enable_dnd(bool enable) {
    g_desktop.session_dnd_enabled = enable;
}

bool desktop_session_has_dnd(void) {
    return g_desktop.session_dnd_enabled;
}

