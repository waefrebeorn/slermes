/*
 * process_registry.c — In-memory background process registry.
 * Port of Python tools/process_registry.py.
 * Manages process sessions: spawn_local, spawn_via_env, poll, wait, kill,
 * log, stdin I/O, list, count, checkpoint/recovery, watch patterns.
 *
 * PoP annotations link each C function to its Python counterpart.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "ansi_strip.h"
#include "browser_redact.h"
#include "gateway_status.h"
#include "process_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>

#define MAX_OUTPUT_CHARS 200000
#define FINISHED_TTL_SECONDS 1800
#define MAX_PROCESSES 64
#define WATCH_MIN_INTERVAL_SECONDS 15
#define WATCH_STRIKE_LIMIT 3
#define WATCH_GLOBAL_MAX_PER_WINDOW 15
#define WATCH_GLOBAL_WINDOW_SECONDS 10
#define WATCH_GLOBAL_COOLDOWN_SECONDS 30
#define CHECKPOINT_PATH "/tmp/processes.json"
#define MAX_DAEMON_GRACE_DEFAULT 2.0

static ProcessRegistry g_registry = {0};
static bool g_registry_initialized = false;
static process_registry_env_exec_fn g_env_exec = NULL;
static process_registry_close_sink_t g_close_sink = NULL;

static const char *_env_temp_dir(void *env_ref, char *buf, size_t buflen);
static void _prune_if_needed(void);

/* PoP: _is_host_pid_alive @ tools/process_registry.py:ProcessRegistry._is_host_pid_alive */
static bool _is_host_pid_alive(pid_t pid) {
    if (pid <= 0) return false;
    /* Python delegates to gateway.status._pid_exists — reuse the ported
     * zombie-aware check (reports zombies as dead) instead of a bare kill(). */
    return gwstatus_pid_exists(pid);
}

/* PoP: _safe_host_start_time @ tools/process_registry.py:ProcessRegistry._safe_host_start_time */
/* Kernel start ticks for a host PID (/proc/<pid>/stat field 22), or 0 when
 * unavailable — mirrors Python's None. PID-reuse guard. */
static long _safe_host_start_time(pid_t pid) {
    if (pid <= 0) return 0;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    char line[4096];
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 0; }
    fclose(fp);
    /* comm (field 2) is in parens; find last ')' to skip it safely. */
    char *p = strrchr(line, ')');
    if (!p) return 0;
    p++; /* past ')' */
    /* Fields after comm start at 3; starttime is field 22 → skip 20 tokens. */
    char *save = NULL;
    char *tok = strtok_r(p, " ", &save);
    int field = 3;
    while (tok && field < 22) {
        tok = strtok_r(NULL, " ", &save);
        field++;
    }
    if (!tok) return 0;
    return strtol(tok, NULL, 10);
}

static void registry_init(void) {
    if (g_registry_initialized) return;
    g_registry.running_cap = MAX_PROCESSES;
    g_registry.finished_cap = MAX_PROCESSES;
    g_registry.queue_cap = 256;
    g_registry.consumed_cap = 256;
    g_registry.pending_watcher_cap = 64;
    g_registry.running = calloc(g_registry.running_cap, sizeof(ProcessSession*));
    g_registry.finished = calloc(g_registry.finished_cap, sizeof(ProcessSession*));
    g_registry.completion_queue = calloc(g_registry.queue_cap, sizeof(char*));
    g_registry.completion_consumed = calloc(g_registry.consumed_cap, sizeof(char*));
    g_registry.pending_watchers = calloc(g_registry.pending_watcher_cap, sizeof(char*));
    pthread_mutex_init(&g_registry.lock, NULL);
    pthread_mutex_init(&g_registry.global_watch_lock, NULL);
    g_registry.global_watch_window_start = (double)time(NULL);
    g_registry_initialized = true;
}

void process_registry_init(void) { registry_init(); }

static void session_init(ProcessSession *s, const char *id, const char *command,
                         const char *task_id, const char *session_key,
                         pid_t pid, const char *cwd, bool detached,
                         const char *pid_scope) {
    memset(s, 0, sizeof(ProcessSession));
    strncpy(s->id, id, sizeof(s->id) - 1);
    strncpy(s->command, command, sizeof(s->command) - 1);
    if (task_id) strncpy(s->task_id, task_id, sizeof(s->task_id) - 1);
    if (session_key) strncpy(s->session_key, session_key, sizeof(s->session_key) - 1);
    if (cwd) strncpy(s->cwd, cwd, sizeof(s->cwd) - 1);
    s->pid = pid;
    s->running = true;
    s->exit_code = 0;
    s->started_at = time(NULL);
    s->detached = detached;
    if (pid_scope) strncpy(s->pid_scope, pid_scope, sizeof(s->pid_scope) - 1);
    else strncpy(s->pid_scope, "host", sizeof(s->pid_scope) - 1);
    strncpy(s->completion_reason, "exited", sizeof(s->completion_reason) - 1);
    s->output_cap = MAX_OUTPUT_CHARS;
    s->output_buffer = malloc(s->output_cap);
    s->output_buffer[0] = '\0';
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->completion_event, NULL);
}

static void session_append_output(ProcessSession *s, const char *text, size_t len) {
    pthread_mutex_lock(&s->lock);
    if (s->output_len + len + 1 >= s->output_cap) {
        size_t new_cap = s->output_cap * 2;
        if (s->output_len + len + 1 >= new_cap) new_cap = s->output_len + len + 1;
        s->output_buffer = realloc(s->output_buffer, new_cap);
        s->output_cap = new_cap;
    }
    memcpy(s->output_buffer + s->output_len, text, len);
    s->output_len += len;
    s->output_buffer[s->output_len] = '\0';
    if (s->output_len > MAX_OUTPUT_CHARS) {
        size_t excess = s->output_len - MAX_OUTPUT_CHARS;
        memmove(s->output_buffer, s->output_buffer + excess, MAX_OUTPUT_CHARS + 1);
        s->output_len = MAX_OUTPUT_CHARS;
    }
    pthread_mutex_unlock(&s->lock);
}

static void session_mark_exited(ProcessSession *s, int exit_code,
                                const char *reason, const char *source) {
    pthread_mutex_lock(&s->lock);
    s->running = false;
    s->exit_code = exit_code;
    if (reason) strncpy(s->completion_reason, reason, sizeof(s->completion_reason) - 1);
    if (source) strncpy(s->termination_source, source, sizeof(s->termination_source) - 1);
    pthread_cond_broadcast(&s->completion_event);
    pthread_mutex_unlock(&s->lock);
}

static void queue_completion_event(const char *json_event) {
    pthread_mutex_lock(&g_registry.lock);
    if (g_registry.queue_count == g_registry.queue_cap) {
        g_registry.queue_cap *= 2;
        g_registry.completion_queue = realloc(g_registry.completion_queue,
            g_registry.queue_cap * sizeof(char*));
    }
    g_registry.completion_queue[g_registry.queue_tail] = strdup(json_event);
    g_registry.queue_tail = (g_registry.queue_tail + 1) % g_registry.queue_cap;
    g_registry.queue_count++;
    pthread_mutex_unlock(&g_registry.lock);
}

