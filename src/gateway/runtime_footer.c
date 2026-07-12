/*
 * runtime_footer.c — Gateway runtime-metadata footer.
 *
 * Port of Python gateway/runtime_footer.py.
 *
 * Renders a compact footer showing runtime state (model, context %, cwd) and
 * appends it to the FINAL message of an agent turn when enabled.  Off by default.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOOTER_SEP " · "

/* ================================================================
 *  Resolve effective runtime-footer config for a platform
 *  Port of Python resolve_footer_config()
 *  AG26: Port of Python gateway/runtime_footer.py:resolve_footer_config().
 * ================================================================ */

json_node_t *resolve_footer_config(json_node_t *user_config,
                                    const char *platform_key) {
    json_node_t *resolved = json_new_object();
    if (!resolved) return NULL;

    /* Built-in defaults */
    json_object_set(resolved, "enabled", json_new_bool(false));
    json_node_t *default_fields = json_new_array();
    json_array_append(default_fields, json_new_string("model"));
    json_array_append(default_fields, json_new_string("context_pct"));
    json_array_append(default_fields, json_new_string("cwd"));
    json_object_set(resolved, "fields", default_fields);

    if (!user_config) return resolved;

    json_node_t *display = json_object_get(user_config, "display");
    if (!display || display->type != JSON_OBJECT) return resolved;

    /* Global display.runtime_footer config */
    json_node_t *global_cfg = json_object_get(display, "runtime_footer");
    if (global_cfg && global_cfg->type == JSON_OBJECT) {
        json_node_t *en = json_object_get(global_cfg, "enabled");
        if (en && en->type == JSON_BOOL) {
            json_object_set(resolved, "enabled", json_new_bool(en->bool_val));
        }
        json_node_t *fields = json_object_get(global_cfg, "fields");
        if (fields && fields->type == JSON_ARRAY && json_array_count(fields) > 0) {
            json_object_set(resolved, "fields", json_copy(fields));
        }
    }

    /* Per-platform override */
    if (platform_key && *platform_key) {
        json_node_t *platforms = json_object_get(display, "platforms");
        if (platforms && platforms->type == JSON_OBJECT) {
            json_node_t *plat_cfg = json_object_get(platforms, platform_key);
            if (plat_cfg && plat_cfg->type == JSON_OBJECT) {
                json_node_t *plat_footer = json_object_get(plat_cfg, "runtime_footer");
                if (plat_footer && plat_footer->type == JSON_OBJECT) {
                    json_node_t *en = json_object_get(plat_footer, "enabled");
                    if (en && en->type == JSON_BOOL) {
                        json_object_set(resolved, "enabled", json_new_bool(en->bool_val));
                    }
                    json_node_t *fields = json_object_get(plat_footer, "fields");
                    if (fields && fields->type == JSON_ARRAY && json_array_count(fields) > 0) {
                        json_object_set(resolved, "fields", json_copy(fields));
                    }
                }
            }
        }
    }

    return resolved;
}

/* ================================================================
 *  Render the footer line
 *  Port of Python format_runtime_footer()
 *  AG26: Port of Python gateway/runtime_footer.py:format_runtime_footer().
 *
 *  Returns malloc'd string (caller must free). Empty string if no
 *  fields have data or footer is disabled.
 * ================================================================ */

char *format_runtime_footer(const char *model,
                             int context_tokens,
                             int context_length,
                             const char *cwd,
                             json_node_t *fields) {
    char buf[1024] = {0};
    size_t pos = 0;

    /* Determine which fields to render. Default if none specified. */
    bool use_defaults = (fields == NULL);

    /* Model field */
    bool show_model = use_defaults;
    bool show_context = use_defaults;
    bool show_cwd = use_defaults;

    if (!use_defaults && fields->type == JSON_ARRAY) {
        json_t *arr = (json_t *)fields;
        for (size_t i = 0; i < arr->c.count; i++) {
            json_node_t *f = arr->c.items[i];
            if (f && f->type == JSON_STRING) {
                if (strcmp(f->str_val, "model") == 0) show_model = true;
                else if (strcmp(f->str_val, "context_pct") == 0) show_context = true;
                else if (strcmp(f->str_val, "cwd") == 0) show_cwd = true;
            }
        }
    }

    /* Build model part */
    if (show_model && model && *model) {
        const char *short_model = model_short(model);
        if (short_model && *short_model) {
            if (pos > 0) { buf[pos++] = ' '; buf[pos++] = 0xC2; buf[pos++] = 0xB7; buf[pos++] = ' '; } /* · */
            size_t ml = strlen(short_model);
            if (pos + ml < sizeof(buf)) {
                memcpy(buf + pos, short_model, ml);
                pos += ml;
            }
        }
    }

    /* Build context % part */
    if (show_context && context_length > 0 && context_tokens >= 0) {
        int pct = context_tokens * 100 / context_length;
        if (pct > 100) pct = 100;
        char pct_buf[16];
        snprintf(pct_buf, sizeof(pct_buf), "%d%%", pct);
        if (pos > 0) { buf[pos++] = ' '; buf[pos++] = 0xC2; buf[pos++] = 0xB7; buf[pos++] = ' '; }
        size_t pl = strlen(pct_buf);
        if (pos + pl < sizeof(buf)) {
            memcpy(buf + pos, pct_buf, pl);
            pos += pl;
        }
    }

    /* Build cwd part */
    if (show_cwd) {
        const char *rel_cwd = NULL;
        char *rel_tmp = NULL;
        if (cwd && *cwd) {
            rel_tmp = home_relative_cwd(cwd);
            rel_cwd = rel_tmp;
        }
        if (rel_cwd && *rel_cwd) {
            if (pos > 0) { buf[pos++] = ' '; buf[pos++] = 0xC2; buf[pos++] = 0xB7; buf[pos++] = ' '; }
            size_t cl = strlen(rel_cwd);
            if (pos + cl < sizeof(buf)) {
                memcpy(buf + pos, rel_cwd, cl);
                pos += cl;
            }
        }
        free(rel_tmp);
    }

    buf[pos] = '\0';
    return strdup(buf);
}

/* ================================================================
 *  Top-level entry point used by gateway/run.py
 *  Port of Python build_footer_line()
 *  AG26: Port of Python gateway/runtime_footer.py:build_footer_line().
 *
 *  Returns the footer text (empty string when disabled or no data).
 *  Caller must free.
 * ================================================================ */

char *build_footer_line(json_node_t *user_config,
                         const char *platform_key,
                         const char *model,
                         int context_tokens,
                         int context_length,
                         const char *cwd) {
    json_node_t *cfg = resolve_footer_config(user_config, platform_key);
    if (!cfg) return strdup("");

    json_node_t *enabled = json_object_get(cfg, "enabled");
    bool is_enabled = enabled && enabled->type == JSON_BOOL && enabled->bool_val;

    if (!is_enabled) {
        json_free(cfg);
        return strdup("");
    }

    json_node_t *fields = json_object_get(cfg, "fields");
    char *result = format_runtime_footer(model, context_tokens, context_length, cwd, fields);

    json_free(cfg);
    return result;
}
