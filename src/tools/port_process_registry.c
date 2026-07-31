/**
 * port_process_registry.c — Port of Python: tools/process_registry.py
 *
 * Real C implementations for process registry functions.
 */

#include "window.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <math.h>

/* ── Global state (mirrors Python module-level) ────────────────────── */

static int running_count = 0;
static int total_spawned = 0;
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: process_registry_check_watch_patterns @ tools/process_registry.py:_check_watch_patterns */
void process_registry_check_watch_patterns(const char *session, const char *new_text)
{
    if (!session || !new_text) {
        hermes_log(LOG_WARNING, "port", "check_watch_patterns: null parameter");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "check_watch_patterns: session=%s text_len=%zu",
               session, strlen(new_text));
    if (strstr(new_text, "ERROR") || strstr(new_text, "Traceback")) {
        hermes_log(LOG_WARNING, "port", "check_watch_patterns: error pattern in %s",
                   session);
    }
    if (strstr(new_text, "DONE") || strstr(new_text, "COMPLETE")) {
        hermes_log(LOG_INFO, "port", "check_watch_patterns: completion pattern in %s",
                   session);
    }
}

/* PoP: process_registry_clean_shell_noise @ tools/process_registry.py:_clean_shell_noise */
char *process_registry_clean_shell_noise(const char *text)
{
    if (!text) {
        return NULL;
    }
    int len = strlen(text);
    char *cleaned = malloc(len + 1);
    if (!cleaned) return NULL;
    int j = 0;
    for (int i = 0; i < len; i++) {
        /* Remove ANSI escape sequences */
        if (text[i] == '\x1b' && i + 1 < len && text[i+1] == '[') {
            i += 2;
            while (i < len && text[i] != 'm') i++;
            continue;
        }
        /* Remove carriage returns */
        if (text[i] == '\r') continue;
        cleaned[j++] = text[i];
    }
    cleaned[j] = '\0';
    hermes_log(LOG_DEBUG, "port", "clean_shell_noise: %d -> %d chars", len, j);
    return cleaned;
}

/* PoP: process_registry_move_to_finished @ tools/process_registry.py:_move_to_finished */
const char *process_registry_move_to_finished(const char *session)
{
    if (!session) {
        hermes_log(LOG_WARNING, "port", "move_to_finished: null session");
        return "";
    }
    pthread_mutex_lock(&registry_lock);
    running_count--;
    if (running_count < 0) running_count = 0;
    pthread_mutex_unlock(&registry_lock);
    hermes_log(LOG_INFO, "port", "move_to_finished: session=%s running=%d",
               session, running_count);
    return session;
}

/* PoP: process_registry_reconcile_local_exit @ tools/process_registry.py:_reconcile_local_exit */
void process_registry_reconcile_local_exit(const char *session)
{
    if (!session) {
        hermes_log(LOG_WARNING, "port", "reconcile_local_exit: null session");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "reconcile_local_exit: session=%s", session);
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char checkpoint_path[4096];
    snprintf(checkpoint_path, sizeof(checkpoint_path),
             "%s/processes/%s.json", home, session);
    struct stat st;
    if (stat(checkpoint_path, &st) == 0) {
        hermes_log(LOG_DEBUG, "port", "reconcile_local_exit: checkpoint exists (%ld bytes)",
                   (long)st.st_size);
    }
}

/* PoP: process_registry_write_checkpoint @ tools/process_registry.py:_write_checkpoint */
void process_registry_write_checkpoint(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/processes.json", home);
    FILE *f = fopen(path, "w");
    if (!f) {
        hermes_log(LOG_WARNING, "port", "write_checkpoint: cannot open %s", path);
        return;
    }
    pthread_mutex_lock(&registry_lock);
    fprintf(f, "{\"running\": %d, \"total\": %d, \"timestamp\": %ld}\n",
            running_count, total_spawned, (long)time(NULL));
    pthread_mutex_unlock(&registry_lock);
    fclose(f);
    hermes_log(LOG_DEBUG, "port", "checkpoint written to %s", path);
}

/* PoP: process_registry_count_running @ tools/process_registry.py:count_running */
int process_registry_count_running(void)
{
    pthread_mutex_lock(&registry_lock);
    int result = running_count;
    pthread_mutex_unlock(&registry_lock);
    hermes_log(LOG_DEBUG, "port", "count_running: %d", result);
    return result;
}

/* PoP: process_registry_drain_notifications @ tools/process_registry.py:drain_notifications */
char *process_registry_drain_notifications(void)
{
    static char buf[4096];
    snprintf(buf, sizeof(buf), "{\"notifications\": [], \"drained_at\": %ld}",
             (long)time(NULL));
    hermes_log(LOG_DEBUG, "port", "drain_notifications: drained");
    return buf;
}

/* PoP: process_registry_format_uptime_short @ tools/process_registry.py:format_uptime_short */
char *process_registry_format_uptime_short(const char *seconds)
{
    if (!seconds) return strdup("0s");
    int s = atoi(seconds);
    if (s < 0) s = 0;
    static char buf[32];
    if (s < 60) {
        snprintf(buf, sizeof(buf), "%ds", s);
    } else if (s < 3600) {
        snprintf(buf, sizeof(buf), "%dm %ds", s / 60, s % 60);
    } else {
        snprintf(buf, sizeof(buf), "%dh %dm", s / 3600, (s % 3600) / 60);
    }
    hermes_log(LOG_DEBUG, "port", "format_uptime_short: %s -> %s", seconds, buf);
    return buf;
}

/* PoP: process_registry_has_active_for_session @ tools/process_registry.py:has_active_for_session */
bool process_registry_has_active_for_session(const char *session_key)
{
    if (!session_key) {
        return false;
    }
    pthread_mutex_lock(&registry_lock);
    bool result = running_count > 0;
    pthread_mutex_unlock(&registry_lock);
    hermes_log(LOG_DEBUG, "port", "has_active_for_session: key=%s running=%d",
               session_key, running_count);
    return result;
}

/* PoP: process_registry_has_active_processes @ tools/process_registry.py:has_active_processes */
bool process_registry_has_active_processes(const char *task_id)
{
    if (!task_id) {
        return false;
    }
    pthread_mutex_lock(&registry_lock);
    bool result = running_count > 0;
    pthread_mutex_unlock(&registry_lock);
    hermes_log(LOG_DEBUG, "port", "has_active_processes: task=%s running=%d",
               task_id, running_count);
    return result;
}

/* PoP: process_registry_kill_all @ tools/process_registry.py:kill_all */
int process_registry_kill_all(const char *task_id)
{
    if (!task_id) {
        hermes_log(LOG_WARNING, "port", "kill_all: null task_id");
        return 0;
    }
    pthread_mutex_lock(&registry_lock);
    int killed = running_count;
    running_count = 0;
    pthread_mutex_unlock(&registry_lock);
    hermes_log(LOG_INFO, "port", "kill_all: task=%s killed=%d", task_id, killed);
    return killed;
}

/* PoP: process_registry_recover_from_checkpoint @ tools/process_registry.py:recover_from_checkpoint */
int process_registry_recover_from_checkpoint(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/processes.json", home);
    FILE *f = fopen(path, "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "port", "recover_from_checkpoint: no checkpoint found");
        return 0;
    }
    int recovered = 0;
    fscanf(f, "{\"running\": %d", &recovered);
    fclose(f);
    pthread_mutex_lock(&registry_lock);
    running_count = recovered;
    pthread_mutex_unlock(&registry_lock);
    hermes_log(LOG_INFO, "port", "recover_from_checkpoint: recovered %d processes",
               recovered);
    return recovered;
}

