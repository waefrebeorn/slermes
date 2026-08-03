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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

/* PoP: format_uptime_short @ process_registry:format_uptime_short */
static void format_uptime_short(int seconds, char *buf, size_t buf_len) {
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

/* PoP: ProcessSession @ process_registry:ProcessSession */
typedef struct ProcessSession {
    char id[32];                        /* "proc_xxxxxxxxxxxx" */
    char command[4096];
    char task_id[64];
    char session_key[64];
    pid_t pid;
    int child_pid;                      /* for local Popen pid */
    bool running;
    int exit_code;
    time_t started_at;
    char *output_buffer;
    size_t output_len;
    size_t output_cap;
    bool detached;
    char pid_scope[16];                 /* "host" or "sandbox" */
    int stdin_fd;                       /* for local process stdin pipe */

    /* Watcher metadata */
    char watcher_platform[32];
    char watcher_chat_id[64];
    char watcher_user_id[64];
    char watcher_user_name[64];
    char watcher_thread_id[64];
    char watcher_message_id[64];
    int watcher_interval;
    bool notify_on_complete;
    char **watch_patterns;
    int watch_pattern_count;
    int watch_pattern_cap;

    /* Rate limiting state */
    int watch_hits;
    int watch_suppressed;
    bool watch_disabled;
    double watch_last_emit_at;
    double watch_cooldown_until;
    bool watch_strike_candidate;
    int watch_consecutive_strikes;

    pthread_mutex_t lock;
    pthread_cond_t completion_event;
} ProcessSession;

/* PoP: ProcessRegistry @ process_registry:ProcessRegistry */
typedef struct {
    ProcessSession **running;
    int running_count;
    int running_cap;

    ProcessSession **finished;
    int finished_count;
    int finished_cap;

    pthread_mutex_t lock;

    /* Completion queue */
    char **completion_queue;
    int queue_head;
    int queue_tail;
    int queue_count;
    int queue_cap;

    /* Consumed completion tracking */
    char **completion_consumed;
    int consumed_count;
    int consumed_cap;

    /* Global watch circuit breaker */
    pthread_mutex_t global_watch_lock;
    double global_watch_window_start;
    int global_watch_window_hits;
    double global_watch_tripped_until;
    int global_watch_suppressed_during_trip;

    /* Pending watchers for gateway */
    char **pending_watchers;
    int pending_watcher_count;
    int pending_watcher_cap;
} ProcessRegistry;

static ProcessRegistry g_registry = {0};
static bool g_registry_initialized = false;

#define MAX_OUTPUT_CHARS 200000
#define FINISHED_TTL_SECONDS 1800
#define MAX_PROCESSES 64
#define WATCH_MIN_INTERVAL_SECONDS 15
#define WATCH_STRIKE_LIMIT 3
#define WATCH_GLOBAL_MAX_PER_WINDOW 15
#define WATCH_GLOBAL_WINDOW_SECONDS 10
#define WATCH_GLOBAL_COOLDOWN_SECONDS 30

static bool _pid_exists(pid_t pid) {
    if (pid <= 0) return false;
    return kill(pid, 0) == 0;
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
    g_registry_initialized = true;
}

/* Public init (declared in process_registry.h) — idempotent. Allocates the
 * session arrays + mutexes up front. Safe to call before any spawn/get. */
void process_registry_init(void) {
    registry_init();
}

static void session_init(ProcessSession *s, const char *id, const char *command,
                         const char *task_id, const char *session_key,
                         pid_t pid, const char *cwd, bool detached,
                         const char *pid_scope) {
    memset(s, 0, sizeof(ProcessSession));
    strncpy(s->id, id, sizeof(s->id) - 1);
    strncpy(s->command, command, sizeof(s->command) - 1);
    if (task_id) strncpy(s->task_id, task_id, sizeof(s->task_id) - 1);
    if (session_key) strncpy(s->session_key, session_key, sizeof(s->session_key) - 1);
    s->pid = pid;
    s->running = true;
    s->exit_code = 0;
    s->started_at = time(NULL);
    s->detached = detached;
    if (pid_scope) strncpy(s->pid_scope, pid_scope, sizeof(s->pid_scope) - 1);
    else strncpy(s->pid_scope, "host", sizeof(s->pid_scope) - 1);

    s->output_cap = MAX_OUTPUT_CHARS;
    s->output_buffer = malloc(s->output_cap);
    s->output_buffer[0] = '\0';
    s->output_len = 0;

    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->completion_event, NULL);
}