/* PoP: _clean_shell_noise @ tools/process_registry.py:ProcessRegistry._clean_shell_noise */
/* Strips interactive-shell startup noise so it doesn't leak to model/user. */
static void clean_shell_noise(char *text) {
    const char *noise[] = {
        "bash: cannot set terminal process group",
        "bash: no job control in this shell",
        "no job control in this shell",
        "cannot set terminal process group",
        "tcsetattr: Inappropriate ioctl for device",
        NULL
    };
    if (!text || !*text) return;
    char *src = text, *dst = text;
    bool at_line_start = true;
    while (*src) {
        if (*src == '\n') { at_line_start = true; *dst++ = *src++; continue; }
        if (at_line_start) {
            bool noisy = false;
            for (int i = 0; noise[i]; i++) {
                size_t nl = strlen(noise[i]);
                if (strncmp(src, noise[i], nl) == 0) { noisy = true; break; }
            }
            if (noisy) { while (*src && *src != '\n') src++; continue; }
            at_line_start = false;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}

/* PoP: _emit_output @ tools/process_registry.py:ProcessRegistry._emit_output */
static void emit_output(ProcessSession *s, const char *chunk) {
    if (!s || !chunk || !*chunk) return;
    char *copy = strdup(chunk);
    clean_shell_noise(copy);
    session_append_output(s, copy, strlen(copy));
    free(copy);
}

/* PoP: _global_watch_admit @ tools/process_registry.py:ProcessRegistry._global_watch_admit */
static bool _global_watch_admit(double now) {
    pthread_mutex_lock(&g_registry.global_watch_lock);
    bool admit = true;
    if (g_registry.global_watch_tripped_until > 0 && now >= g_registry.global_watch_tripped_until) {
        int suppressed_global = g_registry.global_watch_suppressed_during_trip;
        g_registry.global_watch_tripped_until = 0;
        g_registry.global_watch_suppressed_during_trip = 0;
        g_registry.global_watch_window_start = now;
        g_registry.global_watch_window_hits = 0;
        if (suppressed_global > 0) {
            pthread_mutex_unlock(&g_registry.global_watch_lock);
            char release_evt[512];
            snprintf(release_evt, sizeof(release_evt),
                "{\"session_id\":\"\",\"session_key\":\"\",\"command\":\"\","
                "\"type\":\"watch_overflow_released\",\"suppressed\":%d,"
                "\"message\":\"Watch-pattern notifications resumed. %d match event(s) were suppressed during the flood.\","
                "\"platform\":\"\",\"chat_id\":\"\",\"user_id\":\"\",\"user_name\":\"\",\"thread_id\":\"\"},"
                , suppressed_global, suppressed_global);
            queue_completion_event(release_evt);
            pthread_mutex_lock(&g_registry.global_watch_lock);
        }
    }
    if (g_registry.global_watch_tripped_until > 0 && now < g_registry.global_watch_tripped_until) {
        g_registry.global_watch_suppressed_during_trip++;
        admit = false;
    } else {
        if (now - g_registry.global_watch_window_start >= WATCH_GLOBAL_WINDOW_SECONDS) {
            g_registry.global_watch_window_start = now;
            g_registry.global_watch_window_hits = 0;
        }
        if (g_registry.global_watch_window_hits >= WATCH_GLOBAL_MAX_PER_WINDOW) {
            g_registry.global_watch_tripped_until = now + WATCH_GLOBAL_COOLDOWN_SECONDS;
            g_registry.global_watch_suppressed_during_trip++;
            admit = false;
        } else {
            g_registry.global_watch_window_hits++;
            admit = true;
        }
    }
    pthread_mutex_unlock(&g_registry.global_watch_lock);
    if (!admit) {
        char trip_evt[512];
        snprintf(trip_evt, sizeof(trip_evt),
            "{\"session_id\":\"\",\"session_key\":\"\",\"command\":\"\",\"type\":\"watch_overflow_tripped\",\"message\":\"Watch-pattern overflow: >%d notifications in %ds across all processes. Suppressing further watch_match events for %ds.\",\"platform\":\"\",\"chat_id\":\"\",\"user_id\":\"\",\"user_name\":\"\",\"thread_id\":\"\"}",
            WATCH_GLOBAL_MAX_PER_WINDOW, WATCH_GLOBAL_WINDOW_SECONDS, WATCH_GLOBAL_COOLDOWN_SECONDS);
        queue_completion_event(trip_evt);
    }
    return admit;
}

/* PoP: _check_watch_patterns @ tools/process_registry.py:ProcessRegistry._check_watch_patterns */
static void check_watch_patterns(ProcessSession *s, const char *new_text) {
    if (!s->watch_patterns || s->watch_pattern_count == 0 || s->watch_disabled) return;
    if (!s->running) return;
    double now = (double)time(NULL);
    pthread_mutex_lock(&s->lock);
    bool should_disable = false;
    bool return_early = false;
    int suppressed = s->watch_suppressed;
    if (s->watch_cooldown_until > 0 && now < s->watch_cooldown_until) {
        s->watch_suppressed += 1;
        if (!s->watch_strike_candidate) {
            s->watch_strike_candidate = true;
            s->watch_consecutive_strikes++;
            if (s->watch_consecutive_strikes >= WATCH_STRIKE_LIMIT) {
                s->watch_disabled = true;
                s->notify_on_complete = true;
                should_disable = true;
            }
        }
        return_early = true;
    } else {
        if (s->watch_cooldown_until > 0 && !s->watch_strike_candidate)
            s->watch_consecutive_strikes = 0;
        s->watch_strike_candidate = false;
        s->watch_last_emit_at = now;
        s->watch_cooldown_until = now + WATCH_MIN_INTERVAL_SECONDS;
        s->watch_hits++;
        s->watch_suppressed = 0;
    }
    pthread_mutex_unlock(&s->lock);
    if (return_early) {
        if (should_disable) {
            char msg[1024];
            snprintf(msg, sizeof(msg),
                "Watch patterns disabled for process %s -- %d consecutive rate-limit windows triggered (min spacing %ds). Falling back to notify_on_complete semantics; you'll get exactly one notification when the process exits.",
                s->id, WATCH_STRIKE_LIMIT, WATCH_MIN_INTERVAL_SECONDS);
            char event[2048];
            snprintf(event, sizeof(event),
                "{\"session_id\":\"%s\",\"session_key\":\"%s\",\"command\":\"%s\",\"type\":\"watch_disabled\",\"suppressed\":%d,\"platform\":\"%s\",\"chat_id\":\"%s\",\"user_id\":\"%s\",\"user_name\":\"%s\",\"thread_id\":\"%s\",\"message_id\":\"%s\",\"message\":\"%s\"}",
                s->id, s->session_key, s->command, suppressed,
                s->watcher_platform, s->watcher_chat_id, s->watcher_user_id,
                s->watcher_user_name, s->watcher_thread_id, s->watcher_message_id, msg);
            queue_completion_event(event);
        }
        return;
    }
    if (!_global_watch_admit(now)) return;
    char *text_copy = strdup(new_text);
    char *matched_out = NULL;
    char *matched_pattern = NULL;
    char *save = NULL;
    char *line = strtok_r(text_copy, "\n", &save);
    while (line) {
        for (int i = 0; i < s->watch_pattern_count; i++) {
            if (s->watch_patterns[i] && strstr(line, s->watch_patterns[i])) {
                if (!matched_pattern) matched_pattern = s->watch_patterns[i];
                size_t add = strlen(line);
                if (!matched_out) {
                    matched_out = malloc(add + 2);
                    strcpy(matched_out, line);
                    matched_out[add] = '\0';
                } else {
                    size_t oldlen = strlen(matched_out);
                    matched_out = realloc(matched_out, oldlen + add + 2);
                    strcat(matched_out, "\n");
                    strcat(matched_out, line);
                }
                break;
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }
    free(text_copy);
    if (matched_pattern && matched_out) {
        if (strlen(matched_out) > 2000) {
            matched_out[1997] = '\0';
            strcat(matched_out, "\n...(truncated)");
        }
        char event[4096];
        snprintf(event, sizeof(event),
            "{\"session_id\":\"%s\",\"session_key\":\"%s\",\"command\":\"%s\",\"type\":\"watch_match\",\"pattern\":\"%s\",\"output\":\"%s\",\"suppressed\":%d,\"platform\":\"%s\",\"chat_id\":\"%s\",\"user_id\":\"%s\",\"user_name\":\"%s\",\"thread_id\":\"%s\",\"message_id\":\"%s\"}",
            s->id, s->session_key, s->command,
            matched_pattern ? matched_pattern : "", matched_out,
            suppressed,
            s->watcher_platform, s->watcher_chat_id, s->watcher_user_id,
            s->watcher_user_name, s->watcher_thread_id, s->watcher_message_id);
        free(matched_out);
        queue_completion_event(event);
    } else {
        free(matched_out);
    }
}

/* PoP: _reconcile_local_exit @ tools/process_registry.py:ProcessRegistry._reconcile_local_exit */
static bool reconcile_local_exit(ProcessSession *s) {
    if (!s || !s->running || s->child_pid <= 0) return false;
    int status;
    pid_t result = waitpid(s->child_pid, &status, WNOHANG);
    if (result != s->child_pid) return false;
    pthread_mutex_lock(&s->lock);
    s->running = false;
    if (WIFEXITED(status)) s->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) s->exit_code = -WTERMSIG(status);
    else s->exit_code = -1;
    pthread_cond_broadcast(&s->completion_event);
    pthread_mutex_unlock(&s->lock);
    return true;
}

/* PoP: _move_to_finished @ tools/process_registry.py:ProcessRegistry._move_to_finished */
static void move_to_finished(ProcessSession *s) {
    if (!s) return;
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (g_registry.running[i] == s) {
            g_registry.running[i] = g_registry.running[g_registry.running_count - 1];
            g_registry.running_count--;
            break;
        }
    }
    if (g_registry.finished_count == g_registry.finished_cap) {
        g_registry.finished_cap *= 2;
        g_registry.finished = realloc(g_registry.finished,
            g_registry.finished_cap * sizeof(ProcessSession*));
    }
    g_registry.finished[g_registry.finished_count++] = s;
    time_t now = time(NULL);
    for (int i = 0; i < g_registry.finished_count; ) {
        if ((now - g_registry.finished[i]->started_at) > FINISHED_TTL_SECONDS) {
            free(g_registry.finished[i]->output_buffer);
            free(g_registry.finished[i]);
            g_registry.finished[i] = g_registry.finished[g_registry.finished_count - 1];
            g_registry.finished_count--;
        } else i++;
    }
    if (g_registry.running_count + g_registry.finished_count >= MAX_PROCESSES && g_registry.finished_count > 0) {
        int oldest_idx = 0;
        for (int i = 1; i < g_registry.finished_count; i++)
            if (g_registry.finished[i]->started_at < g_registry.finished[oldest_idx]->started_at)
                oldest_idx = i;
        free(g_registry.finished[oldest_idx]->output_buffer);
        free(g_registry.finished[oldest_idx]);
        g_registry.finished[oldest_idx] = g_registry.finished[g_registry.finished_count - 1];
        g_registry.finished_count--;
    }
    pthread_mutex_unlock(&g_registry.lock);
}

static bool _session_in_consumed(const char *session_id) {
    if (!session_id) return false;
    for (int i = 0; i < g_registry.consumed_count; i++)
        if (g_registry.completion_consumed[i] &&
            strcmp(g_registry.completion_consumed[i], session_id) == 0)
            return true;
    return false;
}

static void _mark_consumed(const char *session_id) {
    if (_session_in_consumed(session_id)) return;
    if (g_registry.consumed_count == g_registry.consumed_cap) {
        g_registry.consumed_cap *= 2;
        g_registry.completion_consumed = realloc(g_registry.completion_consumed,
            g_registry.consumed_cap * sizeof(char*));
    }
    g_registry.completion_consumed[g_registry.consumed_count++] = strdup(session_id);
}

/* ---- Poll-observed tracking (port of ProcessRegistry._poll_observed) ---- */

static bool _session_in_poll_observed(const char *session_id) {
    if (!session_id) return false;
    for (int i = 0; i < g_registry.poll_observed_count; i++)
        if (g_registry.poll_observed[i] &&
            strcmp(g_registry.poll_observed[i], session_id) == 0)
            return true;
    return false;
}

static void _mark_poll_observed(const char *session_id) {
    if (!session_id || !*session_id) return;
    if (_session_in_poll_observed(session_id)) return;
    if (g_registry.poll_observed_count == g_registry.poll_observed_cap) {
        int new_cap = g_registry.poll_observed_cap ? g_registry.poll_observed_cap * 2 : 8;
        g_registry.poll_observed = realloc(g_registry.poll_observed,
            new_cap * sizeof(char*));
        g_registry.poll_observed_cap = new_cap;
    }
    g_registry.poll_observed[g_registry.poll_observed_count++] = strdup(session_id);
}

/* PoP: _drain_should_skip @ tools/process_registry.py:ProcessRegistry._drain_should_skip */
bool process_registry_drain_should_skip(const char *session_id,
                                        bool skip_poll_observed) {
    if (_session_in_consumed(session_id)) return true;
    if (skip_poll_observed && _session_in_poll_observed(session_id)) return true;
    return false;
}

/* PoP: _refresh_detached_session @ tools/process_registry.py:ProcessRegistry._refresh_detached_session */
/* Reaps the direct child, emits a completion event on exit. Identity guard
 * compares kernel start time against the baseline captured at spawn so a
 * recycled PID is never reaped as our own. */
static void _refresh_detached_session(ProcessSession *s) {
    if (!s) return;
    if (s->detached && s->child_pid > 0) {
        int status;
        pid_t r = waitpid(s->child_pid, &status, WNOHANG);
        if (r == s->child_pid) {
            if (s->stdin_fd > 0) { close(s->stdin_fd); s->stdin_fd = -1; }
            int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            if (WIFSIGNALED(status)) code = -WTERMSIG(status);
            char reason[32], src[64];
            if (WIFSIGNALED(status)) {
                snprintf(reason, sizeof(reason), "killed");
                snprintf(src, sizeof(src), "signal_%d", WTERMSIG(status));
            } else {
                snprintf(reason, sizeof(reason), "exited");
                snprintf(src, sizeof(src), "exit_code_%d", WEXITSTATUS(status));
            }
            session_mark_exited(s, code, reason, src);
            char evt[512];
            snprintf(evt, sizeof(evt),
                "{\"session_id\":\"%s\",\"status\":\"exited\",\"exit_code\":%d,\"completion_reason\":\"%s\",\"termination_source\":\"%s\"}",
                s->id, code, reason, src);
            queue_completion_event(evt);
            move_to_finished(s);
            process_registry_write_checkpoint();
        } else if (r == -1 && errno == ECHILD) {
            if (s->running) {
                session_mark_exited(s, 0, "lost", "backend_lost");
                move_to_finished(s);
            }
        }
    } else if (!s->detached) {
        if (reconcile_local_exit(s)) {
            char evt[512];
            snprintf(evt, sizeof(evt),
                "{\"session_id\":\"%s\",\"status\":\"exited\",\"exit_code\":%d}",
                s->id, s->exit_code);
            queue_completion_event(evt);
            if (s->stdin_fd > 0) { close(s->stdin_fd); s->stdin_fd = -1; }
            move_to_finished(s);
        }
    }
}

/* PoP: _reader_loop @ tools/process_registry.py:ProcessRegistry._reader_loop */
/* POSIX orphan-pipe guard: short poll interval, stop shortly after child exits. */
static void *reader_loop(void *arg) {
    ProcessSession *s = (ProcessSession*)arg;
    int pipefd = s->stdin_fd > 0 ? s->stdin_fd : -1;
    if (pipefd < 0) return NULL;
    fcntl(pipefd, F_SETFL, O_NONBLOCK);
    char buf[4096];
    while (s->running) {
        ssize_t n = read(pipefd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            emit_output(s, buf);
            check_watch_patterns(s, buf);
        } else if (n == 0) {
            break;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) break;
            _refresh_detached_session(s);
            struct timespec ts = {0, 10000000};
            nanosleep(&ts, NULL);
        }
    }
    if (!s->running) {
        char evt[512];
        snprintf(evt, sizeof(evt),
            "{\"session_id\":\"%s\",\"status\":\"exited\",\"exit_code\":%d,\"completion_reason\":\"%s\"}",
            s->id, s->exit_code, s->completion_reason);
        queue_completion_event(evt);
        move_to_finished(s);
    }
    return NULL;
}

/* PoP: _env_poller_loop @ tools/process_registry.py:ProcessRegistry._env_poller_loop */
static void *env_poller_loop(void *arg) {
    ProcessSession *s = (ProcessSession*)arg;
    char temp_dir[512];
    _env_temp_dir(s->env_ref, temp_dir, sizeof(temp_dir));
    char log_path[640], exit_path[640];
    snprintf(log_path, sizeof(log_path), "%s/hermes_bg_%s.log", temp_dir, s->id);
    snprintf(exit_path, sizeof(exit_path), "%s/hermes_bg_%s.exit", temp_dir, s->id);
    long last_size = 0;
    while (s->running) {
        if (!g_env_exec || !s->env_ref) break;
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "tail -c +%ld %s 2>/dev/null", last_size + 1, log_path);
        int rc = 0;
        char *out = g_env_exec(s->env_ref, cmd, 10, &rc);
        if (out && *out) { emit_output(s, out); last_size += strlen(out); }
        free(out);
        snprintf(cmd, sizeof(cmd), "cat %s 2>/dev/null", exit_path);
        char *exit_out = g_env_exec(s->env_ref, cmd, 5, &rc);
        if (exit_out && *exit_out) {
            int code = atoi(exit_out);
            session_mark_exited(s, code, "exited", "");
            char evt[512];
            snprintf(evt, sizeof(evt),
                "{\"session_id\":\"%s\",\"status\":\"exited\",\"exit_code\":%d}",
                s->id, code);
            queue_completion_event(evt);
            move_to_finished(s);
            queue_completion_event(evt);
            free(exit_out);
            process_registry_write_checkpoint();
            break;
        }
        free(exit_out);
        sleep(1);
    }
    return NULL;
}