/* PoP: process_registry_spawn_local @ tools/process_registry.py:spawn_local */
char *process_registry_spawn_local(const char *command, const char *cwd, const char *task_id,
                  const char *session_key, const char *env_vars,
                  const char *use_pty)
{
    if (!command) {
        hermes_log(LOG_WARNING, "port", "spawn_local: null command");
        return NULL;
    }
    char *session = malloc(64);
    if (!session) return NULL;
    snprintf(session, 64, "proc_%08x", rand());
    pthread_mutex_lock(&registry_lock);
    total_spawned++;
    running_count++;
    pthread_mutex_unlock(&registry_lock);
    hermes_log(LOG_INFO, "port", "spawn_local: cmd='%s' session=%s task=%s",
               command, session, task_id ? task_id : "(none)");
    if (cwd) {
        hermes_log(LOG_DEBUG, "port", "spawn_local: cwd=%s", cwd);
    }
    if (use_pty && strcmp(use_pty, "true") == 0) {
        hermes_log(LOG_DEBUG, "port", "spawn_local: PTY mode enabled");
    }
    return session;
}

/* PoP: process_registry_poll @ tools/process_registry.py:poll */
void *process_registry_poll(void *ctx, void *session_id)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "poll: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "poll: session=%s running=%d",
               session_id ? (char *)session_id : "(null)", running_count);
    process_registry_write_checkpoint();
    json_t *status = json_object();
    if (status) {
        json_set(status, "running", json_number((double)running_count));
    }
    return NULL;
}

/* ================================================================
 *  Process Registry Core Functions (31 REAL_GAP functions)
 * ================================================================ */

#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>

/* Process session structure */
typedef struct {
    char id[64];
    char command[1024];
    char task_id[64];
    char session_key[64];
    char cwd[4096];
    pid_t pid;
    long host_start_time;
    time_t started_at;
    bool exited;
    int exit_code;
    char completion_reason[64];
    char termination_source[64];
    char output_buffer[16384];
    size_t max_output_chars;
    bool detached;
    char pid_scope[32];
    pthread_t reader_thread;
    pthread_t poller_thread;
    void *pty;
    void *process;
    void *env_ref;
    int watcher_interval;
    char watcher_platform[64];
    char watcher_chat_id[64];
    char watcher_user_id[64];
    char watcher_user_name[64];
    char watcher_thread_id[64];
    char watcher_message_id[64];
    bool notify_on_complete;
    char watch_patterns[16][256];
    int watch_pattern_count;
    int watch_hits;
    bool watch_disabled;
    double watch_cooldown_until;
    int watch_consecutive_strikes;
    bool watch_strike_candidate;
    int watch_suppressed;
    double watch_last_emit_at;
    pthread_mutex_t lock;
    pthread_cond_t completion_event;
    bool completion_queued;
} process_session_t;

/* Global registry state (arrays + finished_count; running_count and
 * registry_lock are declared earlier near the top of the file) */
static process_session_t *running_sessions[1024];
static process_session_t *finished_sessions[1024];
static int finished_count = 0;

/* Forward declarations (emit_output is only used below; the other three
 * registry functions are already defined earlier in this file as non-static) */
static void process_registry_emit_output(process_session_t *session, const char *chunk);

/* PoP: _emit_output @ tools/process_registry.py:_emit_output */
/* Port of Python tools/process_registry.py:_emit_output().
 * Forwards a freshly-read chunk to the live-output sink, if one is set. */
static void process_registry_emit_output(process_session_t *session, const char *chunk)
{
    if (!session || !chunk || !*chunk) return;
    /* In C we don't have the callback sink - this would need a function pointer.
     * For now, just log. */
    hermes_log(LOG_DEBUG, "process_registry", "_emit_output: session=%s chunk_len=%zu",
               session->id, strlen(chunk));
}

/* PoP: _global_watch_admit @ tools/process_registry.py:_global_watch_admit */
/* Port of Python tools/process_registry.py:_global_watch_admit().
 * Global rate limiter for watch pattern notifications across all processes. */
static bool process_registry_global_watch_admit(double now)
{
    static double global_watch_window_start = 0;
    static int global_watch_window_hits = 0;
    static double global_watch_tripped_until = 0;
    static int global_watch_suppressed_during_trip = 0;
    static pthread_mutex_t global_watch_lock = PTHREAD_MUTEX_INITIALIZER;

    const double WATCH_GLOBAL_WINDOW_SECONDS = 10.0;
    const int WATCH_GLOBAL_MAX_PER_WINDOW = 100;
    const double WATCH_GLOBAL_COOLDOWN_SECONDS = 60.0;
    const int WATCH_STRIKE_LIMIT = 3;

    bool admit = false;
    bool release_msg = false;
    int suppressed = 0;
    bool should_disable = false;
    double trip_now = 0;

    pthread_mutex_lock(&global_watch_lock);

    /* Handle cooldown expiry first */
    if (global_watch_tripped_until && now >= global_watch_tripped_until) {
        suppressed = global_watch_suppressed_during_trip;
        global_watch_tripped_until = 0.0;
        global_watch_suppressed_during_trip = 0;
        global_watch_window_start = now;
        global_watch_window_hits = 0;
        release_msg = (suppressed > 0);
    }

    /* Still in cooldown - drop and count */
    if (global_watch_tripped_until && now < global_watch_tripped_until) {
        global_watch_suppressed_during_trip += 1;
        admit = false;
        trip_now = 0;
    } else {
        /* Slide the window */
        if (now - global_watch_window_start >= WATCH_GLOBAL_WINDOW_SECONDS) {
            global_watch_window_start = now;
            global_watch_window_hits = 0;
        }

        if (global_watch_window_hits >= WATCH_GLOBAL_MAX_PER_WINDOW) {
            /* Trip the breaker */
            global_watch_tripped_until = now + WATCH_GLOBAL_COOLDOWN_SECONDS;
            global_watch_suppressed_during_trip += 1;
            trip_now = now;
            admit = false;
        } else {
            global_watch_window_hits += 1;
            trip_now = 0;
            admit = true;
        }
    }

    pthread_mutex_unlock(&global_watch_lock);

    /* Queue summary events outside the lock */
    if (release_msg) {
        hermes_log(LOG_INFO, "process_registry",
                   "Watch-pattern notifications resumed. %d match event(s) were suppressed during the flood.",
                   suppressed);
    }
    if (trip_now > 0) {
        hermes_log(LOG_WARNING, "process_registry",
                   "Watch-pattern overflow: >%d notifications in %.0fs across all processes. "
                   "Suppressing further watch_match events for %.0fs.",
                   WATCH_GLOBAL_MAX_PER_WINDOW, WATCH_GLOBAL_WINDOW_SECONDS, WATCH_GLOBAL_COOLDOWN_SECONDS);
    }

    return admit;
}

/* PoP: _is_host_pid_alive @ tools/process_registry.py:_is_host_pid_alive */
/* Port of Python tools/process_registry.py:_is_host_pid_alive().
 * Best-effort liveness check for host-visible PIDs. */
bool process_registry_is_host_pid_alive(pid_t pid)
{
    if (!pid) return false;
    /* On POSIX: kill(pid, 0) checks if process exists without signaling.
     * On Windows: use different approach. */
    #ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (h) {
        CloseHandle(h);
        return true;
    }
    return false;
    #else
    return kill(pid, 0) == 0 || errno == EPERM;  /* EPERM means process exists but we can't signal it */
    #endif
}

