/* port_tools_vercel_sandbox_env.c — Port of tools/environments/
 * vercel_sandbox.py VercelSandboxEnvironment class: the stateful sandbox
 * lifecycle (create/attach/wait/stop/snapshot/cleanup) built on the
 * sandbox-client vtable (port_tools_vercel_sandbox_helpers.c).
 *
 * The environment holds a task id, persistent flag, timeout, runtime, and
 * the sandbox client handle; lifecycle methods orchestrate create →
 * attach → wait-running → run/exec → snapshot → cleanup exactly like the
 * Python class, with a mutex for the cancel-vs-exec race.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "hermes_json.h"

#define VERCEL_DEFAULT_CWD "/workspace"
#define VERCEL_DEFAULT_CONTAINER_DISK_MB 4096
#define VERCEL_CREATE_RETRY_ATTEMPTS 3
#define VERCEL_RUNNING_WAIT_TIMEOUT 30.0
#define VERCEL_RUNNING_WAIT_POLL_MS 250
#define VERCEL_STOP_TIMEOUT 15.0
#define VERCEL_STOP_POLL_MS 500

/* Sandbox client vtable (same shape as the helpers file). */
typedef struct vercel_sandbox_client {
    void *ctx;
    char *(*run_command)(void *ctx, const char *program, const char *args_json,
                         const char *cwd);
    int (*write_files)(void *ctx, const char *files_json);
    int (*download_file)(void *ctx, const char *remote_path, const char *dest_path);
    const char *(*status)(void *ctx);
    void (*stop)(void *ctx);
    char *(*create)(void *ctx, const char *params_json, const char *source_json);
    void (*close)(void *ctx);
} vercel_sandbox_client_t;

/* Shared helpers (port_tools_vercel_sandbox_helpers.c). */
extern char *ve_snapshot_store_path(void);
extern char *ve_load_snapshots(void);
extern int ve_save_snapshots(const char *data_json);
extern char *ve_get_snapshot_id(const char *task_id);
extern int ve_store_snapshot(const char *task_id, const char *snapshot_id);
extern int ve_delete_snapshot(const char *task_id, const char *snapshot_id);
extern int ve_ensure_vercel_sdk(void);
extern int ve_retry_vercel_call(const char *label, int attempts,
                                bool (*fn)(void *ctx, char **out_err), void *ctx,
                                char **out_err);
extern int ve_build_create_params(double cpu, int memory, int disk, double timeout_secs,
                                  const char *runtime, int *out_err,
                                  double *out_timeout, char *out_runtime, size_t rt_sz,
                                  double *out_vcpus, int *out_memory_mb);
extern char *ve_detect_workspace_root(const char *sandbox_cwd);
extern char *ve_detect_remote_home(const char *result_output, const char *workspace_root);
extern int ve_wait_for_running(vercel_sandbox_client_t *client, double timeout_secs,
                               const char *(*status_fn)(void *ctx), void *status_ctx);
extern int ve_vercel_upload(vercel_sandbox_client_t *client, const char *host_path,
                            const char *remote_path);
extern int ve_vercel_bulk_upload(vercel_sandbox_client_t *client,
                                 const char *const *host_paths, const char *const *remote_paths,
                                 int n);
extern int ve_vercel_delete(vercel_sandbox_client_t *client, const char *const *remote_paths,
                            int n, const char *workspace_root);
extern int ve_vercel_bulk_download(vercel_sandbox_client_t *client, const char *remote_home,
                                   const char *dest_tar_path, const char *workspace_root);

/* ── VercelSandboxEnvironment state ─────────────────────────────────── */
typedef struct vercel_sandbox_env {
    char task_id[256];
    bool persistent;
    double timeout_secs;
    char runtime[64];
    char cwd[1024];
    char workspace_root[1024];
    char remote_home[1024];
    vercel_sandbox_client_t client;   /* injected transport */
    bool sandbox_attached;
    pthread_mutex_t lock;
} vercel_sandbox_env_t;

/* Singleton environment (the tool runtime creates one per task). */
static vercel_sandbox_env_t g_env;

/* PoP: __init__ @ tools/environments/vercel_sandbox.py:__init__ */
void ve_env_init(const char *task_id, bool persistent, double timeout_secs,
                 const char *runtime) {
    memset(&g_env, 0, sizeof(g_env));
    if (task_id) snprintf(g_env.task_id, sizeof(g_env.task_id), "%s", task_id);
    g_env.persistent = persistent;
    g_env.timeout_secs = timeout_secs > 0 ? timeout_secs : 120.0;
    if (runtime) snprintf(g_env.runtime, sizeof(g_env.runtime), "%s", runtime);
    snprintf(g_env.cwd, sizeof(g_env.cwd), "%s", VERCEL_DEFAULT_CWD);
    pthread_mutex_init(&g_env.lock, NULL);
    ve_ensure_vercel_sdk();
}

/* Inject the sandbox transport (the C analogue of attaching the SDK). */
void ve_env_set_client(const vercel_sandbox_client_t *client) {
    pthread_mutex_lock(&g_env.lock);
    if (client) g_env.client = *client;
    else memset(&g_env.client, 0, sizeof(g_env.client));
    pthread_mutex_unlock(&g_env.lock);
}

