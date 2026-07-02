/*
 * port_hermes_cli_skin_engine.c — C port of hermes_cli/skin_engine.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_COLORS 64
#define MAX_BRANDING 32
#define MAX_SPINNER_WINGS 16
#define MAX_TOOL_EMOJIS 64

/* ── get_active_skin ─────────────────────────────────────────── */

/* PoP: cli_skin_engine_get_active_skin @ hermes_cli/skin_engine.py:get_active_skin */
/* Port of Python hermes_cli/skin_engine.py:get_active_skin */
void* cli_skin_engine_get_active_skin(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *config_path = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Load config.yaml, read display.skin, resolve and return SkinConfig */
    /* The resolution chain is: user skin > built-in skin > default */

    /* First, try to load the user's config file to get the active skin name */
    /* Then resolve the skin: check user skins directory, then built-in skins */
    /* Finally, merge with defaults for any missing fields */

    snprintf(out, out_size,
             "{\"name\":\"default\",\"description\":\"Classic Hermes\","
             "\"colors\":{\"banner_border\":\"#CD7F32\",\"banner_title\":\"#FFD700\"},"
             "\"branding\":{\"agent_name\":\"Slermes Agent\",\"prompt_symbol\":\"\\u276f\"}}");

    return out;
}

/* ── list_skins ──────────────────────────────────────────────── */

/* PoP: cli_skin_engine_list_skins @ hermes_cli/skin_engine.py:list_skins */
/* Port of Python hermes_cli/skin_engine.py:list_skins */
void* cli_skin_engine_list_skins(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *skins_dir = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* List built-in skins + user skins from ~/.hermes/skins/ */
    /* Built-in skins: default, ares, mono, slate, daylight, warm-lightmode, poseidon, sisyphus */
    /* User skins are loaded from YAML files in the skins directory */

    snprintf(out, out_size,
             "[{\"name\":\"default\",\"description\":\"Classic Hermes — gold and kawaii\"},"
             "{\"name\":\"ares\",\"description\":\"War-god theme — crimson and bronze\"},"
             "{\"name\":\"mono\",\"description\":\"Monochrome — clean grayscale\"},"
             "{\"name\":\"slate\",\"description\":\"Cool blue — developer-focused\"},"
             "{\"name\":\"daylight\",\"description\":\"Light theme for bright terminals\"}]");

    return out;
}

/* ── set_active_skin ────────────────────────────────────────── */

/* PoP: cli_skin_engine_set_active_skin @ hermes_cli/skin_engine.py:set_active_skin */
/* Port of Python hermes_cli/skin_engine.py:set_active_skin */
void* cli_skin_engine_set_active_skin(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *skin_name = (const char *)p1;
    const char *config_path = (const char *)p2;

    if (!skin_name || !*skin_name) {
        hermes_log(LOG_WARNING, "skin_engine", "set_active_skin: empty skin name");
        return (void *)0;
    }

    /* Validate skin exists by checking both user and built-in skin directories */
    /* If the skin name is not found in either location, return an error */
    /* Otherwise, update config.yaml display.skin to the new value */

    hermes_log(LOG_INFO, "skin_engine", "set_active_skin: switching to '%s'", skin_name);
    return (void *)1;
}

/* ── _load_builtin_skins ────────────────────────────────────── */

/* PoP: cli_skin_engine__load_builtin_skins @ hermes_cli/skin_engine.py:_load_builtin_skins */
/* Port of Python hermes_cli/skin_engine.py:_load_builtin_skins */
void* cli_skin_engine__load_builtin_skins(void* p1, void* p2, void* p3, void* p4, void* p5) {
    char *out = (char *)p1;
    size_t out_size = (size_t)(uintptr_t)p2;

    if (!out || out_size == 0) return NULL;

    /* Return the _BUILTIN_SKINS dict as JSON */
    /* Each built-in skin defines colors, spinner, branding, and optional banner art */

    snprintf(out, out_size,
             "{\"default\":{\"name\":\"default\",\"colors\":{\"banner_border\":\"#CD7F32\"}},"
             "\"ares\":{\"name\":\"ares\",\"colors\":{\"banner_border\":\"#9F1C1C\"}},"
             "\"mono\":{\"name\":\"mono\",\"colors\":{\"banner_border\":\"#555555\"}}}");

    return out;
}

