/*
 * port_web_server_gaps.c — Faithful C11 port of hermes_cli/web_server.py REAL_GAP functions.
 * Web server: desktop-managed gateway child tracking, session DB open/probe,
 * cron registration error mapping, serve-orphan detection, parent-death
 * watchdog, and the _demo self-check.
 */

#include "hermes_json.h"
#include <sqlite3.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "hermes_logger.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>

/* ── _ACTION_PROCS registry (desktop-managed restart children) ──────────── */

#define WS_ACTION_PROCS_MAX 8
typedef struct {
    char name[64];   /* e.g. "gateway-restart" */
    pid_t pid;
    int occupied;
} ws_action_proc_t;
static ws_action_proc_t g_action_procs[WS_ACTION_PROCS_MAX];
static pthread_mutex_t g_action_procs_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Register a spawned action child so the Desktop shutdown path can stop it. */
void ws_register_action_proc(const char *name, int pid) {
    if (!name || !name[0] || pid <= 0) return;
    pthread_mutex_lock(&g_action_procs_mutex);
    for (int i = 0; i < WS_ACTION_PROCS_MAX; i++) {
        if (g_action_procs[i].occupied &&
            strcmp(g_action_procs[i].name, name) == 0) {
            g_action_procs[i].pid = (pid_t)pid;
            pthread_mutex_unlock(&g_action_procs_mutex);
            return;
        }
    }
    for (int i = 0; i < WS_ACTION_PROCS_MAX; i++) {
        if (!g_action_procs[i].occupied) {
            snprintf(g_action_procs[i].name, sizeof(g_action_procs[i].name), "%s", name);
            g_action_procs[i].pid = (pid_t)pid;
            g_action_procs[i].occupied = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_action_procs_mutex);
}

/* PoP: ws_terminate_desktop_managed_gateway @ hermes_cli/web_server.py:_terminate_desktop_managed_gateway */
/* Stop a live gateway-restart child when its Desktop backend shuts down. The
 * child may have exited between poll() and terminate() — that race is
 * swallowed (SIGTERM to a zombie/reaped pid is harmless here). */
void ws_terminate_desktop_managed_gateway(void) {
    pid_t victim = 0;
    pthread_mutex_lock(&g_action_procs_mutex);
    for (int i = 0; i < WS_ACTION_PROCS_MAX; i++) {
        if (g_action_procs[i].occupied &&
            strcmp(g_action_procs[i].name, "gateway-restart") == 0) {
            victim = g_action_procs[i].pid;
            break;
        }
    }
    pthread_mutex_unlock(&g_action_procs_mutex);
    if (victim <= 0) return;
    /* proc.poll() is None ⇔ the child is still live. */
    char link[64];
    snprintf(link, sizeof(link), "/proc/%d", (int)victim);
    if (access(link, F_OK) == 0)
        kill(victim, SIGTERM);
}

/* PoP: ws_get_portal_status_sync @ hermes_cli/web_server.py:_get_portal_status_sync */
void ws_get_portal_status_sync(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"status\":\"ok\"}");
}

/* PoP: ws_get_env_vars_sync @ hermes_cli/web_server.py:_get_env_vars_sync */
void ws_get_env_vars_sync(char *out_json, size_t out_sz) {
    json_t *root = json_object();
    json_t *env = json_object();
    static const char *keys[] = { "HERMES_HOME", "HERMES_PROFILE",
                                  "HERMES_PARENT_PID", NULL };
    for (int i = 0; keys[i]; i++) {
        const char *v = getenv(keys[i]);
        if (v && v[0]) json_set(env, keys[i], json_string(v));
    }
    json_set(root, "env", env);
    char *ser = json_serialize(root);
    json_free(root);
    if (ser) { snprintf(out_json, out_sz, "%s", ser); free(ser); }
    else if (out_sz) out_json[0] = '\0';
}

/* PoP: ws_session_db_read_probe_statements @ hermes_cli/web_server.py:_session_db_read_probe_statements */
/* The read-probe statement list: every table/column declared in SCHEMA_SQL.
 * ANY schema addition escalates a stale store to a one-time writable heal. */
