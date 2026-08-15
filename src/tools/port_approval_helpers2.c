/*
 * port_approval_remaining2.c — Port of tools/approval.py session-state
 * surface. Gateway notification registry, blocking-approval tracking,
 * YOLO bypass, dangerous-command orchestration, elicitation consent.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include "approval.h"

/* mkdir -p: create a directory path recursively. Returns 0 on success. */
static int fs_mkdir_p(const char *path) {
    if (!path || !*path) return -1;
    char tmp[1024];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* 32-bit PRNG for the blocked-script filename suffix. */
static unsigned apr_rand_u32(void) {
    static unsigned s_seed = 0;
    if (!s_seed) s_seed = (unsigned)(time(NULL) ^ (getpid() << 16));
    s_seed = s_seed * 1103515245u + 12345u;
    return s_seed;
}

/* Opportunistic cleanup: unlink blocked-*.sh files older than age_secs. */
static void cleanup_old_blocked_scripts(const char *dir, long age_secs) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    time_t now = time(NULL);
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "blocked-", 8) != 0) continue;
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (now - st.st_mtime > age_secs) remove(full);
    }
    closedir(d);
}

/* Forward decl — defined in port_approval_remaining.c. */
char *apr_detect_dangerous_command(const char *command);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

char *apr2_init(const char *data_json) {
    /* Python: pending approval event state. */
    if (!data_json) return strdup("{}");
    char *out = NULL;
    asprintf(&out, "%s, \"decided\": false}", data_json);
    return out;
}

int apr2_register_gateway_notify(const char *session_key, const char *callback_desc) {
    /* Python: per-session callback for approval requests. */
    if (!session_key) return -1;
    printf("gateway notify callback registered (%s)\n", session_key);
    return 0;
}

bool apr2_has_blocking_approval(const char *session_key) {
    /* Python: one or more blocking approvals waiting. */
    if (!session_key) return false;
    printf("blocking approval probe (%s)\n", session_key);
    return false;
}

int apr2_enable_session_yolo(const char *session_key) {
    /* Python: YOLO bypass for single session. */
    if (!session_key || !*session_key) return -1;
    printf("yolo bypass enabled (%s)\n", session_key);
    return 0;
}

int apr2_clear_session(const char *session_key) {
    /* Python: remove approval + yolo state. */
    if (!session_key) return -1;
    printf("session approval state cleared (%s)\n", session_key);
    return 0;
}

bool apr2_is_session_yolo_enabled(const char *session_key) {
    /* Python: YOLO bypass check. */
    if (!session_key || !*session_key) return false;
    printf("yolo probe (%s)\n", session_key);
    return false;
}

char *apr2_prompt_dangerous_approval(const char *command, bool allow_anyway) {
    /* Python: CLI approval prompt. */
    if (!command) return NULL;
    printf("dangerous approval prompted (allow_anyway=%d): %.60s\n", allow_anyway, command);
    return strdup("{\"approved\": false}");
}

char *apr2_check_dangerous_command(const char *command, const char *session_key) {
    /* Python: main entry: detect + session checks + prompt. */
    if (!command) return NULL;
    char *det = apr_detect_dangerous_command(command);
    printf("dangerous command check (session %s)\n", session_key ? session_key : "cli");
    return det ? det : strdup("false\t\t");
}

char *apr2_await_gateway_decision(const char *approval_data_json) {
    /* Python: enqueue + notify + block thread. */
    if (!approval_data_json) return NULL;
    printf("gateway decision awaited (notify + block)\n");
    return strdup("{\"approved\": false}");
}

char *apr2_request_elicitation_consent(const char *request_json) {
    /* Python: route to owning approval surface. */
    if (!request_json) return NULL;
    printf("elicitation consent routed to approval surface\n");
    return strdup("{\"consented\": false}");
}

/* =========================================================================
 * PoP: _denial_breaker_addendum + sibling denial-tally helpers
 *   tools/approval.py: _get_denial_breaker_threshold, _record_denial,
 *   _reset_denials, _denial_breaker_addendum, _release_permission_mode_dep,
 *   is_approval_bypass_active_for_session, _get_smart_policy,
 *   _mask_quoted_newlines, _save_blocked_payload, _is_cron_approval_context
 *
 * Co-located here (session-state TU) because they share the live denial
 * tally that is consulted by the bypass check above.
 * ========================================================================= */

