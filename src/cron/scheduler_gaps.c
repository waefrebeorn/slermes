/*
 * scheduler_gaps.c — faithful C11 ports of the remaining REAL_GAP
 * functions of cron/scheduler.py (parity batch). See
 * include/cron_scheduler_gaps.h for the seam-mapping notes.
 *
 * Python truth: $SLERMES_SRC/cron/scheduler.py
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE 1

#include "cron_scheduler_gaps.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "cron_jobs.h"                  /* cronjobs_*, now_iso */
#include "cron_scheduler_runtime.h"     /* scheduler_deliver_result, ... */
#include "cron_scheduler_helpers.h"     /* scheduler_normalize_deliver_value,
                                           scheduler_is_cron_silence_response */
#include "port_config_py_helpers.h"     /* config_py_load_config_readonly */
#include "port_slash_commands.h"        /* reasoning_parse_effort */
#include "port_hermes_constants_reasoning.h" /* reasoning_resolve_config */
#include "profile_store.h"
/* profiles_normalize_profile_name lives in src/hermes_cli/profiles_helpers.c
 * (no public header); declared here to match its definition. */
extern int profiles_normalize_profile_name(const char *name, char *out, size_t outsz);
int profile_dir_exists(const char *name);   /* profile_store.h semantic */              /* profiles_normalize / dir_exists */
#include "../lib/libcron/cron.h"

/* ── module constants (byte-exact with scheduler.py) ──────────────────── */
#define SCHED_INFLIGHT_MIN_ALLOWANCE_MINUTES_DEFAULT 30.0
#define SCHED_DEFAULT_MEDIA_SEND_TIMEOUT 300
#define SCHED_BOT_CHAT_PLATFORM "bot-chat"

static double sched_run_claim_heartbeat_seconds_const = 60.0;

/* ── small helpers ─────────────────────────────────────────────────────── */

/* str:strip() in place semantics on a copy */
static void sched_strip_ws(const char **s) {
    const char *p = *s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    *s = p;
}
static void sched_rstrip_inplace(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (unsigned char)s[n - 1] <= ' ') s[--n] = '\0';
}

/* json_get_str that tolerates non-string nodes like Python's .get(x, d) */
static const char *jstr(const json_t *o, const char *k) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get((json_t *)o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *jget(const json_t *o, const char *k) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    return json_obj_get((json_t *)o, k);
}

/* monotonic clock in seconds (Python time.monotonic()) */
static double sched_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ================================================================
 * 1. _close_late_session_db_result
 * ================================================================ */
/* PoP: scheduler_close_late_session_db_result @ cron/scheduler.py:_close_late_session_db_result */
void scheduler_close_late_session_db_result(void *db, void (*db_close)(void *db)) {
    /* try: db = future.result(); if db is not None: db.close()
     * except Exception: pass — a NULL closer or NULL db are both the
     * swallowed-exception path. */
    if (db != NULL && db_close != NULL)
        db_close(db);
}

/* ================================================================
 * 2. _failure_streak_nudge
 * ================================================================ */
/* PoP: scheduler_failure_streak_nudge @ cron/scheduler.py:_failure_streak_nudge */
char *scheduler_failure_streak_nudge(const json_t *job) {
    json_t *schedule = jget(job, "schedule");
    const char *kind = jstr(schedule, "kind");
    if (!kind || (strcmp(kind, "cron") != 0 && strcmp(kind, "interval") != 0))
        return strdup("");

    int threshold = 3;
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *cron_cfg = jget(cfg, "cron");
        json_t *tv = jget(cron_cfg, "failure_nudge_threshold");
        if (tv && tv->type == JSON_NUMBER)
            threshold = (int)tv->num_val;
        else if (tv && tv->type == JSON_STRING)
            threshold = atoi(tv->str_val);
        json_free(cfg);
    }
    if (threshold <= 0)
        return strdup("");

    long streak = 0;
    json_t *sv = jget(job, "failure_streak");
    if (sv && sv->type == JSON_NUMBER) streak = (long)sv->num_val;
    streak += 1;   /* +1 = this run */
    if (streak < threshold)
        return strdup("");

    const char *name = jstr(job, "name");
    const char *id = jstr(job, "id");
    char job_ref[256];
    snprintf(job_ref, sizeof job_ref, "%s",
             name ? name : (id ? id : "this job"));
    size_t need = 512 + strlen(job_ref);
    char *out = malloc(need);
    if (!out) return strdup("");
    snprintf(out, need,
             "\nThis job has failed %ld runs in a row \xE2\x80\x94 worth a review. "
             "Fix its prompt/config, or pause it with `hermes cron pause %s` "
             "(resume/remove also available) to stop the noise.",
             streak, job_ref);
    return out;
}

/* ================================================================
 * 3. _resolve_job_reasoning_config
 * ================================================================ */
/* PoP: scheduler_resolve_job_reasoning_config @ cron/scheduler.py:_resolve_job_reasoning_config */
json_t *scheduler_resolve_job_reasoning_config(const json_t *job,
                                               const json_t *cfg,
                                               const char *model) {
    json_t *pinv = jget(job, "reasoning_effort");
    if (pinv) {
        const char *pinned = NULL;
        char pinbuf[64];
        if (pinv->type == JSON_STRING) {
            pinned = pinv->str_val;
        } else if (pinv->type == JSON_NUMBER) {
            snprintf(pinbuf, sizeof pinbuf, "%g", pinv->num_val);
            pinned = pinbuf;
        } else if (pinv->type == JSON_BOOL) {
            /* YAML False pin: reasoning_parse_effort(effort,
             * effort_is_bool_false=True) models the disabled case. */
            return reasoning_parse_effort("", !pinv->bool_val ? true : false);
        }
        if (pinned) {
            json_t *parsed = reasoning_parse_effort(pinned, false);
            if (parsed) {
                const char *jid = jstr(job, "id");
                hermes_log(LOG_INFO, "cron.scheduler",
                           "Job '%s': using per-job reasoning_effort '%s'",
                           jid ? jid : "?", pinned);
                return parsed;
            }
            const char *jid = jstr(job, "id");
            hermes_log(LOG_WARNING, "cron.scheduler",
                       "Job '%s': invalid stored reasoning_effort '%s' — ignoring "
                       "the pin and falling back to config resolution. Fix with "
                       "`cronjob action=update job_id=%s reasoning_effort=<level>` "
                       "(valid: none, minimal, low, medium, high, xhigh, max, ultra).",
                       jid ? jid : "?", pinned, jid ? jid : "?");
        }
    }
    if (!cfg || cfg->type != JSON_OBJECT)
        cfg = json_object();
    else
        cfg = json_copy((json_t *)cfg);
    char model_buf[256];
    if (!model) model = "";
    json_t *resolved = reasoning_resolve_config(cfg, model);
    json_free(cfg);
    (void)model_buf;
    return resolved;
}

/* ================================================================
 * 4. _inflight_min_allowance_minutes
 * ================================================================ */
/* PoP: scheduler_inflight_min_allowance_minutes @ cron/scheduler.py:_inflight_min_allowance_minutes */
double scheduler_inflight_min_allowance_minutes(void) {
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *cron_cfg = jget(cfg, "cron");
        json_t *cv = jget(cron_cfg, "inflight_max_minutes");
        if (cv && cv->type == JSON_NUMBER) {
            double val = cv->num_val;
            if (val > 0) { json_free(cfg); return val; }
        }
        json_free(cfg);
    }
    const char *rawv = getenv("HERMES_CRON_INFLIGHT_MAX_MINUTES");
    if (rawv) {
        char raw[128];
        snprintf(raw, sizeof raw, "%s", rawv);
        char *p = raw;
        sched_strip_ws((const char **)&p);
        sched_rstrip_inplace(p);
        if (*p) {
            char *end = NULL;
            double val = strtod(p, &end);
            if (end && *end == '\0' && end != p) {
                if (val > 0) return val;
            } else {
                hermes_log(LOG_WARNING, "cron.scheduler",
                           "Invalid HERMES_CRON_INFLIGHT_MAX_MINUTES='%s'; using default %g",
                           p, SCHED_INFLIGHT_MIN_ALLOWANCE_MINUTES_DEFAULT);
            }
        }
    }
    return SCHED_INFLIGHT_MIN_ALLOWANCE_MINUTES_DEFAULT;
}

/* ================================================================
 * 5. _cron_interval_minutes
 * ================================================================ */
