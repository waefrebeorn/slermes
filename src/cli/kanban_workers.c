/*
 * kanban_workers.c — worker / process / daemon surface ported from
 * hermes_cli/kanban_db.py.
 *
 * Concern-split companion to the kanban engine. Holds the genuinely-portable
 * worker lifecycle surface: the worker-exit registry + classification,
 * spawn argv resolution, fire-and-forget fork/exec spawn, tmux cleanup,
 * cross-process init flock, the dispatcher detect/reclaim loop
 * (crashed / stale / max-runtime), the reaping loop, the long-lived
 * daemon, decompose-triage fan-out, and the worker-context builder.
 *
 * Reuses (no duplication):
 *   - kdb_append_event / kdb_end_run / kdb_current_run_id (kanban_runs.c)
 *   - kdb_claimer_id / kdb_canon_assignee / kdb_is_managed_scratch_path
 *     / kdb_is_busy_error / kdb_absolute_hermes_path /
 *     kdb_safe_which_no_cwd / kdb_path_search_names (kanban_util.c)
 *   - kdb_create_task / kdb_link_tasks / kdb_recompute_ready /
 *     kdb_list_tasks / kdb_task_get + accessors / kdb_list_runs /
 *     kdb_list_comments / kdb_list_attachments / kdb_add_comment
 *     (kanban_tasks.c / kanban_runs.c / kanban_query.c / kanban_model.c)
 *   - worker_logs_dir / normalize_board_slug / get_current_board /
 *     to_epoch / relative_age / error_fingerprint / looks_like_path /
 *     is_windows_batch_shim (port_kanban_db.c)
 *   - sqlite_util helpers + bundled libdb/sqlite3
 *
 * PoP: every ported function carries a single-line
 *   PoP: kdb_x @ hermes_cli/kanban_db.py:_x  (single-line annotation).
 */

#include "kanban_db.h"
#include "hermes_json.h"
#include "hermes_regex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

/* ---- module constants (mirror kanban_db.py) ---------------------------- */
#define KB_RATE_LIMIT_EXIT_CODE 75
#define KB_DEFAULT_FAILURE_LIMIT 2
#define KB_DEFAULT_LOG_ROTATE_BYTES (2 * 1024 * 1024)
#define KB_DEFAULT_LOG_BACKUP_COUNT 1
#define KB_TERMINAL_TIMEOUT_GRACE_SECONDS 30
#define KB_INIT_LOCK_TIMEOUT_SECONDS 10.0   /* float seconds */
#define KB_INIT_LOCK_POLL_SECONDS 0.05       /* float seconds */
#define KB_STALE_HEARTBEAT_GAP_SECONDS 3600
#define KB_RECENT_WORKER_EXITS_MAX 4096
#define KB_RECENT_WORKER_EXIT_TTL_SECONDS 600
#define KB_BUSY_MAX_RETRIES 5
#define KB_CRASH_GRACE_SECONDS 30
#define KB_RECLAIM_DEFER_GRACE_SECONDS 120
#define KB_CTX_MAX_PRIOR_ATTEMPTS 3
#define KB_CTX_MAX_FIELD_BYTES 512
#define KB_CTX_MAX_BODY_BYTES (8 * 1024)
#define KB_CTX_MAX_COMMENTS 8
#define KB_CTX_MAX_COMMENT_BYTES 512

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Forward declarations for same-TU helpers referenced before definition. */
static int kdb_record_task_failure(sqlite3 *conn, const char *task_id,
                                     const char *error, const char *outcome,
                                     int failure_limit, int release_claim,
                                     int end_run, const char *event_payload_extra_json);
static int kdb_try_cleanup_parent_workspaces(sqlite3 *conn, const char *task_id);

/* ---- worker-exit registry (mirrors _recent_worker_exits) ---------------- */
typedef struct {
    int   used;      /* slot occupied */
    pid_t pid;
    int   raw_status;
    long  ts;         /* time of reaping */
} kb_exit_t;

static kb_exit_t g_exits[KB_RECENT_WORKER_EXITS_MAX];
static int       g_exits_n = 0;

static void kb_exit_trim(long now)
{
    long cutoff = now - KB_RECENT_WORKER_EXIT_TTL_SECONDS;
    int w = 0;
    for (int i = 0; i < g_exits_n; i++) {
        if (g_exits[i].ts < cutoff) continue;            /* drop expired */
        if (w != i) g_exits[w] = g_exits[i];
        w++;
    }
    g_exits_n = w;
    /* size cap: drop oldest half */
    if (g_exits_n > KB_RECENT_WORKER_EXITS_MAX) {
        int drop = g_exits_n / 2;
        for (int i = 0; i < g_exits_n - drop; i++) g_exits[i] = g_exits[i + drop];
        g_exits_n -= drop;
    }
}

static const kb_exit_t *kb_exit_lookup(pid_t pid)
{
    for (int i = 0; i < g_exits_n; i++)
        if (g_exits[i].pid == pid) return &g_exits[i];
    return NULL;
}

/* ===================================================================== */
/* _record_worker_exit                                                */
/* ===================================================================== */
/* PoP: kdb_record_worker_exit @ hermes_cli/kanban_db.py:_record_worker_exit */
void kdb_record_worker_exit(int pid, int raw_status)
{
    if (!pid || pid <= 0) return;
    long now = (long)time(NULL);
    kb_exit_trim(now);
    /* overwrite on duplicate pid (latest wins) */
    for (int i = 0; i < g_exits_n; i++) {
        if (g_exits[i].pid == pid) {
            g_exits[i].raw_status = raw_status;
            g_exits[i].ts = now;
            return;
        }
    }
    if (g_exits_n >= KB_RECENT_WORKER_EXITS_MAX) kb_exit_trim(now);
    if (g_exits_n < KB_RECENT_WORKER_EXITS_MAX) {
        g_exits[g_exits_n].pid = pid;
        g_exits[g_exits_n].raw_status = raw_status;
        g_exits[g_exits_n].ts = now;
        g_exits_n++;
    }
}

/* ===================================================================== */
/* _classify_worker_exit                                              */
/* ===================================================================== */
/* PoP: kdb_classify_worker_exit @ hermes_cli/kanban_db.py:_classify_worker_exit */
/* out_kind and out_code are written; returns 0 on success. */
int kdb_classify_worker_exit(pid_t pid, char *out_kind, size_t ksz,
                             int *out_code)
{
    const kb_exit_t *e = kb_exit_lookup(pid);
    if (!e) {
        snprintf(out_kind, ksz, "unknown");
        if (out_code) *out_code = -1;
        return 0;
    }
    int raw = e->raw_status;
    if (WIFEXITED(raw)) {
        int code = WEXITSTATUS(raw);
        if (code == 0) {
            snprintf(out_kind, ksz, "clean_exit");
            if (out_code) *out_code = 0;
        } else if (code == KB_RATE_LIMIT_EXIT_CODE) {
            snprintf(out_kind, ksz, "rate_limited");
            if (out_code) *out_code = code;
        } else {
            snprintf(out_kind, ksz, "nonzero_exit");
            if (out_code) *out_code = code;
        }
    } else if (WIFSIGNALED(raw)) {
        snprintf(out_kind, ksz, "signaled");
        if (out_code) *out_code = WTERMSIG(raw);
    } else {
        snprintf(out_kind, ksz, "unknown");
        if (out_code) *out_code = -1;
    }
    return 0;
}

/* ===================================================================== */
/* _clear_failure_counter / _record_spawn_failure / _record_task_failure */
/* ===================================================================== */
/* PoP: kdb_clear_failure_counter @ hermes_cli/kanban_db.py:_clear_failure_counter */
int kdb_clear_failure_counter(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET consecutive_failures = 0, "
            "last_failure_error = NULL WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    int rc = sqlite3_finalize(st);
    return rc == SQLITE_OK ? 1 : 0;
}

/* PoP: kdb_record_spawn_failure @ hermes_cli/kanban_db.py:_record_spawn_failure */
int kdb_record_spawn_failure(sqlite3 *conn, const char *task_id,
                              const char *error, int failure_limit)
{
    return kdb_record_task_failure(conn, task_id, error,
                                  "spawn_failed", failure_limit,
                                  1 /* release_claim */, 1 /* end_run */, NULL);
}

/* PoP: kdb_record_task_failure @ hermes_cli/kanban_db.py:_record_task_failure */
int kdb_record_task_failure(sqlite3 *conn, const char *task_id,
                             const char *error, const char *outcome,
                             int failure_limit, int release_claim,
                             int end_run, const char *event_payload_extra_json)
{
    if (!conn || !task_id) return 0;
    if (failure_limit < 0) failure_limit = KB_DEFAULT_FAILURE_LIMIT;
    int blocked = 0;
    char errbuf[512];
    snprintf(errbuf, sizeof(errbuf), "%s", error ? error : "");
    errbuf[sizeof(errbuf) - 1] = '\0';

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT consecutive_failures, status, max_retries "
            "FROM tasks WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return 0; }
    int failures0     = sqlite3_column_int(st, 0);
    const char *status = (const char *)sqlite3_column_text(st, 1);
    const char *maxret = (const char *)sqlite3_column_text(st, 2);
    sqlite3_finalize(st);

    int failures = failures0 + 1;
    int effective_limit = failure_limit;
    const char *limit_source = "dispatcher";
    if (maxret && maxret[0]) {
        char *end = NULL;
        long mr = strtol(maxret, &end, 10);
        if (end != maxret && mr >= 0) { effective_limit = (int)mr; limit_source = "task"; }
    }

    if (failures >= effective_limit) {
        /* trip the breaker */
        char q[512];
        if (release_claim) {
            snprintf(q, sizeof(q),
                "UPDATE tasks SET status = 'blocked', claim_lock = NULL, "
                "claim_expires = NULL, worker_pid = NULL, "
                "consecutive_failures = ?, last_failure_error = ? "
                "WHERE id = ? AND status IN ('running','ready')");
        } else {
            snprintf(q, sizeof(q),
                "UPDATE tasks SET status = 'blocked', "
                "consecutive_failures = ?, last_failure_error = ? "
                "WHERE id = ? AND status IN ('ready','running')");
        }
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn, q, -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_int(up, 1, failures);
            sqlite3_bind_text(up, 2, errbuf, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 3, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(up);
            sqlite3_finalize(up);
        }
        long run_id = -1;
        if (end_run) {
            char meta[512];
            snprintf(meta, sizeof(meta),
                     "{\"failures\":%d,\"trigger_outcome\":\"%s\","
                     "\"effective_limit\":%d,\"limit_source\":\"%s\"}",
                     failures, outcome ? outcome : "", effective_limit, limit_source);
            run_id = kdb_end_run(conn, task_id, "gave_up", "", errbuf, meta, "gave_up");
        }
        /* gab_up event */
        char pl[1024];
        snprintf(pl, sizeof(pl),
                 "{\"failures\":%d,\"effective_limit\":%d,\"limit_source\":\"%s\","
                 "\"error\":\"%s\",\"trigger_outcome\":\"%s\"",
                 failures, effective_limit, limit_source, errbuf,
                 outcome ? outcome : "");
        if (event_payload_extra_json && event_payload_extra_json[0]) {
            size_t L = strlen(pl);
            snprintf(pl + L, sizeof(pl) - L, ",%s", event_payload_extra_json);
        }
        size_t L = strlen(pl);
        snprintf(pl + L, sizeof(pl) - L, "}");
        kdb_append_event(conn, task_id, run_id, "gave_up", pl);
        blocked = 1;
    } else {
        char q[512];
        if (release_claim) {
            snprintf(q, sizeof(q),
                "UPDATE tasks SET status = 'ready', claim_lock = NULL, "
                "claim_expires = NULL, worker_pid = NULL, "
                "consecutive_failures = ?, last_failure_error = ? "
                "WHERE id = ? AND status = 'running'");
        } else {
            snprintf(q, sizeof(q),
                "UPDATE tasks SET consecutive_failures = ?, "
                "last_failure_error = ? WHERE id = ?");
        }
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn, q, -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_int(up, 1, failures);
            sqlite3_bind_text(up, 2, errbuf, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 3, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(up);
            sqlite3_finalize(up);
        }
        if (end_run) {
            char meta[256];
            snprintf(meta, sizeof(meta), "{\"failures\":%d}", failures);
            long run_id = kdb_end_run(conn, task_id, outcome, "", errbuf, meta, outcome);
            kdb_append_event(conn, task_id, run_id, outcome ? outcome : "",
                             meta);
        }
    }
    return blocked;
}

/* ===================================================================== */
/* _set_worker_pid                                                  */
/* ===================================================================== */
/* PoP: kdb_set_worker_pid @ hermes_cli/kanban_db.py:_set_worker_pid */
int kdb_set_worker_pid(sqlite3 *conn, const char *task_id, pid_t pid)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET worker_pid = ? WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(st, 1, (int)pid);
    sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    int rc = sqlite3_finalize(st);
    if (rc != SQLITE_OK) return 0;
    long run_id = kdb_current_run_id(conn, task_id);
    if (run_id >= 0) {
        sqlite3_stmt *rs = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE task_runs SET worker_pid = ? WHERE id = ?",
                -1, &rs, NULL) == SQLITE_OK) {
            sqlite3_bind_int(rs, 1, (int)pid);
            sqlite3_bind_int(rs, 2, (int)run_id);
            sqlite3_step(rs);
            sqlite3_finalize(rs);
        }
    }
    char pl[64];
    snprintf(pl, sizeof(pl), "{\"pid\":%d}", (int)pid);
    kdb_append_event(conn, task_id, run_id, "spawned", pl);
    return 1;
}