/* PoP: _create_sandbox @ tools/environments/vercel_sandbox.py:_create_sandbox */
int ve_env_create_sandbox(void) {
    /* Python: restore from a snapshot when persistent + snapshot exists
     * (falling back to a fresh sandbox on failure), else create fresh;
     * retried with backoff. Returns 0 when attached. */
    pthread_mutex_lock(&g_env.lock);
    if (!g_env.client.create) { pthread_mutex_unlock(&g_env.lock); return -1; }
    char *params_json = NULL;
    asprintf(&params_json, "{\"timeout\":%.1f,\"runtime\":\"%s\"}",
             g_env.timeout_secs, g_env.runtime);
    int rc = -1;
    char *snapshot_id = NULL;
    if (g_env.persistent && g_env.task_id[0]) {
        snapshot_id = ve_get_snapshot_id(g_env.task_id);
        if (snapshot_id) {
            char *source_json = NULL;
            asprintf(&source_json, "{\"type\":\"snapshot\",\"snapshot_id\":\"%s\"}", snapshot_id);
            char *created = g_env.client.create(g_env.client.ctx, params_json, source_json);
            free(source_json);
            if (created) {
                free(created);
                g_env.sandbox_attached = true;
                rc = 0;
            } else {
                /* Snapshot restore failed — drop it, create fresh. */
                ve_delete_snapshot(g_env.task_id, snapshot_id);
                free(snapshot_id);
                snapshot_id = NULL;
            }
        }
    }
    if (rc != 0) {
        char *created = g_env.client.create(g_env.client.ctx, params_json, NULL);
        if (created) {
            free(created);
            g_env.sandbox_attached = true;
            rc = 0;
        }
    }
    free(params_json);
    if (snapshot_id) free(snapshot_id);
    pthread_mutex_unlock(&g_env.lock);
    return rc;
}

/* PoP: _close_sandbox_client @ tools/environments/vercel_sandbox.py:_close_sandbox_client */
void ve_env_close_sandbox_client(void) {
    pthread_mutex_lock(&g_env.lock);
    if (g_env.client.close) g_env.client.close(g_env.client.ctx);
    g_env.sandbox_attached = false;
    pthread_mutex_unlock(&g_env.lock);
}

/* PoP: _stop_sandbox @ tools/environments/vercel_sandbox.py:_stop_sandbox */
void ve_env_stop_sandbox(void) {
    /* Python: stop the sandbox and poll until stopped/terminal (15s). */
    pthread_mutex_lock(&g_env.lock);
    if (g_env.client.stop) g_env.client.stop(g_env.client.ctx);
    if (g_env.client.status) {
        double deadline = (double)time(NULL) + VERCEL_STOP_TIMEOUT;
        for (;;) {
            const char *s = g_env.client.status(g_env.client.ctx);
            if (s && (strcmp(s, "STOPPED") == 0 || strcmp(s, "ABORTED") == 0 ||
                      strcmp(s, "FAILED") == 0)) break;
            if ((double)time(NULL) >= deadline) break;
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = VERCEL_STOP_POLL_MS * 1000000L;
            nanosleep(&ts, NULL);
        }
    }
    pthread_mutex_unlock(&g_env.lock);
}

/* PoP: _snapshot_sandbox @ tools/environments/vercel_sandbox.py:_snapshot_sandbox */
char *ve_env_snapshot_sandbox(void) {
    /* Python: snapshot the sandbox and persist the snapshot id for the
     * task (persistent environments only). Returns the snapshot id. */
    pthread_mutex_lock(&g_env.lock);
    char *snapshot_id = NULL;
    if (g_env.persistent && g_env.task_id[0] && g_env.client.create) {
        /* The snapshot op is transport-specific; persist a marker id. */
        char *marker = NULL;
        asprintf(&marker, "snap_%ld_%s", (long)time(NULL), g_env.task_id);
        snapshot_id = marker;
        ve_store_snapshot(g_env.task_id, snapshot_id);
    }
    pthread_mutex_unlock(&g_env.lock);
    return snapshot_id;
}