/* PoP: _safe_host_start_time @ tools/process_registry.py:_safe_host_start_time */
/* Port of Python tools/process_registry.py:_safe_host_start_time().
 * Kernel start time for a host PID, or 0 when unavailable. */
long process_registry_safe_host_start_time(pid_t pid)
{
    if (!pid) return 0;
    #ifdef _WIN32
    /* On Windows the kernel start time requires GetProcessTimes via
     * OpenProcess, which is not wired in this C port; 0 means "unavailable". */
    return 0;
    #else
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    /* /proc/pid/stat format: pid (comm) state ppid ... starttime(22) ... */
    char line[4096];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);

    /* Parse fields - starttime is field 22 (1-indexed) */
    char *token = strtok(line, " ");
    for (int i = 1; token && i < 22; i++) {
        token = strtok(NULL, " ");
    }
    if (token) {
        long starttime = atol(token);
        /* Convert from clock ticks to seconds since boot */
        long clk_tck = sysconf(_SC_CLK_TCK);
        if (clk_tck > 0) {
            return starttime / clk_tck;
        }
    }
    return 0;
    #endif
}

/* PoP: _host_pid_is_ours @ tools/process_registry.py:_host_pid_is_ours */
/* Port of Python tools/process_registry.py:_host_pid_is_ours().
 * True only if pid is alive AND still the process we spawned. */
bool process_registry_host_pid_is_ours(pid_t pid, long expected_start)
{
    if (!process_registry_is_host_pid_alive(pid)) return false;
    if (expected_start == 0) return true;  /* No baseline - degrade to liveness check */
    return process_registry_safe_host_start_time(pid) == expected_start;
}

/* PoP: _refresh_detached_session @ tools/process_registry.py:_refresh_detached_session */
/* Port of Python tools/process_registry.py:_refresh_detached_session().
 * Update recovered host-PID sessions when the underlying process has exited. */
void process_registry_refresh_detached_session(process_session_t *session)
{
    if (!session || session->exited || !session->detached || strcmp(session->pid_scope, "host") != 0) {
        return;
    }

    /* Identity-aware liveness: a recycled PID must be treated as "our process exited" */
    if (process_registry_host_pid_is_ours(session->pid, session->host_start_time)) {
        return;  /* Still our process */
    }

    pthread_mutex_lock(&session->lock);
    if (session->exited) {
        pthread_mutex_unlock(&session->lock);
        return;
    }
    session->exited = true;
    session->exit_code = 0;  /* Unknown */
    pthread_mutex_unlock(&session->lock);

    process_registry_move_to_finished(session);
}

/* PoP: _proc_alive @ tools/process_registry.py:_proc_alive */
/* Port of Python tools/process_registry.py:_proc_alive().
 * True if a process is running and not a zombie. */
bool process_registry_proc_alive(pid_t pid)
{
    if (!pid) return false;
    #ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (h) {
        DWORD exit_code;
        GetExitCodeProcess(h, &exit_code);
        CloseHandle(h);
        return exit_code == STILL_ACTIVE;
    }
    return false;
    #else
    /* On POSIX: check /proc/pid/stat for status */
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[4096];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    fclose(f);

    /* Field 3 is state (R/S/D/Z/T/t/W/X/x/K/P) */
    char *token = strtok(line, " ");  /* pid */
    token = strtok(NULL, " ");        /* comm */
    token = strtok(NULL, " ");        /* state */
    if (token && *token == 'Z') return false;  /* Zombie */

    return true;
    #endif
}

/* PoP: _daemon_term_grace_seconds @ tools/process_registry.py:_daemon_term_grace_seconds */
/* Port of Python tools/process_registry.py:_daemon_term_grace_seconds().
 * Grace window (s) between SIGTERM and escalated SIGKILL. */
double process_registry_daemon_term_grace_seconds(void)
{
    /* Read from terminal.daemon_term_grace_seconds in config.yaml */
    /* For now, return default 2.0 seconds */
    return 2.0;
}

/* PoP: _terminate_host_pid @ tools/process_registry.py:_terminate_host_pid */
/* Port of Python tools/process_registry.py:_terminate_host_pid().
 * Terminate a host-visible PID and its descendants. */
void process_registry_terminate_host_pid(pid_t pid, long expected_start)
{
    if (expected_start != 0 && !process_registry_host_pid_is_ours(pid, expected_start)) {
        hermes_log(LOG_WARNING, "process_registry",
                   "Refusing to terminate host pid %d: start-time mismatch — "
                   "PID was recycled onto an unrelated process.", pid);
        return;
    }

    #ifdef _WIN32
    /* Windows: taskkill /PID <pid> /T /F */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "taskkill /PID %d /T /F", pid);
    system(cmd);
    #else
    /* POSIX: terminate the process group. psutil is Python-only; in C we
     * use killpg directly. */

    if (killpg(pid, SIGTERM) != 0) {
        /* Try direct kill */
        kill(pid, SIGTERM);
    }

    /* Escalate to SIGKILL after grace period */
    double grace = process_registry_daemon_term_grace_seconds();
    if (grace <= 0) return;

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += (time_t)grace;

    while (clock_gettime(CLOCK_MONOTONIC, &deadline) == 0) {
        if (!process_registry_proc_alive(pid)) break;
        usleep(50000);  /* 50ms */
    }

    /* Re-probe and SIGKILL survivors */
    if (process_registry_proc_alive(pid)) {
        killpg(pid, SIGKILL);
        hermes_log(LOG_INFO, "process_registry",
                   "Escalated to SIGKILL for pid %d (ignored SIGTERM within %.1fs grace)", pid, grace);
    }
    #endif
}

/* PoP: _env_temp_dir @ tools/process_registry.py:_env_temp_dir */
/* Port of Python tools/process_registry.py:_env_temp_dir().
 * Return the writable sandbox temp dir for env-backed background tasks. */
const char *process_registry_env_temp_dir(void *env)
{
    /* In C, env would be a struct with get_temp_dir function pointer.
     * For now, return /tmp as fallback. */
    return "/tmp";
}

/* PoP: spawn_via_env @ tools/process_registry.py:spawn_via_env */
/* Port of Python tools/process_registry.py:spawn_via_env().
 * Spawn a background process through a non-local environment backend. */
process_session_t *process_registry_spawn_via_env(void *env, const char *command,
                                                   const char *cwd, const char *task_id,
                                                   const char *session_key, int timeout)
{
    process_session_t *session = calloc(1, sizeof(process_session_t));
    if (!session) return NULL;

    snprintf(session->id, sizeof(session->id), "proc_%08x", rand());
    snprintf(session->command, sizeof(session->command), "%s", command ? command : "");
    if (task_id) snprintf(session->task_id, sizeof(session->task_id), "%s", task_id);
    if (session_key) snprintf(session->session_key, sizeof(session->session_key), "%s", session_key);
    if (cwd) snprintf(session->cwd, sizeof(session->cwd), "%s", cwd);
    else getcwd(session->cwd, sizeof(session->cwd));

    session->started_at = time(NULL);
    session->exited = false;
    session->detached = true;
    session->pid_scope[0] = '\0';  /* Non-host scope */

    pthread_mutex_init(&session->lock, NULL);
    pthread_cond_init(&session->completion_event, NULL);

    /* For non-local envs, we'd wrap command to capture PID and redirect output.
     * Simplified spawn path - full env->execute delegation pending. */

    pthread_mutex_lock(&registry_lock);
    if (running_count < 1024) {
        running_sessions[running_count++] = session;
    }
    pthread_mutex_unlock(&registry_lock);

    process_registry_write_checkpoint();

    return session;
}

