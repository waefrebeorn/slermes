/*
 * desktop_models.c — concern module extracted from desktop_app_common.c.
 * Self-contained, operates on shared g_desktop (desktop_state.h), C11.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "desktop_state.h"

/* Module-local state for auxiliary models + analytics. */
desktop_auxiliary_model_t g_auxiliary_models[16];
int g_auxiliary_count = 0;
desktop_model_analytics_t g_model_analytics[64];
int g_analytics_count = 0;

int desktop_model_list(desktop_model_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = g_desktop.model_count < max_count ? g_desktop.model_count : max_count;
    memcpy(out, g_desktop.models, count * sizeof(desktop_model_t));
    return count;
}

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

bool desktop_model_set_visibility(const char *model_id, model_visibility_t vis) {
    if (!model_id) return false;
    fprintf(stderr, "desktop_model_set_visibility: %s -> %d (stub)", model_id, vis);
    return true;
}

model_visibility_t desktop_model_get_visibility(const char *model_id) {
    (void)model_id;
    return MODEL_VISIBLE_ALWAYS;
}

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