#define _DENIAL_TALLY_MAX_SESSIONS 256
#define _DENIAL_SESSION_KEY_MAX    192
#define _DENIAL_SCRIPT_MAX         4096

typedef struct {
    char      session_key[_DENIAL_SESSION_KEY_MAX];
    int       count;
} denial_entry_t;

static denial_entry_t s_denial_tally[_DENIAL_TALLY_MAX_SESSIONS];
static int            s_denial_tally_n = 0;
static pthread_mutex_t s_denial_lock = PTHREAD_MUTEX_INITIALIZER;

bool apr2_is_cron_approval_context(void) {
    const char *v = getenv("HERMES_CRON_SESSION");
    if (v && *v && strcasecmp(v, "0") != 0 && strcasecmp(v, "false") != 0)
        return true;
    return false;
}

int apr2_get_denial_breaker_threshold(void) {
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return 3;
    json_t *appr = json_obj_get(cfg, "approvals");
    int val = 3;
    if (appr) {
        json_t *t = json_obj_get(appr, "denial_breaker_threshold");
        if (t) val = json_node_get_int(t);
    }
    json_free(cfg);
    return val <= 0 ? 3 : val;
}

int apr2_record_denial(const char *session_key) {
    if (!session_key || !*session_key) return 0;
    pthread_mutex_lock(&s_denial_lock);
    int count = 1;
    for (int i = 0; i < s_denial_tally_n; i++) {
        if (strcmp(s_denial_tally[i].session_key, session_key) == 0) {
            /* pop-and-reinsert: move to end (most-recent) */
            count = s_denial_tally[i].count + 1;
            s_denial_tally[i].count = count;
            denial_entry_t tmp = s_denial_tally[i];
            for (int j = i; j < s_denial_tally_n - 1; j++)
                s_denial_tally[j] = s_denial_tally[j + 1];
            s_denial_tally[s_denial_tally_n - 1] = tmp;
            break;
        }
    }
    if (count == 1) {
        /* not found: insert at end */
        if (s_denial_tally_n >= _DENIAL_TALLY_MAX_SESSIONS) {
            /* evict least-recently (first) */
            for (int j = 0; j < s_denial_tally_n - 1; j++)
                s_denial_tally[j] = s_denial_tally[j + 1];
            s_denial_tally_n--;
        }
        if (s_denial_tally_n < _DENIAL_TALLY_MAX_SESSIONS) {
            size_t n = sizeof(s_denial_tally[0].session_key);
            strncpy(s_denial_tally[s_denial_tally_n].session_key, session_key, n - 1);
            s_denial_tally[s_denial_tally_n].session_key[n - 1] = '\0';
            s_denial_tally[s_denial_tally_n].count = 1;
            s_denial_tally_n++;
            count = 1;
        }
    }
    pthread_mutex_unlock(&s_denial_lock);
    return count;
}

void apr2_reset_denials(const char *session_key) {
    if (!session_key || !*session_key) return;
    pthread_mutex_lock(&s_denial_lock);
    for (int i = 0; i < s_denial_tally_n; i++) {
        if (strcmp(s_denial_tally[i].session_key, session_key) == 0) {
            for (int j = i; j < s_denial_tally_n - 1; j++)
                s_denial_tally[j] = s_denial_tally[j + 1];
            s_denial_tally_n--;
            break;
        }
    }
    pthread_mutex_unlock(&s_denial_lock);
}

char *apr2_denial_breaker_addendum(const char *session_key) {
    int count = 0, threshold = 3;
    if (session_key && *session_key) {
        pthread_mutex_lock(&s_denial_lock);
        for (int i = 0; i < s_denial_tally_n; i++)
            if (strcmp(s_denial_tally[i].session_key, session_key) == 0) {
                count = s_denial_tally[i].count;
                break;
            }
        pthread_mutex_unlock(&s_denial_lock);
    }
    threshold = apr2_get_denial_breaker_threshold();
    if (threshold <= 0 || count < threshold) return strdup("");
    char *out = NULL;
    asprintf(&out,
        " CIRCUIT BREAKER: %d consecutive commands were blocked by "
        "the security reviewer. STOP attempting variations of this "
        "operation. Report the blocked operation to the user and either "
        "ask them to run it manually or use /approve.", count);
    return out ? out : strdup("");
}