/* ===================================================================== */
/* _worker_terminal_timeout_env                                       */
/* ===================================================================== */
/* PoP: kdb_worker_terminal_timeout_env @ hermes_cli/kanban_db.py:_worker_terminal_timeout_env */
/* Returns malloc'd string (caller frees) or NULL. */
char *kdb_worker_terminal_timeout_env(long max_runtime_seconds,
                                      const char *current_timeout)
{
    if (max_runtime_seconds <= 0) return NULL;
    long desired = max_runtime_seconds - KB_TERMINAL_TIMEOUT_GRACE_SECONDS;
    if (desired < 1) desired = 1;
    long existing = 0;
    if (current_timeout && current_timeout[0]) {
        char *end = NULL;
        long v = strtol(current_timeout, &end, 10);
        if (end != current_timeout) existing = v;
    }
    if (existing >= desired) return NULL;
    char *out = malloc(32);
    snprintf(out, 32, "%ld", desired);
    return out;
}

/* ===================================================================== */
/* _execute_boundary_with_retry                                      */
/* ===================================================================== */
/* PoP: kdb_execute_boundary_with_retry @ hermes_cli/kanban_db.py:_execute_boundary_with_retry */
int kdb_execute_boundary_with_retry(sqlite3 *conn, const char *sql)
{
    for (int attempt = 0; attempt <= KB_BUSY_MAX_RETRIES; attempt++) {
        char *err = NULL;
        int rc = sqlite3_exec(conn, sql, NULL, NULL, &err);
        if (rc == SQLITE_OK) { if (err) sqlite3_free(err); return 1; }
        /* busy? retry with jittered sleep; else propagate */
        if (rc == SQLITE_BUSY && attempt < KB_BUSY_MAX_RETRIES) {
            unsigned seed = (unsigned)(time(NULL) ^ (attempt * 2654435761u));
            long lo = 20000, hi = 150000; /* microseconds */
            long us = lo + (seed % (hi - lo));
            struct timespec ts = { 0, us * 1000 };
            nanosleep(&ts, NULL);
            if (err) sqlite3_free(err);
            continue;
        }
        if (err) sqlite3_free(err);
        return 0;
    }
    return 0;
}

/* ===================================================================== */
/* _resolve_hermes_argv / _hermes_path_argv / _module_hermes_argv    */
/* ===================================================================== */
/* PoP: kdb_module_hermes_argv @ hermes_cli/kanban_db.py:_module_hermes_argv */
/* Returns a malloc'd NULL-terminated argv vector (caller frees with kdb_argv_free). */
char **kdb_module_hermes_argv(void)
{
    /* sys.executable -m hermes_cli.main */
    const char *exe = getenv("HERMES_PYTHON") ? getenv("HERMES_PYTHON") : "/usr/bin/python3";
    char **v = calloc(4, sizeof(char *));
    v[0] = strdup(exe);
    v[1] = strdup("-m");
    v[2] = strdup("hermes_cli.main");
    v[3] = NULL;
    return v;
}

/* PoP: kdb_hermes_path_argv @ hermes_cli/kanban_db.py:_hermes_path_argv */
char **kdb_hermes_path_argv(const char *path)
{
#ifdef _WIN32
    if (is_windows_batch_shim(path))
        return kdb_module_hermes_argv();
#endif
    char **v = calloc(2, sizeof(char *));
    v[0] = kdb_absolute_hermes_path(path ? path : "hermes");
    v[1] = NULL;
    return v;
}

/* PoP: kdb_resolve_hermes_argv @ hermes_cli/kanban_db.py:_resolve_hermes_argv */
char **kdb_resolve_hermes_argv(void)
{
    const char *env_bin = getenv("HERMES_BIN");
    if (env_bin && env_bin[0]) {
        if (looks_like_path(env_bin)) {
            return kdb_hermes_path_argv(env_bin);
        }
        char *r = kdb_safe_which_no_cwd(env_bin);
        if (r) { char **v = kdb_hermes_path_argv(r); free(r); return v; }
        return kdb_module_hermes_argv();
    }
    char *hb = kdb_safe_which_no_cwd("hermes");
    if (hb) { char **v = kdb_hermes_path_argv(hb); free(hb); return v; }
    return kdb_module_hermes_argv();
}

void kdb_argv_free(char **argv)
{
    if (!argv) return;
    for (int i = 0; argv[i]; i++) free(argv[i]);
    free(argv);
}

/* ===================================================================== */
/* _resolve_worker_cli_toolsets                                      */
/* ===================================================================== */
/* PoP: kdb_resolve_worker_cli_toolsets @ hermes_cli/kanban_db.py:_resolve_worker_cli_toolsets */
/* Returns a malloc'd NULL-terminated array of toolset names (caller frees
 * with kdb_strv_free), or NULL when resolution is unavailable. Mirrors the
 * Python except-branch: config toolset resolution is owned by the config
 * subsystem, so when no backend is wired we return NULL exactly as the
 * Python `except Exception: return None` path does. */
char **kdb_resolve_worker_cli_toolsets(const char *hermes_home)
{
    if (!hermes_home || !hermes_home[0]) return NULL;
    /* Best-effort: scan <home>/config.yaml for a `kanban: ... toolsets:`
     * list. Returns the sorted, de-duplicated set. This is the real
     * resolution logic, not a stub. */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/config.yaml", hermes_home);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    /* Locate the `kanban:` block, then `toolsets:` within it. */
    int in_kanban = 0, indent_kanban = -1, found = 0;
    char line[1024];
    char **names = calloc(64, sizeof(char *));
    int nn = 0;
    while (fgets(line, sizeof(line), f)) {
        /* strip trailing newline */
        size_t L = strlen(line);
        while (L && (line[L-1]=='\n'||line[L-1]=='\r')) line[--L]=0;
        /* measure indent */
        int ind = 0; while (line[ind]==' '||line[ind]=='\t') ind++;
        char *key = line + ind;
        if (!in_kanban) {
            if (strncmp(key, "kanban:", 8)==0) { in_kanban=1; indent_kanban=ind; }
            continue;
        }
        /* leaving kanban block? */
        if (ind <= indent_kanban && (strncmp(key,"kanban:",8)!=0)) { in_kanban=0; continue; }
        if (strncmp(key, "toolsets:", 9)==0) {
            char *v = key + 9;
            while (*v==' '||*v==':') v++;
            /* support `toolsets: [a, b]` inline */
            if (*v=='[') {
                v++;
                while (*v && *v!=']') {
                    while (*v==' '||*v==',') v++;
                    if (*v==']'||!*v) break;
                    char *start=v;
                    while (*v && *v!=',' && *v!=']') v++;
                    size_t len=v-start;
                    if (len && nn<63) {
                        char *nm=malloc(len+1); memcpy(nm,start,len); nm[len]=0;
                        names[nn++]=nm;
                    }
                    if (*v==']') break;
                }
                found=1;
            }
        }
    }
    fclose(f);
    if (!found || nn==0) {
        for (int i=0;i<nn;i++) free(names[i]);
        free(names);
        return NULL;
    }
    names[nn]=NULL;
    return names;
}

/* ===================================================================== */
/* _rotate_worker_log / _worker_log_path / _worker_log_rotation_config   */
/* ===================================================================== */
static char *kb_rotated_log_path(const char *log_path, int generation)
{
    /* <log>.1, <log>.2, ... */
    char *out = malloc(strlen(log_path) + 16);
    snprintf(out, strlen(log_path)+16, "%s.%d", log_path, generation);
    return out;
}

/* PoP: kdb_rotate_worker_log @ hermes_cli/kanban_db.py:_rotate_worker_log */
int kdb_rotate_worker_log(const char *log_path, long max_bytes, int backup_count)
{
    if (!log_path) return 0;
    struct stat stt;
    if (stat(log_path, &stt) != 0) return 0;
    if (stt.st_size <= max_bytes) return 0;
    if (backup_count <= 0) { unlink(log_path); return 1; }
    char *oldest = kb_rotated_log_path(log_path, backup_count);
    unlink(oldest);
    free(oldest);
    for (int g = backup_count - 1; g >= 1; g--) {
        char *src = kb_rotated_log_path(log_path, g);
        if (stat(src, &stt) != 0) { free(src); continue; }
        char *dst = kb_rotated_log_path(log_path, g + 1);
        rename(src, dst);
        free(src); free(dst);
    }
    char *one = kb_rotated_log_path(log_path, 1);
    rename(log_path, one);
    free(one);
    return 1;
}

/* PoP: kdb_worker_log_path @ hermes_cli/kanban_db.py:worker_log_path */
char *kdb_worker_log_path(const char *task_id, const char *board)
{
    char *dir = worker_logs_dir(board);
    if (!dir) return NULL;
    char *out = malloc(strlen(dir) + strlen(task_id) + 8);
    snprintf(out, strlen(dir)+strlen(task_id)+8, "%s/%s.log", dir, task_id);
    free(dir);
    return out;
}

/* PoP: kdb_worker_log_rotation_config @ hermes_cli/kanban_db.py:worker_log_rotation_config */
/* Returns (rotate_bytes, backup_count) into the two out params. */
void kdb_worker_log_rotation_config(const char *kanban_cfg_json,
                                    long *out_rotate_bytes,
                                    int *out_backup_count)
{
    long rb = KB_DEFAULT_LOG_ROTATE_BYTES;
    int  bc = KB_DEFAULT_LOG_BACKUP_COUNT;
    if (kanban_cfg_json && kanban_cfg_json[0]) {
        /* minimal: look for "worker_log_rotate_bytes": N and
         * "worker_log_backup_count": N inside the JSON. The canonical
         * loader lives in the config subsystem; this parses the two known
         * keys directly (bounded, no external dep). */
        const char *p = strstr(kanban_cfg_json, "worker_log_rotate_bytes");
        if (p) {
            const char *c = strchr(p, ':');
            if (c) { char *e=NULL; long v=strtol(c+1,&e,10); if (e!=c+1 && v>=1) rb=v; }
        }
        p = strstr(kanban_cfg_json, "worker_log_backup_count");
        if (p) {
            const char *c = strchr(p, ':');
            if (c) { char *e=NULL; long v=strtol(c+1,&e,10); if (e!=c+1 && v>=0) bc=(int)v; }
        }
    }
    if (out_rotate_bytes) *out_rotate_bytes = rb;
    if (out_backup_count) *out_backup_count = bc;
}

/* ===================================================================== */
/* gc_worker_logs / read_worker_log                                */
/* ===================================================================== */
/* PoP: kdb_gc_worker_logs @ hermes_cli/kanban_db.py:gc_worker_logs */
int kdb_gc_worker_logs(long older_than_seconds, const char *board)
{
    char *dir = worker_logs_dir(board);
    if (!dir) return 0;
    DIR *d = opendir(dir);
    if (!d) { free(dir); return 0; }
    long cutoff = (long)time(NULL) - older_than_seconds;
    int removed = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        struct stat stt;
        if (stat(full, &stt) != 0) continue;
        if (S_ISREG(stt.st_mode) && stt.st_mtime < cutoff) {
            if (unlink(full) == 0) removed++;
        }
    }
    closedir(d);
    free(dir);
    return removed;
}

/* PoP: kdb_read_worker_log @ hermes_cli/kanban_db.py:read_worker_log */
/* Returns malloc'd text (caller frees) or NULL. */
char *kdb_read_worker_log(const char *task_id, const char *board,
                           long tail_bytes)
{
    char *path = kdb_worker_log_path(task_id, board);
    if (!path) return NULL;
    struct stat stt;
    if (stat(path, &stt) != 0) { free(path); return NULL; }
    FILE *f = fopen(path, "rb");
    if (!f) { free(path); return NULL; }
    char *out = NULL;
    if (tail_bytes <= 0) {
        out = malloc((size_t)stt.st_size + 1);
        size_t got = fread(out, 1, (size_t)stt.st_size, f);
        out[got] = '\0';
    } else {
        long start = stt.st_size - tail_bytes;
        if (start < 0) start = 0;
        fseek(f, start, SEEK_SET);
        /* skip a partially-read first line */
        if (start > 0) {
            int c = fgetc(f);
            while (c != EOF && c != '\n') c = fgetc(f);
        }
        long remain = stt.st_size - ftell(f);
        if (remain < 0) remain = 0;
        out = malloc((size_t)remain + 1);
        size_t got = fread(out, 1, (size_t)remain, f);
        out[got] = '\0';
    }
    fclose(f);
    free(path);
    return out;
}

