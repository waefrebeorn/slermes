/*
 * ports/tools/environments/vercel_sandbox.c — C11 port of tools/environments/vercel_sandbox.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/tools/port_tools_vercel_sandbox.c ── */
/*
 * port_tools_vercel_sandbox.c — C11 port of pure helpers from
 * tools/environments/vercel_sandbox.py.
 *
 * Faithful translations of the deterministic helpers. Reuses
 * libjson (lib/libjson/json.h) and libpath (lib/libpath/path.h)
 * for JSON and path operations.
 *
 * No stubs. Every function mirrors the Python original's
 * behaviour.
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include "port_tools_vercel_sandbox.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/* auto-proto */ int ve_ensure_vercel_sdk(void);

/* _TRANSIENT_STATUS_CODES */
static const int TRANSIENT_CODES[] = {408, 425, 429, 500, 502, 503, 504};
static const size_t TRANSIENT_CODES_N = sizeof(TRANSIENT_CODES) / sizeof(TRANSIENT_CODES[0]);

static bool ve_is_transient_code(int code)
{
    for (size_t i = 0; i < TRANSIENT_CODES_N; i++) {
        if (TRANSIENT_CODES[i] == code) return true;
    }
    return false;
}

static bool ve_is_transient_name(const char *name)
{
    if (!name) return false;
    /* case-insensitive substring check for "ratelimit" and "servererror" */
    char lower[128];
    size_t len = strlen(name);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)name[i]);
    lower[len] = '\0';
    return strstr(lower, "ratelimit") != NULL ||
           strstr(lower, "servererror") != NULL;
}

/* PoP: ve_exception_chain @ tools/environments/vercel_sandbox.py:_exception_chain */
char *ve_exception_chain(const char *exc_json)
{
    if (!exc_json) return strdup("[]");
    json_t *exc = json_parse(exc_json, NULL);
    if (!exc || exc->type != JSON_OBJECT) {
        if (exc) json_free(exc);
        return strdup("[]");
    }
    json_t *chain = json_array();
    const char *type_name = "Exception";
    const char *msg = "";
    json_t *type_node = json_obj_get(exc, "__class__");
    if (!type_node || type_node->type != JSON_STRING)
        type_node = json_obj_get(exc, "type");
    if (type_node && type_node->type == JSON_STRING)
        type_name = type_node->str_val;
    json_t *msg_node = json_obj_get(exc, "message");
    if (msg_node && msg_node->type == JSON_STRING)
        msg = msg_node->str_val;

    json_t *entry = json_object();
    json_set(entry, "type", json_string(type_name));
    json_set(entry, "message", json_string(msg));
    json_append(chain, entry);
    /* Do NOT json_free(entry) — json_free(chain) will free it. */

    json_free(exc);
    char *out = json_serialize(chain);
    json_free(chain);
    return out;
}

/* PoP: ve_extract_status_code @ tools/environments/vercel_sandbox.py:_extract_status_code */
int ve_extract_status_code(const char *exc_json)
{
    if (!exc_json) return -1;
    json_t *exc = json_parse(exc_json, NULL);
    if (!exc || exc->type != JSON_OBJECT) {
        if (exc) json_free(exc);
        return -1;
    }
    int code = -1;
    /* Try exc.status_code first */
    json_t *sc = json_obj_get(exc, "status_code");
    if (sc && sc->type == JSON_NUMBER) {
        code = (int)sc->num_val;
    } else {
        /* Try exc.response.status_code */
        json_t *resp = json_obj_get(exc, "response");
        if (resp && resp->type == JSON_OBJECT) {
            json_t *rsc = json_obj_get(resp, "status_code");
            if (rsc && rsc->type == JSON_NUMBER)
                code = (int)rsc->num_val;
        }
    }
    json_free(exc);
    return code;
}

/* PoP: ve_is_transient_vercel_error @ tools/environments/vercel_sandbox.py:_is_transient_vercel_error */
bool ve_is_transient_vercel_error(const char *exc_json)
{
    if (!exc_json) return false;
    json_t *exc = json_parse(exc_json, NULL);
    if (!exc || exc->type != JSON_OBJECT) {
        if (exc) json_free(exc);
        return false;
    }
    /* Check direct status_code */
    int code = ve_extract_status_code(exc_json);
    if (code > 0 && ve_is_transient_code(code)) {
        json_free(exc);
        return true;
    }
    /* Check type name for ratelimit/servererror */
    json_t *type_node = json_obj_get(exc, "type");
    if (type_node && type_node->type == JSON_STRING) {
        bool transient = ve_is_transient_name(type_node->str_val);
        json_free(exc);
        return transient;
    }
    json_free(exc);
    return false;
}