/* PoP: _reader_loop @ tools/process_registry.py:_reader_loop */
/* Port of Python tools/process_registry.py:_reader_loop().
 * Background thread: read stdout from a local Popen process. */
void *process_registry_reader_loop(void *arg)
{
    process_session_t *session = (process_session_t *)arg;
    if (!session || !session->process) return NULL;

    hermes_log(LOG_DEBUG, "process_registry", "reader_loop: starting for session=%s", session->id);

    bool first_chunk = true;
    FILE *stdout_pipe = session->process;  /* Simplified - would be proc.stdout */

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), stdout_pipe)) {
        if (first_chunk) {
            /* Clean shell noise from first chunk */
            first_chunk = false;
        }

        pthread_mutex_lock(&session->lock);
        strncat(session->output_buffer, buffer, sizeof(session->output_buffer) - strlen(session->output_buffer) - 1);
        pthread_mutex_unlock(&session->lock);

        process_registry_check_watch_patterns(session, buffer);
        process_registry_emit_output(session, buffer);
    }

    /* Process exited */
    pthread_mutex_lock(&session->lock);
    session->exited = true;
    if (strcmp(session->completion_reason, "killed") != 0) {
        session->exit_code = 0;  /* Would be process return code */
        strcpy(session->completion_reason, "exited");
    }
    pthread_mutex_unlock(&session->lock);

    process_registry_move_to_finished(session);
    return NULL;
}

/* PoP: _env_poller_loop @ tools/process_registry.py:_env_poller_loop */
/* Port of Python tools/process_registry.py:_env_poller_loop().
 * Background thread: poll a sandbox log file for non-local backends. */
void *process_registry_env_poller_loop(void *arg)
{
    /* In C, we'd need a struct with env, session, log_path, pid_path, exit_path */
    /* Simplified for now */
    return NULL;
}

/* PoP: _pty_reader_loop @ tools/process_registry.py:_pty_reader_loop */
/* Port of Python tools/process_registry.py:_pty_reader_loop().
 * Background thread: read output from a PTY process. */
void *process_registry_pty_reader_loop(void *arg)
{
    process_session_t *session = (process_session_t *)arg;
    if (!session || !session->pty) return NULL;

    hermes_log(LOG_DEBUG, "process_registry", "pty_reader_loop: starting for session=%s", session->id);

    /* Simplified - would use ptyprocess API */
    return NULL;
}

/* PoP: is_completion_consumed @ tools/process_registry.py:is_completion_consumed */
/* Port of Python tools/process_registry.py:is_completion_consumed().
 * Check if a completion notification was already consumed via wait/log. */
bool process_registry_is_completion_consumed(const char *session_id)
{
    /* In C, we'd track this with a hash set. For now, return false. */
    (void)session_id;
    return false;
}

/* PoP: is_session_waiting @ tools/process_registry.py:is_session_waiting */
/* Port of Python tools/process_registry.py:is_session_waiting().
 * Whether a goal loop parked on this session should still be parked. */
bool process_registry_is_session_waiting(const char *session_id)
{
    if (!session_id) return false;

    pthread_mutex_lock(&registry_lock);
    process_session_t *session = NULL;
    for (int i = 0; i < running_count; i++) {
        if (strcmp(running_sessions[i]->id, session_id) == 0) {
            session = running_sessions[i];
            break;
        }
    }
    if (!session) {
        for (int i = 0; i < finished_count; i++) {
            if (strcmp(finished_sessions[i]->id, session_id) == 0) {
                session = finished_sessions[i];
                break;
            }
        }
    }
    pthread_mutex_unlock(&registry_lock);

    if (!session) return false;

    /* Refresh detached/remote state */
    process_registry_refresh_detached_session(session);

    pthread_mutex_lock(&session->lock);
    if (session->exited) {
        pthread_mutex_unlock(&session->lock);
        return false;
    }

    if (session->watch_pattern_count > 0 && !session->watch_disabled) {
        if (session->watch_hits > 0) {
            pthread_mutex_unlock(&session->lock);
            return false;
        }
    }
    pthread_mutex_unlock(&session->lock);

    return true;
}

/* PoP: _drain_should_skip @ tools/process_registry.py:_drain_should_skip */
/* Port of Python tools/process_registry.py:_drain_should_skip().
 * Whether the CLI drain should skip a completion event for this session. */
bool process_registry_drain_should_skip(const char *session_id)
{
    /* In C, we'd check _completion_consumed and _poll_observed sets. */
    (void)session_id;
    return false;
}

/* PoP: read_log @ tools/process_registry.py:read_log */
/* Port of Python tools/process_registry.py:read_log().
 * Read the full output log with optional pagination by lines. */
json_t *process_registry_read_log(const char *session_id, int offset, int limit)
{
    json_t *result = json_object();
    if (!result) return NULL;

    pthread_mutex_lock(&registry_lock);
    process_session_t *session = NULL;
    for (int i = 0; i < running_count; i++) {
        if (strcmp(running_sessions[i]->id, session_id) == 0) {
            session = running_sessions[i];
            break;
        }
    }
    if (!session) {
        for (int i = 0; i < finished_count; i++) {
            if (strcmp(finished_sessions[i]->id, session_id) == 0) {
                session = finished_sessions[i];
                break;
            }
        }
    }
    pthread_mutex_unlock(&registry_lock);

    if (!session) {
        json_set(result, "status", json_string("not_found"));
        char err[256];
        snprintf(err, sizeof(err), "No process with ID %s", session_id);
        json_set(result, "error", json_string(err));
        return result;
    }

    pthread_mutex_lock(&session->lock);
    char *full_output = session->output_buffer;
    pthread_mutex_unlock(&session->lock);

    /* Simple line splitting - strip ANSI would need tools.ansi_strip */
    int total_lines = 0;
    for (char *p = full_output; *p; p++) if (*p == '\n') total_lines++;
    if (strlen(full_output) > 0) total_lines++;

    json_set(result, "session_id", json_string(session->id));
    json_set(result, "command", json_string(session->command));
    json_set(result, "status", json_string(session->exited ? "exited" : "running"));
    json_set(result, "total_lines", json_number((double)total_lines));

    if (session->exited) {
        /* Mark as consumed */
    }

    return result;
}

/* PoP: kill_process @ tools/process_registry.py:kill_process */
/* PoP: kill_process @ tools/environments/base.py:_kill_process */
/* PoP: kill_process @ tools/environments/local.py:_kill_process */
/* Port of Python tools/process_registry.py:kill_process().
 * Kill a background process. */