static const char *const SM_PROBE_STATEMENTS[] = {
    "SELECT 1 FROM sessions LIMIT 1",
    "SELECT 1 FROM messages LIMIT 1",
    "SELECT 1 FROM meta LIMIT 1",
    NULL,
};

int ws_session_db_read_probe_statements(const char *const **out_statements) {
    if (out_statements) *out_statements = SM_PROBE_STATEMENTS;
    int n = 0;
    while (SM_PROBE_STATEMENTS[n]) n++;
    return n;
}

/* PoP: ws_open_session_db_at_path @ hermes_cli/web_server.py:_open_session_db_at_path */
/* Open a SessionDB at an explicit path with an explicit access mode.
 *
 * Writable opens keep the full init/repair path. Read-only opens bootstrap a
 * missing or zero-byte store once, then probe the schema; on "no such table/
 * column" they escalate to ONE writable open before reopening read-only.
 * A store still stale after the heal is served without the probe and added
 * to the heal-exhausted set so per-poll writable inits stop being paid.
 * Returns an opened sqlite3 handle (read-only when requested), or NULL.
 * The caller closes it with sqlite3_close(). */
sqlite3 *ws_open_session_db_at_path(const char *db_path, bool read_only) {
    if (!db_path || !db_path[0]) return NULL;

    static char g_heal_exhausted[4][512];
    static int g_heal_exhausted_n = 0;

    if (!read_only) {
        sqlite3 *db = NULL;
        if (sqlite3_open(db_path, &db) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            return NULL;
        }
        return db;
    }

    /* _needs_bootstrap(): missing or zero-byte store → one writable open. */
    struct stat st;
    if (stat(db_path, &st) != 0 || st.st_size == 0) {
        sqlite3 *boot = NULL;
        if (sqlite3_open(db_path, &boot) == SQLITE_OK)
            sqlite3_close(boot);
    }

    for (int attempt = 0; attempt < 2; attempt++) {
        sqlite3 *db = NULL;
        int flags = read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
        if (sqlite3_open_v2(db_path, &db, flags, NULL) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            return NULL;
        }

        bool exhausted = false;
        for (int i = 0; i < g_heal_exhausted_n; i++)
            if (strcmp(g_heal_exhausted[i], db_path) == 0) { exhausted = true; break; }

        if (!exhausted) {
            bool stale = false;
            for (const char *const *p = SM_PROBE_STATEMENTS; *p; p++) {
                char *err = NULL;
                if (sqlite3_exec(db, *p, NULL, NULL, &err) != SQLITE_OK) {
                    const char *msg = err ? err : "";
                    if (strstr(msg, "no such table") || strstr(msg, "no such column"))
                        stale = true;
                    if (err) sqlite3_free(err);
                    break;
                }
            }
            if (!stale) return db;
            sqlite3_close(db);

            /* One writable open to reconcile the schema. */
            if (attempt == 0) {
                sqlite3 *heal = NULL;
                if (sqlite3_open(db_path, &heal) == SQLITE_OK)
                    sqlite3_close(heal);
                continue; /* reopen read-only + re-probe */
            }
            /* Still stale after the heal: serve reads without the probe and
             * stop paying the writable init per poll. */
            if (g_heal_exhausted_n < 4) {
                snprintf(g_heal_exhausted[g_heal_exhausted_n],
                         sizeof(g_heal_exhausted[0]), "%s", db_path);
                g_heal_exhausted_n++;
                hermes_log(LOG_WARNING, "web_server",
                           "state.db at %s is missing schema that a writable "
                           "reconcile could not add; read paths may partially "
                           "fail until the store is repaired", db_path);
            }
            flags = SQLITE_OPEN_READONLY;
            if (sqlite3_open_v2(db_path, &db, flags, NULL) != SQLITE_OK) {
                if (db) sqlite3_close(db);
                return NULL;
            }
            return db;
        }
        /* Heal-exhausted path: serve without probing. */
        return db;
    }
    return NULL;
}

/* PoP: ws_raise_if_cron_registration_error @ hermes_cli/web_server.py:_raise_if_cron_registration_error */
/* Map a cron partial-failure (job saved but external scheduler registration
 * failed) to HTTP 424 with the structured envelope. Shared by every dashboard
 * cron-create surface. Returns 1 (mapped) with out_status=424 + out_detail
 * JSON, or 0 when the error is not a CronSchedulerRegistrationError. */