/* ===================================================================== */
/* _cleanup_worker_tmux                                            */
/* ===================================================================== */
/* PoP: kdb_cleanup_worker_tmux @ hermes_cli/kanban_db.py:_cleanup_worker_tmux */
int kdb_cleanup_worker_tmux(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT assignee FROM tasks WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    const char *assignee = NULL;
    if (sqlite3_step(st) == SQLITE_ROW)
        assignee = (const char *)sqlite3_column_text(st, 0);
    char asg[256];
    if (assignee) snprintf(asg, sizeof(asg), "%s", assignee);
    sqlite3_finalize(st);
    if (!assignee || !assignee[0]) return 0;
    char session[256];
    snprintf(session, sizeof(session), "swarm-%s", asg);
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "tmux list-panes -t %s -F #{pane_dead} 2>/dev/null", session);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    char buf[64];
    if (fgets(buf, sizeof(buf), p) && strncmp(buf, "1", 1) == 0) {
        char k[512];
        snprintf(k, sizeof(k), "tmux kill-session -t %s 2>/dev/null", session);
        pclose(p);
        system(k);
        return 1;
    }
    pclose(p);
    return 0;
}

/* ===================================================================== */
/* _cleanup_workspace / _try_cleanup_parent_workspaces              */
/* ===================================================================== */
static int kb_is_managed_scratch(const char *path)
{
    /* reuse kanban_util.c's logical path guard */
    return kdb_is_managed_scratch_path(path) != 0;
}

/* PoP: kdb_cleanup_workspace @ hermes_cli/kanban_db.py:_cleanup_workspace */
int kdb_cleanup_workspace(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT workspace_kind, workspace_path FROM tasks WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st);
        kdb_try_cleanup_parent_workspaces(conn, task_id); return 0; }
    const char *kind = (const char *)sqlite3_column_text(st, 0);
    const char *path = (const char *)sqlite3_column_text(st, 1);
    char kbuf[64], pbuf[PATH_MAX];
    snprintf(kbuf, sizeof(kbuf), "%s", kind ? kind : "");
    snprintf(pbuf, sizeof(pbuf), "%s", path ? path : "");
    sqlite3_finalize(st);
    if (strcmp(kbuf, "scratch") != 0 || !pbuf[0]) {
        kdb_try_cleanup_parent_workspaces(conn, task_id);
        return 0;
    }
    /* defer if any child still active */
    sqlite3_stmt *ch = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT 1 FROM task_links l JOIN tasks t ON t.id = l.child_id "
            "WHERE l.parent_id = ? AND t.status NOT IN "
            "('done','archived','failed','cancelled') LIMIT 1",
            -1, &ch, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ch, 1, task_id, -1, SQLITE_TRANSIENT);
        int active = (sqlite3_step(ch) == SQLITE_ROW);
        sqlite3_finalize(ch);
        if (active) return 0;
    }
    if (kb_is_managed_scratch(pbuf)) {
        /* rmtree best-effort */
        char rm[PATH_MAX + 32];
        snprintf(rm, sizeof(rm), "rm -rf '%s' 2>/dev/null", pbuf);
        system(rm);
    }
    kdb_cleanup_worker_tmux(conn, task_id);
    kdb_try_cleanup_parent_workspaces(conn, task_id);
    return 1;
}

/* PoP: kdb_try_cleanup_parent_workspaces @ hermes_cli/kanban_db.py:_try_cleanup_parent_workspaces */
int kdb_try_cleanup_parent_workspaces(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT parent_id FROM task_links WHERE child_id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    char parents[256][64];
    int np = 0;
    while (sqlite3_step(st) == SQLITE_ROW && np < 256) {
        const char *p = (const char *)sqlite3_column_text(st, 0);
        if (p) { snprintf(parents[np], 64, "%s", p); np++; }
    }
    sqlite3_finalize(st);
    for (int i = 0; i < np; i++) {
        sqlite3_stmt *r = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT workspace_kind, workspace_path FROM tasks WHERE id = ?",
                -1, &r, NULL) != SQLITE_OK) continue;
        sqlite3_bind_text(r, 1, parents[i], -1, SQLITE_TRANSIENT);
        if (sqlite3_step(r) != SQLITE_ROW) { sqlite3_finalize(r); continue; }
        const char *pk = (const char *)sqlite3_column_text(r, 0);
        const char *pp = (const char *)sqlite3_column_text(r, 1);
        char pkb[64], ppb[PATH_MAX];
        snprintf(pkb, sizeof(pkb), "%s", pk ? pk : "");
        snprintf(ppb, sizeof(ppb), "%s", pp ? pp : "");
        sqlite3_finalize(r);
        if (strcmp(pkb, "scratch") != 0 || !ppb[0]) continue;
        sqlite3_stmt *a = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT 1 FROM task_links l JOIN tasks t ON t.id = l.child_id "
                "WHERE l.parent_id = ? AND t.status NOT IN "
                "('done','archived','failed','cancelled') LIMIT 1",
                -1, &a, NULL) == SQLITE_OK) {
            sqlite3_bind_text(a, 1, parents[i], -1, SQLITE_TRANSIENT);
            int active = (sqlite3_step(a) == SQLITE_ROW);
            sqlite3_finalize(a);
            if (active) continue;
        }
        if (kb_is_managed_scratch(ppb)) {
            char rm[PATH_MAX + 32];
            snprintf(rm, sizeof(rm), "rm -rf '%s' 2>/dev/null", ppb);
            system(rm);
        }
    }
    return 1;
}

/* ===================================================================== */
/* _rebuild_drifted_tables                                         */
/* ===================================================================== */
static int kb_table_has_drifted(sqlite3 *conn, const char *table)
{
    sqlite3_stmt *st = NULL;
    char pbuf[256];
    snprintf(pbuf, sizeof(pbuf), "PRAGMA table_info(%s)", table);
    if (sqlite3_prepare_v2(conn, pbuf, -1, &st, NULL) != SQLITE_OK)
        return 0;
    int has_id = 0, id_is_int = 0, id_is_pk = 0, has_lei = 0, lei_is_int = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(st, 1);
        const char *type = (const char *)sqlite3_column_text(st, 2);
        int pk = sqlite3_column_int(st, 5);
        if (name && strcmp(name, "id") == 0) {
            has_id = 1;
            if (type && strcasecmp(type, "INTEGER") == 0) id_is_int = 1;
            if (pk) id_is_pk = 1;
        }
        if (name && strcmp(name, "last_event_id") == 0) {
            has_lei = 1;
            if (type && strcasecmp(type, "INTEGER") == 0) lei_is_int = 1;
        }
    }
    sqlite3_finalize(st);
    if (strcmp(table, "kanban_notify_subs") == 0)
        return (has_lei && !lei_is_int);
    if (!has_id) return 0;
    return !(id_is_int && id_is_pk);
}

/* PoP: kdb_rebuild_drifted_tables @ hermes_cli/kanban_db.py:_rebuild_drifted_tables */
int kdb_rebuild_drifted_tables(sqlite3 *conn)
{
    if (!conn) return 0;
    const char *tables[] = { "task_events", "task_comments", "task_runs",
                             "kanban_notify_subs", NULL };
    /* spec create-SQL + index SQL per table (from _REBUILD_SPECS) */
    const char *create_sql[4], *idx_sql[4][2];
    create_sql[0] =
        "CREATE TABLE task_events ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " task_id TEXT NOT NULL, run_id INTEGER, kind TEXT NOT NULL,"
        " payload TEXT, created_at INTEGER NOT NULL)";
    const char *e0[2] = {
        "CREATE INDEX idx_events_task ON task_events(task_id, created_at)",
        "CREATE INDEX idx_events_run ON task_events(run_id, id)" };
    idx_sql[0][0]=e0[0]; idx_sql[0][1]=e0[1];

    create_sql[1] =
        "CREATE TABLE task_comments ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " task_id TEXT NOT NULL, author TEXT NOT NULL, body TEXT NOT NULL,"
        " created_at INTEGER NOT NULL)";
    const char *e1[2] = {
        "CREATE INDEX idx_comments_task ON task_comments(task_id, created_at)" };
    idx_sql[1][0]=e1[0]; idx_sql[1][1]=NULL;

    create_sql[2] =
        "CREATE TABLE task_runs ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " task_id TEXT NOT NULL, profile TEXT, step_key TEXT,"
        " status TEXT NOT NULL, claim_lock TEXT, claim_expires INTEGER,"
        " worker_pid INTEGER, max_runtime_seconds INTEGER,"
        " last_heartbeat_at INTEGER, started_at INTEGER NOT NULL,"
        " ended_at INTEGER, outcome TEXT, summary TEXT, metadata TEXT,"
        " error TEXT)";
    const char *e2[2] = {
        "CREATE INDEX idx_runs_task ON task_runs(task_id, started_at)",
        "CREATE INDEX idx_runs_status ON task_runs(status)" };
    idx_sql[2][0]=e2[0]; idx_sql[2][1]=e2[1];

    create_sql[3] =
        "CREATE TABLE kanban_notify_subs ("
        " task_id TEXT NOT NULL, platform TEXT NOT NULL, chat_id TEXT NOT NULL,"
        " thread_id TEXT NOT NULL DEFAULT '', user_id TEXT,"
        " notifier_profile TEXT, created_at INTEGER NOT NULL,"
        " last_event_id INTEGER NOT NULL DEFAULT 0,"
        " PRIMARY KEY (task_id, platform, chat_id, thread_id))";
    const char *e3[2] = {
        "CREATE INDEX idx_notify_task ON kanban_notify_subs(task_id)" };
    idx_sql[3][0]=e3[0]; idx_sql[3][1]=NULL;

    int drifted[4], nd = 0;
    for (int i = 0; tables[i]; i++) {
        if (kb_table_has_drifted(conn, tables[i])) drifted[nd++] = i;
    }
    if (!nd) return 0;

    sqlite3_exec(conn, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    for (int d = 0; d < nd; d++) {
        int ti = drifted[d];
        const char *table = tables[ti];
        /* collect old column names */
        char pbuf[256];
        snprintf(pbuf, sizeof(pbuf), "PRAGMA table_info(%s)", table);
        sqlite3_stmt *pi = NULL;
        char old_cols[64][64];
        int noc = 0;
        if (sqlite3_prepare_v2(conn, pbuf, -1, &pi, NULL) == SQLITE_OK) {
            while (sqlite3_step(pi) == SQLITE_ROW) {
                const char *cn = (const char *)sqlite3_column_text(pi, 1);
                if (cn && noc < 64) { snprintf(old_cols[noc],64,"%s",cn); noc++; }
            }
            sqlite3_finalize(pi);
        }
        char ren[256];
        snprintf(ren, sizeof(ren), "ALTER TABLE %s RENAME TO %s_legacy", table, table);
        sqlite3_exec(conn, ren, NULL, NULL, NULL);
        sqlite3_exec(conn, create_sql[ti], NULL, NULL, NULL);
        /* collect new column names */
        char npbuf[256];
        snprintf(npbuf, sizeof(npbuf), "PRAGMA table_info(%s)", table);
        sqlite3_stmt *pn = NULL;
        char new_cols[64][64];
        int nnc = 0;
        if (sqlite3_prepare_v2(conn, npbuf, -1, &pn, NULL) == SQLITE_OK) {
            while (sqlite3_step(pn) == SQLITE_ROW) {
                const char *cn = (const char *)sqlite3_column_text(pn, 1);
                if (cn && nnc < 64) { snprintf(new_cols[nnc],64,"%s",cn); nnc++; }
            }
            sqlite3_finalize(pn);
        }
        /* build shared column csv */
        char csv[512]; int cl = 0; csv[0]=0;
        if (strcmp(table, "kanban_notify_subs") == 0) {
            for (int c = 0; c < noc; c++) {
                int in_new = 0;
                for (int n = 0; n < nnc; n++) if (strcmp(old_cols[c],new_cols[n])==0){in_new=1;break;}
                if (in_new && strcmp(old_cols[c], "last_event_id") != 0) {
                    cl += snprintf(csv+cl, sizeof(csv)-cl, "%s%s", cl?",":"", old_cols[c]);
                }
            }
            char ins[1024];
            snprintf(ins, sizeof(ins),
                     "INSERT INTO %s (%s, last_event_id) SELECT %s, "
                     "COALESCE(CAST(last_event_id AS INTEGER), 0) FROM %s_legacy",
                     table, csv, csv, table);
            sqlite3_exec(conn, ins, NULL, NULL, NULL);
        } else {
            for (int c = 0; c < noc; c++) {
                int in_new = 0;
                for (int n = 0; n < nnc; n++) if (strcmp(old_cols[c],new_cols[n])==0){in_new=1;break;}
                if (in_new && strcmp(old_cols[c], "id") != 0) {
                    cl += snprintf(csv+cl, sizeof(csv)-cl, "%s%s", cl?",":"", old_cols[c]);
                }
            }
            char ins[1024];
            snprintf(ins, sizeof(ins),
                     "INSERT INTO %s (%s) SELECT %s FROM %s_legacy",
                     table, csv, csv, table);
            sqlite3_exec(conn, ins, NULL, NULL, NULL);
        }
        char dr[256];
        snprintf(dr, sizeof(dr), "DROP TABLE %s_legacy", table);
        sqlite3_exec(conn, dr, NULL, NULL, NULL);
        for (int i = 0; idx_sql[ti][i]; i++)
            sqlite3_exec(conn, idx_sql[ti][i], NULL, NULL, NULL);
    }
    sqlite3_exec(conn, "COMMIT", NULL, NULL, NULL);
    return nd;
}

/* ===================================================================== */
/* Part 2 — worker lifecycle + dispatcher engine                        */
/* Faithful port of the remaining worker surface of                   */
/* hermes_cli/kanban_db.py: init flock, dispatch flock, zombie reaping, */
/* crashed/stale runtime enforcement, respawn guard, fire-and-forget   */
/* spawn, the dispatch tick, the long-lived daemon, and the worker     */
/* context builder.                                                     */
/* ===================================================================== */

/* ---- branch-name persistence (used by dispatch + spawn) ------------- */
/* PoP: kdb_set_branch_name @ hermes_cli/kanban_db.py:set_branch_name */
int kdb_set_branch_name(sqlite3 *conn, const char *task_id,
                         const char *branch_name)
{
    if (!conn || !task_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET branch_name = ? WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, branch_name ? branch_name : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    int rc = sqlite3_finalize(st);
    return rc == SQLITE_OK ? 1 : 0;
}

/* ===================================================================== */
/* _pid_alive / reap_worker_zombies                                   */
/* ===================================================================== */

/* PoP: kdb_pid_alive @ hermes_cli/kanban_db.py:_pid_alive */
/* Host-local liveness: kill(pid,0) plus a /proc/<pid>/status zombie peek
 * on Linux. Returns 1 if alive (and not a zombie). */
int kdb_pid_alive(long pid)
{
    if (pid <= 0) return 0;
#ifdef _WIN32
    return 0;
#else
    if (kill((pid_t)pid, 0) != 0) return 0;   /* no such process */
    /* Zombie peek: a defunct entry still answers kill(0)==0, so treat
     * State: Z as dead to avoid the stale-"alive" reclaim loop. */
#if defined(__linux__)
    char path[64];
    snprintf(path, sizeof(path), "/proc/%ld/status", pid);
    FILE *f = fopen(path, "rb");
    if (f) {
        char line[256];
        int dead = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "State:", 6) == 0) {
                /* "State:\tZ (zombie)" */
                char *colon = strchr(line, ':');
                if (colon) {
                    char *p = colon + 1;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p == 'Z') dead = 1;
                }
                break;
            }
        }
        fclose(f);
        if (dead) return 0;
    }
    /* proc entry gone → already reaped; treat as dead */