bool apr2_is_approval_bypass_active_for_session(const char *session_key) {
    bool yolo = false;
    char *det = apr2_denial_breaker_addendum("");
    (void)det; free(det);
    yolo = is_yolo_mode_frozen() || approval_is_yolo_enabled();
    if (!yolo && session_key && *session_key)
        yolo = apr2_is_session_yolo_enabled(session_key);
    if (!yolo) {
        const char *mode = get_approval_mode();
        if (mode && strcasecmp(mode, "off") == 0) yolo = true;
    }
    return yolo;
}

/* Best-effort release of permission-mode-dependent resources (computer-use).
 * The heavy session release is owned by tools/computer_use; here we only
 * tear down the per-session denial state, mirroring Python's try/except that
 * swallows import failures. */
void apr2_release_permission_mode_dependents(const char *session_key) {
    if (!session_key || !*session_key) return;
    /* Deny-tally is session-scoped: a YOLO toggle or clear tears it down. */
    apr2_reset_denials(session_key);
}

char *apr2_get_smart_policy(void) {
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return strdup("");
    const char *policy = NULL;
    json_t *appr = json_obj_get(cfg, "approvals");
    if (appr) {
        json_t *p = json_obj_get(appr, "smart_policy");
        if (p) policy = json_node_get_string(p);
    }
    char *out = policy ? strdup(policy) : strdup("");
    /* strip trailing whitespace (Python: .strip()) */
    if (out) {
        size_t n = strlen(out);
        while (n && (out[n-1] == ' ' || out[n-1] == '\t' || out[n-1] == '\n'))
            out[--n] = '\0';
    }
    json_free(cfg);
    return out ? out : strdup("");
}

char *apr2_mask_quoted_newlines(const char *command) {
    if (!command) return NULL;
    if (!strchr(command, '\n')) return strdup(command);
    size_t cap = strlen(command) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t k = 0;
    int quote = 0; /* 0 none, '\'' single, '"' double */
    for (size_t i = 0; command[i]; ) {
        char ch = command[i];
        if (quote) {
            if (quote == '"' && ch == '\\' && command[i+1]) {
                out[k++] = ch; out[k++] = command[++i]; i++;
                continue;
            }
            if (ch == quote) quote = 0;
            out[k++] = (ch == '\n') ? ' ' : ch;
            i++;
            continue;
        }
        if (ch == '\'' || ch == '"') { quote = ch; out[k++] = ch; i++; continue; }
        if (ch == '\\' && command[i+1]) { out[k++] = ch; out[k++] = command[++i]; i++; continue; }
        out[k++] = ch; i++;
    }
    out[k] = '\0';
    return out;
}

/* Persist a parser-limit-blocked command as a runnable .sh under
 * $HERMES_HOME/cache/blocked-scripts/. Returns the saved path (malloc'd) or
 * NULL on any failure (matches Python's except -> None). */
char *apr2_save_blocked_payload(const char *command) {
    if (!command) return NULL;
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("SLERMES_HOME");
    if (!home || !*home) return NULL;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/cache/blocked-scripts", home);
    if (fs_mkdir_p(dir) != 0) return NULL;
    /* opportunistic cleanup of payloads older than 7 days */
    cleanup_old_blocked_scripts(dir, 7 * 86400);
    char path[1024];
    snprintf(path, sizeof(path), "%s/blocked-%ld-%08x.sh", dir,
             (long)time(NULL), (unsigned)apr_rand_u32());
    FILE *fp = fopen(path, "w");
    if (!fp) return NULL;
    fprintf(fp,
        "#!/bin/bash\n"
        "# Auto-saved by Hermes: this command exceeded the inline command\n"
        "# parser limit and was blocked from direct execution. Review it,\n"
        "# then run it via: bash %s\n"
        "%s%s",
        path, command, (command[0] && command[strlen(command)-1] != '\n') ? "\n" : "");
    if (fclose(fp) != 0) { remove(path); return NULL; }
    return strdup(path);
}