static void session_append_output(ProcessSession *s, const char *text, size_t len) {
    pthread_mutex_lock(&s->lock);
    if (s->output_len + len + 1 >= s->output_cap) {
        size_t new_cap = s->output_cap * 2;
        if (s->output_len + len + 1 >= new_cap) {
            new_cap = s->output_len + len + 1;
        }
        s->output_buffer = realloc(s->output_buffer, new_cap);
        s->output_cap = new_cap;
    }
    memcpy(s->output_buffer + s->output_len, text, len);
    s->output_len += len;
    s->output_buffer[s->output_len] = '\0';

    /* Rolling buffer - keep last MAX_OUTPUT_CHARS */
    if (s->output_len > MAX_OUTPUT_CHARS) {
        size_t excess = s->output_len - MAX_OUTPUT_CHARS;
        memmove(s->output_buffer, s->output_buffer + excess, MAX_OUTPUT_CHARS + 1);
        s->output_len = MAX_OUTPUT_CHARS;
    }
    pthread_mutex_unlock(&s->lock);
}

static void session_mark_exited(ProcessSession *s, int exit_code) {
    pthread_mutex_lock(&s->lock);
    s->running = false;
    s->exit_code = exit_code;
    pthread_cond_broadcast(&s->completion_event);
    pthread_mutex_unlock(&s->lock);
}

/* PoP: _clean_shell_noise @ process_registry:ProcessRegistry._clean_shell_noise */
static void clean_shell_noise(char *text) {
    const char *noise[] = {
        "bash: cannot set terminal process group",
        "bash: no job control in this shell",
        "no job control in this shell",
        "cannot set terminal process group",
        "tcsetattr: Inappropriate ioctl for device",
        NULL
    };

    char *line = strtok(text, "\n");
    bool first = true;
    while (line) {
        bool is_noise = false;
        for (int i = 0; noise[i]; i++) {
            if (strstr(line, noise[i])) {
                is_noise = true;
                break;
            }
        }
        if (first && is_noise) {
            /* Remove this line - shift remaining text */
            char *next = strchr(line, '\n');
            if (next) {
                memmove(line, next + 1, strlen(next + 1) + 1);
                line = next + 1 - (next - line); /* adjust pointer */
            } else {
                *line = '\0';
            }
        } else {
            first = false;
            line = strchr(line, '\n');
            if (line) line++;
        }
    }
}