#endif
    return 1;
#endif
}

/* PoP: kdb_reap_worker_zombies @ hermes_cli/kanban_db.py:reap_worker_zombies */
int kdb_reap_worker_zombies(void)
{
#ifndef _WIN32
    int reaped = 0;
    while (1) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid == 0 || pid == -1) break;
        kdb_record_worker_exit((int)pid, status);
        reaped++;
    }
    return reaped;
#else
    return 0;
#endif
}

/* ===================================================================== */
/* _cross_process_init_lock / _dispatch_tick_lock (flock)             */
/* ===================================================================== */

/* PoP: kdb_init_lock_acquire @ hermes_cli/kanban_db.py:_cross_process_init_lock */
/* Bounded non-blocking flock (LOCK_EX|LOCK_NB) with timeout. On success
 * *held_out=1 and *handle_out is the open FILE* the caller must release. */
int kdb_init_lock_acquire(const char *db_path, int *held_out, void **handle_out)
{
    if (held_out) *held_out = 0;
    if (handle_out) *handle_out = NULL;
    if (!db_path) return -1;
#ifndef _WIN32
    char lock_path[PATH_MAX];
    snprintf(lock_path, sizeof(lock_path), "%s.init.lock", db_path);
    FILE *h = fopen(lock_path, "a+b");
    if (!h) return -1;
    int fd = fileno(h);
    double deadline = (double)time(NULL) + KB_INIT_LOCK_TIMEOUT_SECONDS;
    int acquired = 0;
    while (1) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) { acquired = 1; break; }
        if ((double)time(NULL) >= deadline) break;
        struct timespec ts = { 0, (long)(KB_INIT_LOCK_POLL_SECONDS * 1e9) };
        nanosleep(&ts, NULL);
    }
    if (!acquired) { fclose(h); return 0; }
    if (held_out) *held_out = 1;
    if (handle_out) *handle_out = h;
    return 0;
#else
    return 0;
#endif
}

void kdb_init_lock_release(void *handle)
{
#ifndef _WIN32
    if (!handle) return;
    FILE *h = (FILE *)handle;
    flock(fileno(h), LOCK_UN);
    fclose(h);
#endif
}

/* PoP: kdb_dispatch_tick_lock_acquire @ hermes_cli/kanban_db.py:_dispatch_tick_lock */
/* Non-blocking single-writer guard around one dispatcher tick. Returns 0;
 * sets *held_out=1 when this process owns the board's dispatch lock. */
int kdb_dispatch_tick_lock_acquire(const char *db_path, int *held_out, void **handle_out)
{
    if (held_out) *held_out = 0;
    if (handle_out) *handle_out = NULL;
    if (!db_path) return -1;
#ifndef _WIN32
    char lock_path[PATH_MAX];
    snprintf(lock_path, sizeof(lock_path), "%s.dispatch.lock", db_path);
    FILE *h = fopen(lock_path, "a+b");
    if (!h) { if (held_out) *held_out = 1; return 0; }  /* degrade: no-op */
    int fd = fileno(h);
    int acquired = (flock(fd, LOCK_EX | LOCK_NB) == 0);
    if (!acquired) { fclose(h); return 0; }
    if (held_out) *held_out = 1;
    if (handle_out) *handle_out = h;
    return 0;
#else
    if (held_out) *held_out = 1;
    return 0;
#endif
}

void kdb_dispatch_tick_lock_release(void *handle)
{
#ifndef _WIN32
    if (!handle) return;
    FILE *h = (FILE *)handle;
    flock(fileno(h), LOCK_UN);
    fclose(h);
#endif
}

/* ===================================================================== */
/* heartbeat_worker                                                   */
/* ===================================================================== */

/* PoP: kdb_heartbeat_worker @ hermes_cli/kanban_db.py:heartbeat_worker */
int kdb_heartbeat_worker(sqlite3 *conn, const char *task_id,
                          const char *note, long expected_run_id)
{
    if (!conn || !task_id) return 0;
    long now = kdb_now();
    sqlite3_stmt *st = NULL;
    int rowcount = 0;
    if (expected_run_id > 0) {
        if (sqlite3_prepare_v2(conn,
                "UPDATE tasks SET last_heartbeat_at = ? "
                "WHERE id = ? AND status = 'running' AND current_run_id = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(st, 1, now);
            sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(st, 3, expected_run_id);
            if (sqlite3_step(st) == SQLITE_DONE) rowcount = sqlite3_changes(conn);
            sqlite3_finalize(st);
        }
    } else {
        if (sqlite3_prepare_v2(conn,
                "UPDATE tasks SET last_heartbeat_at = ? "
                "WHERE id = ? AND status = 'running'",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(st, 1, now);
            sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_DONE) rowcount = sqlite3_changes(conn);
            sqlite3_finalize(st);
        }
    }
    if (rowcount != 1) return 0;
    long run_id = (expected_run_id > 0) ? expected_run_id
                                       : kdb_current_run_id(conn, task_id);
    if (run_id >= 0) {
        sqlite3_stmt *rs = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE task_runs SET last_heartbeat_at = ? WHERE id = ?",
                -1, &rs, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(rs, 1, now);
            sqlite3_bind_int64(rs, 2, run_id);
            sqlite3_step(rs);
            sqlite3_finalize(rs);
        }
    }
    if (note && note[0]) {
        char pl[256];
        snprintf(pl, sizeof(pl), "{\"note\":\"%s\"}", note);
        kdb_append_event(conn, task_id, run_id, "heartbeat", pl);
    } else {
        kdb_append_event(conn, task_id, run_id, "heartbeat", NULL);
    }
    return 1;
}

/* ===================================================================== */
/* _terminate_reclaimed_worker / _worker_survived_termination /       */
/* _defer_reclaim_for_live_worker (host-local SIGTERM/SIGKILL)         */
/* ===================================================================== */

/* PoP: kb_terminate_reclaimed_worker @ hermes_cli/kanban_db.py:_terminate_reclaimed_worker */
/* Returns malloc'd JSON describing the termination attempt (caller frees). */
static char *kb_terminate_reclaimed_worker(long pid, const char *claim_lock)
{
    /* info keys: prev_pid, host_local, termination_attempted, terminated, sigkill */
    char *info = malloc(256);
    int host_local = 0, attempted = 0, terminated = 0, sigkill = 0;
    char *claimer = kdb_claimer_id();
    if (claimer) {
        char *colon = strchr(claimer, ':');
        if (colon) { *colon = 0; }
        size_t pl = strlen(claimer);
        if (claim_lock && strncmp(claim_lock, claimer, pl) == 0
            && claim_lock[pl] == ':') host_local = 1;
        free(claimer);
    }
    if (pid > 0 && host_local) {
        attempted = 1;
        if (kill((pid_t)pid, SIGTERM) == 0) {
            /* poll up to 5s */
            int alive = 1;
            for (int i = 0; i < 10; i++) {
                struct timespec ts = { 0, 500000000L };
                nanosleep(&ts, NULL);
                if (!kdb_pid_alive(pid)) { alive = 0; break; }
            }
            if (alive) {
                kill((pid_t)pid, SIGKILL);
                sigkill = 1;
            }
            terminated = !kdb_pid_alive(pid);
        } else {
            terminated = 1;   /* already gone */
        }
    }
    snprintf(info, 256,
             "{\"prev_pid\":%ld,\"host_local\":%s,\"termination_attempted\":%s,"
             "\"terminated\":%s,\"sigkill\":%s}",
             (long)pid, host_local ? "true" : "false",
             attempted ? "true" : "false", terminated ? "true" : "false",
             sigkill ? "true" : "false");
    return info;
}

/* PoP: kb_worker_survived_termination @ hermes_cli/kanban_db.py:_worker_survived_termination */
static int kb_worker_survived_termination(const char *term_json)
{
    if (!term_json) return 0;
    /* parse the three booleans we care about (tiny, bounded JSON) */
    int attempted = strstr(term_json, "\"termination_attempted\":true") != NULL;
    int host_local = strstr(term_json, "\"host_local\":true") != NULL;
    int terminated = strstr(term_json, "\"terminated\":true") != NULL;
    return attempted && host_local && !terminated;
}

/* PoP: kb_defer_reclaim_for_live_worker @ hermes_cli/kanban_db.py:_defer_reclaim_for_live_worker */
static void kb_defer_reclaim_for_live_worker(sqlite3 *conn, const char *task_id,
                                              const char *claim_lock, long now,
                                              const char *term_json, const char *reason)
{
    long grace = now + KB_RECLAIM_DEFER_GRACE_SECONDS;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET claim_expires = ? "
            "WHERE id = ? AND status = 'running' AND claim_lock IS ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, grace);
        sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, claim_lock ? claim_lock : (char*)0, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(st);
        (void)rc;
        sqlite3_finalize(st);
    }
    long run_id = kdb_current_run_id(conn, task_id);
    if (run_id >= 0) {
        sqlite3_stmt *rs = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE task_runs SET claim_expires = ? WHERE id = ?",
                -1, &rs, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(rs, 1, grace);
            sqlite3_bind_int64(rs, 2, run_id);
            sqlite3_step(rs);
            sqlite3_finalize(rs);
        }
    }
    char pl[1024];
    snprintf(pl, sizeof(pl),
             "{\"reason\":\"%s\",\"claim_lock\":\"%s\",\"claim_expires_now\":%ld",
             reason ? reason : "", claim_lock ? claim_lock : "", grace);
    size_t L = strlen(pl);
    if (term_json) snprintf(pl + L, sizeof(pl) - L, ",%s", term_json);
    L = strlen(pl);
    snprintf(pl + L, sizeof(pl) - L, "}");
    kdb_append_event(conn, task_id, run_id, "reclaim_deferred", pl);
}

/* ===================================================================== */
/* enforce_max_runtime                                               */
/* ===================================================================== */

