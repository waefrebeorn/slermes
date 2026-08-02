/*
 * process_registry.h — In-memory background process registry.
 * Port of Python tools/process_registry.py.
 */

#ifndef HERMES_PROCESS_REGISTRY_H
#define HERMES_PROCESS_REGISTRY_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>

/* Forward declaration */
typedef struct ProcessSession ProcessSession;
typedef struct ProcessRegistry ProcessRegistry;

/* Initialize the global registry */
void process_registry_init(void);

/* PoP: format_uptime_short @ process_registry:format_uptime_short */
void format_uptime_short(int seconds, char *buf, size_t buf_len);

/* Session management */
struct ProcessSession {
    char id[32];
    char command[4096];
    char task_id[64];
    char session_key[64];
    pid_t pid;
    int child_pid;
    bool running;
    int exit_code;
    time_t started_at;
    char *output_buffer;
    size_t output_len;
    size_t output_cap;
    bool detached;
    char pid_scope[16];

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

/* PoP: spawn_local @ process_registry:ProcessRegistry.spawn_local */
ProcessSession* process_registry_spawn_local(const char *command,
                                              const char *cwd,
                                              const char *task_id,
                                              const char *session_key,
                                              const char *env_vars_json);

/* PoP: poll @ process_registry:ProcessRegistry.poll */
char* process_registry_poll(const char *session_id);

/* PoP: wait @ process_registry:ProcessRegistry.wait */
char* process_registry_wait(const char *session_id, int timeout_sec);

/* PoP: kill_process @ process_registry:ProcessRegistry.kill_process */
char* process_registry_kill(const char *session_id);

/* PoP: count_running @ process_registry:ProcessRegistry.count_running */
int process_registry_count_running(void);

/* PoP: list_sessions @ process_registry:ProcessRegistry.list_sessions */
char* process_registry_list(const char *task_id_filter);

/* PoP: has_active_processes @ process_registry:ProcessRegistry.has_active_processes */
bool process_registry_has_active_for_task(const char *task_id);

/* PoP: has_active_for_session @ process_registry:ProcessRegistry.has_active_for_session */
bool process_registry_has_active_for_session(const char *session_key);

/* PoP: kill_all @ process_registry:ProcessRegistry.kill_all */
int process_registry_kill_all(const char *task_id_filter);

/* PoP: _write_checkpoint @ process_registry:ProcessRegistry._write_checkpoint */
void process_registry_write_checkpoint(void);

/* PoP: recover_from_checkpoint @ process_registry:ProcessRegistry.recover_from_checkpoint */
int process_registry_recover_from_checkpoint(void);

/* PoP: drain_notifications @ process_registry:ProcessRegistry.drain_notifications */
char* process_registry_drain_notifications(void);

/* PoP: set_watch_patterns @ process_registry:ProcessRegistry.set_watch_patterns */
void process_registry_set_watch_patterns(const char *session_id, char **patterns, int count);

/* PoP: set_notify_on_complete @ process_registry:ProcessRegistry.set_notify_on_complete */
void process_registry_set_notify_on_complete(const char *session_id, bool notify);

/* Get session by ID */
int process_registry_get_session(const char *session_id, ProcessSession **out);
/* Check whether a completion notification was already consumed. */
bool process_registry_completion_consumed(const char *session_id);
/* Append output to session */
void process_registry_append_output(const char *session_id, const char *text);

/* Close-terminal sink (tools/close_terminal_tool.py: request_close_terminal).
 * The desktop gateway injects an on_close sink that emits a terminal.close
 * event; NULL means close_terminal is unavailable (CLI / messaging). */
typedef void (*process_registry_close_sink_t)(ProcessSession *session, const char *session_id);
void process_registry_set_close_sink(process_registry_close_sink_t sink);
/* Returns a malloc'd JSON string: {status:error,...} when no sink is wired,
 * else {status:ok, closed:<id>, note:...}. Caller frees. */
char *process_registry_request_close_terminal(const char *session_id);

#endif /* HERMES_PROCESS_REGISTRY_H */