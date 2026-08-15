/* port_agent_delegation_context.c
 *
 * Faithful C11 port of agent/delegation_context.py.
 *
 * Python ContextVar has no C equivalent in a single-threaded event-loop
 * runtime; the C port uses a pthread TLS flag so delegated-child marking
 * works correctly even when the agent loop dispatches work on worker threads.
 *
 * PoP: delegated_child_context_enter     @ agent/delegation_context.py:delegated_child_context
 * PoP: delegated_child_context_exit      @ agent/delegation_context.py:delegated_child_context
 * PoP: is_delegated_child_context        @ agent/delegation_context.py:is_delegated_child_context
 * PoP: is_delegated_child_process_context @ agent/delegation_context.py:is_delegated_child_process_context
 * PoP: scrub_kanban_env                  @ agent/delegation_context.py:scrub_kanban_env
 * PoP: delegated_child_subprocess_env    @ agent/delegation_context.py:delegated_child_subprocess_env
 */

#include "hermes_delegation_context.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* POSIX provides `environ`; declared here for callers that include only
 * hermes_delegation_context.h without pulling in <unistd.h>. */
extern char **environ;

/* ------------------------------------------------------------------ */
/*  TLS slot: delegation depth counter (0 = not delegated)             */
/* ------------------------------------------------------------------ */
static pthread_key_t  g_dc_key;
static pthread_once_t g_dc_once = PTHREAD_ONCE_INIT;
static bool           g_dc_key_created = false;

static void dc_key_destructor(void *ptr)
{
    free(ptr);
}

static void dc_key_create(void)
{
    (void)pthread_key_create(&g_dc_key, dc_key_destructor);
    g_dc_key_created = true;
}

static int *dc_get_depth_ptr(void)
{
    pthread_once(&g_dc_once, dc_key_create);
    int *p = (int *)pthread_getspecific(g_dc_key);
    if (!p) {
        p = (int *)calloc(1, sizeof(int));
        if (p) pthread_setspecific(g_dc_key, p);
    }
    return p;
}

/* ------------------------------------------------------------------ */
/*  DelegatedChildEnvCtx helper                                        */
/* ------------------------------------------------------------------ */
struct delegated_child_env_ctx {
    char **env;
    size_t  count;
    size_t  cap;
};

static void dc_env_init(struct delegated_child_env_ctx *ctx)
{
    ctx->env   = NULL;
    ctx->count = 0;
    ctx->cap   = 0;
}

static void dc_env_free(struct delegated_child_env_ctx *ctx)
{
    if (!ctx->env) return;
    for (size_t i = 0; i < ctx->count; i++) free(ctx->env[i]);
    free(ctx->env);
    ctx->env   = NULL;
    ctx->count = 0;
    ctx->cap   = 0;
}

static bool dc_env_push(struct delegated_child_env_ctx *ctx, const char *entry)
{
    if (ctx->count >= ctx->cap) {
        size_t ncap = ctx->cap ? ctx->cap * 2 : 32;
        char **tmp = (char **)realloc(ctx->env, ncap * sizeof(char *));
        if (!tmp) return false;
        ctx->env = tmp;
        ctx->cap  = ncap;
    }
    ctx->env[ctx->count++] = entry ? strdup(entry) : NULL;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/* PoP: delegated_child_context_enter @ agent/delegation_context.py:delegated_child_context */
bool delegated_child_context_enter(void)
{
    int *depth = dc_get_depth_ptr();
    if (!depth) return false;
    (*depth)++;
    return true;
}

/* PoP: delegated_child_context_exit @ agent/delegation_context.py:delegated_child_context */
void delegated_child_context_exit(void)
{
    int *depth = dc_get_depth_ptr();
    if (depth && *depth > 0) (*depth)--;
}

/* PoP: is_delegated_child_context @ agent/delegation_context.py:is_delegated_child_context */
bool is_delegated_child_context(void)
{
    int *depth = dc_get_depth_ptr();
    return (depth && *depth > 0) ? true : false;
}

/* PoP: is_delegated_child_process_context @ agent/delegation_context.py:is_delegated_child_process_context */
bool is_delegated_child_process_context(void)
{
    if (is_delegated_child_context()) return true;
    return getenv("HERMES_DELEGATED_CHILD_CONTEXT") != NULL ? true : false;
}

/* PoP: scrub_kanban_env @ agent/delegation_context.py:scrub_kanban_env */
char **scrub_kanban_env(char **env)
{
    if (!env) return NULL;

    static const char *KANBAN_KEYS[] = {
        "HERMES_KANBAN_TASK",
        "HERMES_KANBAN_RUN_ID",
        "HERMES_KANBAN_WORKSPACE",
        "HERMES_KANBAN_WORKSPACES_ROOT",
        "HERMES_KANBAN_CLAIM_LOCK",
        "HERMES_KANBAN_BOARD",
        "HERMES_KANBAN_DB",
        NULL
    };

    struct delegated_child_env_ctx ctx;
    dc_env_init(&ctx);

    for (size_t i = 0; env[i]; i++) {
        const char *eq = strchr(env[i], '=');
        if (!eq) {
            dc_env_push(&ctx, env[i]);
            continue;
        }
        size_t klen = (size_t)(eq - env[i]);
        const char *key = env[i];

        bool is_kanban = false;
        for (size_t k = 0; KANBAN_KEYS[k]; k++) {
            if (strlen(KANBAN_KEYS[k]) == klen &&
                memcmp(key, KANBAN_KEYS[k], klen) == 0) {
                is_kanban = true;
                break;
            }
        }
        if (is_kanban) continue;

        dc_env_push(&ctx, env[i]);
    }

    /* Add lineage marker */
    dc_env_push(&ctx, "HERMES_DELEGATED_CHILD_CONTEXT=1");

    /* NULL-terminate */
    if (ctx.count < ctx.cap) ctx.env[ctx.count] = NULL;
    else {
        char **tmp = (char **)realloc(ctx.env, (ctx.count + 2) * sizeof(char *));
        if (tmp) { ctx.env = tmp; ctx.env[ctx.count] = NULL; }
    }
    return ctx.env;
}

/* PoP: delegated_child_subprocess_env @ agent/delegation_context.py:delegated_child_subprocess_env */
char **delegated_child_subprocess_env(char **env)
{
    if (!is_delegated_child_process_context()) {
        if (!env) return NULL;
        /* shallow copy of env */
        size_t n = 0;
        while (env[n]) n++;
        char **out = (char **)calloc(n + 1, sizeof(char *));
        if (!out) return NULL;
        for (size_t i = 0; i < n; i++) out[i] = strdup(env[i]);
        return out;
    }

    if (!env) env = environ;
    return scrub_kanban_env(env);
}