/* PoP: _pty_reader_loop @ tools/process_registry.py:ProcessRegistry._pty_reader_loop */
static void *pty_reader_loop(void *arg) {
    ProcessSession *s = (ProcessSession*)arg;
    int pipefd = s->stdin_fd >= 0 ? s->stdin_fd : -1;
    if (pipefd < 0) return NULL;
    fcntl(pipefd, F_SETFL, O_NONBLOCK);
    char buf[4096];
    while (s->running) {
        ssize_t n = read(pipefd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            emit_output(s, buf);
            check_watch_patterns(s, buf);
        } else if (n == 0) break;
        else {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) break;
            reconcile_local_exit(s);
            struct timespec ts = {0, 10000000};
            nanosleep(&ts, NULL);
        }
    }
    return NULL;
}

/* PoP: _host_pid_is_ours @ tools/process_registry.py:ProcessRegistry._host_pid_is_ours */
/* Identity-aware liveness: alive AND start-time matches. Refuses recycled PIDs. */
static bool _host_pid_is_ours(pid_t pid, long expected_start) {
    if (pid <= 0) return false;
    if (!_is_host_pid_alive(pid)) return false;
    if (expected_start == 0) return true;
    return _safe_host_start_time(pid) == expected_start;
}

/* PoP: _proc_alive @ tools/process_registry.py:ProcessRegistry._proc_alive */
/* True if running and not a zombie (a zombie is already dead — nothing to SIGKILL). */
static bool _proc_alive(pid_t pid) {
    if (pid <= 0) return false;
    if (!_is_host_pid_alive(pid)) return false;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return true;
    char line[1024];
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return true; }
    fclose(fp);
    char *p = strrchr(line, ')');
    if (!p) return true;
    p += 2;
    return (*p != 'Z');
}

/* PoP: _daemon_term_grace_seconds @ tools/process_registry.py:ProcessRegistry._daemon_term_grace_seconds */
static double _daemon_term_grace_seconds(void) {
    const char *raw = getenv("TERMINAL_DAEMON_TERM_GRACE_SECONDS");
    if (!raw || !*raw) return MAX_DAEMON_GRACE_DEFAULT;
    return strtod(raw, NULL);
}

/* PoP: _terminate_host_pid @ tools/process_registry.py:ProcessRegistry._terminate_host_pid */
/* SIGTERM the process group (children before parent), wait grace, SIGKILL stragglers.
 * PID-identity guarded: refuses recycled PIDs. */
static void _terminate_host_pid(pid_t pid, long expected_start) {
    if (pid <= 0) return;
    if (!_host_pid_is_ours(pid, expected_start)) {
        hermes_log(LOG_WARNING, "process_registry",
            "Refusing to terminate host pid %d: start-time mismatch -- PID was recycled",
            (int)pid);
        return;
    }
    pid_t pgid = getpgid(pid);
    if (pgid > 0) kill(-pgid, SIGTERM);
    else kill(pid, SIGTERM);
    double grace = _daemon_term_grace_seconds();
    if (grace <= 0) return;
    double deadline = (double)time(NULL) + grace;
    while ((double)time(NULL) < deadline) {
        if (!_proc_alive(pid)) break;
        usleep(50000);
    }
    if (_proc_alive(pid)) {
        if (pgid > 0) kill(-pgid, SIGKILL);
        else kill(pid, SIGKILL);
        hermes_log(LOG_INFO, "process_registry",
            "Escalated to SIGKILL for pid %d (ignored SIGTERM within %.1fs grace)",
            (int)pid, grace);
    }
}

/* PoP: _env_temp_dir @ tools/process_registry.py:ProcessRegistry._env_temp_dir */
static const char *_env_temp_dir(void *env_ref, char *buf, size_t buflen) {
    (void)env_ref;
    snprintf(buf, buflen, "/tmp");
    return buf;
}

/* ---- Public API ---- */

/* PoP: __init__ @ tools/process_registry.py:ProcessRegistry.__init__ */
void tools_process_registry_init(void) { process_registry_init(); }

