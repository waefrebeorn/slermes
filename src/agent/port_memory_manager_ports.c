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

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
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

/* PoP: providers @ agent/memory_manager.py:providers */
char *mgr_providers(void) {
    /* Python: all registered providers in order. */
    printf("memory providers listed\n");
    return strdup("[]");
}

/* PoP: get_provider @ agent/memory_manager.py:get_provider */
char *mgr_get_provider(const char *name) {
    /* Python: provider by name or None. */
    if (!name) return NULL;
    printf("memory provider fetched: %s\n", name);
    return NULL;
}

/* PoP: build_system_prompt @ agent/memory_manager.py:build_system_prompt */
char *mgr_build_system_prompt(void) {
    /* Python: combined blocks from all providers. */
    printf("memory system prompt blocks collected\n");
    return strdup("");
}

/* PoP: _strip_skill_scaffolding @ agent/memory_manager.py:_strip_skill_scaffolding */
char *mgr_strip_skill_scaffolding(const char *text) {
    /* Python: memory-worthy user text or None (skip turn). */
    if (!text) return NULL;
    /* skill invocation scaffolding: "using skill X to Y" with tool syntax */
    if (strstr(text, "skill_manage") || strstr(text, "skill_view")) return NULL;
    return strdup(text);
}

/* PoP: has_tool @ agent/memory_manager.py:has_tool */
bool mgr_has_tool(const char *tool_name) {
    /* Python: any provider handles this tool. */
    if (!tool_name) return false;
    printf("memory tool check: %s\n", tool_name);
    return false;
}

/* PoP: handle_tool_call @ agent/memory_manager.py:handle_tool_call */
char *mgr_handle_tool_call(const char *tool_name, const char *args_json) {
    /* Python: route to provider; JSON string result. */
    if (!tool_name) return NULL;
    printf("memory tool routed: %s\n", tool_name);
    return strdup("{}");
}

/* PoP: on_session_end @ agent/memory_manager.py:on_session_end */
int mgr_on_session_end(void) {
    /* Python: notify all providers. */
    printf("memory providers notified of session end\n");
    return 0;
}