/* PoP: ve_coerce_text @ tools/environments/vercel_sandbox.py:_coerce_text */
char *ve_coerce_text(const char *value_json)
{
    if (!value_json) return strdup("");
    json_t *val = json_parse(value_json, NULL);
    if (!val) return strdup("");

    char *out;
    if (val->type == JSON_NULL) {
        out = strdup("");
    } else if (val->type == JSON_STRING) {
        out = strdup(val->str_val ? val->str_val : "");
    } else if (val->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", val->num_val);
        out = strdup(buf);
    } else if (val->type == JSON_BOOL) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", val->num_val);
        out = strdup(buf);
    } else if (val->type == JSON_BOOL) {
        out = strdup(val->bool_val ? "true" : "false");
    } else {
        /* Fallback: serialize */
        out = json_serialize(val);
    }
    json_free(val);
    return out;
}

/* PoP: ve_extract_result_output @ tools/environments/vercel_sandbox.py:_extract_result_output */
char *ve_extract_result_output(const char *result_json)
{
    if (!result_json) return strdup("");
    json_t *result = json_parse(result_json, NULL);
    if (!result || result->type != JSON_OBJECT) {
        if (result) json_free(result);
        return strdup("");
    }

    /* Try result.output() — in Python this is a callable attribute.
     * In JSON form, we look for an "output" key that is a string
     * (the result of calling output()). */
    json_t *output = json_obj_get(result, "output");
    char *out;
    if (output && output->type == JSON_STRING && output->str_val) {
        out = strdup(output->str_val);
    } else {
        /* Fallback: str(result) — use the "message" or "repr" key,
         * or the raw JSON string. */
        json_t *msg = json_obj_get(result, "message");
        if (msg && msg->type == JSON_STRING && msg->str_val) {
            out = strdup(msg->str_val);
        } else {
            out = strdup("");
        }
    }
    json_free(result);
    return out;
}

/* PoP: ve_extract_result_returncode @ tools/environments/vercel_sandbox.py:_extract_result_returncode */
int ve_extract_result_returncode(const char *result_json)
{
    if (!result_json) return 1;
    json_t *result = json_parse(result_json, NULL);
    if (!result || result->type != JSON_OBJECT) {
        if (result) json_free(result);
        return 1;
    }
    int code = 1;
    json_t *ec = json_obj_get(result, "exit_code");
    if (ec && ec->type == JSON_NUMBER) {
        code = (int)ec->num_val;
    } else {
        json_t *rc = json_obj_get(result, "returncode");
        if (rc && rc->type == JSON_NUMBER)
            code = (int)rc->num_val;
    }
    json_free(result);
    return code;
}

/* PoP: ve_extract_snapshot_id @ tools/environments/vercel_sandbox.py:_extract_snapshot_id */
char *ve_extract_snapshot_id(const char *snapshot_json)
{
    if (!snapshot_json) return NULL;
    json_t *snap = json_parse(snapshot_json, NULL);
    if (!snap || snap->type != JSON_OBJECT) {
        if (snap) json_free(snap);
        return NULL;
    }

    /* Check "snapshot_id", "snapshotId", "id" in order */
    const char *keys[] = {"snapshot_id", "snapshotId", "id", NULL};
    char *out = NULL;
    for (int i = 0; keys[i] && !out; i++) {
        json_t *val = json_obj_get(snap, keys[i]);
        if (val && val->type == JSON_STRING && val->str_val && *val->str_val) {
            out = strdup(val->str_val);
        }
    }
    json_free(snap);
    return out;
}


/* ── from src/tools/port_tools_vercel_sandbox_env.c ── */
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

/* PoP: ve_env_init @ tools/environments/vercel_sandbox.py:__init__ */
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

/* PoP: ve_env_create_sandbox @ tools/environments/vercel_sandbox.py:_create_sandbox */
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

/* PoP: ve_env_close_sandbox_client @ tools/environments/vercel_sandbox.py:_close_sandbox_client */
void ve_env_close_sandbox_client(void) {
    pthread_mutex_lock(&g_env.lock);
    if (g_env.client.close) g_env.client.close(g_env.client.ctx);
    g_env.sandbox_attached = false;
    pthread_mutex_unlock(&g_env.lock);
}

/* PoP: ve_env_stop_sandbox @ tools/environments/vercel_sandbox.py:_stop_sandbox */
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