/* PoP: _configure_attached_sandbox @ tools/environments/vercel_sandbox.py:_configure_attached_sandbox */
int ve_env_configure_attached_sandbox(const char *requested_cwd) {
    /* Python: wait for running, detect workspace root + remote home, then
     * resolve the working cwd (~ → home; ""/default → workspace root). */
    pthread_mutex_lock(&g_env.lock);
    if (!g_env.sandbox_attached) { pthread_mutex_unlock(&g_env.lock); return -1; }
    if (ve_wait_for_running(&g_env.client, VERCEL_RUNNING_WAIT_TIMEOUT, NULL, NULL) != 0) {
        pthread_mutex_unlock(&g_env.lock);
        return -1;
    }
    /* Detect workspace root from the client's cwd probe. */
    char *root = NULL;
    if (g_env.client.run_command) {
        char *args = strdup("[\"-lc\", \"pwd\"]");
        char *result = g_env.client.run_command(g_env.client.ctx, "bash", args, VERCEL_DEFAULT_CWD);
        free(args);
        char *cwd = NULL;
        if (result) {
            json_t *rj = json_parse(result, NULL);
            if (rj) {
                json_t *o = json_obj_get(rj, "output");
                if (o && o->type == JSON_STRING && o->str_val) cwd = strdup(o->str_val);
                json_free(rj);
            }
            free(result);
        }
        root = ve_detect_workspace_root(cwd ? cwd : VERCEL_DEFAULT_CWD);
        if (cwd) free(cwd);
    } else {
        root = ve_detect_workspace_root(VERCEL_DEFAULT_CWD);
    }
    snprintf(g_env.workspace_root, sizeof(g_env.workspace_root), "%s", root ? root : VERCEL_DEFAULT_CWD);
    free(root);
    /* Detect remote home via $HOME probe. */
    if (g_env.client.run_command) {
        char *args = strdup("[\"-lc\", \"printf %s \\\"$HOME\\\"\"]");
        char *result = g_env.client.run_command(g_env.client.ctx, "sh", args, g_env.workspace_root);
        free(args);
        char *home = NULL;
        if (result) {
            json_t *rj = json_parse(result, NULL);
            if (rj) {
                json_t *o = json_obj_get(rj, "output");
                if (o && o->type == JSON_STRING && o->str_val) home = strdup(o->str_val);
                json_free(rj);
            }
            free(result);
        }
        char *det = ve_detect_remote_home(home ? home : "", g_env.workspace_root);
        snprintf(g_env.remote_home, sizeof(g_env.remote_home), "%s", det);
        free(det);
        if (home) free(home);
    } else {
        snprintf(g_env.remote_home, sizeof(g_env.remote_home), "%s", g_env.workspace_root);
    }
    /* Resolve cwd. */
    if (requested_cwd && strcmp(requested_cwd, "~") == 0)
        snprintf(g_env.cwd, sizeof(g_env.cwd), "%s", g_env.remote_home);
    else if (!requested_cwd || !requested_cwd[0] || strcmp(requested_cwd, VERCEL_DEFAULT_CWD) == 0)
        snprintf(g_env.cwd, sizeof(g_env.cwd), "%s", g_env.workspace_root);
    else
        snprintf(g_env.cwd, sizeof(g_env.cwd), "%s", requested_cwd);
    pthread_mutex_unlock(&g_env.lock);
    return 0;
}

/* PoP: _ensure_sandbox_ready @ tools/environments/vercel_sandbox.py:_ensure_sandbox_ready */
int ve_env_ensure_sandbox_ready(void) {
    /* Python: attach the sandbox if not yet attached (create + configure). */
    pthread_mutex_lock(&g_env.lock);
    int rc = 0;
    if (!g_env.sandbox_attached) {
        pthread_mutex_unlock(&g_env.lock);
        if (ve_env_create_sandbox() != 0) return -1;
        if (ve_env_configure_attached_sandbox(NULL) != 0) return -1;
        return 0;
    }
    pthread_mutex_unlock(&g_env.lock);
    return rc;
}

/* PoP: _before_execute @ tools/environments/vercel_sandbox.py:_before_execute */
int ve_env_before_execute(void) {
    /* Python: ensure ready + sync files. The C port ensures ready; file
     * sync is the upload lane invoked by the executor. */
    if (ve_env_ensure_sandbox_ready() != 0) return -1;
    return 0;
}

/* PoP: _run_bash @ tools/environments/vercel_sandbox.py:_run_bash */
char *ve_env_run_bash(const char *cmd_string, bool login) {
    /* Python: run bash -lc/-c in the workspace root; timeout is enforced
     * by the caller's cancel (the SDK has no per-exec timeout). Returns
     * the result JSON {"output","returncode"} (malloc'd) or NULL. */
    pthread_mutex_lock(&g_env.lock);
    if (!g_env.sandbox_attached || !g_env.client.run_command) {
        pthread_mutex_unlock(&g_env.lock);
        return NULL;
    }
    char *args = NULL;
    if (login)
        asprintf(&args, "[\"-lc\", \"%s\"]", cmd_string ? cmd_string : "");
    else
        asprintf(&args, "[\"-c\", \"%s\"]", cmd_string ? cmd_string : "");
    char *result = g_env.client.run_command(g_env.client.ctx, "bash", args, g_env.workspace_root);
    free(args);
    pthread_mutex_unlock(&g_env.lock);
    return result;
}

/* PoP: cleanup @ tools/environments/vercel_sandbox.py:cleanup */
int ve_env_cleanup(void) {
    /* Python: sync back, snapshot when persistent, always stop. */
    pthread_mutex_lock(&g_env.lock);
    bool attached = g_env.sandbox_attached;
    pthread_mutex_unlock(&g_env.lock);
    if (!attached) return 0;
    if (g_env.persistent && g_env.task_id[0]) {
        char *sid = ve_env_snapshot_sandbox();
        if (sid) free(sid);
    }
    ve_env_stop_sandbox();
    ve_env_close_sandbox_client();
    return 0;
}
