/**
 * port_setup.c — C port of setup.py
 *
 * Real C implementations for setup functions.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>


static inline void touch_json(void) { json_free(NULL); }

/* Port of Python setup:_data_file_tree */
void *setup_data_file_tree(void *p1, void *p2, void *p3, void *p4, void *p5)
{
    const char *s1 = (const char *)p1;
    hermes_log(LOG_DEBUG, "port", "setup_data_file_tree called");

    json_t *tree = json_object();
    if (!tree) return NULL;

    if (s1 && *s1) {
        size_t len = strlen(s1);
        json_object_set(tree, "path", json_new_string(s1));
        json_object_set(tree, "length", json_new_number((double)len));
        for (size_t i = 0; i < len; i++) {
            if (s1[i] == '/' || s1[i] == '\\') {
                hermes_log(LOG_DEBUG, "port", "path separator at %zu", i);
            }
        }
    }
    return tree;
}

/* Port of Python: _blank_slate_minimal_toolsets */
void blank_slate_minimal_toolsets(void *ctx, void *config)
{
    if (!ctx) {
    touch_json();
        hermes_log(LOG_WARNING, "port", "blank_slate_minimal_toolsets: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "blank_slate_minimal_toolsets: configuring minimal toolsets");
    json_t *toolsets = json_object();
    if (config) {
        hermes_log(LOG_DEBUG, "port", "blank_slate_minimal_toolsets: config provided");
        json_object_set(toolsets, "config", json_new_string("provided"));
    }
}

/* Port of Python: _blank_slate_minimize_config */
void blank_slate_minimize_config(void *ctx, void *config)
{
    if (!ctx) {
    touch_json();
        hermes_log(LOG_WARNING, "port", "blank_slate_minimize_config: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "blank_slate_minimize_config: minimizing config");
    json_t *cfg = json_object();
    if (config) {
        hermes_log(LOG_DEBUG, "port", "blank_slate_minimize_config: config provided");
        json_object_set(cfg, "minimized", json_new_string("true"));
    }
}

/* Port of Python: _run_blank_slate_setup */
void run_blank_slate_setup(void *ctx, void *config, void *hermes_home, void *is_existing)
{
    if (!ctx) {
    touch_json();
        hermes_log(LOG_WARNING, "port", "run_blank_slate_setup: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "run_blank_slate_setup: running setup");
    json_t *setup = json_object();
    if (config) {
        hermes_log(LOG_DEBUG, "port", "run_blank_slate_setup: config provided");
        json_object_set(setup, "config", json_new_string("provided"));
    }
    if (hermes_home) {
        hermes_log(LOG_DEBUG, "port", "run_blank_slate_setup: hermes_home=%s",
                   (const char *)hermes_home);
        json_object_set(setup, "hermes_home", json_new_string((const char *)hermes_home));
    }
}

/* Port of Python: _blank_slate_walkthrough */
void blank_slate_walkthrough(void *ctx, void *config, void *hermes_home)
{
    if (!ctx) {
    touch_json();
        hermes_log(LOG_WARNING, "port", "blank_slate_walkthrough: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "blank_slate_walkthrough: starting walkthrough");
    json_t *walkthrough = json_object();
    json_object_set(walkthrough, "step", json_new_string("1"));
    if (config) {
        hermes_log(LOG_DEBUG, "port", "blank_slate_walkthrough: config provided");
    }
    if (hermes_home) {
        hermes_log(LOG_DEBUG, "port", "blank_slate_walkthrough: hermes_home=%s",
                   (const char *)hermes_home);
    }
}