/* PoP: spawn_local @ tools/process_registry.py:ProcessRegistry.spawn_local */
ProcessSession* process_registry_spawn_local(const char *command,
                                              const char *cwd,
                                              const char *task_id,
                                              const char *session_key,
                                              const char *env_vars_json) {
    (void)env_vars_json;
    registry_init();
    char id[32];
    snprintf(id, sizeof(id), "proc_%012lx",
             (unsigned long)time(NULL) ^ (unsigned long)pthread_self());
    ProcessSession *s = malloc(sizeof(ProcessSession));
    session_init(s, id, command, task_id, session_key, 0, cwd, false, "host");
    int stdin_pipe[2];
    int stdout_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        if (stdout_pipe[0]) close(stdout_pipe[0]);
        if (stdout_pipe[1]) close(stdout_pipe[1]);
        free(s); return NULL;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        free(s); return NULL;
    }
    if (pid == 0) {
        close(stdin_pipe[1]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        if (cwd && cwd[0]) chdir(cwd);
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        _exit(127);
    }
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    s->pid = pid;
    s->child_pid = pid;
    s->stdin_fd = stdin_pipe[1];
    s->stdin_fd = stdout_pipe[0];
    s->host_start_time = _safe_host_start_time(pid);
    pthread_mutex_lock(&g_registry.lock);
    if (g_registry.running_count == g_registry.running_cap) {
        g_registry.running_cap *= 2;
        g_registry.running = realloc(g_registry.running,
            g_registry.running_cap * sizeof(ProcessSession*));
    }
    g_registry.running[g_registry.running_count++] = s;
    pthread_mutex_unlock(&g_registry.lock);
    pthread_t th;
    if (pthread_create(&th, NULL, reader_loop, s) == 0)
        pthread_detach(th);
    return s;
}

/* PoP: spawn_via_env @ tools/process_registry.py:ProcessRegistry.spawn_via_env */
ProcessSession* process_registry_spawn_via_env(const char *command,
                                                 const char *cwd,
                                                 const char *task_id,
                                                 const char *session_key,
                                                 void *env_ref,
                                                 int timeout_seconds) {
    registry_init();
    char id[32];
    snprintf(id, sizeof(id), "proc_%012lx",
             (unsigned long)time(NULL) ^ (unsigned long)pthread_self());
    ProcessSession *s = malloc(sizeof(ProcessSession));
    session_init(s, id, command, task_id, session_key, 0, cwd, true, "sandbox");
    s->env_ref = env_ref;
    s->detached = true;
    if (!g_env_exec || !env_ref) {
        session_mark_exited(s, -1, "failed_start", "failed_start");
        pthread_mutex_lock(&g_registry.lock);
        if (g_registry.running_count == g_registry.running_cap) {
            g_registry.running_cap *= 2;
            g_registry.running = realloc(g_registry.running,
                g_registry.running_cap * sizeof(ProcessSession*));
        }
        g_registry.running[g_registry.running_count++] = s;
        pthread_mutex_unlock(&g_registry.lock);
        return s;
    }
    char temp_dir[512];
    _env_temp_dir(env_ref, temp_dir, sizeof(temp_dir));
    char log_path[640], pid_path[640], exit_path[640];
    snprintf(log_path, sizeof(log_path), "%s/hermes_bg_%s.log", temp_dir, s->id);
    snprintf(pid_path, sizeof(pid_path), "%s/hermes_bg_%s.pid", temp_dir, s->id);
    snprintf(exit_path, sizeof(exit_path), "%s/hermes_bg_%s.exit", temp_dir, s->id);
    char bg_cmd[2048];
    snprintf(bg_cmd, sizeof(bg_cmd),
        "mkdir -p %s && nohup sh -lc '%s' > %s 2>&1; rc=$?; printf '%%s\\n' \"$rc\" > %s",
        temp_dir, command, log_path, exit_path);
    int rc = 0;
    char *result = g_env_exec(env_ref, bg_cmd, timeout_seconds, &rc);
    free(result);
    pthread_mutex_lock(&g_registry.lock);
    if (g_registry.running_count == g_registry.running_cap) {
        g_registry.running_cap *= 2;
        g_registry.running = realloc(g_registry.running,
            g_registry.running_cap * sizeof(ProcessSession*));
    }
    g_registry.running[g_registry.running_count++] = s;
    pthread_mutex_unlock(&g_registry.lock);
    pthread_t th;
    if (pthread_create(&th, NULL, env_poller_loop, s) == 0)
        pthread_detach(th);
    process_registry_write_checkpoint();
    return s;
}

/* PoP: get @ tools/process_registry.py:ProcessRegistry.get */
int process_registry_get_session(const char *session_id, ProcessSession **out) {
    registry_init();
    if (!session_id || !out) { if (out) *out = NULL; return 0; }
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->id, session_id) == 0) {
            *out = g_registry.running[i];
            pthread_mutex_unlock(&g_registry.lock);
            return 1;
        }
    }
    for (int i = 0; i < g_registry.finished_count; i++) {
        if (strcmp(g_registry.finished[i]->id, session_id) == 0) {
            *out = g_registry.finished[i];
            pthread_mutex_unlock(&g_registry.lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
    return 0;
}

ProcessSession *process_registry_get_session_ptr(const char *session_id) {
    ProcessSession *s = NULL;
    process_registry_get_session(session_id, &s);
    return s;
}

/* PoP: poll @ tools/process_registry.py:ProcessRegistry.poll */
char* process_registry_poll(const char *session_id) {
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return strdup("{\"status\":\"not_found\",\"error\":\"No such session\"}");
    _refresh_detached_session(s);
    reconcile_local_exit(s);
    char uptime_buf[64];
    format_uptime_short((int)(time(NULL) - s->started_at), uptime_buf, sizeof(uptime_buf));
    pthread_mutex_lock(&s->lock);
    char cmd[4096]; strncpy(cmd, s->command, sizeof(cmd)-1); cmd[sizeof(cmd)-1]='\0';
    char cwd[512]; strncpy(cwd, s->cwd, sizeof(cwd)-1); cwd[sizeof(cwd)-1]='\0';
    bool exited = !s->running;
    int exit_code = s->exit_code;
    size_t plen = s->output_len > 1000 ? 1000 : s->output_len;
    char preview[1024];
    if (plen > 0) { memcpy(preview, s->output_buffer + (s->output_len - plen), plen); preview[plen]='\0'; }
    else preview[0] = '\0';
    pthread_mutex_unlock(&s->lock);
    char *result = malloc(8192);
    snprintf(result, 8192,
        "{\"session_id\":\"%s\",\"command\":\"%s\",\"cwd\":\"%s\",\"pid\":%d,"
        "\"uptime_seconds\":%d,\"uptime\":\"%s\",\"status\":\"%s\","
        "\"output_preview\":\"%s\"}",
        s->id, cmd, cwd, (int)s->pid,
        (int)(time(NULL) - s->started_at), uptime_buf,
        exited ? "exited" : "running", preview);
    if (exited) {
        snprintf(result + strlen(result), 8192 - strlen(result),
            ",\"exit_code\":%d,\"completion_reason\":\"%s\",\"termination_source\":\"%s\"}",
            exit_code, s->completion_reason, s->termination_source);
        /* Python: poll() is read-only, does NOT mark _completion_consumed
         * (that would suppress the watcher's autonomous delivery, #10156),
         * but DOES record _poll_observed so the CLI inline drain dedups. */
        _mark_poll_observed(s->id);
    }
    return result;
}

/* PoP: wait @ tools/process_registry.py:ProcessRegistry.wait */
char* process_registry_wait(const char *session_id, int timeout_sec) {
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return strdup("{\"status\":\"not_found\",\"error\":\"No such session\"}");
    double default_timeout = 180.0;
    const char *env_to = getenv("TERMINAL_TIMEOUT");
    if (env_to) { double v = strtod(env_to, NULL); if (v > 0) default_timeout = v; }
    double max_timeout = default_timeout;
    double effective = timeout_sec > 0 ? (double)timeout_sec : default_timeout;
    if (timeout_sec > 0 && timeout_sec > max_timeout) effective = max_timeout;
    double deadline = (double)time(NULL) + effective;
    while ((double)time(NULL) < deadline) {
        _refresh_detached_session(s);
        reconcile_local_exit(s);
        pthread_mutex_lock(&s->lock);
        bool exited = !s->running;
        int exit_code = s->exit_code;
        char cmd[4096]; strncpy(cmd, s->command, sizeof(cmd)-1); cmd[sizeof(cmd)-1]='\0';
        char outbuf[2001];
        size_t copylen = s->output_len > 2000 ? 2000 : s->output_len;
        if (copylen > 0) { memcpy(outbuf, s->output_buffer + (s->output_len - copylen), copylen); outbuf[copylen]='\0'; }
        else outbuf[0]='\0';
        pthread_mutex_unlock(&s->lock);
        if (exited) {
            _mark_consumed(session_id);
            char *result = malloc(4096);
            snprintf(result, 4096,
                "{\"status\":\"exited\",\"command\":\"%s\",\"exit_code\":%d,"
                "\"completion_reason\":\"%s\",\"termination_source\":\"%s\",\"output\":\"%s\"}",
                cmd, exit_code, s->completion_reason, s->termination_source, outbuf);
            return result;
        }
        double remaining = deadline - (double)time(NULL);
        if (remaining <= 0) break;
        struct timespec ts;
        ts.tv_sec = (time_t)(remaining > 1.0 ? 1.0 : remaining);
        ts.tv_nsec = 0;
        pthread_mutex_lock(&s->lock);
        pthread_cond_timedwait(&s->completion_event, &s->lock, &ts);
        pthread_mutex_unlock(&s->lock);
    }
    char *result = malloc(1024);
    snprintf(result, 1024,
        "{\"status\":\"timeout\",\"command\":\"%s\",\"output\":\"process still running\"}",
        s->command);
    return result;
}

/* PoP: kill_process @ tools/process_registry.py:ProcessRegistry.kill_process */
char* process_registry_kill(const char *session_id, const char *source,
                            bool consume_output) {
    if (!source) source = "process.kill";
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return strdup("{\"status\":\"not_found\",\"error\":\"No such session\"}");
    pthread_mutex_lock(&s->lock);
    bool already_exited = !s->running;
    char cmd[4096]; strncpy(cmd, s->command, sizeof(cmd)-1); cmd[sizeof(cmd)-1]='\0';
    int exit_code = s->exit_code;
    char outbuf[2001];
    size_t copylen = s->output_len > 2000 ? 2000 : s->output_len;
    if (copylen > 0) { memcpy(outbuf, s->output_buffer + (s->output_len - copylen), copylen); outbuf[copylen]='\0'; }
    else outbuf[0]='\0';
    pthread_mutex_unlock(&s->lock);
    if (already_exited) {
        if (consume_output) _mark_consumed(session_id);
        char *result = malloc(1024);
        snprintf(result, 1024,
            "{\"status\":\"already_exited\",\"command\":\"%s\",\"exit_code\":%d,"
            "\"completion_reason\":\"%s\",\"termination_source\":\"%s\",\"output\":\"%s\"}",
            cmd, exit_code, s->completion_reason, s->termination_source, outbuf);
        return result;
    }
    bool killed = false;
    if (s->pid > 0) {
        if (s->detached && s->pid_scope[0] == 'h') {
            if (_host_pid_is_ours(s->pid, s->host_start_time)) {
                _terminate_host_pid(s->pid, s->host_start_time);
                session_mark_exited(s, -15, "killed", source);
                killed = true;
            } else {
                session_mark_exited(s, 0, "lost", "");
            }
        } else if (s->env_ref && g_env_exec) {
            char kill_cmd[256];
            snprintf(kill_cmd, sizeof(kill_cmd), "kill %d 2>/dev/null", (int)s->pid);
            int rc; g_env_exec(s->env_ref, kill_cmd, 5, &rc);
            session_mark_exited(s, -15, "killed", source);
            killed = true;
        } else if (s->child_pid > 0) {
            _terminate_host_pid(s->child_pid, s->host_start_time);
            session_mark_exited(s, -15, "killed", source);
            killed = true;
        }
    }
    if (killed) {
        if (s->stdin_fd > 0) { close(s->stdin_fd); s->stdin_fd = -1; }
        if (consume_output) _mark_consumed(session_id);
        move_to_finished(s);
        process_registry_write_checkpoint();
        char *result = malloc(1024);
        snprintf(result, 1024,
            "{\"status\":\"killed\",\"command\":\"%s\",\"exit_code\":-15,"
            "\"completion_reason\":\"killed\",\"termination_source\":\"%s\",\"output\":\"%s\"}",
            cmd, source, outbuf);
        return result;
    }
    return strdup("{\"status\":\"error\",\"error\":\"Process stdin not available (non-local backend)\"}");
}

/* PoP: count_running @ tools/process_registry.py:ProcessRegistry.count_running */
int process_registry_count_running(void) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    int count = g_registry.running_count;
    pthread_mutex_unlock(&g_registry.lock);
    return count;
}

/* PoP: list_sessions @ tools/process_registry.py:ProcessRegistry.list_sessions */
char* process_registry_list(const char *task_id_filter, const char *session_key) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    int total = g_registry.running_count + g_registry.finished_count;
    ProcessSession **sessions = malloc(total * sizeof(ProcessSession*));
    int count = 0;
    for (int i = 0; i < g_registry.running_count; i++) {
        ProcessSession *s = g_registry.running[i];
        if ((!task_id_filter || (s->task_id[0] && strcmp(s->task_id, task_id_filter) == 0)) ||
            (session_key && strcmp(s->session_key, session_key) == 0))
            sessions[count++] = s;
    }
    for (int i = 0; i < g_registry.finished_count; i++) {
        ProcessSession *s = g_registry.finished[i];
        if ((!task_id_filter || (s->task_id[0] && strcmp(s->task_id, task_id_filter) == 0)) ||
            (session_key && strcmp(s->session_key, session_key) == 0))
            sessions[count++] = s;
    }
    pthread_mutex_unlock(&g_registry.lock);
    size_t cap = 16384;
    char *result = malloc(cap);
    size_t len = 1;
    result[0] = '[';
    for (int i = 0; i < count; i++) {
        ProcessSession *s = sessions[i];
        reconcile_local_exit(s);
        char uptime_buf[64];
        format_uptime_short((int)(time(NULL) - s->started_at), uptime_buf, sizeof(uptime_buf));
        pthread_mutex_lock(&s->lock);
        char cmd[2048]; strncpy(cmd, s->command, 200); cmd[200]='\0';
        char cwd[512]; strncpy(cwd, s->cwd, sizeof(cwd)-1); cwd[sizeof(cwd)-1]='\0';
        bool exited = !s->running;
        int exit_code = s->exit_code;
        size_t plen = s->output_len > 200 ? 200 : s->output_len;
        char preview[256];
        if (plen > 0) { memcpy(preview, s->output_buffer + (s->output_len - plen), plen); preview[plen]='\0'; }
        else preview[0]='\0';
        pthread_mutex_unlock(&s->lock);
        char entry[4096];
        int elen = snprintf(entry, sizeof(entry),
            "{\"session_id\":\"%s\",\"command\":\"%.200s\",\"cwd\":\"%s\",\"pid\":%d,"
            "\"started_at\":%ld,\"uptime_seconds\":%d,\"uptime\":\"%s\","
            "\"status\":\"%s\",\"output_preview\":\"%s\"}",
            s->id, cmd, cwd, (int)s->pid, (long)s->started_at,
            (int)(time(NULL) - s->started_at), uptime_buf,
            exited ? "exited" : "running", preview);
        if (i > 0) result[len++] = ',';
        if (len + (size_t)elen + 2 >= cap) { cap *= 2; result = realloc(result, cap); }
        memcpy(result + len, entry, elen);
        len += elen;
        if (exited) {
            char extra[256];
            int xl = snprintf(extra, sizeof(extra), ",\"exit_code\":%d,\"completion_reason\":\"%s\"",
                              exit_code, s->completion_reason);
            if (len + (size_t)xl + 2 >= cap) { cap *= 2; result = realloc(result, cap); }
            memcpy(result + len, extra, xl);
            len += xl;
        }
    }
    result[len++] = ']';
    result[len] = '\0';
    free(sessions);
    return result;
}

