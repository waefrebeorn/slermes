/*
 * process_registry.h — In-memory background process registry.
 * Port of Python tools/process_registry.py.
 * Manages process sessions: spawn_local, spawn_via_env, poll, wait, kill,
 * log, stdin I/O, list, count, checkpoint/recovery, watch patterns.
 *
 * PoP annotations link each C function to its Python counterpart.
 */

#ifndef HERMES_PROCESS_REGISTRY_H
#define HERMES_PROCESS_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

/* Environment backend hook (tools/environments/base.py:execute).
 * env_ref passed by the caller; returns a malloc'd JSON result string
 * (caller frees) or NULL on failure. Non-NULL only when wired; the
 * spawn_via_env/_reader_loop/_env_poller_loop ports stay self-contained. */
typedef char *(*process_registry_env_exec_fn)(void *env_ref, const char *command,
                                              int timeout_seconds, int *out_rc);

/* Inject the env-execute backend (NULL clears). */
void process_registry_set_env_exec(process_registry_env_exec_fn fn);

/* Forward declaration */
typedef struct ProcessSession ProcessSession;
typedef struct ProcessRegistry ProcessRegistry;

/* Port of Python tools/process_registry.py:ProcessSession (dataclass) */
struct ProcessSession {
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
    char cwd[512];                      /* working directory */
    char completion_reason[32];         /* exited|killed|lost|failed_start|already_exited */
    char termination_source[64];        /* process.kill|kill_all|backend_lost|failed_start */
    long host_start_time;               /* kernel start ticks - PID-reuse guard (0 = unset) */
    void *env_ref;                      /* reference to environment backend for non-local */
    char **output_lines;                /* drained reader output lines */
    int output_line_count;
    int output_line_cap;

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
};

/* Port of Python tools/process_registry.py:ProcessRegistry */
struct ProcessRegistry {
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

    /* Poll-observed tracking: sessions whose exit was seen inline via poll().
     * Port of Python ProcessRegistry._poll_observed (#10156). Read-only polls
     * never mark _completion_consumed; this set lets the CLI inline drain
     * dedup without suppressing gateway/tui watcher delivery. */
    char **poll_observed;
    int poll_observed_count;
    int poll_observed_cap;

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
};
typedef struct ProcessRegistry ProcessRegistry;

/* Initialize the global registry */
void process_registry_init(void);

/* PoP: format_uptime_short @ tools/process_registry.py:format_uptime_short */
void format_uptime_short(int seconds, char *buf, size_t buf_len);

/* PoP: spawn_local @ tools/process_registry.py:ProcessRegistry.spawn_local */
ProcessSession* process_registry_spawn_local(const char *command,
                                              const char *cwd,
                                              const char *task_id,
                                              const char *session_key,
                                              const char *env_vars_json);

/* PoP: spawn_via_env @ tools/process_registry.py:ProcessRegistry.spawn_via_env */
ProcessSession* process_registry_spawn_via_env(const char *command,
                                                 const char *cwd,
                                                 const char *task_id,
                                                 const char *session_key,
                                                 void *env_ref,
                                                 int timeout_seconds);

/* PoP: poll @ tools/process_registry.py:ProcessRegistry.poll */
char* process_registry_poll(const char *session_id);

/* PoP: wait @ tools/process_registry.py:ProcessRegistry.wait */
char* process_registry_wait(const char *session_id, int timeout_sec);

/* PoP: kill_process @ tools/process_registry.py:ProcessRegistry.kill_process */
char* process_registry_kill(const char *session_id, const char *source,
                            bool consume_output);

/* PoP: count_running @ tools/process_registry.py:ProcessRegistry.count_running */
int process_registry_count_running(void);

/* PoP: list_sessions @ tools/process_registry.py:ProcessRegistry.list_sessions */
char* process_registry_list(const char *task_id_filter, const char *session_key);

/* PoP: has_active_processes @ tools/process_registry.py:ProcessRegistry.has_active_processes */
bool process_registry_has_active_for_task(const char *task_id);