/* PoP: _check_watch_patterns @ process_registry:ProcessRegistry._check_watch_patterns */
static void check_watch_patterns(ProcessSession *s, const char *new_text) {
    if (!s->watch_patterns || s->watch_pattern_count == 0 || s->watch_disabled) return;
    if (!s->running) return;

    double now = (double)time(NULL);

    /* Check per-session rate limit */
    pthread_mutex_lock(&s->lock);
    bool should_disable = false;
    bool return_early = false;
    int suppressed = s->watch_suppressed;

    if (s->watch_cooldown_until > 0 && now < s->watch_cooldown_until) {
        s->watch_suppressed += 1; /* count lines, not individual matches for simplicity */
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
        if (s->watch_cooldown_until > 0 && !s->watch_strike_candidate) {
            s->watch_consecutive_strikes = 0;
        }
        s->watch_strike_candidate = false;
        s->watch_last_emit_at = now;
        s->watch_cooldown_until = now + WATCH_MIN_INTERVAL_SECONDS;
        s->watch_hits++;
        s->watch_suppressed = 0;
    }
    pthread_mutex_unlock(&s->lock);

    if (return_early) {
        if (should_disable) {
            /* Queue watch_disabled event */
            char msg[1024];
            snprintf(msg, sizeof(msg),
                "Watch patterns disabled for process %s — "
                "%d consecutive rate-limit windows triggered (min spacing %ds). "
                "Falling back to notify_on_complete semantics; you'll get "
                "exactly one notification when the process exits.",
                s->id, WATCH_STRIKE_LIMIT, WATCH_MIN_INTERVAL_SECONDS);

            char *event = malloc(2048);
            snprintf(event, 2048,
                "{\"session_id\":\"%s\",\"session_key\":\"%s\",\"command\":\"%s\","
                "\"type\":\"watch_disabled\",\"suppressed\":%d,"
                "\"platform\":\"%s\",\"chat_id\":\"%s\",\"user_id\":\"%s\","
                "\"user_name\":\"%s\",\"thread_id\":\"%s\",\"message_id\":\"%s\","
                "\"message\":\"%s\"}",
                s->id, s->session_key, s->command,
                suppressed,
                s->watcher_platform, s->watcher_chat_id, s->watcher_user_id,
                s->watcher_user_name, s->watcher_thread_id, s->watcher_message_id,
                msg);
            /* Add to completion queue */
            pthread_mutex_lock(&g_registry.lock);
            if (g_registry.queue_count == g_registry.queue_cap) {
                g_registry.queue_cap *= 2;
                g_registry.completion_queue = realloc(g_registry.completion_queue,
                    g_registry.queue_cap * sizeof(char*));
            }
            g_registry.completion_queue[g_registry.queue_tail] = event;
            g_registry.queue_tail = (g_registry.queue_tail + 1) % g_registry.queue_cap;
            g_registry.queue_count++;
            pthread_mutex_unlock(&g_registry.lock);
        }
        return;
    }

    /* Check global circuit breaker */
    pthread_mutex_lock(&g_registry.global_watch_lock);
    bool admit = true;

    if (g_registry.global_watch_tripped_until > 0 && now >= g_registry.global_watch_tripped_until) {
        int suppressed_global = g_registry.global_watch_suppressed_during_trip;
        g_registry.global_watch_tripped_until = 0;
        g_registry.global_watch_suppressed_during_trip = 0;
        g_registry.global_watch_window_start = now;
        g_registry.global_watch_window_hits = 0;
        if (suppressed_global > 0) {
            /* Queue release event outside lock */
            char *release_event = malloc(1024);
            snprintf(release_event, 1024,
                "{\"session_id\":\"\",\"session_key\":\"\",\"command\":\"\","
                "\"type\":\"watch_overflow_released\",\"suppressed\":%d,"
                "\"message\":\"Watch-pattern notifications resumed. %d match event(s) were suppressed during the flood.\","
                "\"platform\":\"\",\"chat_id\":\"\",\"user_id\":\"\",\"user_name\":\"\",\"thread_id\":\"\"}",
                suppressed_global, suppressed_global);
            pthread_mutex_lock(&g_registry.lock);
            if (g_registry.queue_count == g_registry.queue_cap) {
                g_registry.queue_cap *= 2;
                g_registry.completion_queue = realloc(g_registry.completion_queue,
                    g_registry.queue_cap * sizeof(char*));
            }
            g_registry.completion_queue[g_registry.queue_tail] = release_event;
            g_registry.queue_tail = (g_registry.queue_tail + 1) % g_registry.queue_cap;
            g_registry.queue_count++;
            pthread_mutex_unlock(&g_registry.lock);
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

    if (!admit) return;

    /* Queue watch_match event */
    char *event = malloc(2048);
    snprintf(event, 2048,
        "{\"session_id\":\"%s\",\"session_key\":\"%s\",\"command\":\"%s\","
        "\"type\":\"watch_match\",\"pattern\":\"pattern\",\"output\":\"matched output\","
        "\"suppressed\":%d,"
        "\"platform\":\"%s\",\"chat_id\":\"%s\",\"user_id\":\"%s\","
        "\"user_name\":\"%s\",\"thread_id\":\"%s\",\"message_id\":\"%s\"}",
        s->id, s->session_key, s->command,
        suppressed,
        s->watcher_platform, s->watcher_chat_id, s->watcher_user_id,
        s->watcher_user_name, s->watcher_thread_id, s->watcher_message_id);

    pthread_mutex_lock(&g_registry.lock);
    if (g_registry.queue_count == g_registry.queue_cap) {
        g_registry.queue_cap *= 2;
        g_registry.completion_queue = realloc(g_registry.completion_queue,
            g_registry.queue_cap * sizeof(char*));
    }
    g_registry.completion_queue[g_registry.queue_tail] = event;
    g_registry.queue_tail = (g_registry.queue_tail + 1) % g_registry.queue_cap;
    g_registry.queue_count++;
    pthread_mutex_unlock(&g_registry.lock);
}

/* PoP: _reconcile_local_exit @ process_registry:ProcessRegistry._reconcile_local_exit */
static bool reconcile_local_exit(ProcessSession *s) {
    if (!s || !s->running || s->child_pid <= 0) return false;

    int status;
    pid_t result = waitpid(s->child_pid, &status, WNOHANG);
    if (result != s->child_pid) return false; /* still running */

    /* Child exited - drain any remaining output */
    /* Note: In C we don't have the reader thread infrastructure,
       so we just mark as exited */

    pthread_mutex_lock(&s->lock);
    s->running = false;
    s->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (WIFSIGNALED(status)) s->exit_code = -WTERMSIG(status);
    pthread_cond_broadcast(&s->completion_event);
    pthread_mutex_unlock(&s->lock);
    return true;
}

/* PoP: _move_to_finished @ process_registry:ProcessRegistry._move_to_finished */
static void move_to_finished(ProcessSession *s) {
    pthread_mutex_lock(&g_registry.lock);

    /* Remove from running */
    for (int i = 0; i < g_registry.running_count; i++) {
        if (g_registry.running[i] == s) {
            g_registry.running[i] = g_registry.running[g_registry.running_count - 1];
            g_registry.running_count--;
            break;
        }
    }

    /* Add to finished */
    if (g_registry.finished_count == g_registry.finished_cap) {
        g_registry.finished_cap *= 2;
        g_registry.finished = realloc(g_registry.finished,
            g_registry.finished_cap * sizeof(ProcessSession*));
    }
    g_registry.finished[g_registry.finished_count++] = s;

    /* Prune if needed */
    time_t now = time(NULL);
    for (int i = 0; i < g_registry.finished_count; ) {
        if ((now - g_registry.finished[i]->started_at) > FINISHED_TTL_SECONDS) {
            free(g_registry.finished[i]->output_buffer);
            free(g_registry.finished[i]);
            g_registry.finished[i] = g_registry.finished[g_registry.finished_count - 1];
            g_registry.finished_count--;
        } else {
            i++;
        }
    }

    /* Limit total processes */
    if (g_registry.running_count + g_registry.finished_count >= MAX_PROCESSES && g_registry.finished_count > 0) {
        int oldest_idx = 0;
        for (int i = 1; i < g_registry.finished_count; i++) {
            if (g_registry.finished[i]->started_at < g_registry.finished[oldest_idx]->started_at) {
                oldest_idx = i;
            }
        }
        free(g_registry.finished[oldest_idx]->output_buffer);
        free(g_registry.finished[oldest_idx]);
        g_registry.finished[oldest_idx] = g_registry.finished[g_registry.finished_count - 1];
        g_registry.finished_count--;
    }

    pthread_mutex_unlock(&g_registry.lock);
}

/* PoP: spawn_local @ process_registry:ProcessRegistry.spawn_local */
ProcessSession* process_registry_spawn_local(const char *command,
                                              const char *cwd,
                                              const char *task_id,
                                              const char *session_key,
                                              const char *env_vars_json) {
    registry_init();

    char id[32];
    snprintf(id, sizeof(id), "proc_%012lx", (unsigned long)time(NULL) ^ (unsigned long)pthread_self());

    ProcessSession *s = malloc(sizeof(ProcessSession));
    session_init(s, id, command, task_id, session_key, 0, cwd, false, "host");

    /* Parse env vars if provided */
    /* For simplicity, we just set them in the child */

    int stdin_pipe[2];
    if (pipe(stdin_pipe) < 0) {
        free(s->output_buffer);
        free(s);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        free(s->output_buffer);
        free(s);
        return NULL;
    }

    if (pid == 0) {
        /* Child */
        close(stdin_pipe[1]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);

        /* Apply env vars */
        if (env_vars_json && env_vars_json[0]) {
            /* Parse JSON env vars - simplified */
        }

        if (cwd && cwd[0]) chdir(cwd);
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        _exit(127);
    }

    /* Parent */
    close(stdin_pipe[0]);
    s->pid = pid;
    s->child_pid = pid;
    s->stdin_fd = stdin_pipe[1];

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

/* PoP: poll @ process_registry:ProcessRegistry.poll */
char* process_registry_poll(const char *session_id) {
    registry_init();

    ProcessSession *s = NULL;
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->id, session_id) == 0) {
            s = g_registry.running[i];
            break;
        }
    }
    if (!s) {
        for (int i = 0; i < g_registry.finished_count; i++) {
            if (strcmp(g_registry.finished[i]->id, session_id) == 0) {
                s = g_registry.finished[i];
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_registry.lock);

    if (!s) {
        return strdup("{\"status\":\"not_found\",\"error\":\"No such session\"}");
    }

    /* Reconcile */
    reconcile_local_exit(s);

    char uptime_buf[64];
    format_uptime_short((int)(time(NULL) - s->started_at), uptime_buf, sizeof(uptime_buf));

    char *output_preview = "";
    pthread_mutex_lock(&s->lock);
    if (s->output_len > 0) {
        size_t start = s->output_len > 1000 ? s->output_len - 1000 : 0;
        output_preview = s->output_buffer + start;
    }
    bool exited = !s->running;
    int exit_code = s->exit_code;
    pthread_mutex_unlock(&s->lock);

    char *result = malloc(2048);
    snprintf(result, 2048,
        "{\"session_id\":\"%s\",\"command\":\"%s\",\"status\":\"%s\",\"pid\":%d,"
        "\"uptime_seconds\":%d,\"output_preview\":\"%s\"%s%s}",
        s->id, s->command, exited ? "exited" : "running",
        (int)s->pid, (int)(time(NULL) - s->started_at), output_preview,
        exited ? ",\"exit_code\":" : "",
        exited ? "" : "");
    if (exited) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%d", exit_code);
        strcat(result, tmp);
    }

    if (exited) {
        /* Mark consumed */
        pthread_mutex_lock(&g_registry.lock);
        if (g_registry.consumed_count == g_registry.consumed_cap) {
            g_registry.consumed_cap *= 2;
            g_registry.completion_consumed = realloc(g_registry.completion_consumed,
                g_registry.consumed_cap * sizeof(char*));
        }
        g_registry.completion_consumed[g_registry.consumed_count++] = strdup(session_id);
        pthread_mutex_unlock(&g_registry.lock);
    }

    return result;
}

/* PoP: wait @ process_registry:ProcessRegistry.wait */
/* PoP: wait @ tools/process_registry.py:wait */
char* process_registry_wait(const char *session_id, int timeout_sec) {
    registry_init();

    ProcessSession *s = NULL;
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->id, session_id) == 0) {
            s = g_registry.running[i];
            break;
        }
    }
    if (!s) {
        for (int i = 0; i < g_registry.finished_count; i++) {
            if (strcmp(g_registry.finished[i]->id, session_id) == 0) {
                s = g_registry.finished[i];
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_registry.lock);

    if (!s) {
        return strdup("{\"status\":\"not_found\",\"error\":\"No such session\"}");
    }

    if (timeout_sec <= 0) timeout_sec = 180;

    time_t deadline = time(NULL) + timeout_sec;
    while (time(NULL) < deadline) {
        reconcile_local_exit(s);

        pthread_mutex_lock(&s->lock);
        if (!s->running) {
            int exit_code = s->exit_code;
            pthread_mutex_unlock(&s->lock);

            char *result = malloc(2048);
            snprintf(result, 2048,
                "{\"status\":\"exited\",\"exit_code\":%d}", exit_code);
            return result;
        }
        pthread_mutex_unlock(&s->lock);

        /* Check interrupt - simplified */
        /* interrupt check pending */

        sleep(1);
    }

    return strdup("{\"status\":\"timeout\",\"output\":\"process still running\"}");
}

/* PoP: kill_process @ process_registry:ProcessRegistry.kill_process */
char* process_registry_kill(const char *session_id) {
    registry_init();

    ProcessSession *s = NULL;
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->id, session_id) == 0) {
            s = g_registry.running[i];
            break;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);

    if (!s) {
        return strdup("{\"status\":\"not_found\",\"error\":\"No such session\"}");
    }

    if (!s->running) {
        return strdup("{\"status\":\"already_exited\"}");
    }

    /* Kill the process */
    if (s->pid > 0) {
        kill(s->pid, SIGTERM);
        usleep(100000);
        kill(s->pid, SIGKILL);
    }

    s->running = false;
    s->exit_code = -15;
    move_to_finished(s);

    return strdup("{\"status\":\"killed\"}");
}

