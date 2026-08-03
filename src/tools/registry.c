/*
 * registry.c — Tool registry for Hermes C.
 * Tools register themselves at init time.
 * Agent loop calls registry to find + dispatch tools.
 *
 * Thread safety: all operations on g_registry are serialized via
 * g_registry_mutex. Functions that return a tool_t* pointer return
 * a reference into the array; callers must not hold the pointer
 * across a realloc (registration). The main dispatch path
 * (registry_dispatch) holds the mutex across the handler call
 * for full safety.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_tool_result.h"  /* tool_error(), tool_result_obj() for standardized JSON */
#include "registry.h"
#include <ctype.h>
#include <pthread.h>

/* Weak symbol — tool_error_sanitize may not be linked in test binaries */
__attribute__((weak)) char *tool_error_sanitize(const char *error_msg);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* ================================================================
 *  Type coercion for tool call arguments (TD01 parity)
 * ================================================================ */

/* Coerce string tool call arguments to match their JSON Schema types.
 * LLMs frequently return numbers as strings ("42" instead of 42)
 * and booleans as strings ("true" instead of true).
 *
 * Parses args_json, coerces values in place, re-serializes.
 * Returns allocated string on success, or NULL if args_json is not a JSON object.
 * On failure/parse error, returns a copy of original args_json. */
static char *coerce_args_json(const char *name, const char *args_json,
                               const char *schema_json) {
    if (!args_json || !schema_json) return NULL;

    /* Parse args as JSON */
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args || args->type != JSON_OBJECT) {
        free(err);
        json_free(args);
        return NULL;
    }
    free(err);

    /* Parse schema to get property types */
    json_node_t *schema = json_parse(schema_json, &err);
    if (!schema || schema->type != JSON_OBJECT) {
        free(err);
        json_free(schema);
        json_free(args);
        return NULL;
    }
    free(err);

    json_node_t *params = json_object_get(schema, "parameters");
    json_node_t *properties = params ? json_object_get(params, "properties") : NULL;

    if (properties && properties->type == JSON_OBJECT) {
        /* Iterate over schema properties */
        size_t prop_count = json_len(properties);
        for (size_t pi = 0; pi < prop_count; pi++) {
            const char *key = properties->c.keys ? properties->c.keys[pi] : NULL;
            if (!key) continue;

            json_node_t *prop = json_get(properties, pi);
            if (!prop || prop->type != JSON_OBJECT) continue;

            const char *type_str = json_get_str(prop, "type", "");

            /* Get the current arg value */
            json_node_t *arg_val = json_object_get(args, key);
            if (!arg_val) continue;

            /* Skip if value is already the correct type (not a string) */
            if (arg_val->type != JSON_STRING) continue;

            const char *str_val = json_get_str(args, key, "");

            /* Handle union types: ["integer", "number", "string", "boolean", "array"] */
            const char *types_to_try[16];
            int num_types = 0;

            if (type_str[0] == '[') {
                /* Union type - extract from JSON array string */
                char copy[256];
                snprintf(copy, sizeof(copy), "%s", type_str);
                char *t = copy;
                while (*t && num_types < 16) {
                    /* Skip brackets, quotes, spaces */
                    if (*t == '[' || *t == ']' || *t == '"' || *t == ' ' || *t == '\'') {
                        t++;
                        continue;
                    }
                    const char *tok_start = t;
                    while (*t && *t != ',' && *t != '"' && *t != ']' && *t != ' ') t++;
                    if (t > tok_start) {
                        size_t tlen = (size_t)(t - tok_start);
                        char *tok = malloc(tlen + 1);
                        memcpy(tok, tok_start, tlen);
                        tok[tlen] = '\0';
                        types_to_try[num_types++] = tok;
                    }
                    if (*t == ',') t++;
                }
            } else if (type_str[0]) {
                types_to_try[num_types++] = strdup(type_str);
            }

            bool coerced = false;

            for (int ti = 0; ti < num_types && !coerced; ti++) {
                const char *expected = types_to_try[ti];
                if (strcmp(expected, "integer") == 0) {
                    /* Try to parse as integer */
                    char *end = NULL;
                    long val = strtol(str_val, &end, 10);
                    if (end && *end == '\0' && end != str_val) {
                        json_object_set(args, key, json_new_number((double)val));
                        coerced = true;
                    }
                } else if (strcmp(expected, "number") == 0) {
                    char *end = NULL;
                    double val = strtod(str_val, &end);
                    if (end && *end == '\0' && end != str_val) {
                        json_object_set(args, key, json_new_number(val));
                        coerced = true;
                    }
                } else if (strcmp(expected, "boolean") == 0) {
                    if (strcasecmp(str_val, "true") == 0 || strcmp(str_val, "1") == 0) {
                        json_object_set(args, key, json_new_bool(true));
                        coerced = true;
                    } else if (strcasecmp(str_val, "false") == 0 || strcmp(str_val, "0") == 0) {
                        json_object_set(args, key, json_new_bool(false));
                        coerced = true;
                    }
                } else if (strcmp(expected, "array") == 0) {
                    /* Try to parse as JSON array first */
                    char *arr_err = NULL;
                    json_node_t *parsed = json_parse(str_val, &arr_err);
                    if (parsed && parsed->type == JSON_ARRAY) {
                        json_object_set(args, key, parsed);
                        json_free(parsed);
                        coerced = true;
                    } else {
                        /* Wrap bare value in array */
                        json_node_t *arr = json_new_array();
                        json_node_t *item = json_new_string(str_val);
                        json_array_append(arr, item);
                        json_object_set(args, key, arr);
                        json_free(arr);
                        coerced = true;
                    }
                    free(arr_err);
                }
            }

            /* Free allocated type strings */
            for (int ti = 0; ti < num_types; ti++) {
                free((void *)types_to_try[ti]);
            }
        }
    }

    /* Serialize back */
    char *result = json_serialize(args);
    json_free(schema);
    json_free(args);
    return result;
}

