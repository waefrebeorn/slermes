/*
 * plugin_stream_hooks.h — PoP of agent/plugin_stream_hooks.py.
 * See plugin_stream_hooks.c for the full Python -> C symbol map.
 */
#ifndef HERMES_AGENT_PLUGIN_STREAM_HOOKS_H
#define HERMES_AGENT_PLUGIN_STREAM_HOOKS_H

#include <stddef.h>
#include <stdint.h>

struct json_t;
typedef struct json_t json_t;

typedef struct psh_callback {
    const char *name;                       /* __name__ */
    void (*fn)(void *ctx, const json_t *payload);
    void *ctx;
} psh_callback_t;

typedef struct psh_dispatcher psh_dispatcher_t;

extern const size_t PSH_QUEUE_SIZE;

const char *plugin_stream_hooks__callback_name(const psh_callback_t *cb);

/* Consumer registry (stands in for hermes_cli.plugins.iter_hook_callbacks).
 * Replaces the whole callback set for hook_name. */
void   plugin_stream_hooks_set_consumer_registry(const char *hook_name,
                                                 const psh_callback_t *cbs,
                                                 size_t n);
size_t plugin_stream_hooks__registered_callbacks(const char *hook_name,
                                                 psh_callback_t *out,
                                                 size_t out_cap);

int  enqueue_plugin_stream_hook(const char *hook_name, const json_t *payload);
int  has_stream_observer_hooks(void);
int  stream_reasoning_deltas_enabled(void);
int  has_reasoning_stream_observer_hooks(void);
void shutdown_plugin_stream_hook_dispatcher(double timeout);

size_t plugin_stream_hooks_dispatcher_count(const char *hook_name);
size_t plugin_stream_hooks_dispatchers_for(const char *hook_name);
void   plugin_stream_hooks_wait_idle(void);

#endif /* HERMES_AGENT_PLUGIN_STREAM_HOOKS_H */