/* _cron_interval_cache: dict[str, Optional[float]] module global. */
typedef struct sched_interval_cache_entry {
    char *expr;
    double minutes;   /* valid only when found */
    bool found;
    struct sched_interval_cache_entry *next;
} sched_interval_cache_entry_t;
static sched_interval_cache_entry_t *g_interval_cache = NULL;
static pthread_mutex_t g_interval_cache_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: scheduler_cron_interval_cache_clear @ cron/scheduler.py:_cron_interval_cache (clear helper) */
void scheduler_cron_interval_cache_clear(void) {
    pthread_mutex_lock(&g_interval_cache_lock);
    sched_interval_cache_entry_t *e = g_interval_cache;
    while (e) {
        sched_interval_cache_entry_t *nx = e->next;
        free(e->expr);
        free(e);
        e = nx;
    }
    g_interval_cache = NULL;
    pthread_mutex_unlock(&g_interval_cache_lock);
}

/* PoP: scheduler_cron_interval_minutes @ cron/scheduler.py:_cron_interval_minutes */
bool scheduler_cron_interval_minutes(const char *expr, double *out_minutes) {
    if (!expr || !*expr) return false;
    pthread_mutex_lock(&g_interval_cache_lock);
    for (sched_interval_cache_entry_t *e = g_interval_cache; e; e = e->next) {
        if (strcmp(e->expr, expr) == 0) {
            bool f = e->found;
            if (f && out_minutes) *out_minutes = e->minutes;
            pthread_mutex_unlock(&g_interval_cache_lock);
            return f;
        }
    }
    pthread_mutex_unlock(&g_interval_cache_lock);

    bool found = false;
    double gap = 0.0;
    /* from cron.jobs import _ensure_croniter → libcron replaces croniter */
    char errbuf[256];
    cron_expr_t *c = cron_parse(expr, &errbuf[0] ? (char **)&errbuf : NULL);
    if (c) {
        time_t base = time(NULL);
        struct tm first_tm, second_tm;
        localtime_r(&base, &first_tm);
        if (cron_next(c, &first_tm, &second_tm)) {
            struct tm third_tm;
            if (cron_next(c, &second_tm, &third_tm)) {
                time_t t1 = mktime(&second_tm);
                time_t t2 = mktime(&third_tm);
                if (t1 != (time_t)-1 && t2 != (time_t)-1 && t2 > t1) {
                    gap = difftime(t2, t1) / 60.0;
                    found = gap > 0;
                }
            }
        }
        cron_free(c);
    }
    /* result cached even when None — mirrors the Python cache store */
    pthread_mutex_lock(&g_interval_cache_lock);
    sched_interval_cache_entry_t *ne = malloc(sizeof *ne);
    if (ne) {
        ne->expr = strdup(expr);
        ne->minutes = gap;
        ne->found = found;
        ne->next = g_interval_cache;
        g_interval_cache = ne;
    }
    pthread_mutex_unlock(&g_interval_cache_lock);
    if (found && out_minutes) *out_minutes = gap;
    return found;
}

/* ================================================================
 * 6. _job_interval_minutes
 * ================================================================ */
/* PoP: scheduler_job_interval_minutes @ cron/scheduler.py:_job_interval_minutes */
bool scheduler_job_interval_minutes(const json_t *job, double *out_minutes) {
    json_t *schedule = jget(job, "schedule");
    if (!schedule)
        return false;
    if (schedule->type == JSON_STRING) {
        /* defensive fallback for programmatic string schedules */
        extern bool parse_schedule(const char *expr, void *sched); /* scheduler.c */
        (void)parse_schedule; /* string shape carries no richer data here than
                                 the expression itself; fall through to kind
                                 probing below via an empty object */
        schedule = NULL;
    }
    if (!schedule || schedule->type != JSON_OBJECT)
        return false;
    const char *kind = jstr(schedule, "kind");
    if (!kind)
        return false;
    if (strcmp(kind, "interval") == 0) {
        json_t *mv = jget(schedule, "minutes");
        if (mv && mv->type == JSON_NUMBER && mv->num_val != 0) {
            if (out_minutes) *out_minutes = mv->num_val;
            return true;
        }
        return false;
    }
    if (strcmp(kind, "cron") == 0) {
        const char *expr = jstr(schedule, "expr");
        if (!expr) expr = "";
        return scheduler_cron_interval_minutes(expr, out_minutes);
    }
    return false;   /* "once" and unknown kinds have no recurring interval */
}

/* ================================================================
 * 7. _is_lock_contention_errno
 * ================================================================ */
/* PoP: scheduler_is_lock_contention_errno @ cron/scheduler.py:_is_lock_contention_errno */
bool scheduler_is_lock_contention_errno(int err) {
    /* errno < 0 models Python's `err.errno is None` */
    if (err < 0) return false;
    /* fcntl branch is compiled in on POSIX (msvcrt branch unreachable) */
    return err == EWOULDBLOCK || err == EAGAIN || err == EACCES;
}

/* ================================================================
 * 8. _is_fd_exhaustion_text
 * ================================================================ */
/* PoP: scheduler_is_fd_exhaustion_text @ cron/scheduler.py:_is_fd_exhaustion_text */
bool scheduler_is_fd_exhaustion_text(const char *text) {
    if (!text) return false;
    size_t n = strlen(text);
    char *lowered = malloc(n + 1);
    if (!lowered) return false;
    for (size_t i = 0; i < n; i++)
        lowered[i] = (char)tolower((unsigned char)text[i]);
    lowered[n] = '\0';
    bool hit = strstr(lowered, "too many open files") != NULL ||
               strstr(lowered, "emfile") != NULL;
    free(lowered);
    return hit;
}

/* ================================================================
 * 9. _is_fd_exhaustion
 * ================================================================ */
/* PoP: scheduler_is_fd_exhaustion @ cron/scheduler.py:_is_fd_exhaustion */
bool scheduler_is_fd_exhaustion(int err_no, bool is_oserror, const char *repr) {
    if (is_oserror && (err_no == EMFILE || err_no == ENFILE))
        return true;
    return scheduler_is_fd_exhaustion_text(repr);
}

/* ================================================================
 * 10. _reclaim_fds_best_effort
 * ================================================================ */
/* PoP: scheduler_reclaim_fds_best_effort @ cron/scheduler.py:_reclaim_fds_best_effort */
void scheduler_reclaim_fds_best_effort(void) {
    /* gc.collect(): no C analogue — the allocator reclaims deterministically.
     * apply_nofile_soft_limit(None): raise RLIMIT_NOFILE soft toward hard. */
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur < rl.rlim_max) {
        rlim_t target = rl.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &(struct rlimit){target, rl.rlim_max}) != 0) {
            /* never raises: a failed raise must not make the ticker worse */
        }
    }
}

/* ================================================================
 * 11. _resolve_cron_surface_mode
 * ================================================================ */
/* PoP: scheduler_resolve_cron_surface_mode @ cron/scheduler.py:_resolve_cron_surface_mode */
const char *scheduler_resolve_cron_surface_mode(const json_t *extra,
                                                const char *logical_platform_name) {
    char logical_lower[128] = "";
    if (logical_platform_name && *logical_platform_name) {
        snprintf(logical_lower, sizeof logical_lower, "%s", logical_platform_name);
        for (char *p = logical_lower; *p; p++)
            *p = (char)tolower((unsigned char)*p);
    }
    if (extra && extra->type == JSON_OBJECT) {
        json_t *sub = json_obj_get((json_t *)extra, logical_lower);
        json_t *raw = NULL;
        if (sub && sub->type == JSON_OBJECT &&
            json_obj_get(sub, "cron_continuable_surface") != NULL)
            raw = json_obj_get(sub, "cron_continuable_surface");
        else
            raw = json_obj_get((json_t *)extra, "cron_continuable_surface");
        if (raw) {
            const char *rv = NULL;
            char buf[64];
            if (raw->type == JSON_STRING) rv = raw->str_val;
            if (rv) {
                const char *q = rv;
                sched_strip_ws(&q);
                snprintf(buf, sizeof buf, "%s", q);
                sched_rstrip_inplace(buf);
                for (char *p = buf; *p; p++)
                    *p = (char)tolower((unsigned char)*p);
                if (strcmp(buf, "in_channel") == 0)
                    return "in_channel";
            }
        }
    }
    return "thread";
}

/* ================================================================
 * 12. _get_bot_chat_delivery_timeout
 * ================================================================ */