/* PoP: has_active_for_session @ tools/process_registry.py:ProcessRegistry.has_active_for_session */
bool process_registry_has_active_for_session(const char *session_key,
                                             double max_active_age_seconds);

/* PoP: kill_all @ tools/process_registry.py:ProcessRegistry.kill_all */
int process_registry_kill_all(const char *task_id_filter, const char *source,
                              bool consume_output);

/* PoP: kill_started_since @ tools/process_registry.py:ProcessRegistry.kill_started_since */
int process_registry_kill_started_since(const char *task_id,
                                        const char **baseline_ids,
                                        int baseline_count,
                                        const char *source);

/* PoP: _write_checkpoint @ tools/process_registry.py:ProcessRegistry._write_checkpoint */
void process_registry_write_checkpoint(void);

/* PoP: recover_from_checkpoint @ tools/process_registry.py:ProcessRegistry.recover_from_checkpoint */
int process_registry_recover_from_checkpoint(void);

/* PoP: drain_notifications @ tools/process_registry.py:ProcessRegistry.drain_notifications */
char* process_registry_drain_notifications(const char *session_key_filter,
                                           bool skip_poll_observed);

/* PoP: _drain_should_skip @ tools/process_registry.py:ProcessRegistry._drain_should_skip */
bool process_registry_drain_should_skip(const char *session_id,
                                        bool skip_poll_observed);

/* PoP: set_watch_patterns @ tools/process_registry.py:ProcessRegistry.set_watch_patterns */
void process_registry_set_watch_patterns(const char *session_id, char **patterns, int count);

/* PoP: set_notify_on_complete @ tools/process_registry.py:ProcessRegistry.set_notify_on_complete */
void process_registry_set_notify_on_complete(const char *session_id, bool notify);

/* Get session by ID */
int process_registry_get_session(const char *session_id, ProcessSession **out);
/* Check whether a completion notification was already consumed. */
bool process_registry_completion_consumed(const char *session_id);
/* Append output to session */
void process_registry_append_output(const char *session_id, const char *text);

/* PoP: is_completion_consumed @ tools/process_registry.py:ProcessRegistry.is_completion_consumed */
bool process_registry_completion_consumed(const char *session_id);

/* PoP: is_session_waiting @ tools/process_registry.py:ProcessRegistry.is_session_waiting */
bool process_registry_is_session_waiting(const char *session_id);

/* PoP: read_log @ tools/process_registry.py:ProcessRegistry.read_log */
char* process_registry_read_log(const char *session_id, int offset, int limit);

/* PoP: has_any_active @ tools/process_registry.py:ProcessRegistry.has_any_active */
bool process_registry_has_any_active(void);

/* PoP: snapshot_running_ids @ tools/process_registry.py:ProcessRegistry.snapshot_running_ids */
char** process_registry_snapshot_running_ids(const char *task_id, int *count_out);

/* PoP: write_stdin @ tools/process_registry.py:ProcessRegistry.write_stdin */
char* process_registry_write_stdin(const char *session_id, const char *data);

/* PoP: submit_stdin @ tools/process_registry.py:ProcessRegistry.submit_stdin */
char* process_registry_submit_stdin(const char *session_id, const char *data);

/* PoP: close_stdin @ tools/process_registry.py:ProcessRegistry.close_stdin */
char* process_registry_close_stdin(const char *session_id);

/* Close-terminal sink (tools/close_terminal_tool.py: request_close_terminal).
 * The desktop gateway injects an on_close sink that emits a terminal.close
 * event; NULL means close_terminal is unavailable (CLI / messaging). */
typedef void (*process_registry_close_sink_t)(ProcessSession *session, const char *session_id);
void process_registry_set_close_sink(process_registry_close_sink_t sink);
/* Returns a malloc'd JSON string: {status:error,...} when no sink is wired,
 * else {status:ok, closed:<id>, note:...}. Caller frees. */
char *process_registry_request_close_terminal(const char *session_id);

#endif /* HERMES_PROCESS_REGISTRY_H */
