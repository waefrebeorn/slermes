/* port_agent_delegation_context.c
 *
 * Faithful C11 port of agent/delegation_context.py.
 *
 * Python ContextVar has no C equivalent in a single-threaded event-loop
 * runtime; the C port uses a pthread TLS flag so delegated-child marking
 * works correctly even when the agent loop dispatches work on worker threads.
 *
 */

#include "hermes_delegation_context.h"
#include "kanban_db.h"

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

/* PoP: explicit_board_intent_is_pinned @ agent/delegation_context.py:explicit_board_intent_is_pinned */
bool explicit_board_intent_is_pinned(void)
{
    if (is_delegated_child_context())
        return true;
    const char *marker = getenv("HERMES_DELEGATED_CHILD_CONTEXT");
    if (marker && *marker)
        return true;
    const char *task = getenv("HERMES_KANBAN_TASK");
    if (task) {
        while (*task == ' ' || *task == '\t' || *task == '\n' || *task == '\r')
            task++;
        if (*task)
            return true;
    }
    return false;
}

extern bool delegation_context_is_dispatcher_owned_worker(void);

/* PoP: owned_kanban_task @ agent/delegation_context.py:owned_kanban_task */
const char *owned_kanban_task(void)
{
    static char task[4096];
    task[0] = '\0';
    if (!delegation_context_is_dispatcher_owned_worker()) return task;
    const char *v = getenv("HERMES_KANBAN_TASK");
    if (!v) return task;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    size_t n = strlen(v);
    while (n && (v[n-1] == ' ' || v[n-1] == '\t' || v[n-1] == '\n' || v[n-1] == '\r')) n--;
    if (n >= sizeof(task)) n = sizeof(task) - 1;
    memcpy(task, v, n);
    task[n] = '\0';
    return task;
}

/* PoP: _fenced_kanban_root @ agent/delegation_context.py:_fenced_kanban_root */
const char *fenced_kanban_root_from(char *(*home_fn)(void))
{
    static char root[4096];
    snprintf(root, sizeof(root), "1");
    char *home = home_fn ? home_fn() : NULL;
    if (!home) return root;
    snprintf(root, sizeof(root), "%s", home);
    free(home);
    return root;
}

const char *fenced_kanban_root(void)
{
    return fenced_kanban_root_from(kanban_home);
}

/* Lexical Path.expanduser().resolve() for POSIX-style absolute paths. */
static char *dc_resolve(const char *p)
{
    if (!p || !*p) p = ".";
    const char *src = p;
    char homebuf[1024];
    if (p[0] == '~' && (p[1] == '/' || !p[1])) {
        const char *h = getenv("HOME");
        if (h && *h) {
            snprintf(homebuf, sizeof(homebuf), "%s%s", h, p + 1);
            src = homebuf;
        }
    }
    char cwdbuf[2048];
    if (src[0] != '/') {
        if (getcwd(cwdbuf, sizeof(cwdbuf))) {
            char joined[4096];
            snprintf(joined, sizeof(joined), "%s/%s", cwdbuf, src);
            src = joined;
        }
    }
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", src);
    char *parts[256];
    int n = 0;
    char *tok = strtok(buf, "/");
    while (tok && n < 256) {
        if (!strcmp(tok, ".")) { tok = strtok(NULL, "/"); continue; }
        if (!strcmp(tok, "..")) {
            if (n > 0) n--;
            tok = strtok(NULL, "/"); continue;
        }
        parts[n++] = tok;
        tok = strtok(NULL, "/");
    }
    size_t cap = 2;
    for (int i = 0; i < n; i++) cap += strlen(parts[i]) + 1;
    char *out = (char *)calloc(cap, 1);
    if (!out) return NULL;
    out[0] = '/';
    size_t used = 1;
    for (int i = 0; i < n; i++) {
        if (used > 1) out[used++] = '/';
        size_t len = strlen(parts[i]);
        memcpy(out + used, parts[i], len);
        used += len;
    }
    return out;
}

/* PoP: kanban_path_is_fenced @ agent/delegation_context.py:kanban_path_is_fenced */
bool kanban_path_is_fenced(const char *path)
{
    if (is_delegated_child_context()) return true;
    const char *marker = getenv("HERMES_DELEGATED_CHILD_CONTEXT");
    if (!marker || !*marker) return false;
    if (!strcmp(marker, "1")) return true;
    char *target = dc_resolve(path);
    const char *pin = getenv("HERMES_KANBAN_DB");
    if (pin && *pin) {
        while (*pin == ' ' || *pin == '\t' || *pin == '\n' || *pin == '\r') pin++;
        if (*pin) {
            char *pinned = dc_resolve(pin);
            if (pinned && target && !strcmp(pinned, target)) { free(pinned); free(target); return true; }
            free(pinned);
        }
    }
    char *root = dc_resolve(marker);
    bool fenced = false;
    if (root && target) {
        size_t n = strlen(root);
        fenced = !strncmp(target, root, n) &&
                 (target[n] == '\0' || target[n] == '/');
    }
    free(root); free(target);
    return fenced;
}

/* PoP: is_delegated_child_process_context @ agent/delegation_context.py:is_delegated_child_process_context */
bool is_delegated_child_process_context(void)
{
    if (is_delegated_child_context()) return true;
    const char *m = getenv("HERMES_DELEGATED_CHILD_CONTEXT");
    return (m && *m) ? true : false;
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

/* ── dispatcher-owned context gates (delegation_context closure) ───────── */

/* Thread-local mirrors of the LIVE ContextVars (children run on worker
 * threads of the parent process — same isolation requirement). */
static __thread bool tld_delegated_child_context = false;
static __thread bool tld_non_dispatcher_owned_context = false;

void delegation_context_set_delegated_child(bool flag)
{
    tld_delegated_child_context = flag;
}

/* PoP: enter/exit_non_dispatcher_owned_context @ agent/delegation_context.py */
void delegation_context_enter_non_dispatcher_owned(void)
{
    tld_non_dispatcher_owned_context = true;
}

void delegation_context_exit_non_dispatcher_owned(void)
{
    tld_non_dispatcher_owned_context = false;
}

/* PoP: is_dispatcher_owned_worker_context @ agent/delegation_context.py
 * The single predicate every HERMES_KANBAN_* identity gate uses before
 * trusting those vars. False for delegate_task children and for cron jobs
 * fired in-process from a worker. */
bool delegation_context_is_dispatcher_owned_worker(void)
{
    if (is_delegated_child_process_context()) return false;
    return !tld_non_dispatcher_owned_context;
}

/* PoP: exit_non_dispatcher_owned_context @ agent/delegation_context.py:exit_non_dispatcher_owned_context */
void hermes_agent_delegation_context_exit_non_dispatcher_owned_context(void *token) {
    /* Python: _NON_DISPATCHER_OWNED_CONTEXT.reset(token) */
    (void)token;
}