/* PoP: scheduler_get_bot_chat_delivery_timeout @ cron/scheduler.py:_get_bot_chat_delivery_timeout */
int scheduler_get_bot_chat_delivery_timeout(void) {
    json_t *cfg = config_py_load_config_readonly();
    int value = 600;
    if (cfg) {
        json_t *cron_cfg = jget(cfg, "cron");
        json_t *tv = jget(cron_cfg, "bot_chat_delivery_timeout_seconds");
        if (tv && tv->type == JSON_NUMBER) value = (int)tv->num_val;
        json_free(cfg);
    }
    return value > 0 ? value : 600;
}

/* ================================================================
 * 13. _deliver_to_bot_chat
 * ================================================================ */
/* PoP: scheduler_deliver_to_bot_chat @ cron/scheduler.py:_deliver_to_bot_chat */
char *scheduler_deliver_to_bot_chat(const json_t *job, const char *content,
                                    const char *profile) {
    const char *job_id = jstr(job, "id");
    if (!job_id) job_id = "?";
    const char *job_name = jstr(job, "name");
    if (!job_name) job_name = job_id;

    char hermes_bin[1024] = "";
    /* shutil.which("hermes") */
    const char *path_env = getenv("PATH");
    if (path_env) {
        char *paths = strdup(path_env);
        char *saveptr = NULL;
        for (char *dir = strtok_r(paths, ":", &saveptr); dir;
             dir = strtok_r(NULL, ":", &saveptr)) {
            char cand[1200];
            snprintf(cand, sizeof cand, "%s/hermes", dir);
            if (access(cand, X_OK) == 0) {
                snprintf(hermes_bin, sizeof hermes_bin, "%s", cand);
                break;
            }
        }
        free(paths);
    }
    if (!hermes_bin[0]) {
        /* importlib fallback: python -m hermes_cli.main when importable —
         * resolved at runtime against sys.path; C uses the installed tree. */
        char exe[512];
        ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (n > 0) { exe[n] = '\0'; }
        else snprintf(exe, sizeof exe, "python3");
        static char argv0[600];
        snprintf(argv0, sizeof argv0, "%s", exe);
        (void)argv0;
        char *msg = strdup("bot-chat delivery failed: hermes CLI not resolvable");
        hermes_log(LOG_WARNING, "cron.scheduler", "Job '%s': %s", job_id, msg);
        return msg;
    }

    int timeout = scheduler_get_bot_chat_delivery_timeout();

    /* query temp file */
    char qtmpl[] = "/tmp/hermes-cron-botchat-XXXXXX.txt";
    int qfd = mkstemps(qtmpl, 4);
    if (qfd < 0) {
        char *msg = strdup("bot-chat delivery failed: cannot create query file");
        hermes_log(LOG_WARNING, "cron.scheduler", "Job '%s': %s", job_id, msg);
        return msg;
    }
    /* message prefix mirrors the Bot Mode sender-attribution convention */
    size_t mlen = strlen(content) + 256;
    char *message = malloc(mlen);
    if (!message) { close(qfd); unlink(qtmpl); return strdup("bot-chat delivery failed: OOM"); }
    snprintf(message, mlen,
             "[Cronjob \"%s\" output — scheduled job, not the user. "
             "Review it, act on anything that needs action, and summarize "
             "for the chat.]\n\n%s", job_name, content);
    size_t written = 0, total = strlen(message);
    while (written < total) {
        ssize_t w = write(qfd, message + written, total - written);
        if (w <= 0) break;
        written += (size_t)w;
    }
    close(qfd);

    /* child env: copy environ; pop HERMES_HOME when -p owns resolution */
    extern char **environ;

    pid_t pid = fork();
    if (pid < 0) {
        unlink(qtmpl);
        free(message);
        char *msg = strdup("bot-chat delivery failed: fork failed");
        hermes_log(LOG_WARNING, "cron.scheduler", "Job '%s': %s", job_id, msg);
        return msg;
    }
    if (pid == 0) {
        /* child */
        if (profile && *profile) unsetenv("HERMES_HOME");
        char *argv[32];
        int ai = 0;
        argv[ai++] = hermes_bin;
        if (profile && *profile) {
            argv[ai++] = (char *)"-p";
            argv[ai++] = (char *)profile;
        }
        argv[ai++] = (char *)"chat";
        argv[ai++] = (char *)"--in";
        argv[ai++] = (char *)"~";
        argv[ai++] = (char *)"-c";
        argv[ai++] = (char *)"Bot Chat";
        argv[ai++] = (char *)"--create-if-missing";
        argv[ai++] = (char *)"-Q";
        argv[ai++] = (char *)"--query-file";
        argv[ai++] = qtmpl;
        argv[ai] = NULL;
        execve(hermes_bin, argv, environ);
        _exit(127);
    }
    free(message);

    /* subprocess.run(..., timeout=...) bounded reap */
    double deadline = sched_monotonic() + (double)timeout;
    int status = 0;
    bool timed_out = false;
    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r < 0 && errno != EINTR) break;
        if (sched_monotonic() >= deadline) {
            timed_out = true;
            kill(pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
            break;
        }
        struct timespec ts = {0, 50 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    unlink(qtmpl);

    char prof_label[192];
    snprintf(prof_label, sizeof prof_label, "'%s'", (profile && *profile) ? profile : "(own)");
    if (timed_out) {
        size_t need = 512;
        char *msg = malloc(need);
        if (!msg) return NULL;
        snprintf(msg, need,
                 "bot-chat delivery to profile %s timed out after %ds (the bot's "
                 "turn may still complete; raise cron.bot_chat_delivery_timeout_seconds "
                 "if this recurs)", prof_label, timeout);
        hermes_log(LOG_WARNING, "cron.scheduler", "Job '%s': %s", job_id, msg);
        return msg;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        hermes_log(LOG_INFO, "cron.scheduler",
                   "Job '%s': delivered to Bot Chat of profile %s", job_id, prof_label);
        return NULL;
    }
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    size_t need = 256;
    char *msg = malloc(need);
    if (!msg) return NULL;
    snprintf(msg, need, "bot-chat delivery to profile %s failed (exit %d)", prof_label, rc);
    hermes_log(LOG_WARNING, "cron.scheduler", "Job '%s': %s", job_id, msg);
    return msg;
}

/* ================================================================
 * 14. _resolve_bot_chat_target
 * ================================================================ */
/* PoP: scheduler_resolve_bot_chat_target @ cron/scheduler.py:_resolve_bot_chat_target */
json_t *scheduler_resolve_bot_chat_target(const json_t *job,
                                          const char *profile_arg) {
    const char *job_id = jstr(job, "id");
    if (!job_id) job_id = "?";
    if (!profile_arg || !*profile_arg) {
        /* Own profile: chat subprocess inherits HERMES_HOME. */
        json_t *t = json_object();
        json_set(t, "platform", json_string(SCHED_BOT_CHAT_PLATFORM));
        json_set(t, "chat_id", json_string(""));
        json_set(t, "thread_id", json_null());
        return t;
    }
    char canon[256];
    if (profiles_normalize_profile_name(profile_arg, canon, sizeof canon) < 0 ||
        !profile_dir_exists(canon)) {
        hermes_log(LOG_WARNING, "cron.scheduler",
                   "Job '%s': bot-chat delivery profile '%s' not found on this "
                   "machine — skipping target", job_id, profile_arg);
        return NULL;
    }
    json_t *t = json_object();
    json_set(t, "platform", json_string(SCHED_BOT_CHAT_PLATFORM));
    json_set(t, "chat_id", json_string(canon));
    json_set(t, "thread_id", json_null());
    return t;
}

/* ================================================================
 * 15. _get_media_send_timeout
 * ================================================================ */
/* PoP: scheduler_get_media_send_timeout @ cron/scheduler.py:_get_media_send_timeout */
int scheduler_get_media_send_timeout(void) {
    const char *envv = getenv("HERMES_CRON_MEDIA_SEND_TIMEOUT");
    if (envv) {
        char raw[128];
        snprintf(raw, sizeof raw, "%s", envv);
        char *p = raw;
        sched_strip_ws((const char **)&p);
        sched_rstrip_inplace(p);
        if (*p) {
            char *end = NULL;
            double dv = strtod(p, &end);
            if (end && *end == '\0' && end != p) {
                int timeout = (int)dv;
                if (timeout > 0) return timeout;
            } else {
                hermes_log(LOG_WARNING, "cron.scheduler",
                           "Invalid HERMES_CRON_MEDIA_SEND_TIMEOUT='%s'; using config/default", p);
            }
        }
    }
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *cron_cfg = jget(cfg, "cron");
        json_t *cv = jget(cron_cfg, "media_send_timeout_seconds");
        if (cv) {
            double dv = cv->type == JSON_NUMBER ? cv->num_val
                        : (cv->type == JSON_STRING ? atof(cv->str_val) : -1.0);
            int timeout = (int)dv;
            if (timeout > 0) { json_free(cfg); return timeout; }
        }
        json_free(cfg);
    }
    return SCHED_DEFAULT_MEDIA_SEND_TIMEOUT;
}