/* ================================================================
 *  Registry state — thread-safe via g_registry_mutex
 * ================================================================ */

static tool_registry_t g_registry = {NULL, 0, 0};
static pthread_mutex_t g_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_registry_generation = 0;

/* Toolset alias store (Python ToolRegistry._toolset_aliases). Maps an alias
 * string to its canonical toolset name. Bounded; entries are short. */
#define REG_ALIAS_MAX 64
typedef struct { char alias[64]; char toolset[32]; } reg_alias_t;
static reg_alias_t g_aliases[REG_ALIAS_MAX];
static size_t g_aliases_count = 0;

/* Generation counter — bumped on every mutation.
 * Callers that cache tool metadata can compare against this to
 * detect stale cache entries, mirroring Python's ToolRegistry._generation. */
uint64_t registry_generation(void) {
    pthread_mutex_lock(&g_registry_mutex);
    uint64_t gen = g_registry_generation;
    pthread_mutex_unlock(&g_registry_mutex);
    return gen;
}

/* Internal unlocked find — caller must hold g_registry_mutex */
static tool_t *find_unlocked(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0 && g_registry.tools[i].available)
            return &g_registry.tools[i];
    }
    return NULL;
}

/* ================================================================
 *  Registration
 * ================================================================ */

bool registry_register(const char *name, const char *description,
                        const char *schema_json,
                        char *(*handler)(const char *args_json, const char *task_id))
{
    return registry_register_ex(name, description, schema_json, "", handler);
}