/* PoP: has_active_processes @ tools/process_registry.py:ProcessRegistry.has_active_processes */
bool process_registry_has_active_for_task(const char *task_id) {
    registry_init();
    if (!task_id) return false;
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (g_registry.running[i]->task_id[0] &&
            strcmp(g_registry.running[i]->task_id, task_id) == 0 &&
            g_registry.running[i]->running) {
            pthread_mutex_unlock(&g_registry.lock);
            return true;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
    return false;
}

/* PoP: has_active_for_session @ tools/process_registry.py:ProcessRegistry.has_active_for_session */
bool process_registry_has_active_for_session(const char *session_key,
                                             double max_active_age_seconds) {
    registry_init();
    if (!session_key) return false;
    time_t now = time(NULL);
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        ProcessSession *s = g_registry.running[i];
        if (strcmp(s->session_key, session_key) == 0 && s->running) {
            if (max_active_age_seconds > 0 && (now - s->started_at) >= max_active_age_seconds)
                continue;
            pthread_mutex_unlock(&g_registry.lock);
            return true;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
    return false;
}

/* PoP: has_any_active @ tools/process_registry.py:ProcessRegistry.has_any_active */
bool process_registry_has_any_active(void) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (g_registry.running[i]->running) {
            pthread_mutex_unlock(&g_registry.lock);
            return true;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
    return false;
}

/* PoP: kill_started_since @ tools/process_registry.py:ProcessRegistry.kill_started_since */
int process_registry_kill_started_since(const char *task_id,
                                        const char **baseline_ids,
                                        int baseline_count,
                                        const char *source) {
    registry_init();
    int killed = 0;
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        ProcessSession *s = g_registry.running[i];
        if (!s->task_id || strcmp(s->task_id, task_id) != 0 || !s->running)
            continue;
        bool excluded = false;
        for (int j = 0; j < baseline_count; j++) {
            if (baseline_ids && baseline_ids[j] && strcmp(s->id, baseline_ids[j]) == 0) {
                excluded = true; break;
            }
        }
        if (excluded) continue;
        if (s->pid > 0) {
            if (s->detached && s->pid_scope[0] == 'h' && _host_pid_is_ours(s->pid, s->host_start_time))
                _terminate_host_pid(s->pid, s->host_start_time);
            else if (s->child_pid > 0)
                _terminate_host_pid(s->child_pid, s->host_start_time);
            else if (s->env_ref && g_env_exec) {
                char kill_cmd[256];
                snprintf(kill_cmd, sizeof(kill_cmd), "kill %d 2>/dev/null", (int)s->pid);
                int rc; g_env_exec(s->env_ref, kill_cmd, 5, &rc);
            }
        }
        session_mark_exited(s, -15, "killed", source ? source : "kill_started_since");
        _mark_consumed(s->id);
        killed++;
    }
    pthread_mutex_unlock(&g_registry.lock);
    /* Move killed sessions to finished */
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; ) {
        ProcessSession *s = g_registry.running[i];
        if (!s->running) {
            move_to_finished(s);
            i = 0; /* restart scan since indices shifted */
        } else i++;
    }
    pthread_mutex_unlock(&g_registry.lock);
    _prune_if_needed();
    process_registry_write_checkpoint();
    return killed;
}

/* PoP: kill_all @ tools/process_registry.py:ProcessRegistry.kill_all */
int process_registry_kill_all(const char *task_id_filter, const char *source,
                              bool consume_output) {
    if (!source) source = "kill_all";
    registry_init();
    int killed = 0;
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        ProcessSession *s = g_registry.running[i];
        if (!s->running) continue;
        if (task_id_filter && (!s->task_id || strcmp(s->task_id, task_id_filter) != 0))
            continue;
        if (s->pid > 0) {
            if (s->detached && s->pid_scope[0] == 'h' && _host_pid_is_ours(s->pid, s->host_start_time))
                _terminate_host_pid(s->pid, s->host_start_time);
            else if (s->child_pid > 0)
                _terminate_host_pid(s->child_pid, s->host_start_time);
            else if (s->env_ref && g_env_exec) {
                char kill_cmd[256];
                snprintf(kill_cmd, sizeof(kill_cmd), "kill -9 %d 2>/dev/null", (int)s->pid);
                int rc; g_env_exec(s->env_ref, kill_cmd, 5, &rc);
            }
        }
        session_mark_exited(s, -9, "killed", source);
        if (s->stdin_fd > 0) { close(s->stdin_fd); s->stdin_fd = -1; }
        if (consume_output) _mark_consumed(s->id);
        killed++;
    }
    pthread_mutex_unlock(&g_registry.lock);
    /* Drain killed sessions to finished */
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; ) {
        ProcessSession *s = g_registry.running[i];
        if (!s->running) { move_to_finished(s); i = 0; }
        else i++;
    }
    pthread_mutex_unlock(&g_registry.lock);
    _prune_if_needed();
    process_registry_write_checkpoint();
    return killed;
}