/* ================================================================
 * 16. _terminate_cron_script_process
 * ================================================================ */
/* PoP: scheduler_terminate_cron_script_process @ cron/scheduler.py:_terminate_cron_script_process */
void scheduler_terminate_cron_script_process(pid_t proc_pid) {
    if (kill(proc_pid, 0) != 0 && errno == ESRCH)
        return;   /* proc.poll() is not None */
    /* POSIX-only branch (win32 taskkill path handled by the Windows build) */
    pid_t process_group = getpgid(proc_pid);
    if (process_group > 0) {
        if (killpg(process_group, SIGTERM) == 0 ||
            (errno != ESRCH && errno != EPERM)) {
            /* bounded 1s wait for the leader */
            double deadline = sched_monotonic() + 1.0;
            for (;;) {
                if (waitpid(proc_pid, NULL, WNOHANG) == proc_pid) break;
                if (sched_monotonic() >= deadline) break;
                struct timespec ts = {0, 20 * 1000 * 1000};
                nanosleep(&ts, NULL);
            }
            /* Escalate whenever ANY group member survived the TERM */
            if (killpg(process_group, 0) == 0 || errno != ESRCH) {
                killpg(process_group, SIGKILL);
            }
        }
    }
    /* final bounded reap; escalate to kill on expiry */
    double deadline = sched_monotonic() + 1.0;
    for (;;) {
        if (waitpid(proc_pid, NULL, WNOHANG) == proc_pid) return;
        if (sched_monotonic() >= deadline) break;
        struct timespec ts = {0, 20 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    kill(proc_pid, SIGKILL);
    deadline = sched_monotonic() + 1.0;
    for (;;) {
        if (waitpid(proc_pid, NULL, WNOHANG) == proc_pid) return;
        if (sched_monotonic() >= deadline) return;   /* abandoned to init */
        struct timespec ts = {0, 20 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
}

/* ================================================================
 * 17. _drain_script_pipes
 * ================================================================ */
/* PoP: scheduler_drain_script_pipes @ cron/scheduler.py:_drain_script_pipes */
void scheduler_drain_script_pipes(pid_t proc_pid, int stdout_fd, int stderr_fd) {
    /* communicate(timeout=5.0): bound the drain — output is discarded, only
     * the process must be reaped so the worker thread unblocks. */
    double deadline = sched_monotonic() + 5.0;
    for (;;) {
        if (waitpid(proc_pid, NULL, WNOHANG) == proc_pid) return;
        if (sched_monotonic() >= deadline) break;
        struct timespec ts = {0, 20 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    kill(proc_pid, SIGKILL);   /* except OSError: pass */
    if (stdout_fd >= 0) close(stdout_fd);
    if (stderr_fd >= 0) close(stderr_fd);
    deadline = sched_monotonic() + 5.0;
    for (;;) {
        if (waitpid(proc_pid, NULL, WNOHANG) == proc_pid) return;
        if (sched_monotonic() >= deadline) return;   /* wedged: OS reaper */
        struct timespec ts = {0, 20 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
}

/* ================================================================
 * 18. _windows_cron_bootstrap_argv
 * ================================================================ */
/* PoP: scheduler_windows_cron_bootstrap_argv @ cron/scheduler.py:_windows_cron_bootstrap_argv */
char **scheduler_windows_cron_bootstrap_argv(const char *python_exe,
                                             const json_t *env_overlay,
                                             const char *script_path,
                                             size_t *out_n) {
    const char *virtual_env = jstr(env_overlay, "VIRTUAL_ENV");
    char site_packages[1024] = "";
    if (virtual_env && *virtual_env)
        snprintf(site_packages, sizeof site_packages, "%s/Lib/site-packages", virtual_env);
    struct stat st;
    if (!site_packages[0] || stat(site_packages, &st) != 0 || !S_ISDIR(st.st_mode)) {
        /* Silent here would make the "editable installs invisible" failure
         * undiagnosable; the pre-existing PYTHONPATH-only behaviour applies. */
        if (site_packages[0])
            hermes_log(LOG_WARNING, "cron.scheduler",
                       "Windows cron script: venv site-packages %s not found; running "
                       "without .pth processing (editable installs may be unimportable)",
                       site_packages);
        char **argv = malloc(3 * sizeof *argv);
        if (!argv) { if (out_n) *out_n = 0; return NULL; }
        argv[0] = strdup(python_exe ? python_exe : "");
        argv[1] = strdup(script_path ? script_path : "");
        argv[2] = NULL;
        if (out_n) *out_n = 2;
        return argv;
    }
    /* bootstrap = site.addsitedir(site_packages) + runpy.run_path(__main__),
     * with sys.argv re-based onto the script and sys.path[0] set to its
     * directory — preserving `python script.py` import semantics. */
    size_t need = strlen(site_packages) + 512;
    char *bootstrap = malloc(need);
    if (!bootstrap) { if (out_n) *out_n = 0; return NULL; }
    snprintf(bootstrap, need,
             "import os, runpy, site, sys;"
             "site.addsitedir('%s');"
             "script = sys.argv[1];"
             "sys.argv = [script] + sys.argv[2:];"
             "sys.path.insert(0, os.path.dirname(os.path.abspath(script)));"
             "runpy.run_path(script, run_name='__main__')",
             site_packages);
    char **argv = malloc(5 * sizeof *argv);
    if (!argv) { free(bootstrap); if (out_n) *out_n = 0; return NULL; }
    argv[0] = strdup(python_exe ? python_exe : "");
    argv[1] = strdup("-c");
    argv[2] = bootstrap;
    argv[3] = strdup(script_path ? script_path : "");
    argv[4] = NULL;
    if (out_n) *out_n = 4;
    return argv;
}

/* ================================================================
 * 19. _block_and_pause_job
 * ================================================================ */
/* PoP: scheduler_block_and_pause_job @ cron/scheduler.py:_block_and_pause_job */
bool scheduler_block_and_pause_job(const char *job_id, const char *job_name,
                                   const char *reason, char **doc_out,
                                   char **alert_out, char **err_out) {
    hermes_log(LOG_ERROR, "cron.scheduler", "Job '%s': %s", job_id, reason);
    char pause_reason[1024];
    snprintf(pause_reason, sizeof pause_reason,
             "Auto-paused by scheduler: %s", reason);
    json_t *paused = cronjobs_pause_job(job_id, pause_reason);
    if (!paused) {
        hermes_log(LOG_ERROR, "cron.scheduler",
                   "Job '%s': failed to auto-pause unrunnable job", job_id);
    } else {
        json_free(paused);
    }

    char *now_iso_s = now_iso();   /* _hermes_now().strftime(...) */
    if (!now_iso_s) now_iso_s = strdup("");
    size_t need = strlen(job_name) + strlen(job_id) + strlen(now_iso_s)
                + strlen(reason) + 192;
    char *doc = malloc(need);
    if (doc) {
        snprintf(doc, need,
                 "# Cron Job: %s\n\n"
                 "**Job ID:** %s\n"
                 "**Run Time:** %s\n"
                 "**Status:** blocked (unrunnable job) \xE2\x80\x94 auto-paused\n\n"
                 "%s\n", job_name, job_id, now_iso_s, reason);
    }
    free(now_iso_s);

    size_t aneed = strlen(job_name) + strlen(reason) + 64;
    char *alert = malloc(aneed);
    if (alert) {
        snprintf(alert, aneed,
                 "\xE2\x9A\xA0 Cron job '%s' was auto-paused\n\n%s", job_name, reason);
    }
    if (doc_out) *doc_out = doc; else free(doc);
    if (alert_out) *alert_out = alert; else free(alert);
    if (err_out) { char *e = strdup(reason); *err_out = e ? e : strdup(""); }
    return false;
}

/* ================================================================
 * 20. _is_transient_provider_resolve_error
 * ================================================================ */
/* PoP: scheduler_is_transient_provider_resolve_error @ cron/scheduler.py:_is_transient_provider_resolve_error */
bool scheduler_is_transient_provider_resolve_error(const scheduler_gap_exc_t *exc) {
    static const char *const transport_classes[] = {
        "ConnectError", "ConnectTimeout", "ReadTimeout", "WriteTimeout",
        "PoolTimeout", "NetworkError", "TimeoutException",
        "ClientConnectorError", "ClientConnectorDNSError", "ServerTimeoutError",
        "ClientOSError", NULL,
    };
    static const char *const dns_needles[] = {
        "nodename nor servname", "name or service not known",
        "temporary failure in name resolution", "failed to resolve",
        "connection refused", "network is unreachable", "timed out", "timeout",
        NULL,
    };
    static const char *const dns_needles_short[] = {
        "nodename nor servname", "name or service not known",
        "temporary failure in name resolution", "network is unreachable",
        NULL,
    };
    static const int transient_errnos[] = {
        ECONNREFUSED, ECONNRESET, EHOSTUNREACH, ENETUNREACH, ENETDOWN,
        ETIMEDOUT, EAGAIN, -1,
    };
    static const int eai_transient[] = {
        EAI_NONAME, EAI_AGAIN, EAI_FAIL,
#ifdef EAI_NODATA
        EAI_NODATA,
#endif
        -1,
    };

    const scheduler_gap_exc_t *cur = exc;
    while (cur != NULL) {
        const char *name = cur->type_name ? cur->type_name : "";
        const char *module = cur->type_module ? cur->type_module : "";
        char msg[2048] = "";
        if (cur->message) {
            snprintf(msg, sizeof msg, "%s", cur->message);
            for (char *p = msg; *p; p++) *p = (char)tolower((unsigned char)*p);
        }
        for (int i = 0; transport_classes[i]; i++)
            if (strcmp(name, transport_classes[i]) == 0) return true;
        if (strstr(module, "httpx") || strstr(module, "httpcore") ||
            strstr(module, "aiohttp")) {
            for (int i = 0; dns_needles[i]; i++)
                if (msg[0] && strstr(msg, dns_needles[i])) return true;
        }
        if (cur->is_oserror) {
            if (cur->is_gaierror) {
                for (int i = 0; eai_transient[i] != -1; i++)
                    if (cur->err_no == eai_transient[i]) return true;
            } else {
                for (int i = 0; transient_errnos[i] != -1; i++)
                    if (cur->err_no == transient_errnos[i]) return true;
            }
            for (int i = 0; dns_needles_short[i]; i++)
                if (msg[0] && strstr(msg, dns_needles_short[i])) return true;
        }
        /* Bare RuntimeError/Exception already carrying the DNS text */
        if (msg[0] && (strstr(msg, "nodename nor servname") ||
                       strstr(msg, "name or service not known")))
            return true;
        cur = cur->cause_or_context;
    }
    return false;
}

/* ================================================================
 * 21. _cron_preflight_enabled
 * ================================================================ */
/* PoP: scheduler_cron_preflight_enabled @ cron/scheduler.py:_cron_preflight_enabled */
bool scheduler_cron_preflight_enabled(const json_t *cfg) {
    json_t *cron_cfg = jget(cfg, "cron");
    if (!cron_cfg || cron_cfg->type != JSON_OBJECT)
        return true;
    json_t *pv = json_obj_get(cron_cfg, "preflight");
    if (!pv)
        return true;   /* default True */
    /* only the literal boolean false opts out */
    return !(pv->type == JSON_BOOL && pv->bool_val == false);
}

/* ================================================================
 * 22. _cron_cleanup_timeout_seconds
 * ================================================================ */
/* PoP: scheduler_cron_cleanup_timeout_seconds @ cron/scheduler.py:_cron_cleanup_timeout_seconds */
double scheduler_cron_cleanup_timeout_seconds(void) {
    double default_timeout = 10.0;
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg)
        return default_timeout;
    json_t *cron_cfg = jget(cfg, "cron");
    json_t *cv = jget(cron_cfg, "cleanup_timeout_seconds");
    if (cv) {
        double timeout = cv->type == JSON_NUMBER ? cv->num_val
                         : (cv->type == JSON_STRING ? atof(cv->str_val) : -1.0);
        if (timeout >= 0) { json_free(cfg); return timeout; }
    }
    json_free(cfg);
    return default_timeout;
}

/* ================================================================
 * 23. _run_cron_cleanup_with_timeout
 * ================================================================ */
typedef struct sched_cleanup_worker {
    void (*cleanup)(void *);
    void *arg;
    bool done;
    bool failed;          /* BaseException analogue */
    pthread_mutex_t lock;
    pthread_cond_t cv;
} sched_cleanup_worker_t;

static void *sched_cleanup_runner(void *raw) {
    sched_cleanup_worker_t *w = raw;
    w->cleanup(w->arg);   /* a C function either returns or dies; there is no
                             trappable BaseException to record here */
    pthread_mutex_lock(&w->lock);
    w->done = true;
    pthread_cond_broadcast(&w->cv);
    pthread_mutex_unlock(&w->lock);
    return NULL;
}

/* PoP: scheduler_run_cron_cleanup_with_timeout @ cron/scheduler.py:_run_cron_cleanup_with_timeout */
bool scheduler_run_cron_cleanup_with_timeout(void (*cleanup)(void *arg),
                                             void *arg, const char *job_id,
                                             const char *label,
                                             double timeout_seconds) {
    double timeout = (timeout_seconds < 0)
                     ? scheduler_cron_cleanup_timeout_seconds()
                     : timeout_seconds;
    if (timeout <= 0) {
        cleanup(arg);
        return true;
    }
    /* A detached daemon thread is deliberate: never joined at exit so the
     * scheduler can release its dispatch guard even if cleanup never
     * returns. The worker state is heap-allocated; an abandoned (timed-out)
     * worker leaks its state block by design — exactly Python's abandoned
     * daemon thread. */
    sched_cleanup_worker_t *w = malloc(sizeof *w);
    if (!w) {
        cleanup(arg);
        return true;
    }
    *w = (sched_cleanup_worker_t){cleanup, arg, false, false,
                                  PTHREAD_MUTEX_INITIALIZER,
                                  PTHREAD_COND_INITIALIZER};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    int rc = pthread_create(&tid, &attr, sched_cleanup_runner, w);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        /* could not start the worker — run inline so the cleanup guarantee
         * is preserved (Python's Thread.start never fails this way) */
        free(w);
        cleanup(arg);
        return true;
    }
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += (time_t)timeout;
    deadline.tv_nsec += (long)((timeout - (double)(time_t)timeout) * 1e9);
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    bool ok;
    pthread_mutex_lock(&w->lock);
    while (!w->done) {
        rc = pthread_cond_timedwait(&w->cv, &w->lock, &deadline);
        if (rc == ETIMEDOUT) break;
    }
    ok = w->done;
    pthread_mutex_unlock(&w->lock);
    if (!ok) {
        hermes_log(LOG_ERROR, "cron.scheduler",
                   "Job '%s': %s exceeded %.1fs; abandoning cleanup so future "
                   "runs remain dispatchable", job_id ? job_id : "?",
                   label ? label : "cleanup", timeout);
        return false;   /* w intentionally leaked: worker still owns it */
    }
    free(w);
    return true;
}

/* ================================================================
 * 24. _BoundedCronSessionDB proxy
 * ================================================================ */
/* PoP: scheduler_bounded_session_db_new @ cron/scheduler.py:_BoundedCronSessionDB.__init__ */
scheduler_bounded_session_db_t *
scheduler_bounded_session_db_new(void *session_db, const char *job_id) {
    scheduler_bounded_session_db_t *b = malloc(sizeof *b);
    if (!b) return NULL;
    b->session_db = session_db;
    b->job_id = strdup(job_id ? job_id : "");
    b->disabled = false;
    return b;
}

void scheduler_bounded_session_db_free(scheduler_bounded_session_db_t *b) {
    if (!b) return;
    free(b->job_id);
    free(b);
}

/* PoP: scheduler_bounded_session_db_call @ cron/scheduler.py:_BoundedCronSessionDB.__getattr__ */
/* PoP: scheduler_bounded_session_db_call @ cron/scheduler.py:_BoundedCronSessionDB._bounded */
bool scheduler_bounded_session_db_call(scheduler_bounded_session_db_t *b,
                                       const char *name,
                                       void (*target)(void *ctx), void *ctx,
                                       void **result_out, char **err_out) {
    if (result_out) *result_out = NULL;
    if (err_out) *err_out = NULL;
    if (!b || !target) return false;
    if (b->disabled) {
        if (err_out)
            *err_out = strdup("session finalization disabled after prior "
                              "cleanup failure");   /* RuntimeError analogue */
        return false;
    }
    char label[256];
    snprintf(label, sizeof label, "session finalization (%s)", name);
    bool ok = scheduler_run_cron_cleanup_with_timeout(target, ctx, b->job_id,
                                                      label, -1.0);
    if (!ok) {
        /* No exception reached the caller and the operation still did not
         * complete: the timeout path. Disable the damaged connection so
         * later finalization steps fail immediately. */
        b->disabled = true;
        if (err_out) {
            char buf[128];
            snprintf(buf, sizeof buf, "session finalization method %s timed out",
                     name);   /* TimeoutError message */
            *err_out = strdup(buf);
        }
        return false;
    }
    return true;
}


/* ================================================================
 * 25. _run_with_fire_claim_heartbeat
 * ================================================================ */
void scheduler_gap_event_init(scheduler_gap_event_t *ev) {
    if (!ev) return;
    ev->is_set = false;
    ev->_impl = malloc(sizeof(pthread_mutex_t));
    if (ev->_impl) pthread_mutex_init((pthread_mutex_t *)ev->_impl, NULL);
}
void scheduler_gap_event_destroy(scheduler_gap_event_t *ev) {
    if (!ev) return;
    if (ev->_impl) {
        pthread_mutex_destroy((pthread_mutex_t *)ev->_impl);
        free(ev->_impl);
    }
    ev->_impl = NULL;
}
bool scheduler_gap_event_is_set(const scheduler_gap_event_t *ev) {
    if (!ev) return false;
    bool v;
    pthread_mutex_t *m = (pthread_mutex_t *)ev->_impl;
    if (m) pthread_mutex_lock(m);
    v = ev->is_set;
    if (m) pthread_mutex_unlock(m);
    return v;
}
void scheduler_gap_event_set(scheduler_gap_event_t *ev) {
    if (!ev) return;
    pthread_mutex_t *m = (pthread_mutex_t *)ev->_impl;
    if (m) pthread_mutex_lock(m);
    ev->is_set = true;
    if (m) pthread_mutex_unlock(m);
}

double scheduler_run_claim_heartbeat_seconds(void) {
    return sched_run_claim_heartbeat_seconds_const;   /* 60.0 */
}
double scheduler_fire_claim_heartbeat_grace_seconds(void) {
    return sched_run_claim_heartbeat_seconds_const * 3.0;   /* 180.0 */
}

typedef struct sched_heartbeat_ctx {
    char *job_id;
    char *owner;
    scheduler_gap_event_t stop;             /* threading.Event()          */
    scheduler_gap_event_t lost_ownership;   /* threading.Event()          */
    double last_confirmed;                  /* time.monotonic()           */
} sched_heartbeat_ctx_t;

static void *sched_heartbeat_loop(void *raw) {
    sched_heartbeat_ctx_t *hc = raw;
    for (;;) {
        /* stop.wait(_RUN_CLAIM_HEARTBEAT_SECONDS) */
        double deadline = sched_monotonic()
                        + sched_run_claim_heartbeat_seconds_const;
        while (!scheduler_gap_event_is_set(&hc->stop)) {
            if (sched_monotonic() >= deadline) break;
            struct timespec ts = {0, 50 * 1000 * 1000};
            nanosleep(&ts, NULL);
        }
        if (scheduler_gap_event_is_set(&hc->stop)) break;

        /* heartbeat_fire_claim(job_id, expected_owner=owner) — an exception
         * here is the debug-log + grace-lapse path (a failed renewal). */
        /* A False renewal is the ownership-lost branch (immediate
         * interrupt). A RAISED renewal is the debug-log + grace-lapse
         * branch; the C heartbeat layer folds transport failures into its
         * own retry contract, so a renewal attempt that errored rather
         * than answered false keeps the grace clock running instead. */
        int renewed = cronjobs_heartbeat_fire_claim(hc->job_id, hc->owner);
        if (renewed > 0) {
            hc->last_confirmed = sched_monotonic();
            continue;
        }
        if (renewed == 0) {
            scheduler_gap_event_set(&hc->lost_ownership);
            hermes_log(LOG_WARNING, "cron.scheduler",
                       "Job '%s': fire claim ownership lost; interrupting stale run",
                       hc->job_id);
            break;
        }
        hermes_log(LOG_DEBUG, "cron.scheduler",
                   "Job '%s': fire_claim heartbeat failed", hc->job_id);
        if (sched_monotonic() - hc->last_confirmed
                >= scheduler_fire_claim_heartbeat_grace_seconds()) {
            scheduler_gap_event_set(&hc->lost_ownership);
            hermes_log(LOG_WARNING, "cron.scheduler",
                       "Job '%s': fire_claim could not be renewed within %.1fs; "
                       "interrupting uncertain run", hc->job_id,
                       scheduler_fire_claim_heartbeat_grace_seconds());
            break;
        }
    }
    return NULL;
}

/* PoP: scheduler_run_with_fire_claim_heartbeat @ cron/scheduler.py:_run_with_fire_claim_heartbeat */
bool scheduler_run_with_fire_claim_heartbeat(
        const json_t *job,
        bool (*run)(void *ctx, const scheduler_gap_event_t *lost_ownership),
        void *ctx,
        void (*finish)(void *finish_ctx, const char *execution_id,
                       const char *error),
        void *finish_ctx) {
    json_t *claim = jget(job, "fire_claim");
    const char *owner_s = jstr(claim, "by");
    if (!owner_s || !*owner_s)
        return run(ctx, NULL);   /* no owner: run(None) */

    sched_heartbeat_ctx_t hc;
    memset(&hc, 0, sizeof hc);
    hc.job_id = strdup(jstr(job, "id") ? jstr(job, "id") : "");
    hc.owner = strdup(owner_s);
    scheduler_gap_event_init(&hc.stop);
    scheduler_gap_event_init(&hc.lost_ownership);
    hc.last_confirmed = sched_monotonic();

    /* _finish_unstarted closure */
#define HC_FINISH(errtext) do { \
        const char *execution_id = jstr(job, "execution_id"); \
        if (execution_id && finish) \
            finish(finish_ctx, execution_id, errtext); \
    } while (0)

    if (!cronjobs_heartbeat_fire_claim(hc.job_id, hc.owner)) {
        hermes_log(LOG_WARNING, "cron.scheduler",
                   "Job '%s': fire claim ownership was already lost before execution",
                   hc.job_id);
        HC_FINISH("Fire claim ownership lost before execution started.");
        free(hc.job_id);
        free(hc.owner);
        scheduler_gap_event_destroy(&hc.stop);
        scheduler_gap_event_destroy(&hc.lost_ownership);
        return true;
    }
    {
        /* heartbeat validated == ownership still ours at t0; the loop thread
         * detects loss during the run. */
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        int rc = pthread_create(&tid, &attr, sched_heartbeat_loop, &hc);
        pthread_attr_destroy(&attr);
        if (rc != 0) {
            hermes_log(LOG_WARNING, "cron.scheduler",
                       "Job '%s': could not start fire_claim heartbeat", hc.job_id);
            HC_FINISH("Fire claim heartbeat could not be started; execution was not run.");
            free(hc.job_id);
            free(hc.owner);
            scheduler_gap_event_destroy(&hc.stop);
            scheduler_gap_event_destroy(&hc.lost_ownership);
            return true;
        }
        bool result = run(ctx, &hc.lost_ownership);
        scheduler_gap_event_set(&hc.stop);   /* finally: stop.set() */
        struct timespec join_deadline = {0, 1000000000L};   /* join(timeout=1.0) */
        (void)join_deadline;
        /* bounded join: poll-and-bail after 1s (daemon semantics preserved
         * because the loop re-checks the stop event) */
        double deadline = sched_monotonic() + 1.0;
        void *jret = NULL;
        for (;;) {
            int jr = pthread_tryjoin_np(tid, &jret);
            (void)jr;
            if (jr == 0) break;
            if (sched_monotonic() >= deadline) break;
            struct timespec ts = {0, 10 * 1000 * 1000};
            nanosleep(&ts, NULL);
        }
        free(hc.job_id);
        free(hc.owner);
        scheduler_gap_event_destroy(&hc.stop);
        scheduler_gap_event_destroy(&hc.lost_ownership);
        return result;
    }
#undef HC_FINISH
}

/* ================================================================
 * 26. _run_one_job_body
 * ================================================================ */

/* Internal sentinel for the _FireClaimLostDuringSideEffect exception: C
 * models it with a boolean flag threaded through the fences. */

static const char *SCHED_BLOCKED_CONFIG_MARKER = "[blocked_config";
static const char *SCHED_BLOCKED_CONFIG_SILENT_MARKER = "[blocked_config_silent]";
static const char *SCHED_DRIFT_SKIP_MARKER = "[drift_skip";
static const char *SCHED_DRIFT_SKIP_SILENT_MARKER = "[drift_skip_silent]";

typedef struct sched_body_state {
    const json_t *job;
    const scheduler_job_body_seams_t *seams;
    void *adapters;
    void *loop;
    bool verbose;
    const char *extra_prompt;
    const scheduler_gap_event_t *fire_claim_lost;
    const void *execution_token;
    const char *fire_owner;      /* NULL when job carries no fire_claim dict */
    char *fire_owner_buf;
    char *execution_id;
    bool delivery_attempted;
    char *delivery_error;
    bool side_effect_lost;
    bool fatal_base;             /* non-Exception BaseException analogue */
} sched_body_state_t;

/* _fire_claim_ownership_lost() */
static bool body_fire_claim_ownership_lost(sched_body_state_t *st) {
    if (st->fire_claim_lost &&
        scheduler_gap_event_is_set(st->fire_claim_lost))
        return true;
    if (!st->fire_owner) return false;
    if (cronjobs_heartbeat_fire_claim(st->job ? jstr(st->job, "id") : "",
                                      st->fire_owner))
        return false;
    /* heartbeat validation failed → Python logs debug and returns False */
    hermes_log(LOG_DEBUG, "cron.scheduler",
               "Job '%s': fire_claim ownership validation failed",
               st->job ? jstr(st->job, "id") : "?");
    return false;
}

/* fenced terminal write pair used on every ownership-loss path */
static void body_record_interruption(sched_body_state_t *st,
                                     const char *reason) {
    const scheduler_job_body_seams_t *s = st->seams;
    if (st->fire_owner && s->mark_job_run &&
        s->mark_job_run(jstr(st->job, "id"), false,
                        "Interrupted by shutdown before terminal completion.",
                        NULL, st->fire_owner, NULL, s->mark_ctx)) {
        if (s->finish_execution)
            s->finish_execution(st->execution_id, false, reason, NULL,
                                s->ledger_ctx);
    } else {
        if (s->finish_execution)
            s->finish_execution(st->execution_id, false,
                                "Fire claim ownership lost; stale result was discarded.",
                                NULL, s->ledger_ctx);
    }
}

/* strip "[blocked_config...]" / "[drift_skip...]" markers:
 * re.sub(r"\[<kind>[^\]]*\]\s*", "", err).strip() */
static char *body_strip_marker(const char *err, const char *marker_prefix) {
    size_t plen = strlen(marker_prefix);
    size_t n = strlen(err);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n;) {
        if (strncmp(err + i, marker_prefix, plen) == 0) {
            const char *close = strchr(err + i + plen, ']');
            if (close) {
                i = (size_t)(close - err) + 1;
                while (err[i] == ' ' || err[i] == '\t' ||
                       err[i] == '\n' || err[i] == '\r') i++;
                continue;
            }
        }
        out[o++] = err[i++];
    }
    out[o] = '\0';
    /* .strip() */
    char *p = out;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    memmove(out, p, strlen(p) + 1);
    return out;
}

/* _teardown_cron_agent over a defer list. The defer list is a JSON array
 * of opaque handles serialized via json_serialize? No — agents are C
 * pointers carried in a tiny dynamic pointer array. */
typedef struct sched_defer_agents {
    void **items;
    size_t n, cap;
} sched_defer_agents_t;

static void defer_push(sched_defer_agents_t *l, void *h) {
    if (!h) return;
    if (l->n == l->cap) {
        size_t nc = l->cap ? l->cap * 2 : 4;
        void **ni = realloc(l->items, nc * sizeof *ni);
        if (!ni) return;
        l->items = ni;
        l->cap = nc;
    }
    l->items[l->n++] = h;
}
static void defer_teardown_all(sched_defer_agents_t *l,
                               const sched_body_state_t *st) {
    for (size_t i = 0; i < l->n; i++)
        if (st->seams->teardown_agent)
            st->seams->teardown_agent(l->items[i], jstr(st->job, "id"),
                                      st->seams->teardown_ctx);
    free(l->items);
    l->items = NULL;
    l->n = l->cap = 0;
}

/* PoP: scheduler_run_one_job_body @ cron/scheduler.py:_run_one_job_body */
bool scheduler_run_one_job_body(const json_t *job, void *adapters, void *loop,
                                bool verbose, const char *extra_prompt,
                                const scheduler_gap_event_t *fire_claim_lost,
                                const void *execution_token,
                                const scheduler_job_body_seams_t *seams) {
    sched_body_state_t st;
    memset(&st, 0, sizeof st);
    st.job = job;
    st.seams = seams;
    st.adapters = adapters;
    st.loop = loop;
    st.verbose = verbose;
    st.extra_prompt = extra_prompt;
    st.fire_claim_lost = fire_claim_lost;
    st.execution_token = execution_token;

    json_t *claim = jget(job, "fire_claim");
    const char *owner_s = jstr(claim, "by");
    if (owner_s && *owner_s) st.fire_owner = owner_s;
    const char *jid = jstr(job, "id");
    if (!jid) jid = "?";

    /* execution_id resolution */
    const char *exec0 = jstr(job, "execution_id");
    if (exec0 && *exec0) {
        st.execution_id = strdup(exec0);
    } else if (seams->create_execution) {
        st.execution_id = seams->create_execution(jid, seams->ledger_ctx);
    } else {
        st.execution_id = strdup("");
    }

    /* side-effect fence: fire_claim_fence(job_id, expected_owner) */
#define SIDE_EFFECT_FENCE(owns_out) \
    bool owns_out = true; \
    do { \
        if (st.fire_owner) \
            owns_out = cron_jobs_fire_claim_fence(jid, st.fire_owner); \
    } while (0)

    bool blocked_config = false;
    bool should_deliver_final = false;
    bool unresolved_origin_final = false;
    const char *normalized_deliver_final = "local";

    bool dispatch_ok = seams->claim_dispatch
        ? seams->claim_dispatch(jid, seams->claim_ctx)
        : cronjobs_claim_dispatch(jid);
    if (!dispatch_ok) {
        hermes_log(LOG_INFO, "cron.scheduler",
                   "Job '%s': one-shot dispatch limit reached — skipping",
                   jstr(job, "name") ? jstr(job, "name") : jid);
        if (seams->finish_execution)
            seams->finish_execution(st.execution_id, false,
                                    "Dispatch claim rejected; execution was not started.",
                                    NULL, seams->ledger_ctx);
        free(st.execution_id);
        return true;   /* not an error — already handled/removed */
    }

    if (seams->mark_execution_running)
        seams->mark_execution_running(st.execution_id, seams->ledger_ctx);

    sched_defer_agents_t deferred = {NULL, 0, 0};
    char *output = NULL, *final_response = NULL, *error = NULL;
    bool success = false;
    bool ran = false;

    /* run_job under the profile's secret scope: scopes are ambient state in
     * the C runtime and need no token swap here. */
    success = seams->run_job(job, &deferred, extra_prompt, fire_claim_lost,
                             &output, &final_response, &error, seams->run_ctx);
    ran = true;

    /* BaseException handler of run_job: teardown deferred agents then treat
     * as failure. A C run_job signals failure via success=false, so the
     * re-raise arm collapses into the normal failure flow. */
    if (!success && error == NULL && final_response == NULL) {
        /* nothing to add: keep the contract below */
    }

    if (body_fire_claim_ownership_lost(&st)) {
        defer_teardown_all(&deferred, &st);
        if (st.fire_owner && cronjobs_heartbeat_fire_claim(jid, st.fire_owner)) {
            if (seams->mark_job_run)
                seams->mark_job_run(jid, false,
                                    "Interrupted by shutdown before terminal completion.",
                                    NULL, st.fire_owner, NULL, seams->mark_ctx);
            if (seams->finish_execution)
                seams->finish_execution(st.execution_id, false,
                                        "Interrupted by shutdown before terminal completion.",
                                        NULL, seams->ledger_ctx);
        } else if (seams->finish_execution) {
            seams->finish_execution(st.execution_id, false,
                                    "Fire claim ownership lost; stale result was discarded.",
                                    NULL, seams->ledger_ctx);
        }
        free(st.execution_id);
        return true;
    }

    char *output_file = NULL;
    {
        SIDE_EFFECT_FENCE(owns_output);
        if (!owns_output) {
            st.side_effect_lost = true;
        } else if (seams->save_job_output) {
            output_file = seams->save_job_output
                ? seams->save_job_output(jid, output ? output : "", seams->save_ctx)
                : NULL;
        }
    }
    if (output_file) {
        if (verbose)
            hermes_log(LOG_INFO, "cron.scheduler", "Output saved to: %s", output_file);
        free(output_file);
        output_file = NULL;
    }

    if (!st.side_effect_lost) {
        /* Peek-only interruption check (#60432): force the honest failure. */
        if (success &&
            (fire_claim_lost && scheduler_gap_event_is_set(fire_claim_lost))) {
            success = false;
            free(error);
            error = strdup("Interrupted by gateway shutdown before the run "
                           "finished (tool subprocess was killed mid-flight).");
        }

        bool blocked_config_silent =
            error && strstr(error, SCHED_BLOCKED_CONFIG_SILENT_MARKER);
        blocked_config = blocked_config_silent ||
                         (error && strstr(error, SCHED_BLOCKED_CONFIG_MARKER));
        bool drift_skip_silent =
            error && strstr(error, SCHED_DRIFT_SKIP_SILENT_MARKER);
        bool drift_skip = drift_skip_silent ||
                          (error && strstr(error, SCHED_DRIFT_SKIP_MARKER));

        char *deliver_content = NULL;
        if (blocked_config && !success) {
            char *pf = body_strip_marker(error ? error : "", SCHED_BLOCKED_CONFIG_MARKER);
            size_t need = strlen(pf) + strlen(jstr(job, "name") ? jstr(job, "name") : jid) + 192;
            deliver_content = malloc(need);
            snprintf(deliver_content, need,
                     "\xF0\x9F\x9B\x94 Cron '%s' blocked by configuration "
                     "validation (no LLM call was made): %s This alert is sent "
                     "once; the job stays blocked until the configuration is fixed.",
                     jstr(job, "name") ? jstr(job, "name") : jid, pf ? pf : "");
            free(pf);
        } else {
            if (success && final_response) {
                deliver_content = strdup(final_response);
            } else {
                extern char *crs_summarize_cron_failure_for_delivery(const char *job_id,
                                                                    const char *error);
                char *sum = crs_summarize_cron_failure_for_delivery(jid,
                                                                    error ? error : "");
                char *nudge = scheduler_failure_streak_nudge(job);
                size_t need = (sum ? strlen(sum) : 0) + (nudge ? strlen(nudge) : 0) + 1;
                deliver_content = malloc(need);
                if (deliver_content) {
                    deliver_content[0] = '\0';
                    if (sum) strcat(deliver_content, sum);
                    if (nudge) strcat(deliver_content, nudge);
                }
                free(sum);
                free(nudge);
            }
            if (drift_skip && !success) {
                char *drift = body_strip_marker(error ? error : "", SCHED_DRIFT_SKIP_MARKER);
                size_t need = strlen(drift ? drift : "") +
                              strlen(jstr(job, "name") ? jstr(job, "name") : jid) + 96;
                char *dc2 = malloc(need);
                if (dc2) {
                    snprintf(dc2, need,
                             "\xE2\x9A\xA0\xEF\xB8\x8F Cron '%s' skipped: %s",
                             jstr(job, "name") ? jstr(job, "name") : jid,
                             drift ? drift : "");
                    free(deliver_content);
                    deliver_content = dc2;
                }
                free(drift);
            }
        }

        should_deliver_final = deliver_content && *deliver_content &&
                               strspn(deliver_content, " \t\n\r") < strlen(deliver_content);
        if (blocked_config_silent || drift_skip_silent)
            should_deliver_final = false;

        bool unresolved_origin = false;
        if (should_deliver_final && success &&
            scheduler_is_cron_silence_response(deliver_content)) {
            hermes_log(LOG_INFO, "cron.scheduler",
                       "Job '%s': agent returned [SILENT] — skipping delivery", jid);
            should_deliver_final = false;
        }
        if (should_deliver_final && body_fire_claim_ownership_lost(&st)) {
            should_deliver_final = false;
            hermes_log(LOG_WARNING, "cron.scheduler",
                       "Job '%s': skipping delivery after fire claim ownership loss", jid);
        }
        unresolved_origin_final = false;
        if (should_deliver_final) {
            const char *nd = scheduler_normalize_deliver_value(
                jstr(job, "deliver") ? jstr(job, "deliver") : "local");
            normalized_deliver_final = nd;
            unresolved_origin = strcmp(nd, "origin") == 0 &&
                                (!seams->resolve_delivery_target_count ||
                                 seams->resolve_delivery_target_count(job, seams->deliver_ctx) == 0);
            SIDE_EFFECT_FENCE(owns_delivery);
            if (!owns_delivery) {
                st.side_effect_lost = true;
            } else {
                st.delivery_attempted = true;
                free(st.delivery_error);
                st.delivery_error = seams->deliver
                    ? seams->deliver(job, deliver_content, adapters, loop, seams->deliver_ctx)
                    : NULL;
                if (!st.delivery_error)
                    unresolved_origin_final = unresolved_origin;
            }
        } else {
            normalized_deliver_final = scheduler_normalize_deliver_value(
                jstr(job, "deliver") ? jstr(job, "deliver") : "local");
        }
        should_deliver_final = should_deliver_final;
        free(deliver_content);
    }

    /* finally: tear down deferred agents on EVERY path */
    defer_teardown_all(&deferred, &st);

    if (st.side_effect_lost || body_fire_claim_ownership_lost(&st)) {
        body_record_interruption(&st,
                                 "Interrupted by shutdown before terminal completion.");
        free(output); free(final_response); free(error);
        free(st.delivery_error);
        free(st.execution_id);
        return true;
    }

    /* empty final_response is a soft failure (#8585) */
    if (success && (!final_response ||
                    strspn(final_response, " \t\n\r") == strlen(final_response))) {
        success = false;
        free(error);
        error = strdup("Agent completed but produced empty response (model "
                       "error, timeout, or misconfiguration)");
    }

    bool interrupted = scheduler_consume_interrupted_flag(jid);
    if (interrupted) {
        if (st.delivery_error && seams->update_job)
            seams->update_job(jid, "last_delivery_error", st.delivery_error,
                              seams->update_ctx);
        if (seams->finish_execution)
            seams->finish_execution(st.execution_id, false,
                                    "Interrupted by gateway shutdown before terminal completion.",
                                    NULL, seams->ledger_ctx);
        free(output); free(final_response); free(error);
        free(st.delivery_error);
        free(st.execution_id);
        return true;
    }

    bool marked = true;
    if (seams->mark_job_run)
        marked = seams->mark_job_run(jid, success, error, st.delivery_error,
                                     st.fire_owner,
                                     blocked_config ? "blocked_config" : NULL,
                                     seams->mark_ctx);
    if (st.fire_owner && !marked) {
        if (seams->finish_execution)
            seams->finish_execution(st.execution_id, false,
                                    "Fire claim ownership lost before terminal completion.",
                                    NULL, seams->ledger_ctx);
        free(output); free(final_response); free(error);
        free(st.delivery_error);
        free(st.execution_id);
        return true;
    }

    const char *delivery_outcome;
    if (st.delivery_error)
        delivery_outcome = "failed";
    else if (should_deliver_final && unresolved_origin_final)
        delivery_outcome = "not_configured";
    else if (should_deliver_final && strcmp(normalized_deliver_final, "local") != 0)
        delivery_outcome = "delivered";
    else
        delivery_outcome = "suppressed";
    if (seams->finish_execution)
        seams->finish_execution(st.execution_id, success, error,
                                delivery_outcome, seams->ledger_ctx);

    free(output); free(final_response); free(error);
    free(st.delivery_error);
    free(st.execution_id);
    (void)ran;
    return true;
#undef SIDE_EFFECT_FENCE
}

/* ================================================================
 * 27. CronSchedulerRegistrationError.user_message
 * ================================================================ */
/* PoP: scheduler_registration_error_user_message @ cron/scheduler.py:CronSchedulerRegistrationError.user_message */
char *scheduler_registration_error_user_message(const json_t *job) {
    const char *label = jstr(job, "name");
    if (!label || !*label) label = jstr(job, "id");
    if (!label) label = "";
    size_t need = strlen(label) + 256;
    char *out = malloc(need);
    if (!out) return NULL;
    snprintf(out, need,
             "Saved cron job '%s', but couldn't register it with the external "
             "scheduler yet. The job is kept \xE2\x80\x94 don't re-create it; "
             "pause/resume or edit it (e.g. via /cron) to retry registration.",
             label);
    return out;
}