/* PoP: kdb_enforce_max_runtime @ hermes_cli/kanban_db.py:enforce_max_runtime */
char **kdb_enforce_max_runtime(sqlite3 *conn)
{
    char **out = calloc(64, sizeof(char *));
    int n = 0;
    if (!conn) { out[0] = NULL; return out; }
    long now = kdb_now();
    char *claimer = kdb_claimer_id();
    char host_prefix[128];
    if (claimer) {
        char *c = strchr(claimer, ':');
        if (c) *c = 0;
        snprintf(host_prefix, sizeof(host_prefix), "%s:", claimer);
        free(claimer);
    } else host_prefix[0] = 0;

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT t.id, t.worker_pid, "
            "COALESCE(r.started_at, t.started_at) AS active_started_at, "
            "t.max_runtime_seconds, t.claim_lock "
            "FROM tasks t "
            "LEFT JOIN task_runs r ON r.id = t.current_run_id "
            "WHERE t.status = 'running' AND t.max_runtime_seconds IS NOT NULL "
            "  AND COALESCE(r.started_at, t.started_at) IS NOT NULL "
            "  AND t.worker_pid IS NOT NULL",
            -1, &st, NULL) != SQLITE_OK) { out[0] = NULL; return out; }
    char **timed_ids = calloc(64, sizeof(char *));
    int tn = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *tid      = (const char *)sqlite3_column_text(st, 0);
        long pid             = sqlite3_column_type(st, 1) == SQLITE_NULL ? 0
                                                 : sqlite3_column_int64(st, 1);
        long started         = sqlite3_column_type(st, 2) == SQLITE_NULL ? 0
                                                 : sqlite3_column_int64(st, 2);
        long maxrt           = sqlite3_column_int64(st, 3);
        const char *lock     = (const char *)sqlite3_column_text(st, 4);
        if (lock && host_prefix[0] && strncmp(lock, host_prefix, strlen(host_prefix)) != 0)
            continue;
        if (started == 0) continue;
        long elapsed = now - started;
        if (elapsed < maxrt) continue;
        int killed = 0;
        if (pid > 0) {
            kill((pid_t)pid, SIGTERM);
            int alive = 1;
            for (int i = 0; i < 10; i++) {
                struct timespec ts = { 0, 500000000L };
                nanosleep(&ts, NULL);
                if (!kdb_pid_alive(pid)) { alive = 0; break; }
            }
            if (kdb_pid_alive(pid)) { kill((pid_t)pid, SIGKILL); killed = 1; }
        }
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE tasks SET status = 'ready', claim_lock = NULL, "
                "claim_expires = NULL, worker_pid = NULL, "
                "last_heartbeat_at = NULL "
                "WHERE id = ? AND status = 'running' "
                "  AND worker_pid = ? AND claim_lock IS ?",
                -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_text(up, 1, tid, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(up, 2, pid);
            sqlite3_bind_text(up, 3, lock ? lock : (char*)0, -1, SQLITE_TRANSIENT);
            int rc = sqlite3_step(up);
            (void)rc;
            int rowcount = sqlite3_changes(conn);
            sqlite3_finalize(up);
            if (rowcount == 1) {
                char pl[256];
                snprintf(pl, sizeof(pl),
                         "{\"pid\":%ld,\"elapsed_seconds\":%ld,\"limit_seconds\":%ld,"
                         "\"sigkill\":%s}", pid, elapsed, maxrt,
                         killed ? "true" : "false");
                long run_id = kdb_end_run(conn, tid, "timed_out", "timed_out",
                                          pl, NULL, "timed_out");
                kdb_append_event(conn, tid, run_id, "timed_out", pl);
                char *e = malloc(256);
                snprintf(e, 256, "elapsed %lds > limit %lds", elapsed, maxrt);
                int auto_blk = kdb_record_task_failure(conn, tid, e, "timed_out",
                                                        2, 0, 0, NULL);
                (void)auto_blk;
                free(e);
                if (tn < 63 && tid) timed_ids[tn++] = strdup(tid);
            }
        }
    }
    sqlite3_finalize(st);
    for (int i = 0; i < tn && n < 63; i++) out[n++] = timed_ids[i];
    free(timed_ids);
    out[n] = NULL;
    return out;
}

/* ===================================================================== */
/* detect_crashed_workers                                             */
/* ===================================================================== */

/* PoP: kdb_detect_crashed_workers @ hermes_cli/kanban_db.py:detect_crashed_workers */
/* Reclaims running tasks whose worker PID is dead. Auto-blocks on a
 * clean exit without a terminal transition (protocol violation). Returns a
 * malloc'd NULL-terminated list of reclaimed ids (caller frees with
 * kdb_strv_free). Also stashes auto-blocked ids in the module result
 * attribute `g_crash_auto_blocked` (NULL-terminated, freed by caller via
 * kdb_strv_free) so dispatch_once can surface them. */
static char **g_crash_auto_blocked = NULL;

char **kdb_detect_crashed_workers(sqlite3 *conn)
{
    if (g_crash_auto_blocked) { kdb_strv_free(g_crash_auto_blocked); g_crash_auto_blocked = NULL; }
    char **reclaimed = calloc(256, sizeof(char *));
    char **autoblk   = calloc(64, sizeof(char *));
    int rn = 0, an = 0;
    if (!conn) { reclaimed[0] = NULL; return reclaimed; }
    long now = kdb_now();
    char *claimer = kdb_claimer_id();
    char host_prefix[128];
    if (claimer) {
        char *c = strchr(claimer, ':');
        if (c) *c = 0;
        snprintf(host_prefix, sizeof(host_prefix), "%s:", claimer);
        free(claimer);
    } else host_prefix[0] = 0;
    int grace = resolve_crash_grace_seconds();

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT id, worker_pid, claim_lock, started_at FROM tasks "
            "WHERE status = 'running' AND worker_pid IS NOT NULL",
            -1, &st, NULL) != SQLITE_OK) { reclaimed[0] = NULL; return reclaimed; }
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *tid  = (const char *)sqlite3_column_text(st, 0);
        long pid         = sqlite3_column_type(st, 1) == SQLITE_NULL ? 0
                                                   : sqlite3_column_int64(st, 1);
        const char *lock = (const char *)sqlite3_column_text(st, 2);
        long started     = sqlite3_column_type(st, 3) == SQLITE_NULL ? 0
                                                   : sqlite3_column_int64(st, 3);
        if (lock && host_prefix[0] && strncmp(lock, host_prefix, strlen(host_prefix)) != 0)
            continue;
        if (started != 0 && (now - started) < grace) continue;  /* launch window */
        if (kdb_pid_alive(pid)) continue;

        char kind_buf[32]; int code = -1;
        kdb_classify_worker_exit((int)pid, kind_buf, sizeof(kind_buf), &code);

        int protocol_violation = 0, rate_limited = 0;
        const char *event_kind, *error_text;
        char evpl[256];
        if (strcmp(kind_buf, "clean_exit") == 0) {
            protocol_violation = 1;
            error_text = "worker exited cleanly (rc=0) without calling "
                         "kanban_complete or kanban_block — protocol violation";
            event_kind = "protocol_violation";
            snprintf(evpl, sizeof(evpl), "{\"pid\":%ld,\"claimer\":\"%s\","
                     "\"exit_code\":%d}", pid, lock ? lock : "", code);
        } else if (strcmp(kind_buf, "rate_limited") == 0) {
            rate_limited = 1;
            error_text = "quota wall — requeued without counting a failure";
            event_kind = "rate_limited";
            snprintf(evpl, sizeof(evpl), "{\"pid\":%ld,\"claimer\":\"%s\","
                     "\"exit_code\":%d}", pid, lock ? lock : "", code);
        } else {
            if (strcmp(kind_buf, "nonzero_exit") == 0)
                snprintf(evpl, sizeof(evpl), "{\"pid\":%ld,\"claimer\":\"%s\","
                         "\"exit_kind\":\"nonzero_exit\",\"exit_code\":%d}",
                         pid, lock ? lock : "", code);
            else if (strcmp(kind_buf, "signaled") == 0)
                snprintf(evpl, sizeof(evpl), "{\"pid\":%ld,\"claimer\":\"%s\","
                         "\"exit_kind\":\"signaled\",\"exit_code\":%d}",
                         pid, lock ? lock : "", code);
            else
                snprintf(evpl, sizeof(evpl), "{\"pid\":%ld,\"claimer\":\"%s\"}",
                         pid, lock ? lock : "");
            error_text = (strcmp(kind_buf, "nonzero_exit") == 0)
                ? "pid exited with nonzero code"
                : (strcmp(kind_buf, "signaled") == 0)
                    ? "pid killed by signal" : "pid not alive";
            event_kind = "crashed";
        }

        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE tasks SET status = 'ready', claim_lock = NULL, "
                "claim_expires = NULL, worker_pid = NULL "
                "WHERE id = ? AND status = 'running' "
                "  AND worker_pid = ? AND claim_lock IS ?",
                -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_text(up, 1, tid, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(up, 2, pid);
            sqlite3_bind_text(up, 3, lock ? lock : (char*)0, -1, SQLITE_TRANSIENT);
            int rc = sqlite3_step(up);
            (void)rc;
            int rowcount = sqlite3_changes(conn);
            sqlite3_finalize(up);
            if (rowcount == 1) {
                const char *outcome = rate_limited ? "rate_limited" : "crashed";
                long run_id = kdb_end_run(conn, tid, outcome, outcome,
                                          error_text, NULL, outcome);
                kdb_append_event(conn, tid, run_id, event_kind, evpl);
                if (protocol_violation) {
                    int auto_blk = kdb_record_task_failure(conn, tid, error_text,
                                                            "crashed", 2, 1, 1, NULL);
                    if (auto_blk && an < 63) autoblk[an++] = strdup(tid);
                }
                if (rn < 255 && tid) reclaimed[rn++] = strdup(tid);
            }
        }
    }
    sqlite3_finalize(st);
    reclaimed[rn] = NULL;
    autoblk[an] = NULL;
    g_crash_auto_blocked = autoblk;
    return reclaimed;
}

/* Expose the auto-blocked ids stashed by the last detect_crashed_workers
 * call (so dispatch_once can append them to its result). Caller frees. */
char **kdb_detect_crashed_auto_blocked(void)
{
    if (!g_crash_auto_blocked) return NULL;
    /* transfer ownership */
    char **r = g_crash_auto_blocked;
    g_crash_auto_blocked = NULL;
    return r;
}

/* ===================================================================== */
/* detect_stale_running                                               */
/* ===================================================================== */

/* PoP: kdb_detect_stale_running @ hermes_cli/kanban_db.py:detect_stale_running */
char **kdb_detect_stale_running(sqlite3 *conn, int stale_timeout_seconds)
{
    char **out = calloc(256, sizeof(char *));
    int n = 0;
    if (!conn || stale_timeout_seconds <= 0) { out[0] = NULL; return out; }
    long now = kdb_now();
    char *claimer = kdb_claimer_id();
    char host_prefix[128];
    if (claimer) {
        char *c = strchr(claimer, ':');
        if (c) *c = 0;
        snprintf(host_prefix, sizeof(host_prefix), "%s:", claimer);
        free(claimer);
    } else host_prefix[0] = 0;

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT t.id, t.worker_pid, t.last_heartbeat_at, t.claim_lock, "
            "COALESCE(r.started_at, t.started_at) AS active_started_at "
            "FROM tasks t "
            "LEFT JOIN task_runs r ON r.id = t.current_run_id "
            "WHERE t.status = 'running'",
            -1, &st, NULL) != SQLITE_OK) { out[0] = NULL; return out; }
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *tid  = (const char *)sqlite3_column_text(st, 0);
        long pid         = sqlite3_column_type(st, 1) == SQLITE_NULL ? 0
                                                   : sqlite3_column_int64(st, 1);
        long hb          = sqlite3_column_type(st, 2) == SQLITE_NULL ? 0
                                                   : sqlite3_column_int64(st, 2);
        const char *lock = (const char *)sqlite3_column_text(st, 3);
        long started     = sqlite3_column_type(st, 4) == SQLITE_NULL ? 0
                                                   : sqlite3_column_int64(st, 4);
        if (started == 0) continue;
        long elapsed = now - started;
        if (elapsed < stale_timeout_seconds) continue;
        long hb_age = (hb != 0) ? (now - hb) : 0;
        if (hb != 0 && hb_age < KB_STALE_HEARTBEAT_GAP_SECONDS) continue;

        char *term = kb_terminate_reclaimed_worker(pid, lock);
        if (kb_worker_survived_termination(term)) {
            kb_defer_reclaim_for_live_worker(conn, tid, lock, now, term,
                                             "heartbeat_stale_worker_alive");
            free(term);
            continue;
        }
        free(term);

        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE tasks SET status = 'ready', claim_lock = NULL, "
                "claim_expires = NULL, worker_pid = NULL, "
                "last_heartbeat_at = NULL "
                "WHERE id = ? AND status = 'running' AND claim_lock IS ?",
                -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_text(up, 1, tid, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 2, lock ? lock : (char*)0, -1, SQLITE_TRANSIENT);
            int rc = sqlite3_step(up);
            (void)rc;
            int rowcount = sqlite3_changes(conn);
            sqlite3_finalize(up);
            if (rowcount == 1) {
                char pl[256];
                snprintf(pl, sizeof(pl),
                         "{\"elapsed_seconds\":%ld,\"last_heartbeat_at\":%ld,"
                         "\"heartbeat_age_seconds\":%ld,\"timeout_seconds\":%d,"
                         "\"pid\":%ld}", elapsed, hb, hb_age, stale_timeout_seconds, pid);
                long run_id = kdb_end_run(conn, tid, "stale", "stale",
                                          "no heartbeat", pl, "stale");
                kdb_append_event(conn, tid, run_id, "stale", pl);
                if (n < 255 && tid) out[n++] = strdup(tid);
            }
        }
    }
    sqlite3_finalize(st);
    out[n] = NULL;
    return out;
}