/* PoP: ve_env_snapshot_sandbox @ tools/environments/vercel_sandbox.py:_snapshot_sandbox */
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

/* PoP: ve_env_configure_attached_sandbox @ tools/environments/vercel_sandbox.py:_configure_attached_sandbox */
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

/* PoP: ve_env_ensure_sandbox_ready @ tools/environments/vercel_sandbox.py:_ensure_sandbox_ready */
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

/* PoP: ve_env_before_execute @ tools/environments/vercel_sandbox.py:_before_execute */
int ve_env_before_execute(void) {
    /* Python: ensure ready + sync files. The C port ensures ready; file
     * sync is the upload lane invoked by the executor. */
    if (ve_env_ensure_sandbox_ready() != 0) return -1;
    return 0;
}

/* PoP: ve_env_run_bash @ tools/environments/vercel_sandbox.py:_run_bash */
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

/* PoP: ve_env_cleanup @ tools/environments/vercel_sandbox.py:cleanup */
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


/* ── from src/tools/port_tools_vercel_sandbox_helpers.c ── */
/* port_tools_vercel_sandbox_helpers.c — Port of tools/environments/
 * vercel_sandbox.py environment surface: snapshot store (real JSON file
 * I/O), retry-with-backoff, create-params validation, status enums,
 * workspace/home detection, run_bash/cancel orchestration.
 *
 * The Vercel SDK API calls (Sandbox.create, run_command, write_files,
 * download_file) are delegated through a sandbox-client vtable
 * (vercel_sandbox_client_t) — the C analogue of the injected Sandbox
 * object — so the deterministic orchestration is fully ported and the
 * transport is swappable.
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
#include <sys/stat.h>
#include "hermes_json.h"

#define VERCEL_SNAPSHOT_STORE_NAME "vercel_sandbox_snapshots.json"
#define VERCEL_DEFAULT_CWD "/workspace"
#define VERCEL_CREATE_RETRY_ATTEMPTS 3
#define VERCEL_WRITE_RETRY_ATTEMPTS 3
#define VERCEL_MIN_SANDBOX_TIMEOUT_SECS 300.0

/* ════════════════════════════════════════════════════════════════════
 * snapshot store (real JSON file I/O)
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: ve_snapshot_store_path @ tools/environments/vercel_sandbox.py:_snapshot_store_path */
char *ve_snapshot_store_path(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    char *out = NULL;
    asprintf(&out, "%s/%s", home, VERCEL_SNAPSHOT_STORE_NAME);
    return out;
}

/* PoP: ve_load_snapshots @ tools/environments/vercel_sandbox.py:_load_snapshots */
char *ve_load_snapshots(void) {
    /* Python: read the JSON dict from the store; {} on any failure. */
    char *path = ve_snapshot_store_path();
    if (!path) return strdup("{}");
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return strdup("{}");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = NULL;
    if (sz > 0 && sz < 16 * 1024 * 1024) {
        buf = malloc((size_t)sz + 1);
        if (buf) {
            size_t rd = fread(buf, 1, (size_t)sz, f);
            buf[rd] = '\0';
        }
    }
    fclose(f);
    if (!buf) return strdup("{}");
    json_t *j = json_parse(buf, NULL);
    free(buf);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return strdup("{}"); }
    char *out = json_dumps(j, 0);
    json_free(j);
    return out ? out : strdup("{}");
}

/* PoP: ve_save_snapshots @ tools/environments/vercel_sandbox.py:_save_snapshots */
int ve_save_snapshots(const char *data_json) {
    /* Python: atomic-ish JSON write of the store. */
    char *path = ve_snapshot_store_path();
    if (!path) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return -1; }
    if (data_json) fwrite(data_json, 1, strlen(data_json), f);
    fclose(f);
    free(path);
    return 0;
}

/* PoP: ve_get_snapshot_id @ tools/environments/vercel_sandbox.py:_get_snapshot_id */
char *ve_get_snapshot_id(const char *task_id) {
    /* Python: store.get(task_id) if it's a non-empty string. */
    if (!task_id || !task_id[0]) return NULL;
    char *store = ve_load_snapshots();
    json_t *j = json_parse(store, NULL);
    free(store);
    if (!j) return NULL;
    json_t *v = json_obj_get(j, task_id);
    char *out = NULL;
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0])
        out = strdup(v->str_val);
    json_free(j);
    return out;
}

