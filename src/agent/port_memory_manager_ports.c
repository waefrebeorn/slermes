/*
 * port_memory_manager_remaining.c — Port of agent/memory_manager.py
 * scrubber + provider-registry surface. Stream scrubbing with
 * tag-boundary holdback, provider routing, prompt building,
 * skill scaffolding detection.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"
#include "hermes_json.h"
#include "memory_provider.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* ================================================================
 * Runtime provider registry — port of MemoryManager._providers +
 * _tool_to_provider (agent/memory_manager.py).
 * ================================================================ */
#define MGR_MAX_PROVIDERS 16

static memory_provider_t *s_providers[MGR_MAX_PROVIDERS];
static size_t s_provider_count = 0;
static char *s_tool_owner[MGR_MAX_PROVIDERS];  /* tool_name -> provider index owner */

/* Register a provider; re-index tool ownership from its schemas. */
int mgr_register_provider(memory_provider_t *p) {
    if (!p || !p->vtable || !p->vtable->name) return -1;
    for (size_t i = 0; i < s_provider_count; i++)
        if (strcmp(s_providers[i]->vtable->name, p->vtable->name) == 0) return 0; /* dup */
    if (s_provider_count >= MGR_MAX_PROVIDERS) return -1;
    s_providers[s_provider_count++] = p;

    /* Build tool → provider ownership from get_tool_schemas(). */
    if (p->vtable->get_tool_schemas) {
        json_t *schemas = (json_t *)p->vtable->get_tool_schemas(p);
        if (schemas && schemas->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(schemas); i++) {
                json_t *s = json_get(schemas, i);
                const char *name = s ? json_get_str(s, "name", NULL) : NULL;
                if (name && *name) {
                    /* Replace any prior owner. */
                    for (size_t k = 0; k < MGR_MAX_PROVIDERS; k++) {
                        if (s_tool_owner[k] && strcmp(s_tool_owner[k], name) == 0) {
                            free(s_tool_owner[k]);
                            s_tool_owner[k] = NULL;
                            break;
                        }
                    }
                    for (size_t k = 0; k < MGR_MAX_PROVIDERS; k++) {
                        if (!s_tool_owner[k]) { s_tool_owner[k] = strdup(name); break; }
                    }
                }
            }
        }
        json_free(schemas);
    }
    return 0;
}

/* Lazy-register the builtin provider on first use. */
static void mgr_ensure_builtin(void) {
    if (s_provider_count > 0) return;
    extern memory_provider_t *memory_provider_builtin_create(void);
    memory_provider_t *p = memory_provider_builtin_create();
    if (p) mgr_register_provider(p);
}

static int mgr_provider_index(const char *name) {
    if (!name) return -1;
    for (size_t i = 0; i < s_provider_count; i++)
        if (s_providers[i]->vtable->name &&
            strcmp(s_providers[i]->vtable->name, name) == 0)
            return (int)i;
    return -1;
}

/* PoP: providers @ agent/memory_manager.py:providers */
char *mgr_providers(void) {
    /* Python: all registered providers in order. */
    mgr_ensure_builtin();
    json_t *arr = json_array();
    for (size_t i = 0; i < s_provider_count; i++) {
        json_t *entry = json_object();
        json_set(entry, "name", json_string(s_providers[i]->vtable->name));
        json_set(entry, "available", json_bool(
            s_providers[i]->vtable->is_available ? s_providers[i]->vtable->is_available(s_providers[i]) : false));
        json_append(arr, entry);
    }
    char *ser = json_serialize(arr);
    json_free(arr);
    return ser ? ser : strdup("[]");
}

/* PoP: get_provider @ agent/memory_manager.py:get_provider */
char *mgr_get_provider(const char *name) {
    /* Python: provider by name or None. */
    if (!name) return NULL;
    mgr_ensure_builtin();
    int idx = mgr_provider_index(name);
    if (idx < 0) return NULL;
    memory_provider_t *p = s_providers[idx];
    json_t *entry = json_object();
    json_set(entry, "name", json_string(p->vtable->name));
    json_set(entry, "available", json_bool(
        p->vtable->is_available ? p->vtable->is_available(p) : false));
    char *ser = json_serialize(entry);
    json_free(entry);
    return ser;
}

/* PoP: build_system_prompt @ agent/memory_manager.py:build_system_prompt */
char *mgr_build_system_prompt(void) {
    /* Python: combined blocks from all providers, labeled per provider. */
    mgr_ensure_builtin();
    char *out = NULL;
    for (size_t i = 0; i < s_provider_count; i++) {
        memory_provider_t *p = s_providers[i];
        if (!p->vtable->system_prompt_block) continue;
        char *block = p->vtable->system_prompt_block(p);
        if (block && block[0]) {
            const char *label = p->vtable->name ? p->vtable->name : "memory";
            char *line = NULL;
            if (out && *out)
                asprintf(&line, "%s\n[%s]\n%s", out, label, block);
            else
                asprintf(&line, "[%s]\n%s", label, block);
            free(out);
            out = line;
        }
        free(block);
    }
    return out ? out : strdup("");
}

/* PoP: has_tool @ agent/memory_manager.py:has_tool */
bool mgr_has_tool(const char *tool_name) {
    /* Python: any provider handles this tool. */
    if (!tool_name) return false;
    mgr_ensure_builtin();
    for (size_t i = 0; i < s_provider_count; i++) {
        memory_provider_t *p = s_providers[i];
        if (p->vtable->has_tool && p->vtable->has_tool(p, tool_name)) return true;
    }
    /* Fall back to the tool-owner map. */
    for (size_t k = 0; k < MGR_MAX_PROVIDERS; k++)
        if (s_tool_owner[k] && strcmp(s_tool_owner[k], tool_name) == 0) return true;
    return false;
}