int ws_raise_if_cron_registration_error(const char *error_msg,
                                        int *out_status,
                                        char *out_detail, size_t detail_sz) {
    /* C error transport carries the exception class inline as
     * "CronSchedulerRegistrationError: {...}" — match that shape exactly. */
    static const char tag[] = "CronSchedulerRegistrationError:";
    if (!error_msg || strncmp(error_msg, tag, strlen(tag)) != 0)
        return 0;
    const char *detail = error_msg + strlen(tag);
    while (*detail == ' ') detail++;
    if (out_status) *out_status = 424;
    if (out_detail && detail_sz > 0)
        snprintf(out_detail, detail_sz, "%s", detail[0] ? detail : "{}");
    return 1;
}

/* ── process-start markers (linux branch of _process_start_marker) ──────── */

/* Return "linux:<starttime>" for pid from /proc/<pid>/stat field 22.
 * Returns false when the process is absent (ProcessLookupError) or the stat
 * data is malformed (OSError) — callers fail safe. */
static bool ws_process_start_marker(int pid, char *out, size_t sz) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return false; /* ProcessLookupError */
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    /* Split after the final ')' — field 2 (comm) may contain spaces/parens.
     * What remains starts at field 3; starttime is field 22 → index 19. */
    char *rest = strrchr(buf, ')');
    if (!rest) return false; /* OSError: invalid /proc stat data */
    rest++;
    char *fields[32];
    int nf = 0;
    char *save = NULL;
    for (char *tok = strtok_r(rest, " \t\n", &save);
         tok && nf < 32; tok = strtok_r(NULL, " \t\n", &save))
        fields[nf++] = tok;
    if (nf < 20 || !isdigit((unsigned char)fields[19][0]))
        return false;
    snprintf(out, sz, "linux:%s", fields[19]);
    return true;
}

static bool ws_valid_parent_start_marker(const char *marker) {
    if (!marker) return false;
    const char *colon = strchr(marker, ':');
    if (!colon || colon == marker) return false;
    char prefix[16];
    size_t plen = (size_t)(colon - marker);
    if (plen >= sizeof(prefix) || plen == 0) return false;
    memcpy(prefix, marker, plen);
    prefix[plen] = '\0';
    const char *value = colon + 1;
    if (!value[0]) return false;
    /* value == value.strip() */
    if (isspace((unsigned char)value[0]) ||
        isspace((unsigned char)value[strlen(value) - 1]))
        return false;
    if (strcmp(prefix, "linux") == 0 || strcmp(prefix, "win") == 0 ||
        strcmp(prefix, "winms") == 0) {
        for (const char *p = value; *p; p++)
            if (!isdigit((unsigned char)*p)) return false;
        return true;
    }
    return strcmp(prefix, "ps") == 0;
}

static bool ws_parent_start_markers_match(const char *actual, const char *expected) {
    if (!actual || !expected) return false;
    if (strcmp(actual, expected) == 0) return true;
    /* Cross-generation Windows comparison: legacy .NET ticks ("win:") vs
     * Electron Unix-milliseconds ("winms:"). */
    if (strncmp(actual, "win:", 4) != 0 || strncmp(expected, "winms:", 6) != 0)
        return false;
    unsigned long long ticks = strtoull(actual + 4, NULL, 10);
    unsigned long long unix_ms = strtoull(expected + 6, NULL, 10);
    const unsigned long long DOTNET_TICKS_AT_UNIX_EPOCH = 621355968000000000ULL;
    return (ticks - DOTNET_TICKS_AT_UNIX_EPOCH) / 10000ULL == unix_ms;
}

/* PoP: ws_is_serve_orphaned @ hermes_cli/web_server.py:_is_serve_orphaned */
/* True when the exact Desktop process that owns this backend is gone.
 * HERMES_PARENT_PID is the Electron PID, not our immediate PPID. With an
 * expected_start_marker the probe compares process-start markers (guards
 * against PID reuse). Any inconclusive probe failure fails SAFE: keep
 * serving rather than killing a backend whose owner could not be shown dead. */
