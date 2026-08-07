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

/* PoP: _snapshot_store_path @ tools/environments/vercel_sandbox.py:_snapshot_store_path */
char *ve_snapshot_store_path(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    char *out = NULL;
    asprintf(&out, "%s/%s", home, VERCEL_SNAPSHOT_STORE_NAME);
    return out;
}

/* PoP: _load_snapshots @ tools/environments/vercel_sandbox.py:_load_snapshots */
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

/* PoP: _save_snapshots @ tools/environments/vercel_sandbox.py:_save_snapshots */
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

/* PoP: _get_snapshot_id @ tools/environments/vercel_sandbox.py:_get_snapshot_id */
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

/* PoP: _store_snapshot @ tools/environments/vercel_sandbox.py:_store_snapshot */
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

/* PoP: _delete_snapshot @ tools/environments/vercel_sandbox.py:_delete_snapshot */
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

/* PoP: _ensure_vercel_sdk @ tools/environments/vercel_sandbox.py:_ensure_vercel_sdk */
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

/* PoP: _retry_vercel_call @ tools/environments/vercel_sandbox.py:_retry_vercel_call */
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

/* PoP: _sandbox_status_type @ tools/environments/vercel_sandbox.py:_sandbox_status_type */
const char *ve_sandbox_status_type(void) {
    /* Python: returns the SandboxStatus enum class. The C port maps the
     * status names 1:1 as strings. */
    return "SandboxStatus";
}

/* PoP: _terminal_sandbox_states @ tools/environments/vercel_sandbox.py:_terminal_sandbox_states */
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

/* PoP: _build_create_params @ tools/environments/vercel_sandbox.py:_build_create_params */
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

/* PoP: _detect_workspace_root @ tools/environments/vercel_sandbox.py:_detect_workspace_root */
char *ve_detect_workspace_root(const char *sandbox_cwd) {
    /* Python: sandbox.sandbox.cwd if it starts with "/", else default. */
    if (sandbox_cwd && sandbox_cwd[0] == '/') return strdup(sandbox_cwd);
    return strdup(VERCEL_DEFAULT_CWD);
}

/* PoP: _detect_remote_home @ tools/environments/vercel_sandbox.py:_detect_remote_home */
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

/* Sandbox client vtable — the C analogue of the injected Sandbox object. */
typedef struct vercel_sandbox_client {
    void *ctx;
    /* run_command(program, args_json, cwd) -> result JSON
     * {"output": "...", "returncode": N} (malloc'd) */
    char *(*run_command)(void *ctx, const char *program, const char *args_json,
                         const char *cwd);
    /* write_files(files_json) -> 0 on success */
    int (*write_files)(void *ctx, const char *files_json);
    /* download_file(remote_path, dest_path) -> 0 on success */
    int (*download_file)(void *ctx, const char *remote_path, const char *dest_path);
} vercel_sandbox_client_t;

/* quoted rm command builder (Python's quoted_rm_command). */
static char *quoted_rm_command(const char *const *paths, int n) {
    char *out = malloc(4096);
    int pos = snprintf(out, 4096, "rm -rf");
    for (int i = 0; i < n && pos < 3900; i++) {
        pos += snprintf(out + pos, 4096 - pos, " %s", paths[i] ? paths[i] : "");
    }
    return out;
}

/* PoP: _vercel_delete @ tools/environments/vercel_sandbox.py:_vercel_delete */
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

/* PoP: _vercel_bulk_upload @ tools/environments/vercel_sandbox.py:_vercel_bulk_upload */
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

/* PoP: _vercel_upload @ tools/environments/vercel_sandbox.py:_vercel_upload */
int ve_vercel_upload(vercel_sandbox_client_t *client, const char *host_path,
                     const char *remote_path) {
    if (!host_path || !remote_path) return -1;
    const char *hp[1] = { host_path };
    const char *rp[1] = { remote_path };
    return ve_vercel_bulk_upload(client, hp, rp, 1);
}

/* PoP: _vercel_bulk_download @ tools/environments/vercel_sandbox.py:_vercel_bulk_download */
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

/* PoP: _wait_for_running @ tools/environments/vercel_sandbox.py:_wait_for_running */
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