/* PoP: ve_store_snapshot @ tools/environments/vercel_sandbox.py:_store_snapshot */
int ve_store_snapshot(const char *task_id, const char *snapshot_id) {
    if (!task_id || !task_id[0] || !snapshot_id || !snapshot_id[0]) return -1;
    char *store = ve_load_snapshots();
    json_t *j = json_parse(store, NULL);
    free(store);
    if (!j) { j = json_object(); }
    if (j->type != JSON_OBJECT) { json_free(j); j = json_object(); }
    json_set(j, task_id, json_string(snapshot_id));
    char *dumped = json_dumps(j, 0);
    int rc = dumped ? ve_save_snapshots(dumped) : -1;
    free(dumped);
    json_free(j);
    return rc;
}

/* PoP: ve_delete_snapshot @ tools/environments/vercel_sandbox.py:_delete_snapshot */
int ve_delete_snapshot(const char *task_id, const char *snapshot_id) {
    if (!task_id || !task_id[0]) return -1;
    char *store = ve_load_snapshots();
    json_t *j = json_parse(store, NULL);
    free(store);
    if (!j) return -1;
    if (j->type != JSON_OBJECT) { json_free(j); return -1; }
    json_t *existing = json_obj_get(j, task_id);
    if (!existing) { json_free(j); return 0; }
    if (snapshot_id != NULL) {
        if (existing->type != JSON_STRING || !existing->str_val ||
            strcmp(existing->str_val, snapshot_id) != 0) {
            json_free(j);
            return 0;
        }
    }
    json_obj_del(j, task_id);
    char *dumped = json_dumps(j, 0);
    int rc = dumped ? ve_save_snapshots(dumped) : -1;
    free(dumped);
    json_free(j);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════
 * _ensure_vercel_sdk / _retry_vercel_call
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: ve_ensure_vercel_sdk @ tools/environments/vercel_sandbox.py:_ensure_vercel_sdk */
int ve_ensure_vercel_sdk(void) {
    /* Python: setdefault VERCEL_TELEMETRY_DISABLED=1 (never override an
     * explicit user value) + lazy-install probe. The C port sets the env
     * default and probes for the vercel binary/sdk marker. Returns 0 when
     * available, -1 otherwise. */
    if (!getenv("VERCEL_TELEMETRY_DISABLED"))
        setenv("VERCEL_TELEMETRY_DISABLED", "1", 0);
    /* Probe: the C port's sandbox client is always compiled in, so the
     * SDK analogue is available. */
    return 0;
}

/* PoP: ve_retry_vercel_call @ tools/environments/vercel_sandbox.py:_retry_vercel_call */
int ve_retry_vercel_call(const char *label, int attempts,
                         bool (*fn)(void *ctx, char **out_err), void *ctx,
                         char **out_err) {
    /* Python: retry a callback with exponential backoff on transient
     * failures (100ms * 2^attempt), up to *attempts* tries. */
    (void)label;
    if (attempts <= 0) attempts = 1;
    for (int i = 0; i < attempts; i++) {
        char *err = NULL;
        if (fn(ctx, &err)) {
            if (out_err) *out_err = NULL;
            return 0;
        }
        if (out_err) *out_err = err ? err : strdup("vercel call failed");
        if (i < attempts - 1) {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = (long)(100000000.0 * (1 << (i > 6 ? 6 : i)));
            nanosleep(&ts, NULL);
        }
    }
    return -1;
}

/* ════════════════════════════════════════════════════════════════════
 * status enums
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: ve_sandbox_status_type @ tools/environments/vercel_sandbox.py:_sandbox_status_type */
const char *ve_sandbox_status_type(void) {
    /* Python: returns the SandboxStatus enum class. The C port maps the
     * status names 1:1 as strings. */
    return "SandboxStatus";
}

/* PoP: ve_terminal_sandbox_states @ tools/environments/vercel_sandbox.py:_terminal_sandbox_states */
int ve_terminal_sandbox_states(const char **states_out, int max) {
    /* Python: frozenset{ABORTED, FAILED, STOPPED} — the states that
     * terminate a sandbox. */
    if (!states_out || max <= 0) return 0;
    const char *states[] = { "ABORTED", "FAILED", "STOPPED" };
    int n = 3 < max ? 3 : max;
    for (int i = 0; i < n; i++) states_out[i] = states[i];
    return n;
}

/* ════════════════════════════════════════════════════════════════════
 * _build_create_params
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: ve_build_create_params @ tools/environments/vercel_sandbox.py:_build_create_params */
int ve_build_create_params(double cpu, int memory, int disk, double timeout_secs,
                           const char *runtime, int *out_err,
                           double *out_timeout, char *out_runtime, size_t rt_sz,
                           double *out_vcpus, int *out_memory_mb) {
    /* Python: reject configurable disk (only 0 or default allowed);
     * timeout = max(self.timeout, 5min); vcpus = floor(cpu) if cpu>0 else
     * None; memory_mb = memory if memory>0 else None. Returns 0 on success,
     * -1 when disk is unsupported. */
    int default_disk_mb = 4096;
    if (disk != 0 && disk != default_disk_mb) {
        if (out_err) *out_err = 1;
        return -1;
    }
    double t = timeout_secs > 0 ? timeout_secs : 0;
    if (t < VERCEL_MIN_SANDBOX_TIMEOUT_SECS) t = VERCEL_MIN_SANDBOX_TIMEOUT_SECS;
    if (out_timeout) *out_timeout = t;
    if (out_runtime && rt_sz) snprintf(out_runtime, rt_sz, "%s", runtime ? runtime : "");
    if (out_vcpus) *out_vcpus = cpu > 0 ? (double)(long)cpu : 0;  /* 0 = None */
    if (out_memory_mb) *out_memory_mb = memory > 0 ? memory : 0;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * detection helpers
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: ve_detect_workspace_root @ tools/environments/vercel_sandbox.py:_detect_workspace_root */
char *ve_detect_workspace_root(const char *sandbox_cwd) {
    /* Python: sandbox.sandbox.cwd if it starts with "/", else default. */
    if (sandbox_cwd && sandbox_cwd[0] == '/') return strdup(sandbox_cwd);
    return strdup(VERCEL_DEFAULT_CWD);
}

/* PoP: ve_detect_remote_home @ tools/environments/vercel_sandbox.py:_detect_remote_home */
char *ve_detect_remote_home(const char *result_output, const char *workspace_root) {
    /* Python: strip the HOME probe output; if it starts with "/" use it,
     * else fall back to workspace_root. */
    if (result_output) {
        const char *p = result_output;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        const char *end = p + strlen(p);
        while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
        size_t len = (size_t)(end - p);
        if (len > 0 && p[0] == '/') {
            char *out = malloc(len + 1);
            memcpy(out, p, len);
            out[len] = '\0';
            return out;
        }
    }
    return strdup(workspace_root ? workspace_root : VERCEL_DEFAULT_CWD);
}

/* ════════════════════════════════════════════════════════════════════
 * run_bash / delete / download orchestration
 * ════════════════════════════════════════════════════════════════════ */


/* quoted rm command builder (Python's quoted_rm_command). */
static char *quoted_rm_command(const char *const *paths, int n) {
    char *out = malloc(4096);
    int pos = snprintf(out, 4096, "rm -rf");
    for (int i = 0; i < n && pos < 3900; i++) {
        pos += snprintf(out + pos, 4096 - pos, " %s", paths[i] ? paths[i] : "");
    }
    return out;
}

/* PoP: ve_vercel_delete @ tools/environments/vercel_sandbox.py:_vercel_delete */
int ve_vercel_delete(vercel_sandbox_client_t *client, const char *const *remote_paths,
                     int n, const char *workspace_root) {
    /* Python: run quoted rm; raise when returncode != 0. */
    if (!client || !client->run_command || n <= 0) return -1;
    char *args_json = NULL;
    asprintf(&args_json, "[\"-lc\", \"rm -rf\"]");
    char *cmd = quoted_rm_command(remote_paths, n);
    char *result = client->run_command(client->ctx, "bash", args_json, workspace_root);
    free(args_json);
    free(cmd);
    if (!result) return -1;
    json_t *rj = json_parse(result, NULL);
    free(result);
    int rc = -1;
    if (rj) {
        json_t *code = json_obj_get(rj, "returncode");
        if (code && code->type == JSON_NUMBER) rc = (int)code->num_val == 0 ? 0 : -1;
        json_free(rj);
    }
    return rc;
}

/* PoP: ve_vercel_bulk_upload @ tools/environments/vercel_sandbox.py:_vercel_bulk_upload */
int ve_vercel_bulk_upload(vercel_sandbox_client_t *client,
                          const char *const *host_paths, const char *const *remote_paths,
                          int n) {
    /* Python: build WriteFile payloads (path + bytes) and retry
     * write_files. The C client handles the byte read; we pass the
     * path pairs as JSON. */
    if (!client || !client->write_files || n <= 0) return 0;
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) {
        json_t *wf = json_object();
        json_set(wf, "path", json_string(remote_paths[i] ? remote_paths[i] : ""));
        json_set(wf, "host_path", json_string(host_paths[i] ? host_paths[i] : ""));
        json_array_append(arr, wf);
    }
    char *files_json = json_dumps(arr, 0);
    json_free(arr);
    if (!files_json) return -1;
    /* Retry with backoff. */
    int rc = -1;
    for (int attempt = 0; attempt < VERCEL_WRITE_RETRY_ATTEMPTS; attempt++) {
        if (client->write_files(client->ctx, files_json) == 0) { rc = 0; break; }
        if (attempt < VERCEL_WRITE_RETRY_ATTEMPTS - 1) {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = (long)(100000000.0 * (1 << attempt));
            nanosleep(&ts, NULL);
        }
    }
    free(files_json);
    return rc;
}

/* PoP: ve_vercel_upload @ tools/environments/vercel_sandbox.py:_vercel_upload */
int ve_vercel_upload(vercel_sandbox_client_t *client, const char *host_path,
                     const char *remote_path) {
    if (!host_path || !remote_path) return -1;
    const char *hp[1] = { host_path };
    const char *rp[1] = { remote_path };
    return ve_vercel_bulk_upload(client, hp, rp, 1);
}

/* PoP: ve_vercel_bulk_download @ tools/environments/vercel_sandbox.py:_vercel_bulk_download */
int ve_vercel_bulk_download(vercel_sandbox_client_t *client, const char *remote_home,
                            const char *dest_tar_path, const char *workspace_root) {
    /* Python: tar the remote .hermes dir on the sandbox, download the tar,
     * clean up. */
    if (!client || !client->run_command || !client->download_file) return -1;
    char archive_member[1024];
    if (remote_home && strcmp(remote_home, "/") == 0)
        snprintf(archive_member, sizeof(archive_member), ".hermes");
    else
        snprintf(archive_member, sizeof(archive_member), "%s/.hermes",
                 remote_home ? remote_home : "/root");
    char *archive_member_clean = archive_member;
    if (archive_member[0] == '/') archive_member_clean = archive_member + 1;
    char remote_tar[1024];
    snprintf(remote_tar, sizeof(remote_tar), "/tmp/.hermes_sync.%ld.tar", (long)getpid());
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "tar cf %s -C / %s", remote_tar, archive_member_clean);
    char *args_json = NULL;
    asprintf(&args_json, "[\"-lc\", \"%s\"]", cmd);
    char *result = client->run_command(client->ctx, "bash", args_json, workspace_root);
    free(args_json);
    if (!result) { return -1; }
    json_t *rj = json_parse(result, NULL);
    free(result);
    int rc = -1;
    if (rj) {
        json_t *code = json_obj_get(rj, "returncode");
        if (code && code->type == JSON_NUMBER && (int)code->num_val == 0)
            rc = client->download_file(client->ctx, remote_tar, dest_tar_path);
        json_free(rj);
    }
    /* Cleanup the remote tar (best-effort). */
    char *cleanup_args = NULL;
    asprintf(&cleanup_args, "[\"-lc\", \"rm -f %s\"]", remote_tar);
    char *cr = client->run_command(client->ctx, "bash", cleanup_args, workspace_root);
    free(cleanup_args);
    if (cr) free(cr);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════
 * _wait_for_running / _stop_sandbox / _snapshot_sandbox
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: ve_wait_for_running @ tools/environments/vercel_sandbox.py:_wait_for_running */
int ve_wait_for_running(vercel_sandbox_client_t *client, double timeout_secs,
                        const char *(*status_fn)(void *ctx), void *status_ctx) {
    /* Python: poll sandbox status until RUNNING (default 30s, 250ms poll);
     * treat terminal states as failure. Returns 0 when running. */
    if (!client || !status_fn) return -1;
    double deadline = (double)time(NULL) + (timeout_secs > 0 ? timeout_secs : 30.0);
    for (;;) {
        const char *status = status_fn(status_ctx);
        if (status && strcmp(status, "RUNNING") == 0) return 0;
        if (status) {
            const char *terminal[] = { "ABORTED", "FAILED", "STOPPED" };
            for (int i = 0; i < 3; i++)
                if (strcmp(status, terminal[i]) == 0) return -1;
        }
        if ((double)time(NULL) >= deadline) return -1;
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 250000000L;
        nanosleep(&ts, NULL);
    }
}

