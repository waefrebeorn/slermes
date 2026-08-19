/*
 * memory_provider.c — Built-in memory provider (default, no-op external).
 *
 * Implements the memory_provider_vtable_t for the "builtin" provider.
 * This provider uses the local memory store (memory.c) and exposes
 * no extra tools. All optional hooks are no-ops.
 *
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.name()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.is_available()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.initialize()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.system_prompt_block()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.prefetch()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.queue_prefetch()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.sync_turn()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.get_tool_schemas()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.handle_tool_call()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.shutdown()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.on_turn_start()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.on_session_end()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.on_session_switch()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.on_pre_compress()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.on_delegation()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.get_config_schema()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.save_config()
 * AG26: Port of Python agent/memory_provider.py:MemoryProvider.on_memory_write()
 */

/* PoP: memory provider (port of agent/memory_manager) */

#include "memory_provider.h"
#include "hermes_memory.h"
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Built-in provider vtable implementations
 * ================================================================ */

static bool builtin_is_available(memory_provider_t *self) {
    (void)self;
    /* Built-in is always available — no external deps */
    return true;
}

static void builtin_initialize(memory_provider_t *self, const char *session_id,
                                const char *hermes_home, const char *platform,
                                const char *agent_context,
                                const char *agent_identity,
                                const char *agent_workspace,
                                const char *parent_session_id,
                                const char *user_id, const char *user_id_alt) {
    (void)platform; (void)agent_context; (void)agent_identity;
    (void)agent_workspace; (void)parent_session_id;
    (void)user_id; (void)user_id_alt;

    if (self->session_id) free(self->session_id);
    self->session_id = session_id ? strdup(session_id) : NULL;

    if (self->hermes_home) free(self->hermes_home);
    self->hermes_home = hermes_home ? strdup(hermes_home) : NULL;

    self->initialized = true;
}

/* PoP: builtin_system_prompt_block @ agent/memory_provider.py:system_prompt_block */
static char *builtin_system_prompt_block(memory_provider_t *self) {
    (void)self;
    /* Built-in provider adds no extra system prompt text */
    char *s = malloc(1);
    if (s) s[0] = '\0';
    return s;
}

static char *builtin_prefetch(memory_provider_t *self, const char *query,
                               const char *session_id) {
    (void)self; (void)query; (void)session_id;
    /* Built-in does no external prefetch */
    char *s = malloc(1);
    if (s) s[0] = '\0';
    return s;
}

static json_node_t *builtin_get_tool_schemas(memory_provider_t *self) {
    (void)self;
    /* Built-in exposes no extra tools */
    return json_array();
}

static char *builtin_handle_tool_call(memory_provider_t *self,
                                       const char *tool_name,
                                       json_node_t *args) {
    (void)self; (void)tool_name; (void)args;
    return strdup("{\"error\": \"builtin provider has no tools\"}");
}

static void builtin_shutdown(memory_provider_t *self) {
    self->initialized = false;
}

/* PoP: builtin_get_config_schema @ agent/memory_provider.py:get_config_schema */
static memory_provider_config_field_t *builtin_get_config_schema(memory_provider_t *self) {
    (void)self;
    /* Built-in needs no config */
    return NULL;
}

/* ================================================================
 *  VTable instance
 * ================================================================ */

static const memory_provider_vtable_t builtin_vtable = {
    .name                  = "builtin",
    .is_available          = builtin_is_available,
    .initialize            = builtin_initialize,
    .system_prompt_block   = builtin_system_prompt_block,
    .prefetch              = builtin_prefetch,
    .queue_prefetch        = memory_provider_noop_queue_prefetch,
    .sync_turn             = memory_provider_noop_sync_turn,
    .get_tool_schemas      = builtin_get_tool_schemas,
    .handle_tool_call      = builtin_handle_tool_call,
    .has_tool              = memory_provider_noop_has_tool,
    .shutdown              = builtin_shutdown,
    .on_turn_start         = memory_provider_noop_turn_start,
    .on_session_end        = memory_provider_noop_session_end,
    .on_session_switch     = memory_provider_noop_session_switch,
    .on_pre_compress       = memory_provider_empty_pre_compress,
    .on_delegation         = memory_provider_noop_delegation,
    .get_config_schema     = builtin_get_config_schema,
    .save_config           = memory_provider_noop_save_config,
    .on_memory_write       = memory_provider_noop_memory_write,
};