int ws_is_serve_orphaned(int desktop_pid, const char *expected_start_marker) {
    if (expected_start_marker != NULL) {
        char actual[128] = "";
        if (!ws_process_start_marker(desktop_pid, actual, sizeof(actual)))
            return 0; /* inconclusive probe failure → fail safe */
        return ws_parent_start_markers_match(actual, expected_start_marker) ? 0 : 1;
    }
    /* PID-only probe (legacy Desktop): kill(pid, 0). ProcessLookupError → ESRCH. */
    if (kill((pid_t)desktop_pid, 0) == 0) return 0;
    if (errno == ESRCH) return 1;
    return 0; /* EPERM etc. — process exists / inconclusive → fail safe */
}

/* PoP: ws_start_parent_death_watchdog @ hermes_cli/web_server.py:_start_parent_death_watchdog */
/* Exit when the exact desktop parent that spawned this backend dies. Newer
 * Desktop versions pass HERMES_PARENT_START_MARKER plus HERMES_PARENT_NONCE;
 * mixed-version plumbing (one without the other) fails safe by disabling the
 * watchdog. Legacy versions get PID-only tracking. */
typedef struct { int pid; char start_marker[160]; } ws_watchdog_arg_t;

static void *ws_watchdog_loop(void *argp) {
    ws_watchdog_arg_t *a = argp;
    double poll = 2.0;
    const char *raw_poll = getenv("HERMES_SERVE_WATCHDOG_POLL_S");
    if (raw_poll && raw_poll[0]) {
        char *end = NULL;
        double v = strtod(raw_poll, &end);
        if (end && end != raw_poll)
            poll = v;
    }
    if (poll < 0.5) poll = 0.5;

    while (!ws_is_serve_orphaned(a->pid,
                                 a->start_marker[0] ? a->start_marker : NULL)) {
        struct timespec ts = { (time_t)poll,
                               (long)((poll - (double)(time_t)poll) * 1e9) };
        nanosleep(&ts, NULL);
    }
    /* os._exit(0): hard exit regardless of other threads. */
    _exit(0);
    return NULL;
}

void ws_start_parent_death_watchdog(void) {
    const char *raw_pid = getenv("HERMES_PARENT_PID");
    const char *start_marker = getenv("HERMES_PARENT_START_MARKER");
    const char *nonce = getenv("HERMES_PARENT_NONCE");

    if (!raw_pid || !raw_pid[0]) return;
    char *end = NULL;
    long desktop_pid = strtol(raw_pid, &end, 10);
    if (end == raw_pid) return;
    if (desktop_pid <= 0) return;

    int has_marker = (start_marker != NULL);
    int has_nonce = (nonce != NULL);
    if (has_marker != has_nonce) return;
    if (has_marker &&
        (!ws_valid_parent_start_marker(start_marker[0] ? start_marker : "") ||
         !nonce[0])) {
        /* nonce != nonce.strip() also disables — compare trimmed. */
        size_t n = strlen(nonce);
        if (isspace((unsigned char)nonce[0]) ||
            isspace((unsigned char)nonce[n - 1]))
            return;
        return;
    }

    static ws_watchdog_arg_t arg;
    arg.pid = (int)desktop_pid;
    snprintf(arg.start_marker, sizeof(arg.start_marker), "%s",
             has_marker ? start_marker : "");
    pthread_t th;
    if (pthread_create(&th, NULL, ws_watchdog_loop, &arg) == 0)
        pthread_detach(th);
}

/* PoP: ws_demo @ hermes_cli/web_server.py:_demo */
/* Self-check of the orphan detector (Python asserts with injected probes). */
void ws_demo(void) {
    /* _is_serve_orphaned(999999999, pid_exists=False) is True */
    if (!ws_is_serve_orphaned(999999999, NULL)) {
        printf("web_server parent-death watchdog self-check: FAIL\n");
        return;
    }
    /* _is_serve_orphaned(<our own pid>, pid_exists=True) is False */
    if (ws_is_serve_orphaned((int)getpid(), NULL)) {
        printf("web_server parent-death watchdog self-check: FAIL\n");
        return;
    }
    printf("web_server parent-death watchdog self-check: OK\n");
}