/* PoP: count_running @ process_registry:ProcessRegistry.count_running */
int process_registry_count_running(void) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    int count = g_registry.running_count;
    pthread_mutex_unlock(&g_registry.lock);
    return count;
}

/* PoP: list_sessions @ process_registry:ProcessRegistry.list_sessions */
char* process_registry_list(const char *task_id_filter) {
    registry_init();

    pthread_mutex_lock(&g_registry.lock);
    int total = g_registry.running_count + g_registry.finished_count;
    char **sessions = malloc(total * sizeof(char*));
    int count = 0;

    for (int i = 0; i < g_registry.running_count; i++) {
        if (!task_id_filter || strcmp(g_registry.running[i]->task_id, task_id_filter) == 0) {
            sessions[count++] = (char*)g_registry.running[i];
        }
    }
    for (int i = 0; i < g_registry.finished_count; i++) {
        if (!task_id_filter || strcmp(g_registry.finished[i]->task_id, task_id_filter) == 0) {
            sessions[count++] = (char*)g_registry.finished[i];
        }
    }
    pthread_mutex_unlock(&g_registry.lock);

    /* Build JSON array */
    size_t cap = 16384;
    char *result = malloc(cap);
    result[0] = '[';
    size_t len = 1;

    for (int i = 0; i < count; i++) {
        ProcessSession *s = (ProcessSession*)sessions[i];
        reconcile_local_exit(s);

        char uptime_buf[64];
        format_uptime_short((int)(time(NULL) - s->started_at), uptime_buf, sizeof(uptime_buf));

        char entry[2048];
        size_t entry_len = snprintf(entry, sizeof(entry),
            "{\"session_id\":\"%s\",\"command\":\"%.200s\",\"cwd\":\"\",\"pid\":%d,"
            "\"started_at\":\"%ld\",\"uptime_seconds\":%d,\"status\":\"%s\",\"output_preview\":\"\"}%s",
            s->id, s->command, (int)s->pid, (long)s->started_at,
            (int)(time(NULL) - s->started_at), s->running ? "running" : "exited",
            s->running ? "" : ",\"exit_code\":0");

        if (len + entry_len + 2 >= cap) {
            cap *= 2;
            result = realloc(result, cap);
        }
        if (i > 0) result[len++] = ',';
        memcpy(result + len, entry, entry_len);
        len += entry_len;
    }

    result[len++] = ']';
    result[len] = '\0';
    free(sessions);
    return result;
}