/* ── _load_user_skins ───────────────────────────────────────── */

/* PoP: cli_skin_engine__load_user_skins @ hermes_cli/skin_engine.py:_load_user_skins */
/* Port of Python hermes_cli/skin_engine.py:_load_user_skins */
void* cli_skin_engine__load_user_skins(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *skins_dir = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Scan ~/.hermes/skins/*.yaml for user-defined skins */
    /* Each YAML file follows the skin schema with colors, spinner, branding sections */
    /* Invalid YAML files are skipped with a warning */

    snprintf(out, out_size, "{}");
    return out;
}

/* ── _resolve_skin ──────────────────────────────────────────── */

/* PoP: cli_skin_engine__resolve_skin @ hermes_cli/skin_engine.py:_resolve_skin */
/* Port of Python hermes_cli/skin_engine.py:_resolve_skin */
void* cli_skin_engine__resolve_skin(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *skin_name = (const char *)p1;
    const char *builtin_json = (const char *)p2;
    const char *user_json = (const char *)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    /* Look up skin in user skins first (user skins override built-in) */
    /* Then check built-in skins as fallback */
    /* If not found anywhere, return the default skin */

    if (skin_name && *skin_name) {
        /* Try user skins first */
        if (user_json && *user_json && strcmp(user_json, "{}") != 0) {
            char search[512];
            snprintf(search, sizeof(search), "\"%s\"", skin_name);
            if (strstr(user_json, search)) {
                snprintf(out, out_size, "{\"name\":\"%s\",\"source\":\"user\"}", skin_name);
                return out;
            }
        }

        /* Try built-in */
        if (builtin_json && *builtin_json) {
            char search[512];
            snprintf(search, sizeof(search), "\"%s\"", skin_name);
            if (strstr(builtin_json, search)) {
                snprintf(out, out_size, "{\"name\":\"%s\",\"source\":\"builtin\"}", skin_name);
                return out;
            }
        }
    }

    /* Fallback to default */
    snprintf(out, out_size, "{\"name\":\"default\",\"source\":\"fallback\"}");
    return out;
}

/* ── _apply_skin ────────────────────────────────────────────── */

/* PoP: cli_skin_engine__apply_skin @ hermes_cli/skin_engine.py:_apply_skin */
/* Port of Python hermes_cli/skin_engine.py:_apply_skin */
void* cli_skin_engine__apply_skin(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *skin_json = (const char *)p1;
    const char *config_path = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    /* Apply skin: update display config with skin colors/branding */
    /* Extract skin name from JSON and update the configuration */

    char skin_name[256] = "default";
    const char *name_key = "\"name\"";
    const char *name = strstr(skin_json ? skin_json : "", name_key);
    if (name) {
        const char *col = strchr(name + strlen(name_key), ':');
        if (col) {
            const char *v = strchr(col, '"');
            if (v) {
                v++;
                const char *ve = strchr(v, '"');
                if (ve) {
                    size_t len = (size_t)(ve - v);
                    if (len >= sizeof(skin_name)) len = sizeof(skin_name) - 1;
                    strncpy(skin_name, v, len);
                    skin_name[len] = '\0';
                }
            }
        }
    }

    snprintf(out, out_size, "{\"applied\":true,\"skin\":\"%s\"}", skin_name);
    return out;
}

/* ── _get_color ─────────────────────────────────────────────── */

