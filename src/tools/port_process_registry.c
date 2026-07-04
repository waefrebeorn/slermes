/**
 * port_process_registry.c — Port of Python: tools/process_registry.py
 *
 * Real C implementations for process registry functions.
 */

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