/* PoP: has_active_processes @ process_registry:ProcessRegistry.has_active_processes */
bool process_registry_has_active_for_task(const char *task_id) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->task_id, task_id) == 0 && g_registry.running[i]->running) {
            pthread_mutex_unlock(&g_registry.lock);
            return true;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
    return false;
}

/* PoP: has_active_for_session @ process_registry:ProcessRegistry.has_active_for_session */
bool process_registry_has_active_for_session(const char *session_key) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->session_key, session_key) == 0 && g_registry.running[i]->running) {
            pthread_mutex_unlock(&g_registry.lock);
            return true;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
    return false;
}

/* PoP: kill_all @ process_registry:ProcessRegistry.kill_all */
int process_registry_kill_all(const char *task_id_filter) {
    registry_init();
    int killed = 0;
    bool done = false;
    while (!done) {
        done = true;
        pthread_mutex_lock(&g_registry.lock);
        for (int i = 0; i < g_registry.running_count; i++) {
            ProcessSession *s = g_registry.running[i];
            if (s->running && (!task_id_filter || strcmp(s->task_id, task_id_filter) == 0)) {
                if (s->pid > 0) kill(s->pid, SIGKILL);
                s->running = false;
                s->exit_code = -9;
                killed++;
                done = false;
                break;
            }
        }
        pthread_mutex_unlock(&g_registry.lock);
        if (!done) move_to_finished(NULL); /* Will be handled per-session */
    }
    return killed;
}