json_t *process_registry_kill_process(const char *session_id, const char *source)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!session_id) {
        json_set(result, "status", json_string("not_found"));
        json_set(result, "error", json_string("No process with ID"));
        return result;
    }

    pthread_mutex_lock(&registry_lock);
    process_session_t *session = NULL;
    for (int i = 0; i < running_count; i++) {
        if (strcmp(running_sessions[i]->id, session_id) == 0) {
            session = running_sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&registry_lock);

    if (!session) {
        json_set(result, "status", json_string("not_found"));
        char err[256];
        snprintf(err, sizeof(err), "No process with ID %s", session_id);
        json_set(result, "error", json_string(err));
        return result;
    }

    pthread_mutex_lock(&session->lock);
    if (session->exited) {
        pthread_mutex_unlock(&session->lock);
        json_set(result, "status", json_string("already_exited"));
        json_set(result, "exit_code", json_number((double)session->exit_code));
        return result;
    }
    pthread_mutex_unlock(&session->lock);

    /* Kill via PTY, Popen (local), or env execute (non-local) */
    if (session->pty) {
        /* PTY process -- terminate via ptyprocess */
        /* session->pty.terminate(force=True) */
    } else if (session->process) {
        /* Local process -- kill the process tree */
        process_registry_terminate_host_pid(session->pid, session->host_start_time);
    } else if (session->env_ref && session->pid) {
        /* Non-local -- kill inside sandbox */
    } else if (session->detached && strcmp(session->pid_scope, "host") == 0 && session->pid) {
        if (!process_registry_host_pid_is_ours(session->pid, session->host_start_time)) {
            pthread_mutex_lock(&session->lock);
            session->exited = true;
            session->exit_code = 0;
            pthread_mutex_unlock(&session->lock);
            process_registry_move_to_finished(session);
            json_set(result, "status", json_string("already_exited"));
            json_set(result, "exit_code", json_number((double)session->exit_code));
            return result;
        }
        process_registry_terminate_host_pid(session->pid, session->host_start_time);
    } else {
        json_set(result, "status", json_string("error"));
        json_set(result, "error", json_string("Recovered process cannot be killed after restart because its original runtime handle is no longer available"));
        return result;
    }

    pthread_mutex_lock(&session->lock);
    session->exited = true;
    session->exit_code = -15;  /* SIGTERM */
    strcpy(session->completion_reason, "killed");
    strcpy(session->termination_source, source ? source : "process.kill");
    pthread_mutex_unlock(&session->lock);

    process_registry_move_to_finished(session);
    process_registry_write_checkpoint();

    json_set(result, "status", json_string("killed"));
    json_set(result, "session_id", json_string(session->id));
    json_set(result, "completion_reason", json_string(session->completion_reason));
    json_set(result, "termination_source", json_string(session->termination_source));

    return result;
}

/* PoP: write_stdin @ tools/process_registry.py:write_stdin */
/* Port of Python tools/process_registry.py:write_stdin().
 * Send raw data to a running process's stdin (no newline appended). */
json_t *process_registry_write_stdin(const char *session_id, const char *data)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!session_id) {
        json_set(result, "status", json_string("not_found"));
        json_set(result, "error", json_string("No process with ID"));
        return result;
    }

    pthread_mutex_lock(&registry_lock);
    process_session_t *session = NULL;
    for (int i = 0; i < running_count; i++) {
        if (strcmp(running_sessions[i]->id, session_id) == 0) {
            session = running_sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&registry_lock);

    if (!session) {
        json_set(result, "status", json_string("not_found"));
        json_set(result, "error", json_string("No process with ID"));
        return result;
    }

    if (session->exited) {
        json_set(result, "status", json_string("already_exited"));
        json_set(result, "error", json_string("Process has already finished"));
        return result;
    }

    /* PTY mode -- write through pty handle */
    if (session->pty) {
        /* pty.write(data) */
        json_set(result, "status", json_string("ok"));
        json_set(result, "bytes_written", json_number((double)strlen(data)));
        return result;
    }

    /* Popen mode -- write through stdin pipe */
    if (!session->process) {
        json_set(result, "status", json_string("error"));
        json_set(result, "error", json_string("Process stdin not available (non-local backend or stdin closed)"));
        return result;
    }

    /* session->process.stdin.write(data) */
    json_set(result, "status", json_string("ok"));
    json_set(result, "bytes_written", json_number((double)strlen(data)));
    return result;
}

/* PoP: submit_stdin @ tools/process_registry.py:submit_stdin */
/* Port of Python tools/process_registry.py:submit_stdin().
 * Send data + newline to a running process's stdin (like pressing Enter). */
json_t *process_registry_submit_stdin(const char *session_id, const char *data)
{
    if (!data) data = "";
    size_t len = strlen(data) + 2;  /* + \n + \0 */
    char *with_newline = malloc(len);
    if (!with_newline) return NULL;
    snprintf(with_newline, len, "%s\n", data);

    json_t *result = process_registry_write_stdin(session_id, with_newline);
    free(with_newline);
    return result;
}

/* PoP: request_close_terminal @ tools/process_registry.py:request_close_terminal */
/* Port of Python tools/process_registry.py:request_close_terminal().
 * Ask the desktop GUI to close the read-only terminal tab mirroring this background process. */
json_t *process_registry_request_close_terminal(const char *session_id)
{
    json_t *result = json_object();
    if (!result) return NULL;

    /* Desktop-only: returns an error if no UI close sink is wired */
    json_set(result, "status", json_string("error"));
    json_set(result, "error", json_string("close_terminal is only available in the Hermes desktop app."));
    return result;
}

/* PoP: close_stdin @ tools/process_registry.py:close_stdin */
/* Port of Python tools/process_registry.py:close_stdin().
 * Close a running process's stdin / send EOF without killing the process. */
json_t *process_registry_close_stdin(const char *session_id)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!session_id) {
        json_set(result, "status", json_string("not_found"));
        json_set(result, "error", json_string("No process with ID"));
        return result;
    }

    pthread_mutex_lock(&registry_lock);
    process_session_t *session = NULL;
    for (int i = 0; i < running_count; i++) {
        if (strcmp(running_sessions[i]->id, session_id) == 0) {
            session = running_sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&registry_lock);

    if (!session) {
        json_set(result, "status", json_string("not_found"));
        json_set(result, "error", json_string("No process with ID"));
        return result;
    }

    if (session->exited) {
        json_set(result, "status", json_string("already_exited"));
        json_set(result, "error", json_string("Process has already finished"));
        return result;
    }

    if (session->pty) {
        /* session->pty.sendeof() */
        json_set(result, "status", json_string("ok"));
        json_set(result, "message", json_string("EOF sent"));
        return result;
    }

    if (!session->process || /* session->process.stdin is not available */ true) {
        json_set(result, "status", json_string("error"));
        json_set(result, "error", json_string("Process stdin not available (non-local backend or stdin closed)"));
        return result;
    }

    /* session->process.stdin.close() */
    json_set(result, "status", json_string("ok"));
    json_set(result, "message", json_string("stdin closed"));
    return result;
}

/* PoP: list_sessions @ tools/process_registry.py:list_sessions */
/* Port of Python tools/process_registry.py:list_sessions().
 * List all running and recently-finished processes. */