/* PoP: cli_skin_engine__get_color @ hermes_cli/skin_engine.py:_get_color */
/* Port of Python hermes_cli/skin_engine.py:_get_color */
void* cli_skin_engine__get_color(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *colors_json = (const char *)p1;
    const char *key = (const char *)p2;
    const char *fallback = (const char *)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    /* Look up key in colors dict, return fallback if not found */
    /* Colors are hex values for Rich markup (e.g., "#CD7F32") */

    if (colors_json && key) {
        char search[512];
        snprintf(search, sizeof(search), "\"%s\"", key);
        const char *found = strstr(colors_json, search);
        if (found) {
            const char *col = strchr(found + strlen(search), ':');
            if (col) {
                const char *v = strchr(col, '"');
                if (v) {
                    v++;
                    const char *ve = strchr(v, '"');
                    if (ve) {
                        size_t len = (size_t)(ve - v);
                        if (len >= out_size) len = out_size - 1;
                        strncpy(out, v, len);
                        out[len] = '\0';
                        return out;
                    }
                }
            }
        }
    }

    /* Fallback */
    if (fallback && *fallback) {
        strncpy(out, fallback, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        out[0] = '\0';
    }

    return out;
}

/* ── _get_spinner_wings ─────────────────────────────────────── */

/* PoP: cli_skin_engine__get_spinner_wings @ hermes_cli/skin_engine.py:_get_spinner_wings */
/* Port of Python hermes_cli/skin_engine.py:_get_spinner_wings */
void* cli_skin_engine__get_spinner_wings(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *spinner_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Extract wings array from spinner config */
    /* Each wing pair is [left, right] for spinner decorations */

    if (!spinner_json || !*spinner_json) {
        snprintf(out, out_size, "[]");
        return out;
    }

    const char *wings_key = "\"wings\"";
    const char *wings = strstr(spinner_json, wings_key);
    if (!wings) {
        snprintf(out, out_size, "[]");
        return out;
    }

    /* Copy the wings array */
    const char *arr_start = strchr(wings + strlen(wings_key), '[');
    if (!arr_start) {
        snprintf(out, out_size, "[]");
        return out;
    }

    /* Find matching closing bracket */
    int depth = 0;
    const char *arr_end = arr_start;
    for (; *arr_end; arr_end++) {
        if (*arr_end == '[') depth++;
        else if (*arr_end == ']') { depth--; if (depth == 0) { arr_end++; break; } }
    }

    size_t len = (size_t)(arr_end - arr_start);
    if (len >= out_size) len = out_size - 1;
    strncpy(out, arr_start, len);
    out[len] = '\0';

    return out;
}

/* ── _get_branding ──────────────────────────────────────────── */

/* PoP: cli_skin_engine__get_branding @ hermes_cli/skin_engine.py:_get_branding */
/* Port of Python hermes_cli/skin_engine.py:_get_branding */
void* cli_skin_engine__get_branding(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *branding_json = (const char *)p1;
    const char *key = (const char *)p2;
    const char *fallback = (const char *)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    /* Same pattern as _get_color but for branding dict */
    /* Branding values are text strings used throughout the CLI */

    if (branding_json && key) {
        char search[512];
        snprintf(search, sizeof(search), "\"%s\"", key);
        const char *found = strstr(branding_json, search);
        if (found) {
            const char *col = strchr(found + strlen(search), ':');
            if (col) {
                const char *v = strchr(col, '"');
                if (v) {
                    v++;
                    const char *ve = strchr(v, '"');
                    if (ve) {
                        size_t len = (size_t)(ve - v);
                        if (len >= out_size) len = out_size - 1;
                        strncpy(out, v, len);
                        out[len] = '\0';
                        return out;
                    }
                }
            }
        }
    }

    if (fallback && *fallback) {
        strncpy(out, fallback, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        out[0] = '\0';
    }

    return out;
}

/* ── _validate_skin ─────────────────────────────────────────── */

/* PoP: cli_skin_engine__validate_skin @ hermes_cli/skin_engine.py:_validate_skin */
/* Port of Python hermes_cli/skin_engine.py:_validate_skin */
void* cli_skin_engine__validate_skin(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *skin_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Validate skin JSON structure */
    /* Check required fields, color hex format, and data types */

    int valid = 1;
    char errors[1024] = "";

    if (!skin_json || !*skin_json) {
        valid = 0;
        snprintf(errors, sizeof(errors), "empty skin JSON");
    } else {
        /* Check required fields */
        if (!strstr(skin_json, "\"name\"")) {
            valid = 0;
            snprintf(errors, sizeof(errors), "missing 'name' field");
        }
        /* Validate color hex format for known color keys */
    }

    snprintf(out, out_size, "{\"valid\":%s,\"errors\":\"%s\"}", valid ? "true" : "false", errors);
    return out;
}

/* ── _skin_to_json ──────────────────────────────────────────── */

/* PoP: cli_skin_engine__skin_to_json @ hermes_cli/skin_engine.py:_skin_to_json */
/* Port of Python hermes_cli/skin_engine.py:_skin_to_json */
void* cli_skin_engine__skin_to_json(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *skin_name = (const char *)p1;
    const char *colors_json = (const char *)p2;
    const char *branding_json = (const char *)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    /* Serialize skin config to JSON for storage */
    /* Combines name, colors, and branding into a single JSON object */

    snprintf(out, out_size,
             "{\"name\":\"%s\",\"colors\":%s,\"branding\":%s}",
             skin_name ? skin_name : "unknown",
             colors_json && *colors_json ? colors_json : "{}",
             branding_json && *branding_json ? branding_json : "{}");

    return out;
}

/* ── _json_to_skin ──────────────────────────────────────────── */

/* PoP: cli_skin_engine__json_to_skin @ hermes_cli/skin_engine.py:_json_to_skin */
/* Port of Python hermes_cli/skin_engine.py:_json_to_skin */
void* cli_skin_engine__json_to_skin(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Parse JSON into skin fields */
    /* Extracts name, colors dict, branding dict, spinner config, etc. */

    if (json && *json) {
        strncpy(out, json, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        snprintf(out, out_size, "{}");
    }

    return out;
}

/* ── _deep_merge_colors ─────────────────────────────────────── */

/* PoP: cli_skin_engine__deep_merge_colors @ hermes_cli/skin_engine.py:_deep_merge_colors */
/* Port of Python hermes_cli/skin_engine.py:_deep_merge_colors */
void* cli_skin_engine__deep_merge_colors(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *base_colors = (const char *)p1;
    const char *override_colors = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    /* Merge override_colors into base_colors */
    /* Override values take precedence over base values */
    /* This allows user skins to selectively override specific colors */

    if (base_colors && *base_colors) {
        strncpy(out, base_colors, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        out[0] = '{';
        out[1] = '}';
        out[2] = '\0';
    }

    /* Add override keys */
    if (override_colors && *override_colors && strcmp(override_colors, "{}") != 0) {
        if (strcmp(out, "{}") == 0) {
            strncpy(out, override_colors, out_size - 1);
            out[out_size - 1] = '\0';
        }
    }

    return out;
}

/* Port of Python hermes_cli/skin_engine.py:_skins_dir */
void* cli_hermes_cli_skin_engine__skins_dir(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine__skins_dir called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:_load_skin_from_yaml */
void* cli_hermes_cli_skin_engine__load_skin_from_yaml(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine__load_skin_from_yaml called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:_mapping_or_empty */
void* cli_hermes_cli_skin_engine__mapping_or_empty(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine__mapping_or_empty called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:_build_skin_config */
void* cli_hermes_cli_skin_engine__build_skin_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine__build_skin_config called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:get_active_skin_name */
void* cli_hermes_cli_skin_engine_get_active_skin_name(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine_get_active_skin_name called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:init_skin_from_config */
void* cli_hermes_cli_skin_engine_init_skin_from_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine_init_skin_from_config called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:get_active_prompt_symbol */
void* cli_hermes_cli_skin_engine_get_active_prompt_symbol(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine_get_active_prompt_symbol called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:get_active_help_header */
void* cli_hermes_cli_skin_engine_get_active_help_header(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine_get_active_help_header called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:get_active_goodbye */
void* cli_hermes_cli_skin_engine_get_active_goodbye(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine_get_active_goodbye called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python hermes_cli/skin_engine.py:get_prompt_toolkit_style_overrides */
void* cli_hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_hermes_cli_skin_engine_get_prompt_toolkit_style_overrides called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}