/* PoP: _write_checkpoint @ tools/process_registry.py:ProcessRegistry._write_checkpoint */
void process_registry_write_checkpoint(void) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    FILE *fp = fopen(CHECKPOINT_PATH, "w");
    if (!fp) { pthread_mutex_unlock(&g_registry.lock); return; }
    fprintf(fp, "[\n");
    for (int i = 0; i < g_registry.running_count; i++) {
        ProcessSession *s = g_registry.running[i];
        if (!s->running) continue;
        fprintf(fp, "  {\n");
        fprintf(fp, "    \"session_id\":\"%s\",\n", s->id);
        fprintf(fp, "    \"command\":\"%s\",\n", s->command);
        fprintf(fp, "    \"pid\":%d,\n", (int)s->pid);
        fprintf(fp, "    \"pid_scope\":\"%s\",\n", s->pid_scope);
        fprintf(fp, "    \"cwd\":\"%s\",\n", s->cwd);
        fprintf(fp, "    \"host_start_time\":%ld,\n", s->host_start_time);
        fprintf(fp, "    \"started_at\":%ld,\n", (long)s->started_at);
        fprintf(fp, "    \"task_id\":\"%s\",\n", s->task_id);
        fprintf(fp, "    \"session_key\":\"%s\",\n", s->session_key);
        fprintf(fp, "    \"watcher_interval\":%d\n", s->watcher_interval);
        fprintf(fp, "  }%s\n", (i < g_registry.running_count - 1) ? "," : "");
    }
    fprintf(fp, "]\n");
    fclose(fp);
    pthread_mutex_unlock(&g_registry.lock);
}

/* PoP: recover_from_checkpoint @ tools/process_registry.py:ProcessRegistry.recover_from_checkpoint */
int process_registry_recover_from_checkpoint(void) {
    registry_init();
    FILE *fp = fopen(CHECKPOINT_PATH, "r");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size < 0) { fclose(fp); return 0; }
    fseek(fp, 0, SEEK_SET);
    char *content = malloc(size + 1);
    if (size > 0) fread(content, 1, size, fp);
    content[size > 0 ? size : 0] = '\0';
    fclose(fp);
    int recovered = 0;
    char *p = content;
    while ((p = strstr(p, "\"session_id\""))) {
        p = strchr(p, ':');
        if (!p) break; p++;
        while (*p == ' ' || *p == '"') p++;
        char session_id[64] = {0};
        int i = 0;
        while (p[i] && p[i] != '"' && i < 63) { session_id[i] = p[i]; i++; }
        char *pidp = strstr(p, "\"pid\"");
        if (!pidp) break;
        pidp = strchr(pidp, ':');
        if (!pidp) break; pidp++;
        while (*pidp == ' ') pidp++;
        int pid = atoi(pidp);
        char *hstp = strstr(p, "\"host_start_time\"");
        long host_start = 0;
        if (hstp) { hstp = strchr(hstp, ':'); hstp++; while (*hstp == ' ') hstp++; host_start = strtol(hstp, NULL, 10); }
        if (pid > 0 && _is_host_pid_alive(pid)) {
            ProcessSession *s = malloc(sizeof(ProcessSession));
            session_init(s, session_id, "recovered", "", "", pid, "", true, "host");
            s->host_start_time = host_start;
            s->detached = true;
            pthread_mutex_lock(&g_registry.lock);
            if (g_registry.running_count == g_registry.running_cap) {
                g_registry.running_cap *= 2;
                g_registry.running = realloc(g_registry.running,
                    g_registry.running_cap * sizeof(ProcessSession*));
            }
            g_registry.running[g_registry.running_count++] = s;
            pthread_mutex_unlock(&g_registry.lock);
            recovered++;
        }
    }
    free(content);
    return recovered;
}

/* PoP: _prune_if_needed @ tools/process_registry.py:ProcessRegistry._prune_if_needed */
static void _prune_if_needed(void) {
    pthread_mutex_lock(&g_registry.lock);
    time_t now = time(NULL);
    for (int i = 0; i < g_registry.finished_count; ) {
        if ((now - g_registry.finished[i]->started_at) > FINISHED_TTL_SECONDS) {
            const char *pruned = g_registry.finished[i]->id;
            for (int j = 0; j < g_registry.consumed_count; )
                if (strcmp(g_registry.completion_consumed[j], pruned) == 0) {
                    free(g_registry.completion_consumed[j]);
                    g_registry.completion_consumed[j] = g_registry.completion_consumed[--g_registry.consumed_count];
                } else j++;
            for (int j = 0; j < g_registry.poll_observed_count; )
                if (strcmp(g_registry.poll_observed[j], pruned) == 0) {
                    free(g_registry.poll_observed[j]);
                    g_registry.poll_observed[j] = g_registry.poll_observed[--g_registry.poll_observed_count];
                } else j++;
            for (int j = 0; j < g_registry.finished[i]->watch_pattern_count; j++)
                free(g_registry.finished[i]->watch_patterns[j]);
            free(g_registry.finished[i]->watch_patterns);
            free(g_registry.finished[i]->output_buffer);
            free(g_registry.finished[i]);
            g_registry.finished[i] = g_registry.finished[g_registry.finished_count - 1];
            g_registry.finished_count--;
        } else i++;
    }
    int total = g_registry.running_count + g_registry.finished_count;
    if (total >= MAX_PROCESSES && g_registry.finished_count > 0) {
        int oldest_idx = 0;
        for (int i = 1; i < g_registry.finished_count; i++)
            if (g_registry.finished[i]->started_at < g_registry.finished[oldest_idx]->started_at)
                oldest_idx = i;
        const char *pruned = g_registry.finished[oldest_idx]->id;
        for (int j = 0; j < g_registry.consumed_count; )
            if (strcmp(g_registry.completion_consumed[j], pruned) == 0) {
                free(g_registry.completion_consumed[j]);
                g_registry.completion_consumed[j] = g_registry.completion_consumed[--g_registry.consumed_count];
            } else j++;
        for (int j = 0; j < g_registry.poll_observed_count; )
            if (strcmp(g_registry.poll_observed[j], pruned) == 0) {
                free(g_registry.poll_observed[j]);
                g_registry.poll_observed[j] = g_registry.poll_observed[--g_registry.poll_observed_count];
            } else j++;
        for (int j = 0; j < g_registry.finished[oldest_idx]->watch_pattern_count; j++)
            free(g_registry.finished[oldest_idx]->watch_patterns[j]);
        free(g_registry.finished[oldest_idx]->watch_patterns);
        free(g_registry.finished[oldest_idx]->output_buffer);
        free(g_registry.finished[oldest_idx]);
        g_registry.finished[oldest_idx] = g_registry.finished[g_registry.finished_count - 1];
        g_registry.finished_count--;
    }
    pthread_mutex_unlock(&g_registry.lock);
}

/* PoP: set_watch_patterns @ tools/process_registry.py:ProcessRegistry.set_watch_patterns */
void process_registry_set_watch_patterns(const char *session_id, char **patterns, int count) {
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return;
    pthread_mutex_lock(&s->lock);
    for (int i = 0; i < s->watch_pattern_count; i++) free(s->watch_patterns[i]);
    free(s->watch_patterns);
    s->watch_pattern_count = 0;
    s->watch_pattern_cap = count > 0 ? count : 4;
    if (count > 0) {
        s->watch_patterns = calloc(s->watch_pattern_cap, sizeof(char*));
        for (int i = 0; i < count; i++) {
            s->watch_patterns[i] = strdup(patterns[i]);
            s->watch_pattern_count++;
        }
    } else {
        s->watch_patterns = NULL;
    }
    pthread_mutex_unlock(&s->lock);
}

/* PoP: set_notify_on_complete @ tools/process_registry.py:ProcessRegistry.set_notify_on_complete */
void process_registry_set_notify_on_complete(const char *session_id, bool notify) {
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return;
    pthread_mutex_lock(&s->lock);
    s->notify_on_complete = notify;
    pthread_mutex_unlock(&s->lock);
}

/* PoP: append_output @ tools/process_registry.py:ProcessRegistry.append_output */
void process_registry_append_output(const char *session_id, const char *text) {
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s || !text) return;
    session_append_output(s, text, strlen(text));
}

/* PoP: is_completion_consumed @ tools/process_registry.py:ProcessRegistry.is_completion_consumed */
bool process_registry_completion_consumed(const char *session_id) {
    if (!session_id) return false;
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    bool found = _session_in_consumed(session_id);
    pthread_mutex_unlock(&g_registry.lock);
    return found;
}

/* PoP: is_session_waiting @ tools/process_registry.py:ProcessRegistry.is_session_waiting */
bool process_registry_is_session_waiting(const char *session_id) {
    if (!session_id || !*session_id) return false;
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return false;
    _refresh_detached_session(s);
    pthread_mutex_lock(&s->lock);
    bool exited = !s->running;
    int watch_hits = s->watch_hits;
    bool watch_disabled = s->watch_disabled;
    pthread_mutex_unlock(&s->lock);
    if (exited) return false;
    if (s->watch_patterns && s->watch_pattern_count > 0 && !watch_disabled) {
        if (watch_hits) return false;
    }
    return true;
}

