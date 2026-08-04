/*
 * desktop_profiles.c — concern module extracted from desktop_app_common.c.
 * Self-contained, operates on shared g_desktop (desktop_state.h), C11.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "desktop_state.h"

int desktop_profile_list(desktop_profile_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = g_desktop.profile_count < max_count ? g_desktop.profile_count : max_count;
    memcpy(out, g_desktop.profiles, count * sizeof(desktop_profile_t));
    return count;
}

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
            if (src->soul) p->soul = strdup(src->soul);
            p->skill_count = src->skill_count;
            p->scope = src->scope;
        }
    } else {
        p->scope = PROFILE_SCOPE_LOCAL;
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

    /* Free heap soul before the struct copy shifts pointers */
    free(g_desktop.profiles[idx].soul);
    g_desktop.profiles[idx].soul = NULL;

    /* Shift remaining */
    for (int i = idx; i < g_desktop.profile_count - 1; i++) {
        g_desktop.profiles[i] = g_desktop.profiles[i + 1];
        g_desktop.profiles[i + 1].soul = NULL;  /* ownership moved; clear dup */
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

bool desktop_profile_set_soul(const char *name, const char *soul_content) {
    int idx = find_profile_by_name(name);
    if (idx < 0) return false;

    desktop_profile_t *p = &g_desktop.profiles[idx];
    free(p->soul);
    p->soul = soul_content ? strdup(soul_content) : NULL;

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
        strncpy(out, p->soul ? p->soul : "", out_size - 1);
    }
    out[out_size - 1] = '\0';
    return true;
}

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

bool desktop_profile_set_scope(const char *name, profile_scope_t scope) {
    if (!name) return false;
    int idx = find_profile_by_name(name);
    if (idx < 0) {
        fprintf(stderr, "desktop_profile_set_scope: '%s' not found\n", name);
        return false;
    }
    if (scope < PROFILE_SCOPE_LOCAL || scope > PROFILE_SCOPE_GLOBAL) {
        fprintf(stderr, "desktop_profile_set_scope: invalid scope %d\n", scope);
        return false;
    }
    g_desktop.profiles[idx].scope = scope;
    notify_status("Profile scope: %s -> %d", name, scope);
    return true;
}

profile_scope_t desktop_profile_get_scope(const char *name) {
    if (!name) return PROFILE_SCOPE_LOCAL;
    int idx = find_profile_by_name(name);
    if (idx < 0) return PROFILE_SCOPE_LOCAL;
    return g_desktop.profiles[idx].scope;
}