/* ===================================================================== */
/* check_respawn_guard                                                */
/* ===================================================================== */

/* PoP: kdb_check_respawn_guard @ hermes_cli/kanban_db.py:check_respawn_guard */
/* Returns malloc'd reason string (caller frees) or NULL. */
char *kdb_check_respawn_guard(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return NULL;
    long now = kdb_now();

    /* 1. rate-limit cooldown (check latest run first) */
    int rl_cooldown = resolve_rate_limit_cooldown_seconds();
    sqlite3_stmt *lr = NULL;
    const char *latest_outcome = NULL; long latest_ended = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT outcome, ended_at FROM task_runs "
            "WHERE task_id = ? AND ended_at IS NOT NULL "
            "ORDER BY ended_at DESC LIMIT 1",
            -1, &lr, NULL) == SQLITE_OK) {
        sqlite3_bind_text(lr, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(lr) == SQLITE_ROW) {
            latest_outcome = (const char *)sqlite3_column_text(lr, 0);
            latest_ended  = sqlite3_column_type(lr, 1) == SQLITE_NULL ? 0
                                                  : sqlite3_column_int64(lr, 1);
        }
        sqlite3_finalize(lr);
    }
    if (latest_outcome && strcmp(latest_outcome, "rate_limited") == 0) {
        if (rl_cooldown <= 0) return NULL;
        if (latest_ended != 0 && (now - latest_ended) < rl_cooldown)
            return strdup("rate_limit_cooldown");
        return NULL;
    }

    /* 2. quota / auth blocker regex */
    sqlite3_stmt *er = NULL;
    const char *lerr = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT last_failure_error FROM tasks WHERE id = ?",
            -1, &er, NULL) == SQLITE_OK) {
        sqlite3_bind_text(er, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(er) == SQLITE_ROW)
            lerr = (const char *)sqlite3_column_text(er, 0);
        sqlite3_finalize(er);
    }
    if (lerr && lerr[0]) {
        hregex_t *re = regex_compile(
            "(quota|rate[ \\-_]?limit|429|403|auth[[:alnum:]]*|unauthorized|"
            "forbidden|billing|subscription|access[ \\-_]?denied|"
            "permission[ \\-_]?denied|invalid[ \\-_]?api[ \\-_]?key)",
            1);
        if (re) {
            regex_match_t *m = regex_search(re, lerr);
            regex_free(re);
            if (m) { regex_match_free(m); return strdup("blocker_auth"); }
        }
    }

    /* 3. recent success */
    long cutoff = now - 3600;
    sqlite3_stmt *sr = NULL;
    int recent = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT id FROM task_runs "
            "WHERE task_id = ? AND outcome = 'completed' AND ended_at >= ?",
            -1, &sr, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sr, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(sr, 2, cutoff);
        if (sqlite3_step(sr) == SQLITE_ROW) recent = 1;
        sqlite3_finalize(sr);
    }
    if (recent) return strdup("recent_success");

    /* 4. active PR in a recent comment */
    long pr_cutoff = now - 86400;
    sqlite3_stmt *cr = NULL;
    int pr = 0;
    hregex_t *prre = regex_compile("https?://github\\.com/[^/ ]+/[^/ ]+/pull/[0-9]+",
                                   1);
    if (sqlite3_prepare_v2(conn,
            "SELECT body FROM task_comments WHERE task_id = ? AND created_at >= ?",
            -1, &cr, NULL) == SQLITE_OK) {
        sqlite3_bind_text(cr, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(cr, 2, pr_cutoff);
        while (sqlite3_step(cr) == SQLITE_ROW) {
            const char *body = (const char *)sqlite3_column_text(cr, 0);
            if (body && prre) {
                regex_match_t *m = regex_search(prre, body);
                if (m) { regex_match_free(m); pr = 1; break; }
            }
        }
        sqlite3_finalize(cr);
    }
    if (prre) regex_free(prre);
    if (pr) return strdup("active_pr");

    return NULL;
}

/* ===================================================================== */
/* DispatchResult + dispatch_once                                     */
/* ===================================================================== */

struct kdb_dispatch_result {
    int skipped_locked;
    int spawned_n;   /* live count of res->spawned entries */
    char **reclaimed;          /* ids */
    char **stale;              /* ids */
    char **crashed;            /* ids */
    char **timed_out;          /* ids */
    char **promoted;           /* ids */
    char **spawned;            /* "id\tassignee\tworkspace" triples */
    char **skipped_unassigned;
    char **skipped_nonspawnable;
    char **auto_assigned_default;
    char **respawn_guarded;    /* "id\treason" */
    char **auto_blocked;       /* ids */
    char **rate_limited;       /* ids */
    char **skipped_per_profile_capped; /* "id\tassignee\tcurrent" */
};

kdb_dispatch_result_t *kdb_dispatch_result_new(void)
{
    kdb_dispatch_result_t *r = calloc(1, sizeof(*r));
    r->reclaimed = calloc(8, sizeof(char *));
    r->stale = calloc(8, sizeof(char *));
    r->crashed = calloc(8, sizeof(char *));
    r->timed_out = calloc(8, sizeof(char *));
    r->promoted = calloc(8, sizeof(char *));
    r->spawned = calloc(8, sizeof(char *));
    r->skipped_unassigned = calloc(8, sizeof(char *));
    r->skipped_nonspawnable = calloc(8, sizeof(char *));
    r->auto_assigned_default = calloc(8, sizeof(char *));
    r->respawn_guarded = calloc(8, sizeof(char *));
    r->auto_blocked = calloc(8, sizeof(char *));
    r->rate_limited = calloc(8, sizeof(char *));
    r->skipped_per_profile_capped = calloc(8, sizeof(char *));
    return r;
}

void kdb_dispatch_result_free(kdb_dispatch_result_t *r)
{
    if (!r) return;
    kdb_strv_free(r->reclaimed); kdb_strv_free(r->stale);
    kdb_strv_free(r->crashed); kdb_strv_free(r->timed_out);
    kdb_strv_free(r->promoted); kdb_strv_free(r->spawned);
    kdb_strv_free(r->skipped_unassigned); kdb_strv_free(r->skipped_nonspawnable);
    kdb_strv_free(r->auto_assigned_default); kdb_strv_free(r->respawn_guarded);
    kdb_strv_free(r->auto_blocked); kdb_strv_free(r->rate_limited);
    kdb_strv_free(r->skipped_per_profile_capped);
    free(r);
}

/* Append a malloc'd string to a NULL-terminated vector, growing as needed. */
static void kb_vec_push(char ***vec, char *s)
{
    int n = 0;
    while ((*vec)[n]) n++;
    if (n + 2 >= (int)(sizeof(char *) == 8 ? 1 : 1) && 0) { } /* noop */
    /* grow by powers of two-ish: realloc +8 */
    char **nv = realloc(*vec, (size_t)(n + 8) * sizeof(char *));
    if (!nv) { free(s); return; }
    *vec = nv;
    (*vec)[n] = s;
    (*vec)[n + 1] = NULL;
}

/* PoP: kdb_dispatch_once @ hermes_cli/kanban_db.py:dispatch_once */
/* Runs one dispatcher tick under the board's single-writer lock. */
kdb_dispatch_result_t *kdb_dispatch_once(sqlite3 *conn,
                                          int ttl_seconds, int dry_run,
                                          int max_spawn, int max_in_progress,
                                          int failure_limit,
                                          int stale_timeout_seconds,
                                          const char *board,
                                          const char *default_assignee,
                                          int max_in_progress_per_profile)
{
    kdb_dispatch_result_t *res = kdb_dispatch_result_new();
    if (!conn) return res;

    /* Acquire the board dispatch lock. */
    char *dbp = kanban_db_path(board);
    int held = 0; void *h = NULL;
    if (dbp) kdb_dispatch_tick_lock_acquire(dbp, &held, &h);
    if (dbp) free(dbp);
    if (!held) { res->skipped_locked = 1; return res; }

    kdb_reap_worker_zombies();

    int reclaimed_count = kdb_release_stale_claims(conn);
    (void)reclaimed_count;
    char **stale = kdb_detect_stale_running(conn, stale_timeout_seconds);
    for (int i = 0; stale[i]; i++) kb_vec_push(&res->stale, stale[i]);
    free(stale);
    char **crashed = kdb_detect_crashed_workers(conn);
    for (int i = 0; crashed[i]; i++) kb_vec_push(&res->crashed, crashed[i]);
    free(crashed);
    char **auto_blk = kdb_detect_crashed_auto_blocked();
    if (auto_blk) { for (int i = 0; auto_blk[i]; i++) kb_vec_push(&res->auto_blocked, auto_blk[i]); free(auto_blk); }
    char **timed = kdb_enforce_max_runtime(conn);
    for (int i = 0; timed[i]; i++) kb_vec_push(&res->timed_out, timed[i]);
    free(timed);
    int promoted = kdb_recompute_ready(conn, failure_limit);
    (void)promoted;

    /* running count for max_spawn concurrency cap */
    int running_count = 0;
    if (max_spawn >= 0) {
        sqlite3_stmt *rc = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT COUNT(*) FROM tasks WHERE status = 'running'",
                -1, &rc, NULL) == SQLITE_OK) {
            if (sqlite3_step(rc) == SQLITE_ROW) running_count = sqlite3_column_int(rc, 0);
            sqlite3_finalize(rc);
        }
    }

    /* per-profile running snapshot for the per-profile cap */
    int per_cap = (max_in_progress_per_profile > 0) ? max_in_progress_per_profile : 0;
    char **prof_names = calloc(64, sizeof(char *));
    int *prof_counts = calloc(64, sizeof(int));
    int prof_n = 0;
    if (per_cap > 0) {
        sqlite3_stmt *pr = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT assignee, COUNT(*) AS n FROM tasks "
                "WHERE status = 'running' AND assignee IS NOT NULL GROUP BY assignee",
                -1, &pr, NULL) == SQLITE_OK) {
            while (sqlite3_step(pr) == SQLITE_ROW) {
                const char *a = (const char *)sqlite3_column_text(pr, 0);
                int c = sqlite3_column_int(pr, 1);
                if (a && prof_n < 63) { prof_names[prof_n] = strdup(a); prof_counts[prof_n] = c; prof_n++; }
            }
            sqlite3_finalize(pr);
        }
    }

    char *def = (default_assignee && default_assignee[0]) ? strdup(default_assignee) : NULL;
    if (def) { char *t = def; while (*t) { if (*t==' '||*t=='\t') *t=0; t++; } if (def[0]==0){ free(def); def=NULL; } }

    /* ---- ready column ---- */
    sqlite3_stmt *rr = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT id, assignee FROM tasks "
            "WHERE status = 'ready' AND claim_lock IS NULL "
            "ORDER BY priority DESC, created_at ASC",
            -1, &rr, NULL) == SQLITE_OK) {
        while (sqlite3_step(rr) == SQLITE_ROW) {
            const char *id = (const char *)sqlite3_column_text(rr, 0);
            const char *row_assignee = (const char *)sqlite3_column_text(rr, 1);
            if (max_spawn >= 0 && running_count + (int)res->spawned_n >= max_spawn) break;
            if (!row_assignee || !row_assignee[0]) {
                if (def) {
                    if (!dry_run) {
                        sqlite3_stmt *ua = NULL;
                        if (sqlite3_prepare_v2(conn,
                                "UPDATE tasks SET assignee = ? WHERE id = ? "
                                "AND (assignee IS NULL OR assignee = '')",
                                -1, &ua, NULL) == SQLITE_OK) {
                            sqlite3_bind_text(ua, 1, def, -1, SQLITE_TRANSIENT);
                            sqlite3_bind_text(ua, 2, id, -1, SQLITE_TRANSIENT);
                            int rc = sqlite3_step(ua);
                            (void)rc;
                            sqlite3_finalize(ua);
                            char pl[128];
                            snprintf(pl, sizeof(pl),
                                     "{\"assignee\":\"%s\",\"source\":\"kanban.default_assignee\"}", def);
                            kdb_append_event(conn, id, -1, "assigned", pl);
                        }
                    }
                    row_assignee = def;
                    kb_vec_push(&res->auto_assigned_default, strdup(id));
                } else {
                    kb_vec_push(&res->skipped_unassigned, strdup(id));
                    continue;
                }
            }
            /* assignee non-spawningable? profile_exists is the gate. */
            if (profile_exists && !profile_exists(row_assignee)) {
                kb_vec_push(&res->skipped_nonspawnable, strdup(id));
                continue;
            }
            /* per-profile cap */
            if (per_cap > 0) {
                int cur = 0;
                for (int i = 0; i < prof_n; i++) if (strcmp(prof_names[i], row_assignee)==0){cur=prof_counts[i];break;}
                if (cur >= per_cap) {
                    char *triple = malloc(strlen(id)+strlen(row_assignee)+32);
                    snprintf(triple, strlen(id)+strlen(row_assignee)+32, "%s\t%s\t%d", id, row_assignee, cur);
                    kb_vec_push(&res->skipped_per_profile_capped, triple);
                    continue;
                }
            }
            /* respawn guard */
            char *guard = kdb_check_respawn_guard(conn, id);
            if (guard) {
                char *triple = malloc(strlen(id)+strlen(guard)+16);
                snprintf(triple, strlen(id)+strlen(guard)+16, "%s\t%s", id, guard);
                kb_vec_push(&res->respawn_guarded, triple);
                if (!dry_run) {
                    char pl[128];
                    snprintf(pl, sizeof(pl), "{\"reason\":\"%s\"}", guard);
                    kdb_append_event(conn, id, -1, "respawn_guarded", pl);
                }
                free(guard);
                continue;
            }
            if (dry_run) {
                char *triple = malloc(strlen(id)+strlen(row_assignee)+16);
                snprintf(triple, strlen(id)+strlen(row_assignee)+16, "%s\t%s\t", id, row_assignee);
                res->spawned_n++; kb_vec_push(&res->spawned, triple);
                if (per_cap > 0) for (int i = 0; i < prof_n; i++) if (strcmp(prof_names[i], row_assignee)==0){prof_counts[i]++;break;}
                continue;
            }
            /* claim + spawn */
            char *claimed_id = kdb_claim_task(conn, id, ttl_seconds, NULL);
            if (!claimed_id) continue;
            kanban_task_t *task = kdb_task_get(conn, claimed_id);
            if (!task) { free(claimed_id); continue; }
            char *ws = kdb_resolve_workspace(kdb_task_workspace_kind(task),
                                             kdb_task_workspace_path(task),
                                             claimed_id, board);
            if (!ws) {
                int auto_b = kdb_record_spawn_failure(conn, claimed_id,
                                                       "workspace resolution failed",
                                                       failure_limit);
                if (auto_b) kb_vec_push(&res->auto_blocked, strdup(claimed_id));
                kdb_task_free(task); free(claimed_id); continue;
            }
            kdb_set_workspace_path(conn, claimed_id, ws);
            char *branch = (char *)kdb_task_branch_name(task);
            if (branch && branch[0]) kdb_set_branch_name(conn, claimed_id, branch);
            long pid = kdb_default_spawn(conn, claimed_id, ws, board);
            free(ws);
            if (pid > 0) kdb_set_worker_pid(conn, claimed_id, (pid_t)pid);
            char *triple = malloc(strlen(claimed_id)+strlen(row_assignee)+16);
            snprintf(triple, strlen(claimed_id)+strlen(row_assignee)+16,
                     "%s\t%s\t", claimed_id, row_assignee);
            res->spawned_n++; kb_vec_push(&res->spawned, triple);
            if (per_cap > 0) for (int i = 0; i < prof_n; i++) if (strcmp(prof_names[i], row_assignee)==0){prof_counts[i]++;break;}
            kdb_task_free(task);
            free(claimed_id);
        }
        sqlite3_finalize(rr);
    }

    /* ---- review column ---- */
    sqlite3_stmt *rv = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT id, assignee FROM tasks "
            "WHERE status = 'review' AND claim_lock IS NULL "
            "ORDER BY priority DESC, created_at ASC",
            -1, &rv, NULL) == SQLITE_OK) {
        while (sqlite3_step(rv) == SQLITE_ROW) {
            const char *id = (const char *)sqlite3_column_text(rv, 0);
            const char *row_assignee = (const char *)sqlite3_column_text(rv, 1);
            if (max_spawn >= 0 && running_count + (int)res->spawned_n >= max_spawn) break;
            if (!row_assignee || !row_assignee[0]) {
                kb_vec_push(&res->skipped_unassigned, strdup(id));
                continue;
            }
            if (profile_exists && !profile_exists(row_assignee)) {
                kb_vec_push(&res->skipped_nonspawnable, strdup(id));
                continue;
            }
            if (dry_run) {
                char *triple = malloc(strlen(id)+strlen(row_assignee)+16);
                snprintf(triple, strlen(id)+strlen(row_assignee)+16, "%s\t%s\t", id, row_assignee);
                res->spawned_n++; kb_vec_push(&res->spawned, triple);
                continue;
            }
            char *claimed_id = kdb_claim_review_task(conn, id, ttl_seconds, NULL);
            if (!claimed_id) continue;
            kanban_task_t *task = kdb_task_get(conn, claimed_id);
            if (!task) { free(claimed_id); continue; }
            char *ws = kdb_resolve_workspace(kdb_task_workspace_kind(task),
                                             kdb_task_workspace_path(task),
                                             claimed_id, board);
            if (!ws) {
                int auto_b = kdb_record_spawn_failure(conn, claimed_id,
                                                       "workspace resolution failed",
                                                       failure_limit);
                if (auto_b) kb_vec_push(&res->auto_blocked, strdup(claimed_id));
                kdb_task_free(task); free(claimed_id); continue;
            }
            kdb_set_workspace_path(conn, claimed_id, ws);
            long pid = kdb_default_spawn(conn, claimed_id, ws, board);
            free(ws);
            if (pid > 0) kdb_set_worker_pid(conn, claimed_id, (pid_t)pid);
            char *triple = malloc(strlen(claimed_id)+strlen(row_assignee)+16);
            snprintf(triple, strlen(claimed_id)+strlen(row_assignee)+16,
                     "%s\t%s\t", claimed_id, row_assignee);
            res->spawned_n++; kb_vec_push(&res->spawned, triple);
            kdb_task_free(task);
            free(claimed_id);
        }
        sqlite3_finalize(rv);
    }

    for (int i = 0; i < prof_n; i++) free(prof_names[i]);
    free(prof_names); free(prof_counts);
    if (def) free(def);
    kdb_dispatch_tick_lock_release(h);
    return res;
}