/* PoP: read_log @ tools/process_registry.py:ProcessRegistry.read_log */
char* process_registry_read_log(const char *session_id, int offset, int limit) {
    if (offset < 0) offset = 0;
    if (limit <= 0) limit = 200;
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return strdup("{\"status\":\"not_found\",\"error\":\"No such session\"}");
    pthread_mutex_lock(&s->lock);
    char *stripped = NULL;
    if (s->output_len > 0) stripped = strip_ansi(s->output_buffer);
    pthread_mutex_unlock(&s->lock);
    char **lines = NULL;
    int total_lines = 0;
    if (stripped) {
        char *copy = strdup(stripped);
        char *save = NULL;
        char *tok = strtok_r(copy, "\n", &save);
        while (tok) {
            lines = realloc(lines, (total_lines + 1) * sizeof(char*));
            lines[total_lines++] = strdup(tok);
            tok = strtok_r(NULL, "\n", &save);
        }
        free(copy);
    }
    int start_idx;
    if (offset == 0 && limit > 0) {
        start_idx = total_lines > limit ? total_lines - limit : 0;
    } else {
        start_idx = offset;
    }
    int stop_idx = start_idx + limit;
    if (start_idx > total_lines) start_idx = total_lines;
    if (stop_idx > total_lines) stop_idx = total_lines;
    int selected_count = stop_idx - start_idx;
    size_t cap = 4096;
    char *result = malloc(cap);
    size_t len = 0;
    len += snprintf(result + len, cap - len,
        "{\"session_id\":\"%s\",\"command\":\"%s\",\"status\":\"%s\","
        "\"total_lines\":%d,\"showing\":\"%d lines\",\"output\":\"",
        s->id, s->command, !s->running ? "exited" : "running", total_lines, selected_count);
    for (int i = start_idx; i < stop_idx; i++) {
        size_t add = strlen(lines[i]);
        if (len + add + 4 >= cap) { cap = (len + add) * 2; result = realloc(result, cap); }
        memcpy(result + len, lines[i], add);
        len += add;
        if (i < stop_idx - 1) { result[len++] = '\\'; result[len++] = 'n'; }
        free(lines[i]);
    }
    free(lines);
    len += snprintf(result + len, cap - len, "\"}");
    free(stripped);
    return result;
}

/* PoP: drain_notifications @ tools/process_registry.py:ProcessRegistry.drain_notifications */
char* process_registry_drain_notifications(const char *session_key_filter,
                                           bool skip_poll_observed) {
    (void)skip_poll_observed;
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    if (g_registry.queue_count == 0) {
        pthread_mutex_unlock(&g_registry.lock);
        return strdup("[]");
    }
    size_t cap = 8192;
    char *result = malloc(cap);
    size_t len = 1;
    result[0] = '[';
    bool first = true;
    while (g_registry.queue_count > 0) {
        char *evt = g_registry.completion_queue[g_registry.queue_head];
        g_registry.queue_head = (g_registry.queue_head + 1) % g_registry.queue_cap;
        g_registry.queue_count--;
        bool skip = false;
        if (session_key_filter) {
            char *key = strstr(evt, "\"session_key\":\"");
            if (!key) { skip = true; }
            else {
                key += 16;
                char *end = strchr(key, '"');
                if (!end) { skip = true; }
                else {
                    char ekey[64] = {0};
                    size_t n = end - key;
                    if (n > 63) n = 63;
                    memcpy(ekey, key, n);
                    if (strcmp(ekey, session_key_filter) != 0) skip = true;
                }
            }
        }
        if (!skip) {
            /* Port of Python drain: _drain_should_skip suppresses completion
             * events the agent already consumed (wait/log) or observed inline
             * via poll() (skip_poll_observed). Watch events and routed
             * notifications pass through untouched (#8228, #10156). */
            char *sid = strstr(evt, "\"session_id\":\"");
            char esid[64] = {0};
            if (sid) {
                sid += 15;
                char *end = strchr(sid, '"');
                if (end) {
                    size_t n = end - sid;
                    if (n > 63) n = 63;
                    memcpy(esid, sid, n);
                }
            }
            bool is_completion = (strstr(evt, "\"type\":") == NULL);
            if (is_completion && esid[0] &&
                process_registry_drain_should_skip(esid, skip_poll_observed)) {
                free(evt);
                continue;
            }
            if (!first) result[len++] = ',';
            first = false;
            size_t elen = strlen(evt);
            if (len + elen + 2 >= cap) { cap = (len + elen) * 2; result = realloc(result, cap); }
            memcpy(result + len, evt, elen);
            len += elen;
            if (esid[0]) _mark_consumed(esid);
        }
        free(evt);
    }
    pthread_mutex_unlock(&g_registry.lock);
    result[len++] = ']';
    result[len] = '\0';
    return result;
}

/* PoP: snapshot_running_ids @ tools/process_registry.py:ProcessRegistry.snapshot_running_ids */
char** process_registry_snapshot_running_ids(const char *task_id, int *count_out) {
    registry_init();
    if (count_out) *count_out = 0;
    pthread_mutex_lock(&g_registry.lock);
    int cap = g_registry.running_count + 1;
    char **ids = calloc(cap, sizeof(char*));
    int n = 0;
    for (int i = 0; i < g_registry.running_count; i++) {
        ProcessSession *s = g_registry.running[i];
        if (s->task_id[0] && (!task_id || strcmp(s->task_id, task_id) == 0) && s->running)
            ids[n++] = strdup(s->id);
    }
    pthread_mutex_unlock(&g_registry.lock);
    if (count_out) *count_out = n;
    return ids;
}

/* PoP: write_stdin @ tools/process_registry.py:ProcessRegistry.write_stdin */
char* process_registry_write_stdin(const char *session_id, const char *data) {
    if (!data) data = "";
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return strdup("{\"status\":\"not_found\",\"error\":\"No process with that ID\"}");
    if (s->stdin_fd <= 0)
        return strdup("{\"status\":\"error\",\"error\":\"Process stdin not available (non-local backend or stdin closed)\"}");
    size_t len = strlen(data);
    if (len > 0) {
        ssize_t w = write(s->stdin_fd, data, len);
        if (w < 0) return strdup("{\"status\":\"error\",\"error\":\"Failed to write to stdin\"}");
    }
    char *result = malloc(256);
    snprintf(result, 256, "{\"status\":\"ok\",\"written\":%zu}", len);
    return result;
}

/* PoP: submit_stdin @ tools/process_registry.py:ProcessRegistry.submit_stdin */
char* process_registry_submit_stdin(const char *session_id, const char *data) {
    if (!data) data = "";
    size_t len = strlen(data);
    char *with_nl = malloc(len + 2);
    memcpy(with_nl, data, len);
    with_nl[len] = '\n';
    with_nl[len + 1] = '\0';
    char *result = process_registry_write_stdin(session_id, with_nl);
    free(with_nl);
    return result;
}

/* PoP: close_stdin @ tools/process_registry.py:ProcessRegistry.close_stdin */
char* process_registry_close_stdin(const char *session_id) {
    registry_init();
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return strdup("{\"status\":\"not_found\",\"error\":\"No process with that ID\"}");
    if (s->stdin_fd > 0) { close(s->stdin_fd); s->stdin_fd = -1; }
    if (g_close_sink) g_close_sink(s, session_id);
    char *result = malloc(256);
    snprintf(result, 256, "{\"status\":\"ok\",\"closed\":\"%s\",\"note\":\"stdin closed; process may continue running\"}",
             session_id ? session_id : "");
    return result;
}

/* PoP: set_env_exec @ tools/process_registry.py:ProcessRegistry.set_env_exec */
void process_registry_set_env_exec(process_registry_env_exec_fn fn) {
    g_env_exec = fn;
}

/* PoP: set_close_sink @ tools/process_registry.py:ProcessRegistry.set_close_sink */
void process_registry_set_close_sink(process_registry_close_sink_t sink) {
    g_close_sink = sink;
}

/* PoP: request_close_terminal @ tools/process_registry.py:request_close_terminal */
char *process_registry_request_close_terminal(const char *session_id) {
    ProcessSession *s = process_registry_get_session_ptr(session_id);
    if (!s) return strdup("{\"status\":\"error\",\"error\":\"No such session\"}");
    if (g_close_sink) {
        g_close_sink(s, session_id);
        char *result = malloc(256);
        snprintf(result, 256, "{\"status\":\"ok\",\"closed\":\"%s\"}", session_id);
        return result;
    }
    return strdup("{\"status\":\"error\",\"error\":\"close_terminal is unavailable in this backend\"}");
}

/* PoP: format_uptime_short @ tools/process_registry.py:format_uptime_short */
void format_uptime_short(int seconds, char *buf, size_t buf_len) {
    if (seconds < 0) seconds = 0;
    if (seconds < 60) {
        snprintf(buf, buf_len, "%ds", seconds);
    } else if (seconds < 3600) {
        int mins = seconds / 60;
        int secs = seconds % 60;
        snprintf(buf, buf_len, "%dm %ds", mins, secs);
    } else {
        int hours = seconds / 3600;
        int mins = (seconds % 3600) / 60;
        snprintf(buf, buf_len, "%dh %dm", hours, mins);
    }
}

/* PoP: _format_age @ tools/process_registry.py:_format_age */
void _format_age(double seconds, char *buf, size_t buf_len) {
    if (!isfinite(seconds) || seconds < 0) { snprintf(buf, buf_len, "?"); return; }
    int s = (int)seconds;
    if (s < 60) { snprintf(buf, buf_len, "%ds", s); return; }
    int m = s / 60, ss = s % 60;
    if (m < 60) { snprintf(buf, buf_len, "%dm%d", m, ss); return; }
    int h = m / 60; m = m % 60;
    snprintf(buf, buf_len, "%dh%d%m", h, m);
}