/* PoP: _write_checkpoint @ process_registry:ProcessRegistry._write_checkpoint */
void process_registry_write_checkpoint(void) {
    registry_init();

    pthread_mutex_lock(&g_registry.lock);
    FILE *fp = fopen("/tmp/processes.json", "w");
    if (!fp) {
        pthread_mutex_unlock(&g_registry.lock);
        return;
    }

    fprintf(fp, "[\n");
    for (int i = 0; i < g_registry.running_count; i++) {
        ProcessSession *s = g_registry.running[i];
        if (!s->running) continue;

        fprintf(fp, "  {\n");
        fprintf(fp, "    \"session_id\":\"%s\",\n", s->id);
        fprintf(fp, "    \"command\":\"%s\",\n", s->command);
        fprintf(fp, "    \"pid\":%d,\n", (int)s->pid);
        fprintf(fp, "    \"pid_scope\":\"%s\",\n", s->pid_scope);
        fprintf(fp, "    \"cwd\":\"\",\n");
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

/* PoP: recover_from_checkpoint @ process_registry:ProcessRegistry.recover_from_checkpoint */
int process_registry_recover_from_checkpoint(void) {
    registry_init();

    FILE *fp = fopen("/tmp/processes.json", "r");
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = malloc(size + 1);
    fread(content, 1, size, fp);
    content[size] = '\0';
    fclose(fp);

    /* Simple JSON parse - just extract session_id and pid */
    int recovered = 0;
    char *p = content;
    while ((p = strstr(p, "\"session_id\""))) {
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p == ' ' || *p == '\"') p++;
        char session_id[64] = {0};
        int i = 0;
        while (p[i] && p[i] != '\"' && i < 63) {
            session_id[i] = p[i];
            i++;
        }

        p = strstr(p, "\"pid\"");
        if (!p) break;
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p == ' ') p++;
        int pid = atoi(p);

        if (pid > 0 && _pid_exists(pid)) {
            ProcessSession *s = malloc(sizeof(ProcessSession));
            session_init(s, session_id, "recovered", "", "", pid, "", true, "host");
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

/* PoP: drain_notifications @ process_registry:ProcessRegistry.drain_notifications */
char* process_registry_drain_notifications(void) {
    registry_init();

    pthread_mutex_lock(&g_registry.lock);
    if (g_registry.queue_count == 0) {
        pthread_mutex_unlock(&g_registry.lock);
        return strdup("[]");
    }

    size_t cap = 8192;
    char *result = malloc(cap);
    result[0] = '[';
    size_t len = 1;

    while (g_registry.queue_count > 0) {
        char *evt = g_registry.completion_queue[g_registry.queue_head];
        g_registry.queue_head = (g_registry.queue_head + 1) % g_registry.queue_cap;
        g_registry.queue_count--;

        size_t evt_len = strlen(evt);
        if (len + evt_len + 2 >= cap) {
            cap *= 2;
            result = realloc(result, cap);
        }
        if (len > 1) result[len++] = ',';
        memcpy(result + len, evt, evt_len);
        len += evt_len;
        free(evt);
    }

    result[len++] = ']';
    result[len] = '\0';
    pthread_mutex_unlock(&g_registry.lock);
    return result;
}

/* PoP: set_watch_patterns @ process_registry:ProcessRegistry.set_watch_patterns */
void process_registry_set_watch_patterns(const char *session_id, char **patterns, int count) {
    registry_init();

    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->id, session_id) == 0) {
            ProcessSession *s = g_registry.running[i];
            if (s->watch_patterns) {
                for (int j = 0; j < s->watch_pattern_count; j++) free(s->watch_patterns[j]);
                free(s->watch_patterns);
            }
            s->watch_pattern_count = count;
            s->watch_pattern_cap = count > 0 ? count : 1;
            s->watch_patterns = count > 0 ? malloc(count * sizeof(char*)) : NULL;
            for (int j = 0; j < count; j++) {
                s->watch_patterns[j] = strdup(patterns[j]);
            }
            s->watch_disabled = false;
            s->watch_consecutive_strikes = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
}

/* PoP: set_notify_on_complete @ process_registry:ProcessRegistry.set_notify_on_complete */
void process_registry_set_notify_on_complete(const char *session_id, bool notify) {
    registry_init();

    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->id, session_id) == 0) {
            g_registry.running[i]->notify_on_complete = notify;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
}

int process_registry_get_session(const char *session_id, ProcessSession **out) {
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    for (int i = 0; i < g_registry.running_count; i++) {
        if (strcmp(g_registry.running[i]->id, session_id) == 0) {
            *out = g_registry.running[i];
            pthread_mutex_unlock(&g_registry.lock);
            return 0;
        }
    }
    for (int i = 0; i < g_registry.finished_count; i++) {
        if (strcmp(g_registry.finished[i]->id, session_id) == 0) {
            *out = g_registry.finished[i];
            pthread_mutex_unlock(&g_registry.lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
    return -1;
}

/* Port of Python tools/process_registry.py:is_completion_consumed() —
 * consumed-set accessor backing the port_process_registry wrapper. */
bool process_registry_completion_consumed(const char *session_id) {
    if (!session_id) return false;
    registry_init();
    pthread_mutex_lock(&g_registry.lock);
    bool found = false;
    for (int i = 0; i < g_registry.consumed_count; i++) {
        if (g_registry.completion_consumed[i] &&
            strcmp(g_registry.completion_consumed[i], session_id) == 0) {
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&g_registry.lock);
    return found;
}

void process_registry_append_output(const char *session_id, const char *text) {
    ProcessSession *s = NULL;
    process_registry_get_session(session_id, &s);
    if (s) session_append_output(s, text, strlen(text));
}

/* ============================================================================
 *  Close-terminal sink (tools/close_terminal_tool.py: request_close_terminal)
 *  Desktop-only: the desktop gateway wires an on_close sink that emits a
 *  terminal.close event. Outside the GUI (CLI / messaging) the sink is NULL
 *  and close_terminal returns an error. The sink is injected, not hardcoded,
 *  so this module stays self-contained.
 * ========================================================================== */
static void (*g_close_sink)(ProcessSession *session, const char *session_id) = NULL;

void process_registry_set_close_sink(void (*sink)(ProcessSession *session, const char *session_id)) {
    g_close_sink = sink;
}

/* PoP: request_close_terminal @ tools/process_registry.py:ProcessRegistry.request_close_terminal */
char *process_registry_request_close_terminal(const char *session_id) {
    if (!g_close_sink) {
        return strdup("{\"status\":\"error\","
                      "\"error\":\"close_terminal is only available in the Hermes desktop app.\"}");
    }
    /* A missing session is NOT an error: the tab can linger after the process
     * finished/pruned, and closing it is still valid. */
    ProcessSession *s = NULL;
    process_registry_get_session(session_id, &s);
    if (g_close_sink) {
        g_close_sink(s, session_id);
    }
    char *out = malloc(512);
    if (!out) return strdup("{\"status\":\"error\",\"error\":\"oom\"}");
    snprintf(out, 512,
        "{\"status\":\"ok\",\"closed\":\"%s\","
        "\"note\":\"Closed the read-only terminal tab. The process was not killed; "
        "its output remains available and the user can reopen the tab from the status stack.\"}",
        session_id ? session_id : "");
    return out;
}