static bool registry_register_ex_locked(const char *name, const char *description,
                          const char *schema_json, const char *toolset,
                          char *(*handler)(const char *args_json, const char *task_id),
                          const char *const *requires_env, size_t requires_env_n,
                          int max_result_size_chars)
{
    if (!name || !handler) return false;

    /* Check if already registered — detect cross-toolset shadowing */
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            if (toolset && toolset[0] && g_registry.tools[i].toolset[0] &&
                strcmp(g_registry.tools[i].toolset, toolset) != 0) {
                fprintf(stderr, "[registry] Shadow warning: '%s' from toolset '%s' shadows existing '%s' in '%s'\n",
                        name, toolset, g_registry.tools[i].name, g_registry.tools[i].toolset);
            }
            return false;
        }
    }

    /* Grow if needed */
    if (g_registry.count >= g_registry.capacity) {
        size_t new_cap = g_registry.capacity == 0 ? 32 : g_registry.capacity * 2;
        tool_t *new_tools = (tool_t *)realloc(g_registry.tools,
                                               new_cap * sizeof(tool_t));
        if (!new_tools) return false;
        g_registry.tools = new_tools;
        g_registry.capacity = new_cap;
    }

    tool_t *t = &g_registry.tools[g_registry.count++];
    memset(t, 0, sizeof(*t));
    snprintf(t->name, sizeof(t->name), "%s", name);
    snprintf(t->description, sizeof(t->description), "%s", description ? description : "");
    if (schema_json)
        snprintf(t->schema_json, sizeof(t->schema_json), "%s", schema_json);
    if (toolset)
        snprintf(t->toolset, sizeof(t->toolset), "%s", toolset);
    t->handler = handler;
    t->available = true;

    /* Set default emoji based on toolset */
    if (toolset) {
        if (strcmp(toolset, "terminal") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xF0\x9F\x96\xA5");  /* 🖥️ */
        else if (strcmp(toolset, "file") == 0 || strcmp(toolset, "filesystem") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xF0\x9F\x93\x81");  /* 📁 */
        else if (strcmp(toolset, "web") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xF0\x9F\x8C\x90");  /* 🌐 */
        else if (strcmp(toolset, "code") == 0 || strcmp(toolset, "exec_code") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xE2\x9C\x8F");      /* ✏️ */
        else if (strcmp(toolset, "browser") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xF0\x9F\x94\x8D");  /* 🔍 */
        else if (strcmp(toolset, "memory") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xF0\x9F\xA7\xA0");  /* 🧠 */
        else if (strcmp(toolset, "session") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xF0\x9F\x93\x9D");  /* 📝 */
        else if (strcmp(toolset, "cron") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xE2\x8F\xB0");      /* ⏰ */
        else if (strcmp(toolset, "messaging") == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xF0\x9F\x93\xA8");  /* 📨 */
        else if (strcmp(toolset, "plugin") == 0 || strcmp(toolset, "mcp") == 0 ||
                 strncmp(toolset, "mcp-", 4) == 0)
            snprintf(t->emoji, sizeof(t->emoji), "\xE2\x9A\xA1");      /* ⚡ */
        else
            snprintf(t->emoji, sizeof(t->emoji), "\xE2\x9A\xA1");      /* ⚡ default */
    } else {
        snprintf(t->emoji, sizeof(t->emoji), "\xE2\x9A\xA1");          /* ⚡ default */
    }

    /* requires_env + per-tool max result size. */
    if (requires_env && requires_env_n) {
        size_t n = requires_env_n < 8 ? requires_env_n : 8;
        for (size_t k = 0; k < n; k++) {
            if (requires_env[k] && requires_env[k][0]) {
                snprintf(t->requires_env[t->requires_env_count],
                         sizeof(t->requires_env[t->requires_env_count]),
                         "%s", requires_env[k]);
                t->requires_env_count++;
            }
        }
    }
    t->max_result_size_chars = max_result_size_chars;

    return true;
}

bool registry_register_ex(const char *name, const char *description,
                          const char *schema_json, const char *toolset,
                          char *(*handler)(const char *args_json, const char *task_id))
{
    return registry_register_ex_full(name, description, schema_json, toolset,
                                     handler, NULL, 0, 0);
}

bool registry_register_ex_full(const char *name, const char *description,
                               const char *schema_json, const char *toolset,
                               char *(*handler)(const char *args_json, const char *task_id),
                               const char *const *requires_env, size_t requires_env_n,
                               int max_result_size_chars)
{
    pthread_mutex_lock(&g_registry_mutex);
    bool ok = registry_register_ex_locked(name, description, schema_json, toolset,
                                          handler, requires_env, requires_env_n,
                                          max_result_size_chars);
    if (ok) g_registry_generation++;
    pthread_mutex_unlock(&g_registry_mutex);
    return ok;
}

void registry_set_available(const char *name, bool available) {
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            g_registry.tools[i].available = available;
            g_registry_generation++;
            pthread_mutex_unlock(&g_registry_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
}

/* ================================================================
 *  Lookup + dispatch
 * ================================================================ */

tool_t *registry_find(const char *name) {
    pthread_mutex_lock(&g_registry_mutex);
    tool_t *t = find_unlocked(name);
    pthread_mutex_unlock(&g_registry_mutex);
    return t;
}

/* ================================================================
 *  Tool name repair (port of Python repair_tool_call())
 * ================================================================ */

/* Levenshtein distance between two strings */
static size_t levenshtein_distance(const char *s1, const char *s2) {
    size_t len1 = strlen(s1), len2 = strlen(s2);
    size_t *row = malloc((len2 + 1) * sizeof(size_t));
    if (!row) return SIZE_MAX;
    for (size_t j = 0; j <= len2; j++) row[j] = j;
    for (size_t i = 1; i <= len1; i++) {
        size_t prev = row[0];
        row[0] = i;
        for (size_t j = 1; j <= len2; j++) {
            size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            size_t min = row[j - 1] + 1; /* insert */
            size_t del = row[j] + 1;     /* delete */
            if (del < min) min = del;
            size_t sub = prev + cost;    /* substitute */
            if (sub < min) min = sub;
            prev = row[j];
            row[j] = min;
        }
    }
    size_t dist = row[len2];
    free(row);
    return dist;
}

/* Levenshtein similarity ratio 0.0-1.0 */
static double levenshtein_ratio(const char *s1, const char *s2) {
    if (!s1 || !s2) return 0.0;
    size_t len1 = strlen(s1), len2 = strlen(s2);
    if (len1 == 0 && len2 == 0) return 1.0;
    size_t max_len = len1 > len2 ? len1 : len2;
    size_t dist = levenshtein_distance(s1, s2);
    return 1.0 - (double)dist / (double)max_len;
}

/* Normalize: lowercase + hyphens/spaces -> underscores.
 * Returns malloc'd string (caller frees). */
static char *name_norm(const char *s) {
    if (!s) return NULL;
    char *r = strdup(s);
    if (!r) return NULL;
    for (char *p = r; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
        if (*p == '-' || *p == ' ') *p = '_';
    }
    return r;
}

/* CamelCase -> snake_case: "TodoTool" -> "todo_tool".
 * Returns malloc'd string (caller frees). */
static char *name_camel_to_snake(const char *s) {
    if (!s || !s[0]) return NULL;
    size_t len = strlen(s), out_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && isupper((unsigned char)s[i]) && islower((unsigned char)s[i - 1]))
            out_len++;
        out_len++;
    }
    char *r = malloc(out_len + 1);
    if (!r) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && isupper((unsigned char)s[i]) && islower((unsigned char)s[i - 1]))
            r[pos++] = '_';
        r[pos++] = (char)tolower((unsigned char)s[i]);
    }
    r[pos] = '\0';
    return r;
}

/* Strip _tool/-tool/tool suffix (case-insensitive).
 * Returns malloc'd string (caller frees), or NULL if no suffix found. */
static char *name_strip_tool_suffix(const char *s) {
    if (!s || !s[0]) return NULL;
    size_t len = strlen(s);
    static const char *suffixes[] = {"_tool", "-tool", "tool"};
    for (size_t i = 0; i < 3; i++) {
        size_t slen = strlen(suffixes[i]);
        if (len > slen) {
            const char *tail = s + len - slen;
            bool match = true;
            for (size_t j = 0; j < slen; j++) {
                if (tolower((unsigned char)tail[j]) != (unsigned char)suffixes[i][j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                size_t new_len = len - slen;
                while (new_len > 0 && (s[new_len - 1] == '_' || s[new_len - 1] == '-'))
                    new_len--;
                return strndup(s, new_len);
            }
        }
    }
    return NULL;
}

/* Attempt to repair a mismatched tool name.
 *
 * Port of Python agent/agent_runtime_helpers.py:repair_tool_call().
 * Applies normalization pipeline: lowercase, hyphens->underscores,
 * CamelCase->snake_case, _tool suffix stripping (up to 2x), fuzzy match.
 *
 * Returns malloc'd repaired name (caller frees), or NULL if no match. */
char *repair_tool_call(const char *tool_name) {
    if (!tool_name || !tool_name[0]) return NULL;

    pthread_mutex_lock(&g_registry_mutex);

    /* Fast path: exact match already handled by caller */
    char *lowered = strdup(tool_name);
    if (!lowered) { pthread_mutex_unlock(&g_registry_mutex); return NULL; }
    for (char *p = lowered; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* 1. Lowercase direct match */
    if (find_unlocked(lowered)) { pthread_mutex_unlock(&g_registry_mutex); return lowered; }

    /* 2. Normalized: lowercase + hyphens/spaces -> underscores */
    char *normalized = name_norm(tool_name);
    if (normalized && find_unlocked(normalized)) { free(lowered); pthread_mutex_unlock(&g_registry_mutex); return normalized; }

    /* 3. CamelCase -> snake_case */
    char *camel = name_camel_to_snake(tool_name);
    if (camel && find_unlocked(camel)) { free(lowered); free(normalized); pthread_mutex_unlock(&g_registry_mutex); return camel; }

    /* 4. Build variant set and try suffix stripping (up to 2x) */
    #define MAX_VARIANTS 64
    char *variants[MAX_VARIANTS];
    size_t n_variants = 0;

    #define ADD_VAR(v) do { \
        if ((v) && n_variants < MAX_VARIANTS) { \
            bool _dup = false; \
            for (size_t _i = 0; _i < n_variants; _i++) { \
                if (strcmp(variants[_i], (v)) == 0) { _dup = true; break; } \
            } \
            if (!_dup) variants[n_variants++] = strdup(v); \
        } \
    } while(0)

    /* Seed variants */
    ADD_VAR(tool_name);
    ADD_VAR(lowered);
    ADD_VAR(normalized);
    ADD_VAR(camel);

    /* Apply suffix stripping iteratively */
    for (int round = 0; round < 2; round++) {
        size_t old_count = n_variants;
        for (size_t i = 0; i < old_count; i++) {
            char *stripped = name_strip_tool_suffix(variants[i]);
            if (stripped) {
                ADD_VAR(stripped);
                char *sn = name_norm(stripped);
                ADD_VAR(sn);
                free(sn);
                char *sc = name_camel_to_snake(stripped);
                ADD_VAR(sc);
                free(sc);
            }
            free(stripped);
        }
    }

    /* Check all variants */
    for (size_t i = 0; i < n_variants; i++) {
        if (variants[i][0] && find_unlocked(variants[i])) {
            char *result = strdup(variants[i]);
            for (size_t j = 0; j < n_variants; j++) free(variants[j]);
            free(lowered); free(normalized); free(camel);
            pthread_mutex_unlock(&g_registry_mutex);
            return result;
        }
    }

    /* 5. Fuzzy match (Levenshtein ratio >= 0.7) */
    const char *best_match = NULL;
    double best_ratio = 0.7;
    for (size_t i = 0; i < g_registry.count; i++) {
        double ratio = levenshtein_ratio(lowered, g_registry.tools[i].name);
        if (ratio > best_ratio) {
            best_ratio = ratio;
            best_match = g_registry.tools[i].name;
        }
    }

    for (size_t j = 0; j < n_variants; j++) free(variants[j]);
    free(lowered); free(normalized); free(camel);

    if (best_match) {
        char *r = strdup(best_match);
        pthread_mutex_unlock(&g_registry_mutex);
        return r;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return NULL;

    #undef ADD_VAR
    #undef MAX_VARIANTS
}

char *registry_dispatch(const char *name, const char *args_json,
                        const char *task_id)
{
    if (!name) return tool_error("null tool name", NULL);

    /* Lock, find, dispatch handler under lock for safety */
    pthread_mutex_lock(&g_registry_mutex);
    tool_t *tool = find_unlocked(name);
    if (tool) {
        if (tool->handler) {
            /* Coerce string args to match JSON Schema types (TD01) */
            char *coerced = coerce_args_json(tool->name, args_json, tool->schema_json);
            const char *dispatch_args = coerced ? coerced : args_json;
            char *result = tool->handler(dispatch_args, task_id);
            free(coerced);
            pthread_mutex_unlock(&g_registry_mutex);
            /* Sanitize error results — strip CDATA, role tags, fences from tool
             * error messages before they enter conversation history.
             * Port of Python model_tools._sanitize_tool_error(). */
            if (result && strncmp(result, "{\"error\"", 7) == 0 && tool_error_sanitize) {
                char *sanitized = tool_error_sanitize(result);
                if (sanitized) {
                    free(result);
                    result = sanitized;
                }
            }
            return result;
        }
        pthread_mutex_unlock(&g_registry_mutex);
        return tool_error("tool has no handler", NULL);
    }
    pthread_mutex_unlock(&g_registry_mutex);

    /* Tool not found — try repair (locks internally) */
    char *repaired = repair_tool_call(name);
    if (repaired) {
        pthread_mutex_lock(&g_registry_mutex);
        tool = find_unlocked(repaired);
        free(repaired);
        if (tool) {
            if (tool->handler) {
                /* Coerce string args to match JSON Schema types (TD01) */
                char *coerced = coerce_args_json(tool->name, args_json, tool->schema_json);
                const char *dispatch_args = coerced ? coerced : args_json;
                char *result = tool->handler(dispatch_args, task_id);
                free(coerced);
                pthread_mutex_unlock(&g_registry_mutex);
                /* Sanitize error results on repaired path too */
                if (result && strncmp(result, "{\"error\"", 7) == 0 && tool_error_sanitize) {
                    char *sanitized = tool_error_sanitize(result);
                    if (sanitized) {
                        free(result);
                        result = sanitized;
                    }
                }
                return result;
            }
            pthread_mutex_unlock(&g_registry_mutex);
            return tool_error("tool has no handler", NULL);
        }
        pthread_mutex_unlock(&g_registry_mutex);
    }

    /* Not found after repair either — error */
    char buf[512];
    char *sanitized = tool_error_sanitize ? tool_error_sanitize(name) : NULL;
    snprintf(buf, sizeof(buf), "{\"error\": \"tool '%s' not found\"}",
             sanitized ? sanitized : (name ? name : "(null)"));
    free(sanitized);
    return strdup(buf);
}

/* Port of Python tools/file_state.py:get_registry(). */
/* ================================================================
 *  Get registry for agent loop
 * ================================================================ */

tool_registry_t *get_registry(void) {
    pthread_mutex_lock(&g_registry_mutex);
    tool_registry_t *r = &g_registry;
    pthread_mutex_unlock(&g_registry_mutex);
    return r;
}

size_t registry_count(void) {
    pthread_mutex_lock(&g_registry_mutex);
    size_t c = g_registry.count;
    pthread_mutex_unlock(&g_registry_mutex);
    return c;
}

/* ================================================================
 *  Generate tools JSON for LLM API call
 * ================================================================ */

json_node_t *registry_to_json(void) {
    json_node_t *tools = json_new_array();

    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (!g_registry.tools[i].available) continue;

        json_node_t *tool = json_new_object();
        json_object_set(tool, "type", json_new_string("function"));

        json_node_t *fn = json_new_object();
        json_object_set(fn, "name", json_new_string(g_registry.tools[i].name));
        json_object_set(fn, "description", json_new_string(g_registry.tools[i].description));

        /* Parse schema from JSON string */
        if (g_registry.tools[i].schema_json[0]) {
            char *err = NULL;
            json_node_t *schema = json_parse(g_registry.tools[i].schema_json, &err);
            if (schema) {
                json_object_set(fn, "parameters", schema);
            } else {
                json_object_set(fn, "parameters", json_new_object());
                free(err);
            }
        } else {
            json_object_set(fn, "parameters", json_new_object());
        }

        json_object_set(tool, "function", fn);
        json_array_append(tools, tool);
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return tools;
}

/* Accessors for testing */
size_t registry_get_count(void) {
    pthread_mutex_lock(&g_registry_mutex);
    size_t c = g_registry.count;
    pthread_mutex_unlock(&g_registry_mutex);
    return c;
}

const char *registry_get_name(size_t i) {
    pthread_mutex_lock(&g_registry_mutex);
    const char *n = NULL;
    if (i < g_registry.count) n = g_registry.tools[i].name;
    pthread_mutex_unlock(&g_registry_mutex);
    return n;
}

/* P52: Per-tool timeout */
void registry_set_timeout(const char *name, int seconds) {
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            g_registry.tools[i].timeout_sec = seconds;
            g_registry_generation++;
            pthread_mutex_unlock(&g_registry_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
}

int  registry_get_timeout(const char *name) {
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            int t = g_registry.tools[i].timeout_sec;
            pthread_mutex_unlock(&g_registry_mutex);
            return t;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return 0; /* Not found */
}

/* P150: Set toolset for a registered tool */
void registry_set_toolset(const char *name, const char *toolset) {
    if (!name || !toolset) return;
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            snprintf(g_registry.tools[i].toolset, sizeof(g_registry.tools[i].toolset), "%s", toolset);
            g_registry_generation++;
            pthread_mutex_unlock(&g_registry_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
}

/* P150: Get toolset for a registered tool */
const char *registry_get_toolset(const char *name) {
    if (!name) return "";
    pthread_mutex_lock(&g_registry_mutex);
    const char *t = "";
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            t = g_registry.tools[i].toolset;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return t;
}

/* S14 gap #16: Rich query API — get tool schema JSON */
/* PoP: get_schema @ tools/registry.py:get_schema */
/* Port of Python tools/registry.py:get_schema(). */
const char *registry_get_schema(const char *name) {
    if (!name) return "";
    pthread_mutex_lock(&g_registry_mutex);
    const char *s = "";
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            s = g_registry.tools[i].schema_json;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return s;
}

/* Rich query: return display emoji for a tool, or default (⚡) if unset */
/* PoP: get_emoji @ tools/registry.py:get_emoji */
/* Port of Python tools/registry.py:get_emoji(). */
const char *registry_get_emoji(const char *name, const char *default_emoji) {
    if (!name) return default_emoji ? default_emoji : "\xE2\x9A\xA1";
    pthread_mutex_lock(&g_registry_mutex);
    const char *e = NULL;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            e = g_registry.tools[i].emoji;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return e ? e : (default_emoji ? default_emoji : "\xE2\x9A\xA1");
}

/* S14 gap #16: Rich query API — check if any tool in a toolset is available */
/* PoP: is_toolset_available @ tools/registry.py:is_toolset_available */
/* Port of Python tools/registry.py:is_toolset_available(). */
bool registry_is_toolset_available(const char *toolset) {
    if (!toolset) return false;
    pthread_mutex_lock(&g_registry_mutex);
    bool avail = false;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].toolset, toolset) == 0 && g_registry.tools[i].available) {
            avail = true;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return avail;
}

/* Per-tool availability getter (mirrors Python registry.is_available). */
bool registry_is_available(const char *name) {
    if (!name) return false;
    pthread_mutex_lock(&g_registry_mutex);
    bool avail = false;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0 && g_registry.tools[i].available) {
            avail = true;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return avail;
}

/* P150: Check if a toolset is present in comma-separated list */
static bool toolset_in_list(const char *toolset, const char *csv) {
    if (!csv || !*csv) return true; /* empty list means "all" */
    if (!toolset || !*toolset) return true; /* unlabeled tools always visible */

    /* Tokenize the CSV list */
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", csv);
    char *save;
    char *tok = strtok_r(buf, ", ", &save);
    while (tok) {
        if (strcmp(tok, toolset) == 0) return true;
        tok = strtok_r(NULL, ", ", &save);
    }
    return false;
}

/* P150: Filter a tool_registry_t copy by enabled/disabled toolsets.
 * Modifies the 'available' flag on tools. Only operates on the COPY,
 * not the global registry. Pass NULL for either to skip that filter.
 * Empty string means "all" (no filter applied). */
void registry_filter_by_toolset(tool_registry_t *reg,
                                 const char *enabled_csv,
                                 const char *disabled_csv) {
    if (!reg) return;

    for (size_t i = 0; i < reg->count; i++) {
        /* If disabled list is set and tool is in it, mark unavailable */
        if (disabled_csv && *disabled_csv) {
            if (toolset_in_list(reg->tools[i].toolset, disabled_csv)) {
                reg->tools[i].available = false;
                continue;
            }
        }
        /* If enabled list is set and tool is NOT in it, mark unavailable */
        if (enabled_csv && *enabled_csv) {
            if (!toolset_in_list(reg->tools[i].toolset, enabled_csv)) {
                reg->tools[i].available = false;
            }
        }
    }
}

/* P55: Wildcard pattern matching — simple glob support */
bool registry_name_matches(const char *name, const char *pattern) {
    if (!name || !pattern) return false;

    /* If no wildcard, exact match */
    if (!strchr(pattern, '*'))
        return strcmp(name, pattern) == 0;

    /* Find positions of '*' */
    const char *star = strchr(pattern, '*');

    /* Pattern: "prefix*" — match prefix */
    if (star[1] == '\0') {
        size_t plen = (size_t)(star - pattern);
        return strncmp(name, pattern, plen) == 0;
    }

    /* Pattern: "*suffix" — match suffix */
    if (star == pattern) {
        const char *suffix = pattern + 1;
        size_t slen = strlen(name);
        size_t suflen = strlen(suffix);
        if (slen < suflen) return false;
        return strcmp(name + slen - suflen, suffix) == 0;
    }

    /* Pattern: "prefix*suffix" — match both */
    size_t plen = (size_t)(star - pattern);
    const char *suffix = star + 1;
    size_t slen = strlen(name);
    size_t suflen = strlen(suffix);
    if (slen < plen + suflen) return false;
    return strncmp(name, pattern, plen) == 0 &&
           strcmp(name + slen - suflen, suffix) == 0;
}

void registry_set_available_pattern(const char *pattern, bool available) {
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (registry_name_matches(g_registry.tools[i].name, pattern))
            g_registry.tools[i].available = available;
    }
    g_registry_generation++;
    pthread_mutex_unlock(&g_registry_mutex);
}

/* S14 gap #9: Register availability check function for a toolset.
 * All tools with this toolset name will run check_fn during refresh_availability. */
void registry_set_toolset_check_fn(const char *toolset, bool (*fn)(void)) {
    if (!toolset || !fn) return;
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].toolset, toolset) == 0) {
            g_registry.tools[i].check_fn = fn;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
}

void registry_set_check_fn(const char *name, bool (*fn)(void)) {
    if (!name || !fn) return;
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            g_registry.tools[i].check_fn = fn;
            /* Evaluate immediately so availability is correct without waiting
             * for the next refresh_availability() pass. */
            g_registry.tools[i].available = fn();
            g_registry_generation++;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
}

/* Refresh availability of all tools with check_fn registered.
 * Caches results for 30 seconds. */
void registry_refresh_availability(void) {
    pthread_mutex_lock(&g_registry_mutex);
    time_t now = time(NULL);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (!g_registry.tools[i].check_fn) continue;
        if (now - g_registry.tools[i].check_fn_last < 30) continue;
        g_registry.tools[i].check_fn_last = now;
        bool ok = g_registry.tools[i].check_fn();
        if (g_registry.tools[i].available != ok) {
            g_registry.tools[i].available = ok;
            g_registry_generation++;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
}

/* S14 gap #11: Deregister a tool by name. Removes from array by shifting. */
/* PoP: deregister @ tools/registry.py:deregister */
/* Port of Python tools/registry.py:deregister(). */
bool registry_deregister(const char *name) {
    if (!name) return false;
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            g_registry.count--;
            if (i < g_registry.count) {
                memmove(&g_registry.tools[i], &g_registry.tools[i + 1],
                        (g_registry.count - i) * sizeof(tool_t));
            }
            g_registry_generation++;
            pthread_mutex_unlock(&g_registry_mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return false;
}

/* ============================================================================
 *  Toolset enumeration + alias API (faithful port of Python ToolRegistry
 *  get_registered_toolset_names / get_tool_names_for_toolset /
 *  get_all_tool_names / get_tool_to_toolset_map / get_toolset_for_tool /
 *  register_toolset_alias / get_toolset_alias_target /
 *  get_registered_toolset_aliases).
 *  Pure data, thread-safe, no I/O.
 * ========================================================================== */

/* Return sorted unique toolset names present in the registry.
 * Caller frees the returned NULL-terminated char* array. */
/* PoP: get_registered_toolset_names @ tools/registry.py:get_registered_toolset_names */
char **registry_get_registered_toolset_names(size_t *out_n) {
    pthread_mutex_lock(&g_registry_mutex);
    char **names = calloc(g_registry.count + 1, sizeof(char*));
    size_t n = 0;
    for (size_t i = 0; i < g_registry.count; i++) {
        const char *ts = g_registry.tools[i].toolset;
        if (!ts || !ts[0]) continue;
        bool dup = false;
        for (size_t j = 0; j < n; j++) if (strcmp(names[j], ts) == 0) { dup = true; break; }
        if (!dup) names[n++] = strdup(ts);
    }
    /* insertion sort (short lists) */
    for (size_t a = 1; a < n; a++) {
        char *key = names[a]; size_t b = a;
        while (b > 0 && strcmp(names[b-1], key) > 0) { names[b] = names[b-1]; b--; }
        names[b] = key;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    if (out_n) *out_n = n;
    return names;
}

/* Return sorted tool names registered under a given toolset.
 * Caller frees the returned NULL-terminated char* array. */
/* PoP: get_tool_names_for_toolset @ tools/registry.py:get_tool_names_for_toolset */
char **registry_get_tool_names_for_toolset(const char *toolset, size_t *out_n) {
    pthread_mutex_lock(&g_registry_mutex);
    char **names = calloc(g_registry.count + 1, sizeof(char*));
    size_t n = 0;
    if (toolset) {
        for (size_t i = 0; i < g_registry.count; i++) {
            if (strcmp(g_registry.tools[i].toolset, toolset) == 0)
                names[n++] = strdup(g_registry.tools[i].name);
        }
    }
    for (size_t a = 1; a < n; a++) {
        char *key = names[a]; size_t b = a;
        while (b > 0 && strcmp(names[b-1], key) > 0) { names[b] = names[b-1]; b--; }
        names[b] = key;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    if (out_n) *out_n = n;
    return names;
}

/* Return sorted names of every registered tool.
 * Caller frees the returned NULL-terminated char* array. */
/* PoP: get_all_tool_names @ tools/registry.py:get_all_tool_names */
char **registry_get_all_tool_names(size_t *out_n) {
    pthread_mutex_lock(&g_registry_mutex);
    char **names = calloc(g_registry.count + 1, sizeof(char*));
    size_t n = 0;
    for (size_t i = 0; i < g_registry.count; i++)
        names[n++] = strdup(g_registry.tools[i].name);
    for (size_t a = 1; a < n; a++) {
        char *key = names[a]; size_t b = a;
        while (b > 0 && strcmp(names[b-1], key) > 0) { names[b] = names[b-1]; b--; }
        names[b] = key;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    if (out_n) *out_n = n;
    return names;
}

/* Return {tool_name: toolset_name} for every registered tool as JSON object. */
/* PoP: registry_get_tool_to_toolset_map @ tools/registry.py:get_tool_to_toolset_map */
char *registry_get_tool_to_toolset_map(void) {
    pthread_mutex_lock(&g_registry_mutex);
    json_node_t *obj = json_new_object();
    for (size_t i = 0; i < g_registry.count; i++) {
        json_object_set(obj, g_registry.tools[i].name,
                        json_new_string(g_registry.tools[i].toolset));
    }
    pthread_mutex_unlock(&g_registry_mutex);
    char *result = json_serialize(obj);
    json_free(obj);
    return result;
}

/* Return the toolset a tool belongs to, or NULL. Caller does not own. */
const char *registry_get_toolset_for_tool(const char *name) {
    pthread_mutex_lock(&g_registry_mutex);
    const char *ts = NULL;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) { ts = g_registry.tools[i].toolset; break; }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return ts;
}

/* Register an explicit alias for a canonical toolset name. Overwrites a
 * prior alias mapping (mirroring Python's collision-warning-then-overwrite). */
/* PoP: registry_register_toolset_alias @ tools/registry.py:register_toolset_alias */
void registry_register_toolset_alias(const char *alias, const char *toolset) {
    if (!alias || !alias[0] || !toolset) return;
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_aliases_count; i++) {
        if (strcmp(g_aliases[i].alias, alias) == 0) {
            snprintf(g_aliases[i].toolset, sizeof(g_aliases[i].toolset), "%s", toolset);
            pthread_mutex_unlock(&g_registry_mutex);
            return;
        }
    }
    if (g_aliases_count < REG_ALIAS_MAX) {
        snprintf(g_aliases[g_aliases_count].alias, sizeof(g_aliases[g_aliases_count].alias), "%s", alias);
        snprintf(g_aliases[g_aliases_count].toolset, sizeof(g_aliases[g_aliases_count].toolset), "%s", toolset);
        g_aliases_count++;
    }
    pthread_mutex_unlock(&g_registry_mutex);
}

/* Return the canonical toolset name for an alias, or NULL. Caller does not own. */
/* PoP: registry_get_toolset_alias_target @ tools/registry.py:get_toolset_alias_target */
const char *registry_get_toolset_alias_target(const char *alias) {
    if (!alias) return NULL;
    pthread_mutex_lock(&g_registry_mutex);
    const char *target = NULL;
    for (size_t i = 0; i < g_aliases_count; i++) {
        if (strcmp(g_aliases[i].alias, alias) == 0) { target = g_aliases[i].toolset; break; }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    return target;
}

/* Return a JSON {"alias": "toolset"} snapshot of all alias mappings. */
/* PoP: registry_get_registered_toolset_aliases @ tools/registry.py:get_registered_toolset_aliases */
char *registry_get_registered_toolset_aliases(void) {
    pthread_mutex_lock(&g_registry_mutex);
    json_node_t *obj = json_new_object();
    for (size_t i = 0; i < g_aliases_count; i++)
        json_object_set(obj, g_aliases[i].alias, json_new_string(g_aliases[i].toolset));
    pthread_mutex_unlock(&g_registry_mutex);
    char *result = json_serialize(obj);
    json_free(obj);
    return result;
}

/* Internal unlocked: does toolset have at least one available tool?
 * Mirrors Python _toolset_has_exposable_tools. */
/* PoP: toolset_has_exposable_tools @ tools/registry.py:_toolset_has_exposable_tools */
static bool toolset_has_exposable_tools(const char *toolset) {
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].toolset, toolset) == 0 &&
            g_registry.tools[i].available)
            return true;
    }
    return false;
}

/* Return per-tool max result size, or *default*, or the global default. */
/* PoP: registry_get_max_result_size @ tools/registry.py:get_max_result_size */
int registry_get_max_result_size(const char *name, int default_size) {
    pthread_mutex_lock(&g_registry_mutex);
    int out = REGISTRY_DEFAULT_RESULT_SIZE_CHARS;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            if (g_registry.tools[i].max_result_size_chars > 0)
                out = g_registry.tools[i].max_result_size_chars;
            else if (default_size > 0)
                out = default_size;
            pthread_mutex_unlock(&g_registry_mutex);
            return out;
        }
    }
    if (default_size > 0) out = default_size;
    pthread_mutex_unlock(&g_registry_mutex);
    return out;
}

/* {toolset: available_bool} for every registered toolset. */
/* PoP: registry_check_toolset_requirements @ tools/registry.py:check_toolset_requirements */
char *registry_check_toolset_requirements(void) {
    pthread_mutex_lock(&g_registry_mutex);
    json_node_t *obj = json_new_object();
    for (size_t i = 0; i < g_registry.count; i++) {
        const char *ts = g_registry.tools[i].toolset;
        if (!ts || !ts[0] || json_object_get(obj, ts)) continue;
        json_object_set(obj, ts, json_new_bool(toolset_has_exposable_tools(ts)));
    }
    pthread_mutex_unlock(&g_registry_mutex);
    char *result = json_serialize(obj);
    json_free(obj);
    return result;
}

/* {toolset: {available, tools[]}} metadata for UI display. */
/* PoP: registry_get_available_toolsets @ tools/registry.py:get_available_toolsets */
char *registry_get_available_toolsets(void) {
    pthread_mutex_lock(&g_registry_mutex);
    json_node_t *obj = json_new_object();
    for (size_t i = 0; i < g_registry.count; i++) {
        const char *ts = g_registry.tools[i].toolset;
        if (!ts || !ts[0]) continue;
        json_node_t *ts_obj = json_object_get(obj, ts);
        if (!ts_obj) {
            ts_obj = json_new_object();
            json_object_set(ts_obj, "available", json_new_bool(toolset_has_exposable_tools(ts)));
            json_object_set(ts_obj, "tools", json_new_array());
            json_object_set(obj, ts, ts_obj);
        }
        json_array_append(json_object_get(ts_obj, "tools"),
                          json_new_string(g_registry.tools[i].name));
    }
    pthread_mutex_unlock(&g_registry_mutex);
    char *result = json_serialize(obj);
    json_free(obj);
    return result;
}

/* {toolset: {name, env_vars[], check_fn, setup_url, tools[]}}. */
/* PoP: registry_get_toolset_requirements @ tools/registry.py:get_toolset_requirements */
char *registry_get_toolset_requirements(void) {
    pthread_mutex_lock(&g_registry_mutex);
    json_node_t *obj = json_new_object();
    for (size_t i = 0; i < g_registry.count; i++) {
        const char *ts = g_registry.tools[i].toolset;
        if (!ts || !ts[0]) continue;
        json_node_t *ts_obj = json_object_get(obj, ts);
        if (!ts_obj) {
            ts_obj = json_new_object();
            json_object_set(ts_obj, "name", json_new_string(ts));
            json_object_set(ts_obj, "env_vars", json_new_array());
            json_object_set(ts_obj, "check_fn", json_new_null());
            json_object_set(ts_obj, "setup_url", json_new_null());
            json_object_set(ts_obj, "tools", json_new_array());
            json_object_set(obj, ts, ts_obj);
        }
        for (size_t k = 0; k < g_registry.tools[i].requires_env_count; k++) {
            const char *e = g_registry.tools[i].requires_env[k];
            bool have = false;
            json_node_t *ev = json_object_get(ts_obj, "env_vars");
            for (size_t m = 0; m < json_len(ev); m++) {
                if (!strcmp(json_get_str(json_get(ev, m), NULL, ""), e)) { have = true; break; }
            }
            if (!have) json_array_append(ev, json_new_string(e));
        }
        json_array_append(json_object_get(ts_obj, "tools"),
                          json_new_string(g_registry.tools[i].name));
    }
    pthread_mutex_unlock(&g_registry_mutex);
    char *result = json_serialize(obj);
    json_free(obj);
    return result;
}

/* S14 gap #2: Tool Search bridge — search tools by keyword (name/description).
 * Searches tool name and description. Returns JSON array of matching names. */
char *registry_search(const char *keyword) {
    if (!keyword || !keyword[0])
        return strdup("[]");
    pthread_mutex_lock(&g_registry_mutex);
    json_node_t *arr = json_new_array();
    for (size_t i = 0; i < g_registry.count; i++) {
        if (!g_registry.tools[i].available) continue;
        if (strstr(g_registry.tools[i].name, keyword) ||
            strstr(g_registry.tools[i].description, keyword)) {
            json_array_append(arr, json_new_string(g_registry.tools[i].name));
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    char *result = json_serialize(arr);
    json_free(arr);
    return result;
}

/* S14 gap #2: Tool Search bridge — describe a tool by name.
 * Returns JSON object with name, description, schema, toolset. */
char *registry_describe(const char *name) {
    if (!name) return strdup("{\"error\":\"null name\"}");
    pthread_mutex_lock(&g_registry_mutex);
    char *result = NULL;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            json_node_t *obj = json_new_object();
            json_object_set(obj, "name", json_new_string(g_registry.tools[i].name));
            json_object_set(obj, "description", json_new_string(g_registry.tools[i].description));
            /* Parse schema string to JSON for clean output */
            char *err = NULL;
            json_node_t *schema = json_parse(g_registry.tools[i].schema_json, &err);
            if (schema) {
                json_object_set(obj, "schema", schema);
            } else {
                json_object_set(obj, "schema", json_new_string(g_registry.tools[i].schema_json));
                free(err);
            }
            json_object_set(obj, "toolset", json_new_string(g_registry.tools[i].toolset));
            json_object_set(obj, "available", json_new_bool(g_registry.tools[i].available));
            result = json_serialize(obj);
            json_free(obj);
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
    if (!result)
        result = strdup("{\"error\":\"tool not found\"}");
    return result;
}

/* S14 gap #8: Mark a tool as async (handlers that should run in detached thread) */
void registry_set_async(const char *name, bool async) {
    if (!name) return;
    pthread_mutex_lock(&g_registry_mutex);
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            g_registry.tools[i].async = async;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_mutex);
}