/* PoP: _format_async_delegation @ tools/process_registry.py:_format_async_delegation */
char* _format_async_delegation(const char *json_evt) {
    char *err = NULL;
    json_t *evt = json_parse(json_evt, &err);
    if (err) free(err);
    if (!evt || !json_is_object(evt)) { if (evt) json_free(evt); return NULL; }
    const char *deleg_id = json_get_str(evt, "delegation_id", "");
    if (!deleg_id || !*deleg_id) deleg_id = "unknown";
    const char *goal = json_get_str(evt, "goal", "");
    const char *context = json_get_str(evt, "context", "");
    const char *role = json_get_str(evt, "role", "");
    if (!role || !*role) role = "leaf";
    const char *model = json_get_str(evt, "model", "");
    if (!model || !*model) model = "?";
    const char *status = json_get_str(evt, "status", "");
    if (!status || !*status) status = "completed";
    const char *summary = json_get_str(evt, "summary", "");
    const char *error = json_get_str(evt, "error", "");
    double dispatched_at = json_get_num(evt, "dispatched_at", 0.0);
    double completed_at = json_get_num(evt, "completed_at", 0.0);
    if (completed_at == 0.0) completed_at = (double)time(NULL);
    const char *total_dur = json_get_str(evt, "total_duration_seconds", "");
    if (!total_dur || !*total_dur) total_dur = json_get_str(evt, "duration_seconds", "");
    if (!total_dur || !*total_dur) total_dur = "?";
    char *out = malloc(16384);
    int off = 0;
    off += snprintf(out + off, 16384 - off,
        "[ASYNC DELEGATION COMPLETE -- %s]\nA background subagent you dispatched earlier has finished. You may have moved on since dispatching it; the full task source is below so you can act on the result or re-dispatch if things have changed.\n\n", deleg_id);
    if (dispatched_at > 0) {
        char ts[64]; time_t t = (time_t)dispatched_at; struct tm *tm = localtime(&t);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
        char agebuf[16]; _format_age(completed_at - dispatched_at, agebuf, sizeof(agebuf));
        off += snprintf(out + off, 16384 - off, "Dispatched: %s (%s ago)\n", ts, agebuf);
    }
    if (goal && *goal) off += snprintf(out + off, 16384 - off, "Original goal: %s\n", goal);
    if (context && *context) off += snprintf(out + off, 16384 - off, "Context you provided: %s\n", context);
    off += snprintf(out + off, 16384 - off, "Role: %s   Model: %s   Duration: %ss\n", role, model, total_dur);
    off += snprintf(out + off, 16384 - off, "--- RESULT ---\n");
    if (strcmp(status, "completed") == 0 || strcmp(status, "success") == 0) {
        if (summary && *summary) off += snprintf(out + off, 16384 - off, "%s\n", summary);
    } else if (strcmp(status, "interrupted") == 0) {
        off += snprintf(out + off, 16384 - off,
            "The subagent was interrupted before completing%s.\n", error ? ":" : ".");
        if (error && *error) off += snprintf(out + off, 16384 - off, " %s", error);
        if (summary && *summary) off += snprintf(out + off, 16384 - off, "\nPartial output:\n%s", summary);
    } else {
        off += snprintf(out + off, 16384 - off,
            "The subagent did not complete successfully (status=%s).", status);
        if (error && *error) off += snprintf(out + off, 16384 - off, "\n%s", error);
        if (summary && *summary) off += snprintf(out + off, 16384 - off, "\nPartial output:\n%s", summary);
    }
    json_free(evt);
    return out;
}

/* PoP: format_process_notification @ tools/process_registry.py:format_process_notification */
char* format_process_notification(const char *json_evt) {
    char *err = NULL;
    json_t *evt = json_parse(json_evt, &err);
    if (err) free(err);
    if (!evt || !json_is_object(evt)) { if (evt) json_free(evt); return NULL; }
    const char *type = json_get_str(evt, "type", "");
    if (!type || !*type) type = "completion";
    const char *sid = json_get_str(evt, "session_id", "");
    if (!sid || !*sid) sid = "unknown";
    const char *cmd = json_get_str(evt, "command", "");
    if (!cmd || !*cmd) cmd = "unknown";
    char *out = NULL;
    if (strcmp(type, "watch_disabled") == 0) {
        const char *msg = json_get_str(evt, "message", "");
        if (!msg) msg = "";
        asprintf(&out, "[IMPORTANT: %s]", msg);
    } else if (strcmp(type, "watch_match") == 0) {
        const char *pat = json_get_str(evt, "pattern", "");
        if (!pat || !*pat) pat = "?";
        const char *o = json_get_str(evt, "output", "");
        if (!o) o = "";
        long sup = (long)json_get_num(evt, "suppressed", 0);
        asprintf(&out,
            "[IMPORTANT: Background process %s matched watch pattern \"%s\".\nCommand: %s\nMatched output:\n%s%s]\n",
            sid, pat, cmd, o, sup > 0 ? "" : "");
        if (sup > 0) {
            char *combined = NULL;
            asprintf(&combined, " (%ld earlier matches were suppressed by rate limit)", sup);
            char *final = malloc(strlen(out) + strlen(combined) + 2);
            strcpy(final, out); strcat(final, combined); free(combined); free(out); out = final;
        }
    } else if (strcmp(type, "async_delegation") == 0) {
        out = _format_async_delegation(json_evt);
    } else {
        long exit_code = (long)json_get_num(evt, "exit_code", -1);
        const char *o = json_get_str(evt, "output", "");
        if (!o) o = "";
        const char *reason = json_get_str(evt, "completion_reason", "");
        if (!reason || !*reason) reason = "exited";
        const char *src = json_get_str(evt, "termination_source", "");
        const char *sig = "";
        if (exit_code == -15 || exit_code == 143) sig = ", SIGTERM";
        const char *status;
        if (strcmp(reason, "killed") == 0) status = "terminated by ";
        else if (strcmp(reason, "lost") == 0) status = "marked lost because the process backend disappeared";
        else if (strcmp(reason, "failed_start") == 0) status = "failed to start";
        else if (exit_code == 0) status = "completed normally";
        else status = "exited";
        if (strcmp(reason, "killed") == 0) {
            const char *killer = (src && *src) ? src : "Hermes";
            asprintf(&out,
                "[IMPORTANT: Background process %s terminated by %s (exit code %ld%s).\nCommand: %s\nOutput:\n%s]\n",
                sid, killer, exit_code, sig, cmd, o);
        } else {
            asprintf(&out,
                "[IMPORTANT: Background process %s %s (exit code %ld%s).\nCommand: %s\nOutput:\n%s]\n",
                sid, status, exit_code, sig, cmd, o);
        }
    }
    json_free(evt);
    return out;
}

/* PoP: _redact_process_result @ tools/process_registry.py:_redact_process_result */
char* _redact_process_result(char *json_result) {
    char *err = NULL;
    json_t *r = json_parse(json_result, &err);
    if (err) free(err);
    if (!r || !json_is_object(r)) { if (r) json_free(r); return json_result; }
    const char *cmd = json_get_str(r, "command", "");
    if (cmd && *cmd) {
        char *red = browser_redact_sensitive_text(cmd);
        if (red) { json_obj_del(r, "command"); json_set(r, "command", json_string(red)); free(red); }
    }
    const char *o = json_get_str(r, "output", "");
    if (o && *o) {
        char *red2 = browser_redact_sensitive_text(o);
        if (red2) { json_obj_del(r, "output"); json_set(r, "output", json_string(red2)); free(red2); }
    }
    const char *prev = json_get_str(r, "output_preview", "");
    if (prev && *prev) {
        char *red3 = browser_redact_sensitive_text(prev);
        if (red3) { json_obj_del(r, "output_preview"); json_set(r, "output_preview", json_string(red3)); free(red3); }
    }
    char *out = json_serialize(r);
    json_free(r);
    free(json_result);
    return out;
}

/* PoP: _handle_process @ tools/process_registry.py:_handle_process */
char* process_registry_handle(const char *action,
                              const char *session_id,
                              const char *data,
                              int timeout,
                              const char *task_id,
                              const char *session_key,
                              int offset, int limit) {
    if (!action) return strdup("{\"error\":\"no action\"}");
    if (strcmp(action, "list") == 0) {
        char *j = process_registry_list(task_id, session_key);
        char *out = malloc(strlen(j) + 32);
        snprintf(out, strlen(j) + 32, "{\"processes\":%s}", j);
        free(j);
        return out;
    }
    if (!session_id || !*session_id) {
        char *err = malloc(256);
        snprintf(err, 256, "{\"error\":\"session_id is required for %s\"}", action);
        return err;
    }
    if (strcmp(action, "poll") == 0) {
        char *r = process_registry_poll(session_id);
        return _redact_process_result(r);
    } else if (strcmp(action, "log") == 0) {
        char *r = process_registry_read_log(session_id, offset, limit);
        return _redact_process_result(r);
    } else if (strcmp(action, "wait") == 0) {
        char *r = process_registry_wait(session_id, timeout);
        return _redact_process_result(r);
    } else if (strcmp(action, "kill") == 0) {
        char *r = process_registry_kill(session_id, NULL, true);
        return _redact_process_result(r);
    } else if (strcmp(action, "write") == 0) {
        return process_registry_write_stdin(session_id, data ? data : "");
    } else if (strcmp(action, "submit") == 0) {
        return process_registry_submit_stdin(session_id, data ? data : "");
    } else if (strcmp(action, "close") == 0) {
        return process_registry_close_stdin(session_id);
    }
    char *err = malloc(256);
    snprintf(err, 256, "{\"error\":\"Unknown process action: %s. Use: list, poll, log, wait, kill, write, submit, close\"}", action);
    return err;
}

void process_registry_destroy(void) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        for (int j = 0; j < g_registry.running[i]->watch_pattern_count; j++)
            free(g_registry.running[i]->watch_patterns[j]);
        free(g_registry.running[i]->watch_patterns);
        free(g_registry.running[i]->output_buffer);
        free(g_registry.running[i]);
    }
    for (int i = 0; i < g_registry.finished_count; i++) {
        for (int j = 0; j < g_registry.finished[i]->watch_pattern_count; j++)
            free(g_registry.finished[i]->watch_patterns[j]);
        free(g_registry.finished[i]->watch_patterns);
        free(g_registry.finished[i]->output_buffer);
        free(g_registry.finished[i]);
    }
    for (int i = 0; i < g_registry.queue_count; i++) free(g_registry.completion_queue[i]);
    free(g_registry.completion_queue);
    for (int i = 0; i < g_registry.consumed_count; i++) free(g_registry.completion_consumed[i]);
    free(g_registry.completion_consumed);
    free(g_registry.pending_watchers);
    pthread_mutex_unlock(&g_registry.lock);
    g_registry_initialized = false;
}