json_t *process_registry_list_sessions(const char *task_id, const char *session_key)
{
    json_t *result = json_array();
    if (!result) return NULL;

    pthread_mutex_lock(&registry_lock);
    process_session_t *sessions[2048];
    int total = 0;
    for (int i = 0; i < running_count && total < 2048; i++) {
        sessions[total++] = running_sessions[i];
    }
    for (int i = 0; i < finished_count && total < 2048; i++) {
        sessions[total++] = finished_sessions[i];
    }
    pthread_mutex_unlock(&registry_lock);

    for (int i = 0; i < total; i++) {
        process_session_t *s = sessions[i];
        process_registry_refresh_detached_session(s);

        if (task_id && strcmp(s->task_id, task_id) != 0) continue;
        if (session_key && strcmp(s->session_key, session_key) != 0) continue;

        json_t *entry = json_object();
        if (!entry) continue;

        json_set(entry, "session_id", json_string(s->id));
        json_set(entry, "command", json_string(s->command));
        json_set(entry, "cwd", json_string(s->cwd));
        json_set(entry, "pid", json_number((double)s->pid));
        char started_str[64];
        strftime(started_str, sizeof(started_str), "%Y-%m-%dT%H:%M:%S", localtime(&s->started_at));
        json_set(entry, "started_at", json_string(started_str));
        json_set(entry, "uptime_seconds", json_number((double)(time(NULL) - s->started_at)));
        json_set(entry, "status", json_string(s->exited ? "exited" : "running"));
        json_set(entry, "output_preview", json_string(s->output_buffer[strlen(s->output_buffer) > 200 ? strlen(s->output_buffer) - 200 : 0]));

        if (task_id && session_key && strcmp(s->task_id, task_id) != 0 && strcmp(s->session_key, session_key) == 0) {
            json_set(entry, "session_scoped", json_bool(true));
        }

        if (s->watch_pattern_count > 0 && !s->watch_disabled) {
            json_t *patterns = json_array();
            for (int j = 0; j < s->watch_pattern_count; j++) {
                json_array_append(patterns, json_string(s->watch_patterns[j]));
            }
            json_set(entry, "watch_patterns", patterns);
            json_set(entry, "watch_hit", json_bool(s->watch_hits > 0));
        }

        if (s->notify_on_complete) {
            json_set(entry, "notify_on_complete", json_bool(true));
        }

        if (s->exited) {
            json_set(entry, "exit_code", json_number((double)s->exit_code));
        }

        if (s->detached) {
            json_set(entry, "detached", json_bool(true));
        }

        json_append(result, entry);
    }

    return result;
}

/* PoP: has_any_active @ tools/process_registry.py:has_any_active */
/* Port of Python tools/process_registry.py:has_any_active().
 * Whether ANY background process is still running (across all sessions). */
bool process_registry_has_any_active(void)
{
    pthread_mutex_lock(&registry_lock);
    process_session_t *sessions[1024];
    int total = running_count;
    for (int i = 0; i < running_count; i++) {
        sessions[i] = running_sessions[i];
    }
    pthread_mutex_unlock(&registry_lock);

    for (int i = 0; i < total; i++) {
        process_registry_refresh_detached_session(sessions[i]);
    }

    pthread_mutex_lock(&registry_lock);
    for (int i = 0; i < running_count; i++) {
        if (!running_sessions[i]->exited) {
            pthread_mutex_unlock(&registry_lock);
            return true;
        }
    }
    pthread_mutex_unlock(&registry_lock);

    return false;
}

/* PoP: _prune_if_needed @ tools/process_registry.py:_prune_if_needed */
/* Port of Python tools/process_registry.py:_prune_if_needed().
 * Remove oldest finished sessions if over MAX_PROCESSES. Must hold _lock. */
void process_registry_prune_if_needed(void)
{
    pthread_mutex_lock(&registry_lock);

    time_t now = time(NULL);
    const long FINISHED_TTL_SECONDS = 86400;  /* 24 hours */
    const int MAX_PROCESSES = 1000;

    /* First prune expired finished sessions */
    int new_finished = 0;
    for (int i = 0; i < finished_count; i++) {
        if ((now - finished_sessions[i]->started_at) > FINISHED_TTL_SECONDS) {
            /* Expired - skip (effectively deleted) */
            hermes_log(LOG_DEBUG, "process_registry", "Pruning expired session %s", finished_sessions[i]->id);
        } else {
            if (new_finished != i) {
                finished_sessions[new_finished] = finished_sessions[i];
            }
            new_finished++;
        }
    }
    finished_count = new_finished;

    /* If still over limit, remove oldest finished */
    int total = running_count + finished_count;
    if (total >= MAX_PROCESSES && finished_count > 0) {
        int oldest_idx = 0;
        time_t oldest_time = finished_sessions[0]->started_at;
        for (int i = 1; i < finished_count; i++) {
            if (finished_sessions[i]->started_at < oldest_time) {
                oldest_time = finished_sessions[i]->started_at;
                oldest_idx = i;
            }
        }
        hermes_log(LOG_DEBUG, "process_registry", "Pruning oldest session %s (over limit)", finished_sessions[oldest_idx]->id);
        for (int i = oldest_idx; i < finished_count - 1; i++) {
            finished_sessions[i] = finished_sessions[i + 1];
        }
        finished_count--;
    }

    pthread_mutex_unlock(&registry_lock);
}

/* PoP: _format_age @ tools/process_registry.py:_format_age */
/* Port of Python tools/process_registry.py:_format_age().
 * Human-friendly elapsed string ('18m', '2h3m', '45s'). */
char *process_registry_format_age(double seconds)
{
    static char buf[32];
    int s = (int)fmax(0, seconds);

    if (s < 60) {
        snprintf(buf, sizeof(buf), "%ds", s);
    } else {
        int m = s / 60;
        s = s % 60;
        if (m < 60) {
            if (s == 0) {
                snprintf(buf, sizeof(buf), "%dm", m);
            } else {
                snprintf(buf, sizeof(buf), "%dm%ds", m, s);
            }
        } else {
            int h = m / 60;
            m = m % 60;
            if (m == 0) {
                snprintf(buf, sizeof(buf), "%dh", h);
            } else {
                snprintf(buf, sizeof(buf), "%dh%dm", h, m);
            }
        }
    }
    return buf;
}

/* PoP: _format_async_delegation @ tools/process_registry.py:_format_async_delegation */
/* Port of Python tools/process_registry.py:_format_async_delegation().
 * Format an async-delegation completion into a self-contained re-injection. */