/* ================================================================
 *  Constructor / Destructor
 * ================================================================ */

memory_provider_t *memory_provider_builtin_create(void) {
    memory_provider_t *p = calloc(1, sizeof(memory_provider_t));
    if (!p) return NULL;
    p->vtable = &builtin_vtable;
    p->initialized = false;
    return p;
}

void memory_provider_builtin_destroy(memory_provider_t *p) {
    if (!p) return;
    free(p->session_id);
    free(p->hermes_home);
    free(p->platform);
    free(p);
}

/* === merged from port_agent_memory_provider.c === */
/*
 * port_agent_memory_provider.c — Port of Python agent/memory_provider.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: backup_paths */
typedef struct {
    char paths[16][1024];
    int count;
} backup_path_list_t;

backup_path_list_t memory_provider_backup_paths(void) {
    backup_path_list_t result = {0};
    /* Return external paths that store provider state outside HERMES_HOME */
    const char *home = getenv("HOME");
    if (!home) return result;
    
    /* Common external memory provider paths */
    const char *external_paths[] = {
        "/.honcho",
        "/.hindsight",
        "/.openviking",
        NULL
    };
    
    for (int i = 0; external_paths[i] && result.count < 16; i++) {
        snprintf(result.paths[result.count], 1024, "%s%s", home, external_paths[i]);
        result.count++;
    }
    return result;
}


/* PoP: is_trailing_utf8_punct @ agent/memory_provider.py:is_trivial_prompt */

/* Trivial words: (yes|no|ok|okay|sure|thanks|thank you|y|n|yep|nope|yeah|nah|
 * hi|hey|hello|yo|sup|continue|go ahead|do it|proceed|got it|cool|nice|great|
 * done|next|lgtm|k) — case-insensitive, anchored; only whitespace/punctuation
 * may follow (so "k8s" / "note" / "hindsight" do NOT match). */
static const char *TRIVIAL_WORDS[] = {
    "yes","no","ok","okay","sure","thanks","thank you","y","n","yep","nope",
    "yeah","nah","hi","hey","hello","yo","sup","continue","go ahead","do it",
    "proceed","got it","cool","nice","great","done","next","lgtm","k",
    NULL
};

/* Return 1 when the UTF-8 sequence at *p is one of the allowed trailing
 * punctuation codepoints: \u2018 \u2019 \u201c \u201d \u2014 \u2013 \u2026
 * \u00a0. Advances *p past it. */
static int is_trailing_utf8_punct(const char **p) {
    const unsigned char *u = (const unsigned char *)*p;
    if (u[0] == 0xC2 && u[1] == 0xA0) { *p += 2; return 1; }            /* \u00a0 */
    if (u[0] == 0xE2 && u[1] == 0x80) {
        unsigned char b = u[2];
        if (b == 0x98 || b == 0x99 || b == 0x9C || b == 0x9D ||          /* ' ' " " */
            b == 0x94 || b == 0x93 || b == 0xA6) {                      /* em dash, en dash, ellipsis */
            *p += 3; return 1;
        }
    }
    return 0;
}

/* Return 1 when the remainder is only whitespace / allowed punctuation. */
static int trivial_trailing_only(const char *s) {
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (isspace(c)) { s++; continue; }
        if (strchr("!?.:;,\"'~()[]{}<>*&^%$#@!+=`", c)) { s++; continue; }
        if (c >= 0x80) {
            if (is_trailing_utf8_punct(&s)) continue;
            return 0;
        }
        return 0;
    }
    return 1;
}

int memory_provider_is_trivial_prompt(const char *text) {
    if (!text || !*text) return 1;
    /* strip */
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 1;
    if (*p == '/') return 1;  /* slash command */

    /* Case-insensitive word-prefix match + trailing-only remainder. */
    for (int i = 0; TRIVIAL_WORDS[i]; i++) {
        const char *w = TRIVIAL_WORDS[i];
        size_t wl = strlen(w);
        size_t pl = strlen(p);
        if (pl < wl) continue;
        /* casefold compare */
        size_t j;
        for (j = 0; j < wl; j++) {
            unsigned char a = (unsigned char)p[j], b = (unsigned char)w[j];
            if (tolower(a) != tolower(b)) break;
        }
        if (j == wl && trivial_trailing_only(p + wl)) return 1;
    }
    return 0;
}