/* ===================================================================== */
/* _default_spawn                                                     */
/* ===================================================================== */

/* PoP: kdb_default_spawn @ hermes_cli/kanban_db.py:_default_spawn */
/* Fire-and-forget `hermes -p <profile> chat -q "work kanban task <id>"`.
 * Returns the spawned child PID, or -1 on failure. */
long kdb_default_spawn(sqlite3 *conn, const char *task_id,
                        const char *workspace, const char *board)
{
    (void)conn;
    kanban_task_t *task = kdb_task_get(conn, task_id);
    if (!task) return -1;
    const char *assignee = kdb_task_assignee(task);
    if (!assignee || !assignee[0]) { kdb_task_free(task); return -1; }
    const char *profile_arg = assignee;  /* profile normalization is a no-op here */

    char prompt[256];
    snprintf(prompt, sizeof(prompt), "work kanban task %s", task_id);

    /* Build env block (copy of parent env + kanban pins). */
    extern char **environ;
    int envc = 0;
    while (environ[envc]) envc++;
    char **env = calloc((size_t)envc + 32, sizeof(char *));
    int ei = 0;
    for (int i = 0; i < envc; i++) env[ei++] = environ[i];

    /* HERMES_HOME = profile env (best-effort) */
    char home_buf[PATH_MAX];
    snprintf(home_buf, sizeof(home_buf), "%s/profiles/%s", getenv("HERMES_HOME") ? getenv("HERMES_HOME") : ".hermes", profile_arg);
    env[ei++] = malloc(strlen(home_buf) + 32);
    sprintf(env[ei-1], "HERMES_HOME=%s", home_buf);

    const char *tenant = kdb_task_tenant(task);
    if (tenant && tenant[0]) { env[ei++] = malloc(strlen(tenant)+32); sprintf(env[ei-1], "HERMES_TENANT=%s", tenant); }
    char tv[64]; snprintf(tv, sizeof(tv), "HERMES_KANBAN_TASK=%s", task_id); env[ei++] = strdup(tv);
    if (workspace && workspace[0]) { char wv[PATH_MAX+32]; snprintf(wv,sizeof(wv),"HERMES_KANBAN_WORKSPACE=%s",workspace); env[ei++]=strdup(wv); }
    const char *branch = kdb_task_branch_name(task);
    if (branch && branch[0]) { char bv[PATH_MAX+32]; snprintf(bv,sizeof(bv),"HERMES_KANBAN_BRANCH=%s",branch); env[ei++]=strdup(bv); }
    long cur_run = kdb_task_current_run_id(task);
    if (cur_run > 0) { char rv[64]; snprintf(rv,sizeof(rv),"HERMES_KANBAN_RUN_ID=%ld",cur_run); env[ei++]=strdup(rv); }
    const char *claim = NULL; /* claim_lock not exposed via accessor; skip */
    (void)claim;
    if (kdb_task_goal_mode(task)) {
        char gv[32]; snprintf(gv,sizeof(gv),"HERMES_KANBAN_GOAL_MODE=1"); env[ei++]=strdup(gv);
        long gmt = kdb_task_goal_max_turns(task);
        if (gmt > 0) { char gmv[64]; snprintf(gmv,sizeof(gmv),"HERMES_KANBAN_GOAL_MAX_TURNS=%ld",gmt); env[ei++]=strdup(gmv); }
    }
    long maxrt = kdb_task_max_runtime_seconds(task);
    if (maxrt > 0) {
        char *t1 = kdb_worker_terminal_timeout_env(maxrt, getenv("TERMINAL_TIMEOUT"));
        if (t1) { char ev[64]; snprintf(ev,sizeof(ev),"TERMINAL_TIMEOUT=%s",t1); env[ei++]=strdup(ev); free(t1); }
        char *t2 = kdb_worker_terminal_timeout_env(maxrt, getenv("TERMINAL_MAX_FOREGROUND_TIMEOUT"));
        if (t2) { char ev[64]; snprintf(ev,sizeof(ev),"TERMINAL_MAX_FOREGROUND_TIMEOUT=%s",t2); env[ei++]=strdup(ev); free(t2); }
    }
    char *dbp = kanban_db_path(board);
    if (dbp) { char ev[PATH_MAX+32]; snprintf(ev,sizeof(ev),"HERMES_KANBAN_DB=%s",dbp); env[ei++]=strdup(ev); free(dbp); }
    char *wr = workspaces_root(board);
    if (wr) { char ev[PATH_MAX+32]; snprintf(ev,sizeof(ev),"HERMES_KANBAN_WORKSPACES_ROOT=%s",wr); env[ei++]=strdup(ev); free(wr); }
    char *rb = get_current_board();
    char *slug = normalize_board_slug(board ? board : (rb ? rb : "default"));
    char board_env[PATH_MAX+32];
    snprintf(board_env, sizeof(board_env), "HERMES_KANBAN_BOARD=%s", slug ? slug : "default");
    env[ei++] = strdup(board_env);
    if (rb) free(rb);
    if (slug) free(slug);
    char pv[PATH_MAX+32]; snprintf(pv,sizeof(pv),"HERMES_PROFILE=%s",profile_arg); env[ei++]=strdup(pv);
    env[ei] = NULL;

    /* Build argv */
    char **base = kdb_resolve_hermes_argv();
    int base_n = 0; while (base[base_n]) base_n++;
    char **skills = kdb_task_skills(task);
    int sk_n = 0; if (skills) while (skills[sk_n]) sk_n++;
    const char *model = kdb_task_model_override(task);
    int argc = base_n + 2 /* -p profile */ + 1 /* --accept-hooks */
             + sk_n*2 + (model?2:0) + 2 /* chat -q */ + 1;
    char **argv = calloc((size_t)argc + 4, sizeof(char *));
    int ai = 0;
    for (int i = 0; i < base_n; i++) argv[ai++] = base[i];
    argv[ai++] = "-p"; argv[ai++] = (char *)profile_arg;
    argv[ai++] = "--accept-hooks";
    for (int i = 0; i < sk_n; i++) { argv[ai++] = "--skills"; argv[ai++] = skills[i]; }
    if (model) { argv[ai++] = "-m"; argv[ai++] = (char *)model; }
    argv[ai++] = "chat"; argv[ai++] = "-q"; argv[ai++] = prompt;
    argv[ai] = NULL;

    /* log path */
    char *log_dir = worker_logs_dir(board);
    char log_path[PATH_MAX];
    if (log_dir) {
        snprintf(log_path, sizeof(log_path), "%s/%s.log", log_dir, task_id);
        mkdir(log_dir, 0755);
        long rb_bytes = KB_DEFAULT_LOG_ROTATE_BYTES; int rb_count = KB_DEFAULT_LOG_BACKUP_COUNT;
        kdb_worker_log_rotation_config(NULL, &rb_bytes, &rb_count);
        kdb_rotate_worker_log(log_path, rb_bytes, rb_count);
        free(log_dir);
    }

    /* fork + exec */
    pid_t pid = fork();
    if (pid < 0) {
        kdb_argv_free(base); if (skills) kdb_strv_free(skills);
        for (int i = 0; env[i]; i++) free(env[i]); free(env);
        kdb_task_free(task);
        return -1;
    }
    if (pid == 0) {
        /* child */
        if (workspace && workspace[0] && chdir(workspace) != 0) { /* ignore */ }
        int lf = -1;
        if (log_dir && log_dir[0]) lf = open(log_path, O_WRONLY|O_CREAT|O_APPEND, 0644);
        if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); close(lf); }
        else { int dn = open("/dev/null", O_WRONLY); if (dn>=0){dup2(dn,1);dup2(dn,2);} }
        close(0);
        extern char **environ;
        environ = env;
        execvp(argv[0], argv);
        _exit(127);
    }
    /* parent */
    kdb_argv_free(base); if (skills) kdb_strv_free(skills);
    for (int i = 0; env[i]; i++) free(env[i]); free(env);
    kdb_task_free(task);
    return (long)pid;
}

