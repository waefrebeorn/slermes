/*
 * models_dev_info.c - model capability / format / cache helpers.
 *
 * Split from models_dev.c (soul.md: self-contained modules). Capability
 * predicates, cost/capability formatting and the on-disk cache helpers.
 * No file-scope state.
 */
#include "models_dev_info.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* PoP: get_models_dev_cache_path @ agent/models_dev.py:_get_cache_path */ char *get_models_dev_cache_path(void) {
    const char *home = getenv("XDG_CONFIG_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) return NULL;
    size_t len = strlen(home) + 64;
    char *path = (char *)malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/.hermes/%s", home, MODELS_DEV_CACHE_FILE);
    return path;
}


static double disk_cache_age_seconds(void) {
    char *path = get_models_dev_cache_path();
    if (!path) return -1.0;

    struct stat st;
    if (stat(path, &st) != 0) { free(path); return -1.0; }
    free(path);

    time_t now = time(NULL);
    double age = difftime(now, st.st_mtime);
    if (age < 0) return -1.0; /* clock skew — treat as unknown */
    return age;
}
 json_t *models_dev_load_disk_cache(void) {
    char *path = get_models_dev_cache_path();
    if (!path) return NULL;

    /* Check disk cache freshness using the age function */
    double age = disk_cache_age_seconds();
    if (age < 0 || age > MODELS_DEV_CACHE_TTL * 2) {
        free(path);
        return NULL;
    }

    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[nread] = '\0';

    char *err = NULL;
    json_t *root = json_parse(buf, &err);
    free(buf);
    free(err);
    return root;
}


/* Port of Python agent/models_dev.py:ModelCapabilities.format_cost().
 * Build human-readable cost string. Returns malloc'd string, caller must free(). */
/* Port of Python agent/models_dev.py:format_cost() */ char *models_dev_format_cost(json_t *entry) {
    if (!models_dev_has_cost_data(entry))
        return strdup("unknown");

    json_t *cost = json_obj_get(entry, "cost");
    if (!cost || cost->type != JSON_OBJECT) return strdup("unknown");

    double cost_in = 0, cost_out = 0, cost_cache = -1;
    json_t *v = json_obj_get(cost, "input");
    if (v && v->type == JSON_NUMBER) cost_in = v->num_val;
    v = json_obj_get(cost, "output");
    if (v && v->type == JSON_NUMBER) cost_out = v->num_val;
    v = json_obj_get(cost, "cache_read");
    if (v && v->type == JSON_NUMBER) cost_cache = v->num_val;

    char buf[256];
    int n = snprintf(buf, sizeof(buf), "$%.2f/M in, $%.2f/M out", cost_in, cost_out);
    if (cost_cache >= 0 && n < (int)sizeof(buf) - 32)
        snprintf(buf + n, sizeof(buf) - (size_t)n, ", cache read $%.2f/M", cost_cache);
    return strdup(buf);
}


/* Port of Python agent/models_dev.py:ModelCapabilities.format_capabilities().
 * Build human-readable capabilities string. Returns malloc'd string, caller must free(). */
/* Port of Python agent/models_dev.py:format_capabilities() */ char *models_dev_format_capabilities(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return strdup("basic");

    bool has_reasoning = json_get_bool(entry, "reasoning", false);
    bool has_tools = json_get_bool(entry, "tool_call", false);
    bool has_vision = models_dev_supports_vision(entry);
    bool has_pdf = models_dev_supports_pdf(entry);
    bool has_audio = models_dev_supports_audio_input(entry);
    bool has_structured = json_get_bool(entry, "structured_output", false);
    bool has_open = json_get_bool(entry, "open_weights", false);

    char buf[256];
    size_t pos = 0;
    buf[0] = '\0';
    if (has_reasoning) pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%sreasoning", pos > 0 ? ", " : "");
    if (has_tools)     pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%stools", pos > 0 ? ", " : "");
    if (has_vision)    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%svision", pos > 0 ? ", " : "");
    if (has_pdf)       pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%sPDF", pos > 0 ? ", " : "");
    if (has_audio)     pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%saudio", pos > 0 ? ", " : "");
    if (has_structured) pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%sstructured output", pos > 0 ? ", " : "");
    if (has_open)      pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%sopen weights", pos > 0 ? ", " : "");

    if (pos == 0) return strdup("basic");
    return strdup(buf);
}


bool models_dev_has_cost_data(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    json_t *cost = json_obj_get(entry, "cost");
    if (!cost || cost->type != JSON_OBJECT) return false;
    json_t *inp = json_obj_get(cost, "input");
    if (inp && inp->type == JSON_NUMBER && inp->num_val > 0) return true;
    json_t *out = json_obj_get(cost, "output");
    if (out && out->type == JSON_NUMBER && out->num_val > 0) return true;
    return false;
}


bool models_dev_supports_vision(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    if (json_get_bool(entry, "attachment", false)) return true;
    json_t *mods = json_obj_get(entry, "modalities");
    if (mods && mods->type == JSON_OBJECT) {
        json_t *input_mods = json_obj_get(mods, "input");
        if (input_mods && input_mods->type == JSON_ARRAY) {
            for (size_t i = 0; i < input_mods->c.count; i++) {
                json_t *item = input_mods->c.items[i];
                if (item && item->type == JSON_STRING && strcmp(item->str_val, "image") == 0)
                    return true;
            }
        }
    }
    return false;
}


bool models_dev_supports_pdf(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    json_t *mods = json_obj_get(entry, "modalities");
    if (mods && mods->type == JSON_OBJECT) {
        json_t *input_mods = json_obj_get(mods, "input");
        if (input_mods && input_mods->type == JSON_ARRAY) {
            for (size_t i = 0; i < input_mods->c.count; i++) {
                json_t *item = input_mods->c.items[i];
                if (item && item->type == JSON_STRING && strcmp(item->str_val, "pdf") == 0)
                    return true;
            }
        }
    }
    return false;
}


/* PoP: models_dev_supports_audio_input @ agent/models_dev.py:supports_audio_input */
bool models_dev_supports_audio_input(json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    json_t *mods = json_obj_get(entry, "modalities");
    if (mods && mods->type == JSON_OBJECT) {
        json_t *input_mods = json_obj_get(mods, "input");
        if (input_mods && input_mods->type == JSON_ARRAY) {
            for (size_t i = 0; i < input_mods->c.count; i++) {
                json_t *item = input_mods->c.items[i];
                if (item && item->type == JSON_STRING && strcmp(item->str_val, "audio") == 0)
                    return true;
            }
        }
    }
    return false;
}