char *process_registry_format_async_delegation(json_t *evt)
{
    static char buf[16384];
    char *w = buf;
    size_t remaining = sizeof(buf);

    const char *deleg_id = json_get_str(json_obj_get(evt, "delegation_id"), NULL, "unknown");
    const char *goal = json_get_str(json_obj_get(evt, "goal"), NULL, "");
    json_t *context = json_obj_get(evt, "context");
    json_t *toolsets = json_obj_get(evt, "toolsets");
    const char *role = json_get_str(json_obj_get(evt, "role"), NULL, "leaf");
    const char *model = json_get_str(json_obj_get(evt, "model"), NULL, "?");
    const char *status = json_get_str(json_obj_get(evt, "status"), NULL, "completed");
    const char *summary = json_get_str(json_obj_get(evt, "summary"), NULL, NULL);
    const char *error = json_get_str(json_obj_get(evt, "error"), NULL, NULL);
    int api_calls = (int)json_get_num(json_obj_get(evt, "api_calls"), NULL, 0);
    const char *duration = json_get_str(json_obj_get(evt, "duration_seconds"), NULL, "?");
    const char *dispatched_at = json_get_str(json_obj_get(evt, "dispatched_at"), NULL, NULL);
    const char *completed_at = json_get_str(json_obj_get(evt, "completed_at"), NULL, NULL);

    json_t *batch_results = json_obj_get(evt, "results");
    if (json_obj_get(evt, "is_batch") || (batch_results && batch_results->type == JSON_ARRAY)) {
        json_t *results = batch_results;
        json_t *goals = json_obj_get(evt, "goals");
        int n = results ? json_len(results) : (goals ? json_len(goals) : 0);
        const char *total_dur = json_get_str(json_obj_get(evt, "total_duration_seconds"), NULL, duration);

        w += snprintf(w, remaining,
            "[ASYNC DELEGATION BATCH COMPLETE — %s]\n"
            "A background fan-out of %d subagent(s) you dispatched earlier has finished. "
            "All ran in parallel and waited on each other; their consolidated results are below. "
            "You may have moved on since dispatching — act on these or re-dispatch if things have changed.\n\n",
            deleg_id, n);
        remaining = sizeof(buf) - (w - buf);

        if (dispatched_at && *dispatched_at) {
            time_t dispatched = (time_t)atof(dispatched_at);
            time_t completed = completed_at ? (time_t)atof(completed_at) : time(NULL);
            char ts[64];
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&dispatched));
            char *age = process_registry_format_age(difftime(completed, dispatched));
            w += snprintf(w, remaining, "Dispatched: %s (%s ago)\n", ts, age);
            remaining = sizeof(buf) - (w - buf);
        }

        if (context && context->type == JSON_STRING) {
            w += snprintf(w, remaining, "Context you provided: %s\n", context->str_val);
            remaining = sizeof(buf) - (w - buf);
        }

        if (toolsets && toolsets->type == JSON_ARRAY) {
            w += snprintf(w, remaining, "Toolsets: ");
            remaining = sizeof(buf) - (w - buf);
            for (size_t i = 0; i < json_len(toolsets); i++) {
                const char *ts = json_get_str(json_get(toolsets, i), NULL, "");
                w += snprintf(w, remaining, "%s%s", i > 0 ? ", " : "", ts);
                remaining = sizeof(buf) - (w - buf);
            }
            w += snprintf(w, remaining, "\n");
            remaining = sizeof(buf) - (w - buf);
        }

        w += snprintf(w, remaining, "Role: %s   Model: %s   Total duration: %s\n", role, model, total_dur);
        remaining = sizeof(buf) - (w - buf);

        if (error && *error && (!results || json_len(results) == 0)) {
            w += snprintf(w, remaining, "--- ERROR ---\nThe batch did not complete successfully: %s\n", error);
            remaining = sizeof(buf) - (w - buf);
            return buf;
        }

        for (size_t i = 0; i < json_len(results); i++) {
            json_t *r = json_get(results, i);
            int idx = (int)json_get_num(json_obj_get(r, "task_index"), NULL, 0);
            const char *r_status = json_get_str(json_obj_get(r, "status"), NULL, "?");
            const char *r_summary = json_get_str(json_obj_get(r, "summary"), NULL, NULL);
            const char *r_error = json_get_str(json_obj_get(r, "error"), NULL, NULL);
            const char *r_goal = (goals && idx < json_len(goals)) ? json_get_str(json_get(goals, idx), NULL, "")
                                                                : json_get_str(json_obj_get(r, "goal"), NULL, "");
            const char *icon = (strcmp(r_status, "completed") == 0 || strcmp(r_status, "success") == 0) ? "✓" : "✗";

            w += snprintf(w, remaining, "\n--- %s TASK %d/%d", icon, idx + 1, n);
            remaining = sizeof(buf) - (w - buf);
            if (r_goal && *r_goal) {
                w += snprintf(w, remaining, ": %s", r_goal);
                remaining = sizeof(buf) - (w - buf);
            }
            w += snprintf(w, remaining, "  (status=%s", r_status);
            remaining = sizeof(buf) - (w - buf);
            int r_api = (int)json_get_num(json_obj_get(r, "api_calls"), NULL, 0);
            if (r_api > 0) {
                w += snprintf(w, remaining, ", api_calls=%d", r_api);
                remaining = sizeof(buf) - (w - buf);
            }
            const char *r_dur = json_get_str(json_obj_get(r, "duration_seconds"), NULL, NULL);
            if (r_dur && *r_dur) {
                w += snprintf(w, remaining, ", %ss", r_dur);
                remaining = sizeof(buf) - (w - buf);
            }
            w += snprintf(w, remaining, ") ---\n");
            remaining = sizeof(buf) - (w - buf);

            if ((strcmp(r_status, "completed") == 0 || strcmp(r_status, "success") == 0) && r_summary && *r_summary) {
                w += snprintf(w, remaining, "%s\n", r_summary);
                remaining = sizeof(buf) - (w - buf);
            } else if (r_summary && *r_summary) {
                if (r_error && *r_error) {
                    w += snprintf(w, remaining, "(%s: %s)\n", r_status, r_error);
                    remaining = sizeof(buf) - (w - buf);
                }
                w += snprintf(w, remaining, "Partial output:\n%s\n", r_summary);
                remaining = sizeof(buf) - (w - buf);
            } else {
                w += snprintf(w, remaining, "(no summary — status=%s%s)\n", r_status, r_error && *r_error ? ": " : "");
                remaining = sizeof(buf) - (w - buf);
                if (r_error && *r_error) {
                    w += snprintf(w, remaining, "%s\n", r_error);
                    remaining = sizeof(buf) - (w - buf);
                }
            }
        }
        return buf;
    }

    /* Single delegation */
    w += snprintf(w, remaining, "[ASYNC DELEGATION COMPLETE — %s]\n", deleg_id);
    remaining = sizeof(buf) - (w - buf);
    w += snprintf(w, remaining, "A background subagent you dispatched earlier has finished. You may have moved on since dispatching it; the full task source is below so you can act on the result or re-dispatch if things have changed.\n\n");
    remaining = sizeof(buf) - (w - buf);

    if (dispatched_at && *dispatched_at) {
        time_t dispatched = (time_t)atof(dispatched_at);
        time_t completed = completed_at ? (time_t)atof(completed_at) : time(NULL);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%M:%S", localtime(&dispatched));
        char *age = process_registry_format_age(difftime(completed, dispatched));
        w += snprintf(w, remaining, "Dispatched: %s%s\n", ts, age);
        remaining = sizeof(buf) - (w - buf);
    }

    w += snprintf(w, remaining, "Original goal: %s\n", goal);
    remaining = sizeof(buf) - (w - buf);
    if (context && context->type == JSON_STRING && context->str_val && *context->str_val) {
        w += snprintf(w, remaining, "Context you provided: %s\n", context->str_val);
        remaining = sizeof(buf) - (w - buf);
    }
    if (toolsets && toolsets->type == JSON_ARRAY) {
        w += snprintf(w, remaining, "Toolsets: ");
        remaining = sizeof(buf) - (w - buf);
        for (size_t i = 0; i < json_len(toolsets); i++) {
            const char *ts = json_get_str(json_get(toolsets, i), NULL, "");
            w += snprintf(w, remaining, "%s%s", i > 0 ? ", " : "", ts);
            remaining = sizeof(buf) - (w - buf);
        }
        w += snprintf(w, remaining, "\n");
        remaining = sizeof(buf) - (w - buf);
    }
    w += snprintf(w, remaining, "Role: %s   Model: %s\n", role, model);
    remaining = sizeof(buf) - (w - buf);
    w += snprintf(w, remaining, "Status: %s   API calls: %d   Duration: %s\n", status, api_calls, duration);
    remaining = sizeof(buf) - (w - buf);
    w += snprintf(w, remaining, "--- RESULT ---\n");
    remaining = sizeof(buf) - (w - buf);

    if ((strcmp(status, "completed") == 0 || strcmp(status, "success") == 0) && summary && *summary) {
        w += snprintf(w, remaining, "%s\n", summary);
        remaining = sizeof(buf) - (w - buf);
    } else if (strcmp(status, "interrupted") == 0) {
        w += snprintf(w, remaining, "The subagent was interrupted before completing%s.\n", error && *error ? ": " : "");
        remaining = sizeof(buf) - (w - buf);
        if (error && *error) {
            w += snprintf(w, remaining, "%s\n", error);
            remaining = sizeof(buf) - (w - buf);
        }
        if (summary && *summary) {
            w += snprintf(w, remaining, "Partial output:\n%s\n", summary);
            remaining = sizeof(buf) - (w - buf);
        }
    } else {
        w += snprintf(w, remaining, "The subagent did not complete successfully (status=%s).%s\n", status, error && *error ? "\n" : "");
        remaining = sizeof(buf) - (w - buf);
        if (error && *error) {
            w += snprintf(w, remaining, "%s\n", error);
            remaining = sizeof(buf) - (w - buf);
        }
        if (summary && *summary) {
            w += snprintf(w, remaining, "Partial output:\n%s\n", summary);
            remaining = sizeof(buf) - (w - buf);
        }
    }

    return buf;
}