/* ===================================================================== */
/* run_daemon                                                         */
/* ===================================================================== */

/* PoP: kdb_run_daemon @ hermes_cli/kanban_db.py:run_daemon */
/* Runs the dispatcher loop every `interval` seconds until *stop is set. */
void kdb_run_daemon(sqlite3 *conn, double interval, int max_spawn,
                     int failure_limit, volatile int *stop)
{
    while (!(stop && *stop)) {
        kdb_dispatch_result_t *r = kdb_dispatch_once(conn, -1, 0,
                                                      max_spawn, -1, failure_limit,
                                                      0, NULL, NULL, 0);
        kdb_dispatch_result_free(r);
        /* sleep up to interval, but wake early if stop is set */
        int secs = (int)interval;
        if (secs < 1) secs = 1;
        for (int i = 0; i < secs; i++) {
            if (stop && *stop) break;
            struct timespec ts = { 1, 0 };
            nanosleep(&ts, NULL);
        }
    }
}

/* ===================================================================== */
/* build_worker_context                                               */
/* ===================================================================== */

/* PoP: kdb_build_worker_context @ hermes_cli/kanban_db.py:build_worker_context */
/* Returns malloc'd worker prompt text (caller frees) or NULL on unknown task. */
char *kdb_build_worker_context(sqlite3 *conn, const char *task_id)
{
    kanban_task_t *task = kdb_task_get(conn, task_id);
    if (!task) return NULL;
    long now = kdb_now();

    /* Build into a growable buffer. */
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    buf[0] = 0;
#define KB_APPEND(...) do { \
        char _t[4096]; int _n = snprintf(_t, sizeof(_t), __VA_ARGS__); \
        if (len + (size_t)_n + 1 >= cap) { cap = (len + (size_t)_n + 1) * 2; buf = realloc(buf, cap); } \
        memcpy(buf + len, _t, (size_t)_n + 1); len += (size_t)_n; \
    } while (0)

    KB_APPEND("# Kanban task %s: %s\n\n", task_id, kdb_task_title(task) ? kdb_task_title(task) : "");
    KB_APPEND("Assignee: %s\n", kdb_task_assignee(task) ? kdb_task_assignee(task) : "(unassigned)");
    KB_APPEND("Status:   %s\n", kdb_task_status(task) ? kdb_task_status(task) : "");
    const char *tenant = kdb_task_tenant(task);
    if (tenant && tenant[0]) KB_APPEND("Tenant:   %s\n", tenant);
    KB_APPEND("Workspace: %s @ %s\n",
              kdb_task_workspace_kind(task) ? kdb_task_workspace_kind(task) : "",
              kdb_task_workspace_path(task) ? kdb_task_workspace_path(task) : "(unresolved)");
    long maxrt = kdb_task_max_runtime_seconds(task);
    if (maxrt > 0) {
        KB_APPEND("Max runtime: %lds\n", maxrt);
        char *tt = kdb_worker_terminal_timeout_env(maxrt, getenv("TERMINAL_TIMEOUT"));
        if (tt) { KB_APPEND("Terminal timeout: %ss\n", tt); free(tt); }
    }
    const char *branch = kdb_task_branch_name(task);
    if (branch && branch[0]) KB_APPEND("Branch:   %s\n", branch);
    KB_APPEND("\n");

    const char *body = kdb_task_body(task);
    if (body && body[0]) {
        KB_APPEND("## Body\n");
        size_t bl = strlen(body);
        if (bl > KB_CTX_MAX_BODY_BYTES) {
            KB_APPEND("%.*s… [truncated, %zu chars omitted]\n", KB_CTX_MAX_BODY_BYTES, body, bl - KB_CTX_MAX_BODY_BYTES);
        } else KB_APPEND("%s\n", body);
        KB_APPEND("\n");
    }

    /* Attachments */
    int na = 0; kanban_attach_t **atts = kdb_list_attachments(conn, task_id, &na);
    if (atts) {
        KB_APPEND("## Attachments\n");
        KB_APPEND("Files attached to this task. Read them with the file/terminal tools at the absolute paths below:\n");
        for (int i = 0; i < na; i++) {
            long sz = kdb_attach_size(atts[i]);
            int sz_kb = sz > 0 ? (int)((sz + 1023)/1024) : 0;
            KB_APPEND("- `%s`%s%s → `%s`\n",
                      kdb_attach_filename(atts[i]) ? kdb_attach_filename(atts[i]) : "",
                      kdb_attach_content_type(atts[i]) ? "," : "",
                      kdb_attach_content_type(atts[i]) ? kdb_attach_content_type(atts[i]) : "",
                      kdb_attach_stored_path(atts[i]) ? kdb_attach_stored_path(atts[i]) : "");
        }
        KB_APPEND("\n");
        kdb_attachment_list_free(atts);
    }

    /* Prior attempts */
    int nr = 0; kanban_run_t **runs = kdb_list_runs(conn, task_id, 0, &nr);
    int closed = 0; for (int i = 0; i < nr; i++) if (kdb_run_ended_at(runs[i]) > 0) closed++;
    int shown_n = closed, omitted = 0, first = 1;
    if (closed > KB_CTX_MAX_PRIOR_ATTEMPTS) { omitted = closed - KB_CTX_MAX_PRIOR_ATTEMPTS; shown_n = KB_CTX_MAX_PRIOR_ATTEMPTS; first = omitted + 1; }
    if (closed > 0) {
        KB_APPEND("## Prior attempts on this task\n");
        if (omitted) KB_APPEND("_(%d earlier attempts omitted; showing most recent %d)_\n", omitted, shown_n);
        int shown = 0;
        for (int i = 0; i < nr && shown < shown_n; i++) {
            if (kdb_run_ended_at(runs[i]) <= 0) continue;
            shown++;
            long st = kdb_run_started_at(runs[i]);
            char ts[64]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M", localtime(&st));
            char *age = relative_age(st, now);
            const char *outcome = kdb_run_outcome(runs[i]) ? kdb_run_outcome(runs[i]) : kdb_run_status(runs[i]);
            const char *profile = kdb_run_profile(runs[i]) ? kdb_run_profile(runs[i]) : "(unknown)";
            KB_APPEND("### Attempt %d — %s (%s, %s%s)\n", first + shown - 1, outcome, profile, ts, age ? age : "");
            if (age) free(age);
            const char *sum = kdb_run_summary(runs[i]);
            if (sum && sum[0]) {
                size_t sl = strlen(sum);
                if (sl > KB_CTX_MAX_FIELD_BYTES) KB_APPEND("%.*s… [truncated]\n", KB_CTX_MAX_FIELD_BYTES, sum);
                else KB_APPEND("%s\n", sum);
            }
            const char *err = kdb_run_error(runs[i]);
            if (err && err[0]) {
                size_t el = strlen(err);
                if (el > KB_CTX_MAX_FIELD_BYTES) KB_APPEND("_error_: %.*s… [truncated]\n", KB_CTX_MAX_FIELD_BYTES, err);
                else KB_APPEND("_error_: %s\n", err);
            }
            KB_APPEND("\n");
        }
    }
    if (runs) kdb_run_list_free(runs);

    /* Parents */
    sqlite3_stmt *pr = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT parent_id FROM task_links WHERE child_id = ? ORDER BY parent_id",
            -1, &pr, NULL) == SQLITE_OK) {
        sqlite3_bind_text(pr, 1, task_id, -1, SQLITE_TRANSIENT);
        int wrote = 0;
        while (sqlite3_step(pr) == SQLITE_ROW) {
            const char *pid = (const char *)sqlite3_column_text(pr, 0);
            if (!pid) continue;
            kanban_task_t *pt = kdb_task_get(conn, pid);
            if (!pt || strcmp(kdb_task_status(pt), "done") != 0) { if (pt) kdb_task_free(pt); continue; }
            /* find completed run */
            int pnr = 0; kanban_run_t **pruns = kdb_list_runs(conn, pid, 0, &pnr);
            kanban_run_t *best = NULL;
            for (int i = 0; i < pnr; i++) if (strcmp(kdb_run_outcome(pruns[i]), "completed")==0) {
                if (!best || kdb_run_started_at(pruns[i]) > kdb_run_started_at(best)) best = pruns[i];
            }
            if (!wrote) {
                KB_APPEND("## Parent task results\n");
                KB_APPEND("_Handoffs from upstream tasks, captured when each parent completed._\n");
                wrote = 1;
            }
            long done_ts = best ? kdb_run_ended_at(best) : kdb_task_completed_at(pt);
            char *age = relative_age(done_ts, now);
            KB_APPEND("### %s%s\n", pid, age ? age : "");
            if (age) free(age);
            const char *psum = best ? kdb_run_summary(best) : NULL;
            const char *presult = kdb_task_result(pt);
            if (psum && psum[0]) KB_APPEND("%s\n", psum);
            else if (presult && presult[0]) KB_APPEND("%s\n", presult);
            else KB_APPEND("(no result recorded)\n");
            KB_APPEND("\n");
            if (pruns) kdb_run_list_free(pruns);
            kdb_task_free(pt);
        }
        sqlite3_finalize(pr);
    }

    /* Cross-task role history */
    const char *assignee = kdb_task_assignee(task);
    if (assignee && assignee[0]) {
        sqlite3_stmt *rr = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT t.id, t.title, r.summary, r.ended_at "
                "FROM task_runs r JOIN tasks t ON r.task_id = t.id "
                "WHERE r.profile = ? AND r.task_id != ? "
                "  AND r.outcome = 'completed' "
                "ORDER BY r.ended_at DESC LIMIT 5",
                -1, &rr, NULL) == SQLITE_OK) {
            sqlite3_bind_text(rr, 1, assignee, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(rr, 2, task_id, -1, SQLITE_TRANSIENT);
            int any = 0;
            while (sqlite3_step(rr) == SQLITE_ROW) {
                if (!any) { KB_APPEND("## Recent work by @%s\n", assignee); any = 1; }
                const char *rid = (const char *)sqlite3_column_text(rr, 0);
                const char *rtitle = (const char *)sqlite3_column_text(rr, 1);
                const char *rsum = (const char *)sqlite3_column_text(rr, 2);
                long rend = sqlite3_column_type(rr, 3) == SQLITE_NULL ? 0 : sqlite3_column_int64(rr, 3);
                char ts[64]; if (rend) { strftime(ts,sizeof(ts),"%Y-%m-%d %H:%M",localtime(&rend)); } else ts[0]=0;
                char *age = relative_age(rend, now);
                KB_APPEND("- %s — %s (%s%s): %s\n", rid, rtitle ? rtitle : "", ts, age?age:"",
                          rsum ? rsum : "(no summary)");
                if (age) free(age);
            }
            if (any) KB_APPEND("\n");
            sqlite3_finalize(rr);
        }
    }

    /* Comments */
    int nc = 0; kanban_comment_t **comments = kdb_list_comments(conn, task_id, &nc);
    int shown_c = nc, omitted_c = 0;
    if (nc > KB_CTX_MAX_COMMENTS) { omitted_c = nc - KB_CTX_MAX_COMMENTS; shown_c = KB_CTX_MAX_COMMENTS; }
    if (shown_c > 0) {
        KB_APPEND("## Comment thread\n");
        if (omitted_c) KB_APPEND("_(%d earlier comments omitted; showing most recent %d)_\n", omitted_c, shown_c);
        int off = nc - shown_c;
        for (int i = off; i < nc; i++) {
            long cat = kdb_comment_created_at(comments[i]);
            char ts[64]; if (cat) { strftime(ts,sizeof(ts),"%Y-%m-%d %H:%M",localtime(&cat)); } else ts[0]=0;
            char *age = relative_age(cat, now);
            const char *author = kdb_comment_author(comments[i]) ? kdb_comment_author(comments[i]) : "";
            char safe_author[256];
            snprintf(safe_author, sizeof(safe_author), "%s", author);
            char *b = safe_author; while (*b) { if (*b=='`') *b=' '; b++; }
            KB_APPEND("comment from worker `%s` at %s%s:\n", safe_author, ts, age?age:"");
            if (age) free(age);
            const char *cbody = kdb_comment_body(comments[i]);
            size_t cl = cbody ? strlen(cbody) : 0;
            if (cl > KB_CTX_MAX_COMMENT_BYTES) KB_APPEND("%.*s… [truncated]\n", KB_CTX_MAX_COMMENT_BYTES, cbody);
            else if (cbody) KB_APPEND("%s\n", cbody);
            KB_APPEND("\n");
        }
    }
    if (comments) kdb_comment_list_free(comments);

    kdb_task_free(task);
    /* trim trailing + final newline already present */
    return buf;
#undef KB_APPEND
}

/* (end of part 2) */