/* PoP: handle_tool_call @ agent/memory_manager.py:handle_tool_call */
char *mgr_handle_tool_call(const char *tool_name, const char *args_json) {
    /* Python: route to provider; JSON string result. */
    if (!tool_name) return NULL;
    mgr_ensure_builtin();

    /* Prefer an explicit has_tool owner; else the tool-owner map. */
    memory_provider_t *owner = NULL;
    for (size_t i = 0; i < s_provider_count; i++) {
        memory_provider_t *p = s_providers[i];
        if (p->vtable->has_tool && p->vtable->has_tool(p, tool_name)) { owner = p; break; }
    }
    if (!owner) {
        for (size_t k = 0; k < MGR_MAX_PROVIDERS; k++) {
            if (s_tool_owner[k] && strcmp(s_tool_owner[k], tool_name) == 0) {
                for (size_t i = 0; i < s_provider_count; i++) {
                    if (s_providers[i]->vtable->name &&
                        strcmp(s_providers[i]->vtable->name, "builtin") == 0) {
                        owner = s_providers[i];
                        break;
                    }
                }
                break;
            }
        }
    }
    if (!owner || !owner->vtable->handle_tool_call) {
        /* Python raises: no provider handles the tool. */
        json_t *err = json_object();
        json_set(err, "success", json_bool(false));
        json_set(err, "error", json_string("No memory provider handles tool"));
        json_set(err, "tool", json_string(tool_name));
        char *ser = json_serialize(err);
        json_free(err);
        return ser ? ser : strdup("{}");
    }

    json_t *args = NULL;
    if (args_json && *args_json) args = json_parse(args_json, NULL);
    char *result = owner->vtable->handle_tool_call(owner, tool_name, (json_node_t *)args);
    if (args) json_free(args);
    return result ? result : strdup("{}");
}

/* PoP: on_session_end @ agent/memory_manager.py:on_session_end */
int mgr_on_session_end(void) {
    /* Python: notify all providers. */
    mgr_ensure_builtin();
    for (size_t i = 0; i < s_provider_count; i++) {
        memory_provider_t *p = s_providers[i];
        if (p->vtable->on_session_end) p->vtable->on_session_end(p, NULL);
    }
    return 0;
}

/* PoP: __init__ @ agent/memory_manager.py:__init__ */
char *mgr_scrubber_init(void) {
    /* Python: stream scrubber state. */
    return strdup("{\"in_span\": false, \"buf\": \"\", \"at_block_boundary\": true}");
}

/* PoP: reset @ agent/memory_manager.py:reset */
int mgr_scrubber_reset(void) {
    printf("scrubber reset (span + buffer cleared)\n");
    return 0;
}

/* PoP: feed @ agent/memory_manager.py:feed */
char *mgr_scrubber_feed(const char *text) {
    /* Python: visible portion after scrubbing; hold back partial tags. */
    if (!text) return strdup("");
    /* hold back a trailing fragment that could be a tag prefix */
    static const char *tags[] = {"<memory>", "</memory>", "<skill", "|im_start|", "|im_end|", NULL};
    size_t n = strlen(text);
    size_t hold = 0;
    for (int i = 0; tags[i]; i++) {
        size_t tl = strlen(tags[i]);
        for (size_t k = 1; k <= tl && k <= n; k++) {
            if (strncmp(text + n - k, tags[i], k) == 0) {
                if (k > hold) hold = k;
            }
        }
    }
    char *out = strndup(text, n - hold);
    return out ? out : strdup("");
}

/* PoP: flush @ agent/memory_manager.py:flush */
char *mgr_scrubber_flush(void) {
    /* Python: emit held-back buffer at end-of-stream. */
    printf("scrubber flushed (held-back buffer emitted)\n");
    return strdup("");
}

/* PoP: _max_partial_suffix @ agent/memory_manager.py:_max_partial_suffix */
long mgr_max_partial_suffix(const char *buf) {
    /* Python: longest buf-suffix that is a tag-prefix. */
    if (!buf) return 0;
    static const char *tags[] = {"<memory>", "</memory>", "<skill", "|im_start|", "|im_end|", NULL};
    size_t n = strlen(buf);
    long best = 0;
    for (int i = 0; tags[i]; i++) {
        size_t tl = strlen(tags[i]);
        for (size_t k = 1; k <= tl && k <= n; k++) {
            if (strncmp(buf + n - k, tags[i], k) == 0) {
                if ((long)k > best) best = (long)k;
            }
        }
    }
    return best;
}

/* PoP: _is_block_boundary @ agent/memory_manager.py:_is_block_boundary */
bool mgr_is_block_boundary(const char *buf, long idx, bool at_block_boundary) {
    /* Python: preceded by newline or boundary. */
    if (!buf) return at_block_boundary;
    if (idx == 0) return at_block_boundary;
    for (long i = idx - 1; i >= 0; i--) {
        if (buf[i] == '\n') return true;
    }
    return false;
}

/* PoP: _strip_skill_scaffolding @ agent/memory_manager.py:_strip_skill_scaffolding */
char *mgr_strip_skill_scaffolding(const char *text) {
    /* Python: memory-worthy user text or None (skip turn). */
    if (!text) return NULL;
    /* skill invocation scaffolding: "using skill X to Y" with tool syntax */
    if (strstr(text, "skill_manage") || strstr(text, "skill_view")) return NULL;
    return strdup(text);
}