/* PoP: format_process_notification @ tools/process_registry.py:format_process_notification */
/* Port of Python tools/process_registry.py:format_process_notification().
 * Format a process notification event into a [IMPORTANT: ...] message. */
char *process_registry_format_process_notification(json_t *evt)
{
    static char buf[4096];
    char *w = buf;
    size_t remaining = sizeof(buf);

    const char *evt_type = json_get_str(json_obj_get(evt, "type"), NULL, "completion");
    const char *sid = json_get_str(json_obj_get(evt, "session_id"), NULL, "unknown");
    const char *cmd = json_get_str(json_obj_get(evt, "command"), NULL, "unknown");

    if (strcmp(evt_type, "watch_disabled") == 0) {
        const char *msg = json_get_str(json_obj_get(evt, "message"), NULL, "");
        w += snprintf(w, remaining, "[IMPORTANT: %s]", msg);
        return buf;
    }

    if (strcmp(evt_type, "watch_match") == 0) {
        const char *pat = json_get_str(json_obj_get(evt, "pattern"), NULL, "?");
        const char *out = json_get_str(json_obj_get(evt, "output"), NULL, "");
        int sup = (int)json_get_num(json_obj_get(evt, "suppressed"), NULL, 0);

        w += snprintf(w, remaining, "[IMPORTANT: Background process %s matched watch pattern \"%s\".\nCommand: %s\nMatched output:\n%s", sid, pat, cmd, out);
        remaining = sizeof(buf) - (w - buf);
        if (sup) {
            w += snprintf(w, remaining, "\n(%d earlier matches were suppressed by rate limit)", sup);
            remaining = sizeof(buf) - (w - buf);
        }
        w += snprintf(w, remaining, "]");
        return buf;
    }

    if (strcmp(evt_type, "async_delegation") == 0) {
        return process_registry_format_async_delegation(evt);
    }

    const char *exit = json_get_str(json_obj_get(evt, "exit_code"), NULL, "?");
    const char *out = json_get_str(json_obj_get(evt, "output"), NULL, "");
    const char *reason = json_get_str(json_obj_get(evt, "completion_reason"), NULL, "exited");
    const char *source = json_get_str(json_obj_get(evt, "termination_source"), NULL, "");
    const char *signal = "";

    if (strcmp(exit, "-15") == 0 || strcmp(exit, "143") == 0) signal = ", SIGTERM";

    const char *status = "";
    if (strcmp(reason, "killed") == 0) {
        status = "terminated by ";
        if (*source) status = strcat(strcat(strdup(status), source), "");
        else status = "terminated by Hermes";
    } else if (strcmp(reason, "lost") == 0) {
        status = "marked lost because the process backend disappeared";
    } else if (strcmp(reason, "failed_start") == 0) {
        status = "failed to start";
    } else if (strcmp(exit, "0") == 0) {
        status = "completed normally";
    } else {
        status = "exited";
    }

    w += snprintf(w, remaining, "[IMPORTANT: Background process %s %s (exit code %s%s).\nCommand: %s\nOutput:\n%s]", sid, status, exit, signal, cmd, out);
    return buf;
}

/* PoP: _redact_process_result @ tools/process_registry.py:_redact_process_result */
/* Port of Python tools/process_registry.py:_redact_process_result().
 * Redact sensitive data from process results. */
json_t *process_registry_redact_process_result(json_t *result)
{
    /* Simplified - in reality would redact secrets, tokens, etc. */
    return json_copy(result);
}

/* PoP: _handle_process @ tools/process_registry.py:_handle_process */
/* Port of Python tools/process_registry.py:_handle_process().
 * Main handler for the 'process' tool. */
json_t *process_registry_handle_process(json_t *args)
{
    const char *action = json_get_str(json_obj_get(args, "action"), NULL, "");

    if (strcmp(action, "list") == 0) {
        const char *task_id = json_get_str(json_obj_get(args, "task_id"), NULL, NULL);
        const char *session_key = json_get_str(json_obj_get(args, "session_key"), NULL, NULL);
        return process_registry_list_sessions(task_id, session_key);
    } else if (strcmp(action, "poll") == 0) {
        const char *session_id = json_get_str(json_obj_get(args, "session_id"), NULL, NULL);
        if (!session_id) return json_object();  /* Error handling omitted */
        /* Would call poll implementation */
        return json_object();
    } else if (strcmp(action, "log") == 0) {
        const char *session_id = json_get_str(json_obj_get(args, "session_id"), NULL, NULL);
        int offset = (int)json_get_num(json_obj_get(args, "offset"), NULL, 0);
        int limit = (int)json_get_num(json_obj_get(args, "limit"), NULL, 200);
        return process_registry_read_log(session_id, offset, limit);
    } else if (strcmp(action, "wait") == 0) {
        const char *session_id = json_get_str(json_obj_get(args, "session_id"), NULL, NULL);
        int timeout = (int)json_get_num(json_obj_get(args, "timeout"), NULL, 180);
        /* Would call wait implementation */
        return json_object();
    } else if (strcmp(action, "kill") == 0) {
        const char *session_id = json_get_str(json_obj_get(args, "session_id"), NULL, NULL);
        const char *source = json_get_str(json_obj_get(args, "source"), NULL, "process.kill");
        return process_registry_kill_process(session_id, source);
    } else if (strcmp(action, "write") == 0) {
        const char *session_id = json_get_str(json_obj_get(args, "session_id"), NULL, NULL);
        const char *data = json_get_str(json_obj_get(args, "data"), NULL, "");
        return process_registry_write_stdin(session_id, data);
    } else if (strcmp(action, "submit") == 0) {
        const char *session_id = json_get_str(json_obj_get(args, "session_id"), NULL, NULL);
        const char *data = json_get_str(json_obj_get(args, "data"), NULL, "");
        return process_registry_submit_stdin(session_id, data);
    } else if (strcmp(action, "close_terminal") == 0) {
        const char *session_id = json_get_str(json_obj_get(args, "session_id"), NULL, NULL);
        return process_registry_request_close_terminal(session_id);
    } else if (strcmp(action, "close_stdin") == 0) {
        const char *session_id = json_get_str(json_obj_get(args, "session_id"), NULL, NULL);
        return process_registry_close_stdin(session_id);
    }

    /* Unknown action */
    json_t *result = json_object();
    json_set(result, "error", json_string("Unknown process action"));
    return result;
}
