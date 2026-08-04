/*
 * desktop_settings.c — concern module extracted from desktop_app_common.c.
 * Self-contained, operates on shared g_desktop (desktop_state.h), C11.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include "hermes_json.h"
#include "desktop_state.h"

typedef struct {
    char key[256];
    char *value;  /* heap-allocated (was char[2048] × 128 ≈ 288KB .bss) */
} safe_entry_t;

#define MAX_SAFE_ENTRIES 128

static struct {
    safe_entry_t entries[MAX_SAFE_ENTRIES];
    int count;
    bool loaded;
} g_safe = {0};

desktop_setting_t *find_setting(const char *key) {
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
        if (s->type == SETTING_STRING) {
            free(s->value.s);
            s->value.s = strdup(value ? value : "");
        } else {
            /* Type changed from numeric to string — safe to assign. */
            s->type = SETTING_STRING;
            s->value.s = strdup(value ? value : "");
        }
        return true;
    }
    if (g_desktop.setting_count >= DESKTOP_MAX_SETTINGS) return false;
    s = &g_desktop.settings[g_desktop.setting_count++];
    strncpy(s->key, key, sizeof(s->key) - 1);
    s->type = SETTING_STRING;
    s->value.s = strdup(value ? value : "");
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

desktop_theme_t desktop_settings_get_theme(void) {
    return g_desktop.theme;
}

bool desktop_settings_set_theme(desktop_theme_t theme) {
    g_desktop.theme = theme;
    desktop_settings_set_int("theme", (int)theme);
    notify_status("Theme: %s", theme == THEME_DARK ? "dark" : theme == THEME_LIGHT ? "light" : "system");
    return true;
}

/* PoP: _get_gateway_url @ gateway/platforms/qqbot/adapter.py:_get_gateway_url */
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

void safe_storage_load(void) {
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
                e->value = strdup(val);
            }
        }
    }
    free(content);
}

void safe_storage_save(void) {
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
            free(g_safe.entries[i].value);
            g_safe.entries[i].value = strdup(value ? value : "");
            safe_storage_save();
            return true;
        }
    }
    if (g_safe.count >= MAX_SAFE_ENTRIES) return false;
    safe_entry_t *e = &g_safe.entries[g_safe.count++];
    strncpy(e->key, key, sizeof(e->key) - 1);
    e->value = strdup(value ? value : "");
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
            free(g_safe.entries[i].value);
            memmove(&g_safe.entries[i], &g_safe.entries[i + 1],
                    (g_safe.count - i - 1) * sizeof(safe_entry_t));
            g_safe.count--;
            safe_storage_save();
            return true;
        }
    }
    return false;
}

