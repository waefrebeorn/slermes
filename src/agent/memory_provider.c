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

/* PoP: system_prompt_block @ agent/memory_provider.py:system_prompt_block */
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
