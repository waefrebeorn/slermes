/*
 * port_cron_jobs.c — C11 port of cron/jobs.py
 *
 * Cron job storage and management. Jobs live in
 * <SLERMES_HOME>/cron/jobs.json as {"jobs": [...], "updated_at": ISO}.
 * This is a faithful, self-contained port: schedule parsing, JSON-backed
 * persistence with atomic writes + cross-process advisory locking, the full
 * job CRUD surface, run bookkeeping (mark/claim/advance), due-job computation,
 * per-run output with retention, and curator skill-ref rewriting.
 *
 * Upstream boundary: the Python module anchors on get_hermes_home() (the
 * active profile home). Slermes is single-home, so we anchor on slermes_home()
 * exactly as the sibling cron ports (port_lifecycle_guard.c) do. The
 * croniter-backed "cron" schedule kind is served by lib/libcron (a real C
 * cron parser), so — unlike the Python fallback where croniter may be absent —
 * cron next-run is always computed here.
 *
 * MIT License — Slermes Fork
 */

#include "cron_jobs.h"
#include "slermes_home.h"
#include "datetime.h"
#include "cron.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <limits.h>

/* cron/lifecycle_guard.py port — reject gateway-lifecycle cron commands.
 * Returns a malloc'd error message when the command is rejected, else NULL. */
extern char *cron_lifecycle_check_gateway_lifecycle(const char *prompt,
                                                    const char *script);

/* Forward declarations (defined later, referenced before their definition). */
static long find_job_index(const json_t *jobs, const char *id);
static char *normalize_optional_text(const json_t *value, bool strip_trailing_slash);

/* ── small utilities ──────────────────────────────────────────────── */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *set_err(char **err, const char *msg) {
    if (err) *err = xstrdup(msg);
    return NULL;
}

/* strip leading+trailing ASCII whitespace, return malloc'd copy */
static char *str_strip_dup(const char *s) {
    if (!s) return xstrdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* in-place ASCII lowercase */
static void str_tolower_inplace(char *s) {
    if (!s) return;
    for (; *s; s++) if (isupper((unsigned char)*s)) *s = (char)tolower((unsigned char)*s);
}

/* Join home + relpath into a malloc'd absolute path. */
static char *home_join(const char *relpath) {
    const char *home = slermes_home();
    if (!home) home = "/tmp/.slermes";
    size_t n = strlen(home) + 1 + strlen(relpath) + 1;
    char *p = malloc(n);
    if (!p) return NULL;
    snprintf(p, n, "%s/%s", home, relpath);
    return p;
}

/* mkdir -p for a single path (parents assumed to exist or created by caller). */
static int mkdir_p(const char *path, mode_t mode) {
    if (!path || !*path) return -1;
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* ── Paths (PoP: module-level path helpers) ───────────────────────── */

/* PoP: cronjobs_cron_dir @ cron/jobs.py:CRON_DIR */
char *cronjobs_cron_dir(void) { return home_join("cron"); }

/* PoP: cronjobs_jobs_file @ cron/jobs.py:JOBS_FILE */
char *cronjobs_jobs_file(void) { return home_join("cron/jobs.json"); }

/* PoP: cronjobs_jobs_lock_file @ cron/jobs.py:_jobs_lock_file */
char *cronjobs_jobs_lock_file(void) { return home_join("cron/.jobs.lock"); }

/* PoP: cronjobs_output_dir @ cron/jobs.py:OUTPUT_DIR */
char *cronjobs_output_dir(void) { return home_join("cron/output"); }

/* PoP: cronjobs_ticker_heartbeat_file @ cron/jobs.py:TICKER_HEARTBEAT_FILE */
char *cronjobs_ticker_heartbeat_file(void) { return home_join("cron/ticker_heartbeat"); }

/* PoP: cronjobs_ticker_success_file @ cron/jobs.py:TICKER_SUCCESS_FILE */
char *cronjobs_ticker_success_file(void) { return home_join("cron/ticker_last_success"); }

/* PoP: cronjobs_job_output_dir @ cron/jobs.py:_job_output_dir */
char *cronjobs_job_output_dir(const char *job_id, char **err) {
    char *text = str_strip_dup(job_id ? job_id : "");
    if (!text) return set_err(err, "out of memory");
    /* Reject empty, ".", "..", or anything with path separators. */
    if (!*text || strcmp(text, ".") == 0 || strcmp(text, "..") == 0 ||
        strchr(text, '/') || strchr(text, '\\') || text[0] == '/') {
        free(text);
        return set_err(err, "Invalid cron job id for output path");
    }
    char *base = cronjobs_output_dir();
    if (!base) { free(text); return set_err(err, "out of memory"); }
    size_t n = strlen(base) + 1 + strlen(text) + 1;
    char *out = malloc(n);
    if (out) snprintf(out, n, "%s/%s", base, text);
    free(base);
    free(text);
    if (!out) return set_err(err, "out of memory");
    return out;
}

/* PoP: cronjobs_secure_dir @ cron/jobs.py:_secure_dir */
static void cronjobs_secure_dir(const char *path) {
    if (path) chmod(path, 0700); /* best-effort; ignore errors */
}

/* PoP: cronjobs_secure_file @ cron/jobs.py:_secure_file */
static void cronjobs_secure_file(const char *path) {
    struct stat st;
    if (path && stat(path, &st) == 0) chmod(path, 0600);
}

/* PoP: cronjobs_ensure_dirs @ cron/jobs.py:ensure_dirs */
bool cronjobs_ensure_dirs(void) {
    char *cron = cronjobs_cron_dir();
    char *out = cronjobs_output_dir();
    bool ok = false;
    if (cron && out) {
        if (mkdir_p(cron, 0700) == 0 && mkdir_p(out, 0700) == 0) {
            cronjobs_secure_dir(cron);
            cronjobs_secure_dir(out);
            ok = true;
        }
    }
    free(cron);
    free(out);
    return ok;
}

/* ── Cross-process advisory lock (mirrors _jobs_lock) ─────────────── */
/* Returns an open fd holding LOCK_EX on <cron>/.jobs.lock, or -1 (degrade to
 * no-lock, matching the Python fall-open behaviour). Release with
 * cronjobs_unlock(). */
static int cronjobs_lock(void) {
    cronjobs_ensure_dirs();
    char *lp = cronjobs_jobs_lock_file();
    if (!lp) return -1;
    int fd = open(lp, O_CREAT | O_RDWR, 0600);
    free(lp);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX) != 0) {
        /* fall open: keep fd (still gives us a handle to close) */
    }
    return fd;
}

static void cronjobs_unlock(int fd) {
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}

/* ── Skill list normalization ─────────────────────────────────────── */

/* PoP: cronjobs_normalize_skill_list @ cron/jobs.py:_normalize_skill_list */
json_t *cronjobs_normalize_skill_list(const char *skill, const json_t *skills) {
    json_t *out = json_array();
    if (!out) return NULL;

    /* Build the raw item list. */
    json_t *raw = json_array();
    if (!raw) { json_free(out); return NULL; }
    if (!skills || json_is_null(skills)) {
        if (skill && *skill) json_append(raw, json_string(skill));
    } else if (json_is_string(skills)) {
        json_append(raw, json_string(json_string_value(skills)));
    } else if (json_is_array(skills)) {
        size_t n = json_array_size(skills);
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_array_get(skills, i);
            if (json_is_string(it))
                json_append(raw, json_string(json_string_value(it)));
            else if (it && !json_is_null(it)) {
                /* str(item) coercion for non-strings: numbers/bools */
                char buf[64];
                if (json_is_number(it)) {
                    double d = json_number_value(it);
                    if (d == (long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
                    else snprintf(buf, sizeof(buf), "%g", d);
                    json_append(raw, json_string(buf));
                } else if (json_is_bool(it)) {
                    json_append(raw, json_string(json_is_true(it) ? "True" : "False"));
                }
            }
        }
    }

    /* Strip + dedupe preserving order. */
    size_t rn = json_array_size(raw);
    for (size_t i = 0; i < rn; i++) {
        json_t *it = json_array_get(raw, i);
        char *t = str_strip_dup(json_string_value(it));
        if (t && *t) {
            bool dup = false;
            size_t on = json_array_size(out);
            for (size_t j = 0; j < on; j++) {
                if (strcmp(json_string_value(json_array_get(out, j)), t) == 0) { dup = true; break; }
            }
            if (!dup) json_append(out, json_string(t));
        }
        free(t);
    }
    json_free(raw);
    return out;
}

/* PoP: cronjobs_apply_skill_fields @ cron/jobs.py:_apply_skill_fields */
json_t *cronjobs_apply_skill_fields(const json_t *job) {
    json_t *norm = json_copy(job);
    if (!norm) return NULL;
    const char *legacy = json_object_get_string(norm, "skill", NULL);
    json_t *skills = json_object_get(norm, "skills");
    json_t *canon = cronjobs_normalize_skill_list(legacy, skills);
    if (!canon) { json_free(norm); return NULL; }
    json_set(norm, "skills", json_copy(canon));
    if (json_array_size(canon) > 0)
        json_set(norm, "skill", json_string(json_string_value(json_array_get(canon, 0))));
    else
        json_set(norm, "skill", json_null());
    json_free(canon);
    return norm;
}

/* ── Text coercion / display ──────────────────────────────────────── */

/* PoP: cronjobs_coerce_job_text @ cron/jobs.py:_coerce_job_text */
char *cronjobs_coerce_job_text(const json_t *value, const char *fallback) {
    if (!value || json_is_null(value)) return xstrdup(fallback ? fallback : "");
    if (json_is_string(value)) return xstrdup(json_string_value(value));
    if (json_is_number(value)) {
        double d = json_number_value(value);
        char buf[64];
        if (d == (long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else snprintf(buf, sizeof(buf), "%g", d);
        return xstrdup(buf);
    }
    if (json_is_bool(value)) return xstrdup(json_is_true(value) ? "True" : "False");
    return xstrdup(fallback ? fallback : "");
}

/* PoP: cronjobs_schedule_display_for_job @ cron/jobs.py:_schedule_display_for_job */
char *cronjobs_schedule_display_for_job(const json_t *job) {
    json_t *sd = json_object_get(job, "schedule_display");
    char *display = cronjobs_coerce_job_text(sd, "");
    char *stripped = str_strip_dup(display);
    free(display);
    if (stripped && *stripped) return stripped;
    free(stripped);

    json_t *schedule = json_object_get(job, "schedule");
    if (json_is_object(schedule)) {
        const char *keys[] = { "display", "value", "expr", "run_at" };
        for (size_t i = 0; i < 4; i++) {
            json_t *v = json_object_get(schedule, keys[i]);
            char *text = cronjobs_coerce_job_text(v, "");
            char *ts = str_strip_dup(text);
            free(text);
            if (ts && *ts) return ts;
            free(ts);
        }
    } else if (schedule && !json_is_null(schedule)) {
        return cronjobs_coerce_job_text(schedule, "");
    }
    return xstrdup("?");
}

/* PoP: cronjobs_normalize_job_record @ cron/jobs.py:_normalize_job_record */
json_t *cronjobs_normalize_job_record(const json_t *job) {
    json_t *norm = cronjobs_apply_skill_fields(job);
    if (!norm) return NULL;

    char *job_id = cronjobs_coerce_job_text(json_object_get(norm, "id"), "unknown");
    char *prompt = cronjobs_coerce_job_text(json_object_get(norm, "prompt"), "");
    json_set(norm, "id", json_string(job_id));
    json_set(norm, "prompt", json_string(prompt));

    char *name_raw = cronjobs_coerce_job_text(json_object_get(norm, "name"), "");
    char *name = str_strip_dup(name_raw);
    free(name_raw);
    if (!name || !*name) {
        free(name);
        char *script_raw = cronjobs_coerce_job_text(json_object_get(norm, "script"), "");
        char *script = str_strip_dup(script_raw);
        free(script_raw);
        const char *label = NULL;
        json_t *skills = json_object_get(norm, "skills");
        if (prompt && *prompt) label = prompt;
        else if (json_is_array(skills) && json_array_size(skills) > 0)
            label = json_string_value(json_array_get(skills, 0));
        else if (script && *script) label = script;
        else if (job_id && *job_id) label = job_id;
        else label = "cron job";
        /* first 50 chars, then strip; empty → "cron job" */
        char buf[64];
        strncpy(buf, label ? label : "", 50);
        buf[50] = '\0';
        char *nm = str_strip_dup(buf);
        free(script);
        if (!nm || !*nm) { free(nm); name = xstrdup("cron job"); }
        else name = nm;
    }
    json_set(norm, "name", json_string(name));

    char *disp = cronjobs_schedule_display_for_job(norm);
    json_set(norm, "schedule_display", json_string(disp));
    free(disp);

    char *state_raw = cronjobs_coerce_job_text(json_object_get(norm, "state"), "");
    char *state = str_strip_dup(state_raw);
    free(state_raw);
    if (!state || !*state) {
        free(state);
        bool enabled = json_get_bool(norm, "enabled", true);
        state = xstrdup(enabled ? "scheduled" : "paused");
    }
    json_set(norm, "state", json_string(state));

    free(job_id);
    free(prompt);
    free(name);
    free(state);
    return norm;
}

/* ── ISO helpers (local wall-clock with numeric offset, like Python) ── */

/* Format a time_t as local ISO8601 with numeric offset: 2026-07-16T04:16:17+02:00.
 * Matches Python datetime.isoformat() on an aware local datetime. */
static char *iso_local(time_t ts) {
    struct tm lt;
    if (!localtime_r(&ts, &lt)) return NULL;
    char base[32];
    if (strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &lt) == 0) return NULL;
    long off = lt.tm_gmtoff;               /* seconds east of UTC */
    char sign = off < 0 ? '-' : '+';
    long ao = off < 0 ? -off : off;
    int oh = (int)(ao / 3600), om = (int)((ao % 3600) / 60);
    char *out = malloc(40);
    if (!out) return NULL;
    snprintf(out, 40, "%s%c%02d:%02d", base, sign, oh, om);
    return out;
}

/* PoP: now_iso @ tools/skill_usage.py:_now_iso */
/* Current time ISO (local, aware). Mirrors _hermes_now().isoformat(). */
char *now_iso(void) { return iso_local(time(NULL)); }

/* Parse an ISO timestamp → time_t (UTC epoch). -1 on failure. */
static time_t iso_to_ts(const char *iso) {
    if (!iso || !*iso) return (time_t)-1;
    return datetime_parse_iso8601(iso);
}

/* ── Schedule parsing ─────────────────────────────────────────────── */

/* PoP: cronjobs_parse_duration @ cron/jobs.py:parse_duration */
int cronjobs_parse_duration(const char *s) {
    if (!s) return -1;
    char *stripped = str_strip_dup(s);
    if (!stripped) return -1;
    str_tolower_inplace(stripped);
    /* ^(\d+)\s*(m|min|mins|minute|minutes|h|hr|hrs|hour|hours|d|day|days)$ */
    const char *p = stripped;
    long value = 0; int digits = 0;
    while (*p && isdigit((unsigned char)*p)) { value = value * 10 + (*p - '0'); p++; digits++; }
    if (!digits) { free(stripped); return -1; }
    while (*p && isspace((unsigned char)*p)) p++;
    /* unit must be one of the accepted whole words */
    static const char *units[] = {
        "m","min","mins","minute","minutes",
        "h","hr","hrs","hour","hours",
        "d","day","days", NULL
    };
    int mult = 0;
    for (int i = 0; units[i]; i++) {
        if (strcmp(p, units[i]) == 0) {
            mult = (units[i][0] == 'm') ? 1 : (units[i][0] == 'h') ? 60 : 1440;
            break;
        }
    }
    free(stripped);
    if (!mult) return -1;
    return (int)(value * mult);
}

/* Return true when every char in s is one of [0-9*\-,/]. */
static bool is_cron_field(const char *s) {
    if (!s || !*s) return false;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s) && *s != '*' && *s != '-' && *s != ',' && *s != '/')
            return false;
    return true;
}

/* PoP: cronjobs_parse_schedule @ cron/jobs.py:parse_schedule */
json_t *cronjobs_parse_schedule(const char *schedule_in, char **err) {
    char *schedule = str_strip_dup(schedule_in ? schedule_in : "");
    if (!schedule) { set_err(err, "out of memory"); return NULL; }
    char *original = xstrdup(schedule);
    char *lower = xstrdup(schedule);
    str_tolower_inplace(lower);

    json_t *out = NULL;

    /* "every X" → recurring interval */
    if (strncmp(lower, "every ", 6) == 0) {
        char *dur = str_strip_dup(schedule + 6);
        int minutes = cronjobs_parse_duration(dur);
        free(dur);
        if (minutes < 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Invalid duration in 'every' schedule");
            set_err(err, msg);
            goto done;
        }
        out = json_object();
        json_set(out, "kind", json_string("interval"));
        json_set(out, "minutes", json_number(minutes));
        char disp[32];
        snprintf(disp, sizeof(disp), "every %dm", minutes);
        json_set(out, "display", json_string(disp));
        goto done;
    }

    /* Cron expression: >=5 space-separated fields, first 5 all cron-field chars */
    {
        /* tokenize on whitespace */
        char *dup = xstrdup(schedule);
        char *saveptr = NULL;
        char *parts[8]; int np = 0;
        for (char *tok = strtok_r(dup, " \t", &saveptr); tok && np < 8;
             tok = strtok_r(NULL, " \t", &saveptr))
            parts[np++] = tok;
        bool looks_cron = (np >= 5);
        for (int i = 0; i < 5 && looks_cron; i++)
            if (!is_cron_field(parts[i])) looks_cron = false;
        free(dup);
        if (looks_cron) {
            char *cerr = NULL;
            cron_expr_t *ce = cron_parse(schedule, &cerr);
            if (!ce) {
                char msg[192];
                snprintf(msg, sizeof(msg), "Invalid cron expression '%s': %s",
                         schedule, cerr ? cerr : "parse error");
                free(cerr);
                set_err(err, msg);
                goto done;
            }
            cron_free(ce);
            free(cerr);
            out = json_object();
            json_set(out, "kind", json_string("cron"));
            json_set(out, "expr", json_string(schedule));
            json_set(out, "display", json_string(schedule));
            goto done;
        }
    }

    /* ISO timestamp: contains 'T' or matches ^\d{4}-\d{2}-\d{2} */
    {
        bool has_t = strchr(schedule, 'T') != NULL;
        bool date_like = (strlen(schedule) >= 10 &&
                          isdigit((unsigned char)schedule[0]) && isdigit((unsigned char)schedule[1]) &&
                          isdigit((unsigned char)schedule[2]) && isdigit((unsigned char)schedule[3]) &&
                          schedule[4] == '-' && isdigit((unsigned char)schedule[5]) &&
                          isdigit((unsigned char)schedule[6]) && schedule[7] == '-' &&
                          isdigit((unsigned char)schedule[8]) && isdigit((unsigned char)schedule[9]));
        if (has_t || date_like) {
            time_t ts = iso_to_ts(schedule);
            if (ts == (time_t)-1) {
                char msg[192];
                snprintf(msg, sizeof(msg), "Invalid timestamp '%s'", schedule);
                set_err(err, msg);
                goto done;
            }
            char *run_at = iso_local(ts);
            struct tm lt; localtime_r(&ts, &lt);
            char disp_time[32];
            strftime(disp_time, sizeof(disp_time), "%Y-%m-%d %H:%M", &lt);
            out = json_object();
            json_set(out, "kind", json_string("once"));
            json_set(out, "run_at", json_string(run_at));
            char disp[64];
            snprintf(disp, sizeof(disp), "once at %s", disp_time);
            json_set(out, "display", json_string(disp));
            free(run_at);
            goto done;
        }
    }

    /* Duration like "30m"/"2h"/"1d" → one-shot from now */
    {
        int minutes = cronjobs_parse_duration(schedule);
        if (minutes >= 0) {
            time_t run = time(NULL) + (time_t)minutes * 60;
            char *run_at = iso_local(run);
            out = json_object();
            json_set(out, "kind", json_string("once"));
            json_set(out, "run_at", json_string(run_at));
            char disp[64];
            snprintf(disp, sizeof(disp), "once in %s", original);
            json_set(out, "display", json_string(disp));
            free(run_at);
            goto done;
        }
    }

    {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Invalid schedule '%s'. Use: duration '30m'/'2h'/'1d', "
                 "interval 'every 30m', cron '0 9 * * *', or timestamp "
                 "'2026-02-03T14:00:00'", original);
        set_err(err, msg);
    }

done:
    free(schedule);
    free(original);
    free(lower);
    return out;
}

/* PoP: cronjobs_recoverable_oneshot_run_at @ cron/jobs.py:_recoverable_oneshot_run_at */
static char *recoverable_oneshot_run_at(const json_t *schedule, time_t now,
                                        const char *last_run_at) {
    const char *kind = json_object_get_string(schedule, "kind", "");
    if (strcmp(kind, "once") != 0) return NULL;
    if (last_run_at && *last_run_at) return NULL;
    const char *run_at = json_object_get_string(schedule, "run_at", NULL);
    if (!run_at || !*run_at) return NULL;
    time_t run_ts = iso_to_ts(run_at);
    if (run_ts == (time_t)-1) return NULL;
    if (run_ts >= now - CRONJOBS_ONESHOT_GRACE_SECONDS)
        return xstrdup(run_at);
    return NULL;
}

/* PoP: cronjobs_compute_grace_seconds @ cron/jobs.py:_compute_grace_seconds */
int cronjobs_compute_grace_seconds(const json_t *schedule) {
    const int MIN_GRACE = 120, MAX_GRACE = 7200;
    const char *kind = json_object_get_string(schedule, "kind", "");
    if (strcmp(kind, "interval") == 0) {
        int minutes = (int)json_get_num(schedule, "minutes", 1);
        long period = (long)minutes * 60;
        long grace = period / 2;
        if (grace < MIN_GRACE) grace = MIN_GRACE;
        if (grace > MAX_GRACE) grace = MAX_GRACE;
        return (int)grace;
    }
    if (strcmp(kind, "cron") == 0) {
        const char *expr = json_object_get_string(schedule, "expr", NULL);
        cron_expr_t *ce = expr ? cron_parse(expr, NULL) : NULL;
        if (ce) {
            time_t now = time(NULL);
            struct tm from, first, second;
            localtime_r(&now, &from);
            if (cron_next(ce, &from, &first) && cron_next(ce, &first, &second)) {
                time_t f = mktime(&first), s = mktime(&second);
                long period = (long)(s - f);
                long grace = period / 2;
                cron_free(ce);
                if (grace < MIN_GRACE) grace = MIN_GRACE;
                if (grace > MAX_GRACE) grace = MAX_GRACE;
                return (int)grace;
            }
            cron_free(ce);
        }
    }
    return MIN_GRACE;
}

/* PoP: cronjobs_compute_next_run @ cron/jobs.py:compute_next_run */
char *cronjobs_compute_next_run(const json_t *schedule, const char *last_run_at) {
    if (!json_is_object(schedule)) return NULL;
    time_t now = time(NULL);
    const char *kind = json_object_get_string(schedule, "kind", "");

    if (strcmp(kind, "once") == 0) {
        return recoverable_oneshot_run_at(schedule, now, last_run_at);
    }
    if (strcmp(kind, "interval") == 0) {
        int minutes = (int)json_get_num(schedule, "minutes", 0);
        time_t base;
        if (last_run_at && *last_run_at) {
            base = iso_to_ts(last_run_at);
            if (base == (time_t)-1) base = now;
        } else {
            base = now;
        }
        return iso_local(base + (time_t)minutes * 60);
    }
    if (strcmp(kind, "cron") == 0) {
        const char *expr = json_object_get_string(schedule, "expr", NULL);
        if (!expr) return NULL;
        cron_expr_t *ce = cron_parse(expr, NULL);
        if (!ce) return NULL;
        time_t base = now;
        if (last_run_at && *last_run_at) {
            time_t b = iso_to_ts(last_run_at);
            if (b != (time_t)-1) base = b;
        }
        struct tm from, out;
        localtime_r(&base, &from);
        char *result = NULL;
        if (cron_next(ce, &from, &out)) {
            struct tm tmp = out;
            time_t nt = mktime(&tmp);
            result = iso_local(nt);
        }
        cron_free(ce);
        return result;
    }
    return NULL;
}

/* ── Ticker heartbeat ─────────────────────────────────────────────── */

/* PoP: cronjobs_atomic_write_epoch @ cron/jobs.py:_atomic_write_epoch */
static void atomic_write_epoch(const char *path) {
    cronjobs_ensure_dirs();
    char *dir = cronjobs_cron_dir();
    if (!dir) return;
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/.hb_XXXXXX", dir);
    free(dir);
    int fd = mkstemp(tmp);
    if (fd < 0) return;
    char buf[64];
    int L = snprintf(buf, sizeof(buf), "%ld", (long)time(NULL));
    if (write(fd, buf, (size_t)L) == L) {
        fsync(fd);
        close(fd);
        if (rename(tmp, path) != 0) unlink(tmp);
    } else {
        close(fd);
        unlink(tmp);
    }
}

/* PoP: cronjobs_record_ticker_heartbeat @ cron/jobs.py:record_ticker_heartbeat */
void cronjobs_record_ticker_heartbeat(bool success) {
    char *hb = cronjobs_ticker_heartbeat_file();
    if (hb) { atomic_write_epoch(hb); free(hb); }
    if (success) {
        char *sc = cronjobs_ticker_success_file();
        if (sc) { atomic_write_epoch(sc); free(sc); }
    }
}

/* PoP: cronjobs_epoch_file_age @ cron/jobs.py:_epoch_file_age */
static double epoch_file_age(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1.0;
    char buf[64];
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1.0; }
    fclose(f);
    char *stripped = str_strip_dup(buf);
    if (!stripped || !*stripped) { free(stripped); return -1.0; }
    char *endp = NULL;
    double stamp = strtod(stripped, &endp);
    bool ok = (endp && endp != stripped);
    free(stripped);
    if (!ok) return -1.0;
    double age = (double)time(NULL) - stamp;
    return age < 0.0 ? 0.0 : age;
}

/* PoP: cronjobs_ticker_heartbeat_age @ cron/jobs.py:get_ticker_heartbeat_age */
double cronjobs_ticker_heartbeat_age(void) {
    char *hb = cronjobs_ticker_heartbeat_file();
    if (!hb) return -1.0;
    double a = epoch_file_age(hb);
    free(hb);
    return a;
}

/* PoP: cronjobs_ticker_success_age @ cron/jobs.py:get_ticker_success_age */
double cronjobs_ticker_success_age(void) {
    char *sc = cronjobs_ticker_success_file();
    if (!sc) return -1.0;
    double a = epoch_file_age(sc);
    free(sc);
    return a;
}

/* ── Persistence ──────────────────────────────────────────────────── */

/* PoP: cronjobs_load_jobs @ cron/jobs.py:load_jobs */
json_t *cronjobs_load_jobs(char **err) {
    cronjobs_ensure_dirs();
    char *path = cronjobs_jobs_file();
    if (!path) { set_err(err, "out of memory"); return NULL; }
    struct stat st;
    if (stat(path, &st) != 0) { free(path); return json_array(); }

    char *perr = NULL;
    json_t *data = json_parse_file(path, &perr);
    free(perr);
    free(path);
    if (!data) {
        /* Unrepairable corruption. */
        set_err(err, "Cron database corrupted and unrepairable");
        return NULL;
    }

    if (json_is_object(data)) {
        json_t *jobs = json_object_get(data, "jobs");
        json_t *result;
        if (json_is_array(jobs)) result = json_copy(jobs);
        else result = json_array();
        json_free(data);
        return result;
    }
    if (json_is_array(data)) {
        /* Bare list — auto-repair by rewrapping (write back {"jobs":[...]}). */
        if (json_array_size(data) > 0) cronjobs_save_jobs(data);
        return data; /* return the list itself */
    }
    json_free(data);
    set_err(err, "Cron database corrupted: expected {'jobs': [...]}");
    return NULL;
}

/* PoP: cronjobs_save_jobs @ cron/jobs.py:_save_jobs_unlocked */
/* PoP: cronjobs_save_jobs @ cron/jobs.py:save_jobs */
bool cronjobs_save_jobs(const json_t *jobs) {
    cronjobs_ensure_dirs();
    char *path = cronjobs_jobs_file();
    if (!path) return false;

    json_t *wrapper = json_object();
    json_set(wrapper, "jobs", json_copy(jobs));
    char *ts = now_iso();
    json_set(wrapper, "updated_at", json_string(ts ? ts : ""));
    free(ts);
    char *text = json_serialize_pretty(wrapper, 2);
    json_free(wrapper);
    if (!text) { free(path); return false; }

    char *dir = cronjobs_cron_dir();
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/.jobs_XXXXXX", dir ? dir : "/tmp");
    free(dir);
    int fd = mkstemp(tmp);
    bool ok = false;
    if (fd >= 0) {
        size_t len = strlen(text);
        if (write(fd, text, len) == (ssize_t)len) {
            fsync(fd);
            close(fd);
            if (rename(tmp, path) == 0) {
                cronjobs_secure_file(path);
                ok = true;
            } else unlink(tmp);
        } else {
            close(fd);
            unlink(tmp);
        }
    }
    free(text);
    free(path);
    return ok;
}

/* ── Workdir / optional text ──────────────────────────────────────── */

/* PoP: cronjobs_ensure_aware @ cron/jobs.py:_ensure_aware */
/* Single-home Slermes stores aware ISO timestamps (see iso_local); naive
 * timestamps never occur, so "ensure aware" is identity with a guard that
 * returns NULL for empty input (mirrors fail-open on unparseable input). */
static char *ensure_aware(const char *iso) {
    if (!iso || !*iso) return NULL;
    return xstrdup(iso);
}

/* PoP: cronjobs_resolve_default_model_snapshot @ cron/jobs.py:_resolve_default_model_snapshot */
/* Single-home: there is no multi-profile config resolver wired here, and the
 * Python path fail-opens to None when config is missing/unreadable. We mirror
 * that exactly (null snapshot), so an unpinned-model job carries no snapshot
 * and the fire-time drift guard treats "no snapshot" as "no assertion". */
static char *resolve_default_model_snapshot(void) { return NULL; }

/* PoP: cronjobs_compute_provider_model_snapshots @ cron/jobs.py:_compute_provider_model_snapshots */
static void compute_provider_model_snapshots(const char *provider, const char *model,
                                             const char *base_url, bool no_agent,
                                             char **out_provider, char **out_model) {
    *out_provider = NULL;
    *out_model = NULL;
    if (no_agent) return;
    (void)base_url; /* reserved for future runtime resolution (single-home = null) */
    if (!provider || !*provider) *out_provider = NULL; /* fail-open: no snapshot */
    if (!model || !*model) *out_model = resolve_default_model_snapshot();
}

/* PoP: cronjobs_normalized_inference_axes @ cron/jobs.py:_normalized_inference_axes */
static void normalized_inference_axes(const json_t *job, char **provider, char **model,
                                      char **base_url, bool *no_agent) {
    *provider = normalize_optional_text(json_object_get(job, "provider"), false);
    *model = normalize_optional_text(json_object_get(job, "model"), false);
    *base_url = normalize_optional_text(json_object_get(job, "base_url"), true);
    *no_agent = json_get_bool(job, "no_agent", false);
}

/* PoP: cronjobs_normalize_workdir @ cron/jobs.py:_normalize_workdir */
char *cronjobs_normalize_workdir(const char *workdir, char **err) {
    if (!workdir) return NULL;
    char *raw = str_strip_dup(workdir);
    if (!raw || !*raw) { free(raw); return NULL; }

    /* Expand leading ~ */
    char expanded[PATH_MAX];
    if (raw[0] == '~') {
        const char *hdir = getenv("HOME");
        if (!hdir) hdir = "";
        snprintf(expanded, sizeof(expanded), "%s%s", hdir, raw + 1);
    } else {
        strncpy(expanded, raw, sizeof(expanded) - 1);
        expanded[sizeof(expanded) - 1] = '\0';
    }
    free(raw);

    if (expanded[0] != '/') {
        set_err(err, "Cron workdir must be an absolute path");
        return NULL;
    }
    char resolved[PATH_MAX];
    if (!realpath(expanded, resolved)) {
        set_err(err, "Cron workdir does not exist");
        return NULL;
    }
    struct stat st;
    if (stat(resolved, &st) != 0) { set_err(err, "Cron workdir does not exist"); return NULL; }
    if (!S_ISDIR(st.st_mode)) { set_err(err, "Cron workdir is not a directory"); return NULL; }
    return xstrdup(resolved);
}

/* PoP: cronjobs_normalize_job_optional_text @ cron/jobs.py:_normalize_job_optional_text */
static char *normalize_optional_text(const json_t *value, bool strip_trailing_slash) {
    if (!json_is_string(value)) return NULL;
    char *text = str_strip_dup(json_string_value(value));
    if (!text) return NULL;
    if (strip_trailing_slash) {
        size_t n = strlen(text);
        while (n > 0 && text[n - 1] == '/') text[--n] = '\0';
    }
    if (!*text) { free(text); return NULL; }
    return text;
}

/* Convenience overload taking a C string. */
static char *normalize_optional_cstr(const char *value, bool strip_trailing_slash) {
    if (!value) return NULL;
    json_t *s = json_string(value);
    char *r = normalize_optional_text(s, strip_trailing_slash);
    json_free(s);
    return r;
}



/* ── Job id ───────────────────────────────────────────────────────── */

#include "uuid.h"

/* uuid4().hex[:12] — 12 lowercase hex chars from a v4 UUID. */
static char *new_job_id(void) {
    uint8_t b[16];
    if (!uuid_v4_bytes(b)) {
        char *p = malloc(13);
        if (p) snprintf(p, 13, "%06lx%06x", (long)time(NULL) & 0xffffff, rand() & 0xffffff);
        return p;
    }
    char *p = malloc(13);
    if (!p) return NULL;
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        p[i * 2] = hx[b[i] >> 4];
        p[i * 2 + 1] = hx[b[i] & 0xf];
    }
    p[12] = '\0';
    return p;
}

/* ── CRUD ─────────────────────────────────────────────────────────── */

/* PoP: cronjobs_create_job @ cron/jobs.py:create_job */
json_t *cronjobs_create_job(const cronjobs_create_opts *o, char **err) {
    if (!o || !o->schedule) { set_err(err, "schedule is required"); return NULL; }

    json_t *parsed = cronjobs_parse_schedule(o->schedule, err);
    if (!parsed) return NULL;
    const char *kind = json_object_get_string(parsed, "kind", "");

    int repeat = o->repeat;
    bool has_repeat = o->has_repeat && o->repeat > 0;
    if (strcmp(kind, "once") == 0 && !has_repeat) { repeat = 1; has_repeat = true; }

    const char *deliver = o->deliver;
    char *deliver_owned = NULL;
    if (!deliver)
        deliver = (o->origin && !json_is_null(o->origin)) ? "origin" : "local";

    char *job_id = new_job_id();
    char *now = now_iso();

    json_t *skills = cronjobs_normalize_skill_list(o->skill, o->skills);
    char *model = normalize_optional_cstr(o->model, false);
    char *provider = normalize_optional_cstr(o->provider, false);
    char *base_url = normalize_optional_cstr(o->base_url, true);
    char *script = o->script ? str_strip_dup(o->script) : NULL;
    if (script && !*script) { free(script); script = NULL; }
    bool no_agent = o->no_agent;

    if (no_agent && !script) {
        set_err(err, "no_agent=True requires a script");
        json_free(parsed); json_free(skills);
        free(job_id); free(now); free(model); free(provider); free(base_url);
        free(deliver_owned);
        return NULL;
    }

    json_t *toolsets = NULL;
    if (o->enabled_toolsets && json_is_array(o->enabled_toolsets)) {
        json_t *tmp = json_array();
        size_t n = json_array_size(o->enabled_toolsets);
        for (size_t i = 0; i < n; i++) {
            char *t = str_strip_dup(json_string_value(json_array_get(o->enabled_toolsets, i)));
            if (t && *t) json_append(tmp, json_string(t));
            free(t);
        }
        if (json_array_size(tmp) > 0) toolsets = tmp; else json_free(tmp);
    }

    json_t *context_from = NULL;
    if (o->context_from) {
        if (json_is_string(o->context_from)) {
            char *t = str_strip_dup(json_string_value(o->context_from));
            if (t && *t) { context_from = json_array(); json_append(context_from, json_string(t)); }
            free(t);
        } else if (json_is_array(o->context_from)) {
            json_t *tmp = json_array();
            size_t n = json_array_size(o->context_from);
            for (size_t i = 0; i < n; i++) {
                char *t = str_strip_dup(json_string_value(json_array_get(o->context_from, i)));
                if (t && *t) json_append(tmp, json_string(t));
                free(t);
            }
            if (json_array_size(tmp) > 0) context_from = tmp; else json_free(tmp);
        }
    }

    char *workdir = NULL;
    if (o->workdir) {
        char *werr = NULL;
        workdir = cronjobs_normalize_workdir(o->workdir, &werr);
        if (werr) {
            set_err(err, werr); free(werr);
            json_free(parsed); json_free(skills); json_free(toolsets); json_free(context_from);
            free(job_id); free(now); free(model); free(provider); free(base_url); free(script);
            return NULL;
        }
    }

    char *prompt_text = o->prompt ? xstrdup(o->prompt) : xstrdup("");

    char *lc_err = cron_lifecycle_check_gateway_lifecycle(prompt_text, script);
    if (lc_err) {
        set_err(err, lc_err); free(lc_err);
        json_free(parsed); json_free(skills); json_free(toolsets); json_free(context_from);
        free(job_id); free(now); free(model); free(provider); free(base_url);
        free(script); free(workdir); free(prompt_text);
        return NULL;
    }

    const char *label = NULL;
    if (*prompt_text) label = prompt_text;
    else if (json_array_size(skills) > 0) label = json_string_value(json_array_get(skills, 0));
    else if (no_agent && script) label = script;
    else label = "cron job";
    char label_buf[64];
    strncpy(label_buf, label, 50); label_buf[50] = '\0';
    char *label_stripped = str_strip_dup(label_buf);

    json_t *job = json_object();
    json_set(job, "id", json_string(job_id));
    if (o->name) json_set(job, "name", json_string(o->name));
    else json_set(job, "name", json_string(label_stripped));
    json_set(job, "prompt", json_string(prompt_text));
    json_set(job, "skills", json_copy(skills));
    if (json_array_size(skills) > 0)
        json_set(job, "skill", json_string(json_string_value(json_array_get(skills, 0))));
    else json_set(job, "skill", json_null());
    json_set(job, "model", model ? json_string(model) : json_null());
    json_set(job, "provider", provider ? json_string(provider) : json_null());
    json_set(job, "provider_snapshot", json_null());
    json_set(job, "model_snapshot", json_null());
    json_set(job, "base_url", base_url ? json_string(base_url) : json_null());
    json_set(job, "script", script ? json_string(script) : json_null());
    json_set(job, "no_agent", json_bool(no_agent));
    json_set(job, "context_from", context_from ? context_from : json_null());
    json_set(job, "schedule", json_copy(parsed));
    json_set(job, "schedule_display",
             json_string(json_object_get_string(parsed, "display", o->schedule)));
    json_t *rep = json_object();
    json_set(rep, "times", has_repeat ? json_number(repeat) : json_null());
    json_set(rep, "completed", json_number(0));
    json_set(job, "repeat", rep);
    json_set(job, "enabled", json_bool(true));
    json_set(job, "state", json_string("scheduled"));
    json_set(job, "paused_at", json_null());
    json_set(job, "paused_reason", json_null());
    json_set(job, "created_at", json_string(now));
    char *next = cronjobs_compute_next_run(parsed, NULL);
    json_set(job, "next_run_at", next ? json_string(next) : json_null());
    free(next);
    json_set(job, "last_run_at", json_null());
    json_set(job, "last_status", json_null());
    json_set(job, "last_error", json_null());
    json_set(job, "last_delivery_error", json_null());
    json_set(job, "deliver", json_string(deliver));
    json_set(job, "origin", (o->origin && !json_is_null(o->origin)) ? json_copy(o->origin) : json_null());
    json_set(job, "enabled_toolsets", toolsets ? toolsets : json_null());
    json_set(job, "workdir", workdir ? json_string(workdir) : json_null());
    if (o->attach_to_session >= 0)
        json_set(job, "attach_to_session", json_bool(o->attach_to_session != 0));

    int lk = cronjobs_lock();
    char *lerr = NULL;
    json_t *jobs = cronjobs_load_jobs(&lerr);
    free(lerr);
    if (!jobs) jobs = json_array();
    json_append(jobs, json_copy(job));
    cronjobs_save_jobs(jobs);
    json_free(jobs);
    cronjobs_unlock(lk);

    json_free(parsed); json_free(skills);
    free(job_id); free(now); free(model); free(provider); free(base_url);
    free(script); free(workdir); free(prompt_text); free(label_stripped);
    free(deliver_owned);
    return job;
}

/* Find index of job with id in an array, or -1. */
static long find_job_index(const json_t *jobs, const char *id) {
    size_t n = json_array_size(jobs);
    for (size_t i = 0; i < n; i++) {
        const char *jid = json_object_get_string(json_array_get(jobs, i), "id", "");
        if (strcmp(jid, id) == 0) return (long)i;
    }
    return -1;
}

/* PoP: cronjobs_get_job @ cron/jobs.py:get_job */
json_t *cronjobs_get_job(const char *job_id) {
    if (!job_id) return NULL;
    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    free(err);
    if (!jobs) return NULL;
    json_t *result = NULL;
    long idx = find_job_index(jobs, job_id);
    if (idx >= 0) result = cronjobs_normalize_job_record(json_array_get(jobs, (size_t)idx));
    json_free(jobs);
    return result;
}

/* PoP: cronjobs_resolve_job_ref @ cron/jobs.py:resolve_job_ref */
json_t *cronjobs_resolve_job_ref(const char *ref, bool *ambiguous) {
    if (ambiguous) *ambiguous = false;
    if (!ref || !*ref) return NULL;
    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    free(err);
    if (!jobs) return NULL;

    long idx = find_job_index(jobs, ref);
    if (idx >= 0) {
        json_t *r = cronjobs_normalize_job_record(json_array_get(jobs, (size_t)idx));
        json_free(jobs);
        return r;
    }
    char *ref_lower = xstrdup(ref);
    str_tolower_inplace(ref_lower);
    long match_idx = -1; int matches = 0;
    size_t n = json_array_size(jobs);
    for (size_t i = 0; i < n; i++) {
        const char *nm = json_object_get_string(json_array_get(jobs, i), "name", "");
        char *nl = xstrdup(nm ? nm : "");
        str_tolower_inplace(nl);
        if (strcmp(nl, ref_lower) == 0) { matches++; if (match_idx < 0) match_idx = (long)i; }
        free(nl);
    }
    free(ref_lower);
    json_t *result = NULL;
    if (matches == 1)
        result = cronjobs_normalize_job_record(json_array_get(jobs, (size_t)match_idx));
    else if (matches > 1 && ambiguous)
        *ambiguous = true;
    json_free(jobs);
    return result;
}

/* PoP: cronjobs_list_jobs @ cron/jobs.py:list_jobs */
json_t *cronjobs_list_jobs(bool include_disabled) {
    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    free(err);
    if (!jobs) return json_array();
    json_t *out = json_array();
    size_t n = json_array_size(jobs);
    for (size_t i = 0; i < n; i++) {
        json_t *norm = cronjobs_normalize_job_record(json_array_get(jobs, i));
        if (!norm) continue;
        if (!include_disabled && !json_get_bool(norm, "enabled", true)) { json_free(norm); continue; }
        json_append(out, norm);
    }
    json_free(jobs);
    return out;
}


/* ── Update / lifecycle ───────────────────────────────────────────── */

/* Return the semantic (provider,model,base_url,no_agent) tuple as a string key
 * for change comparison. Caller frees. */
static char *inference_axes_key(const json_t *job) {
    char *p = normalize_optional_text(json_object_get(job, "provider"), false);
    char *m = normalize_optional_text(json_object_get(job, "model"), false);
    char *b = normalize_optional_text(json_object_get(job, "base_url"), true);
    bool na = json_get_bool(job, "no_agent", false);
    size_t n = (p ? strlen(p) : 0) + (m ? strlen(m) : 0) + (b ? strlen(b) : 0) + 16;
    char *key = malloc(n);
    if (key) snprintf(key, n, "%s|%s|%s|%d", p ? p : "", m ? m : "", b ? b : "", na ? 1 : 0);
    free(p); free(m); free(b);
    return key;
}

/* PoP: cronjobs_update_job @ cron/jobs.py:update_job */
json_t *cronjobs_update_job(const char *job_id, const json_t *updates, char **err) {
    if (!job_id || !json_is_object(updates)) return NULL;
    /* Immutable field guard: id. */
    if (json_object_get(updates, "id")) {
        set_err(err, "Cron job field(s) cannot be updated: id");
        return NULL;
    }

    int lk = cronjobs_lock();
    char *lerr = NULL;
    json_t *jobs = cronjobs_load_jobs(&lerr);
    free(lerr);
    if (!jobs) { cronjobs_unlock(lk); return NULL; }

    json_t *result = NULL;
    long idx = find_job_index(jobs, job_id);
    if (idx >= 0) {
        json_t *job = json_array_get(jobs, (size_t)idx);
        char *prev_axes = inference_axes_key(job);

        /* merged = {**job, **updates} */
        json_t *merged = json_copy(job);
        size_t un = json_array_size(updates); (void)un;
        /* iterate updates object keys */
        for (size_t i = 0; ; i++) {
            const char *k = json_object_get_key_at(updates, i);
            if (!k) break;
            json_t *v = json_object_get_at(updates, i);
            /* workdir special handling */
            if (strcmp(k, "workdir") == 0) {
                if (!v || json_is_null(v) ||
                    (json_is_string(v) && !*json_string_value(v)) ||
                    (json_is_bool(v) && !json_is_true(v))) {
                    json_set(merged, "workdir", json_null());
                    continue;
                }
                char *werr = NULL;
                char *wd = cronjobs_normalize_workdir(json_string_value(v), &werr);
                if (werr) { set_err(err, werr); free(werr); json_free(merged); free(prev_axes); goto unlock; }
                json_set(merged, "workdir", wd ? json_string(wd) : json_null());
                free(wd);
                continue;
            }
            json_set(merged, k, json_copy(v));
        }

        /* apply_skill_fields */
        json_t *updated = cronjobs_apply_skill_fields(merged);
        json_free(merged);

        bool schedule_changed = json_object_get(updates, "schedule") != NULL;
        bool inf_touched = json_object_get(updates, "provider") || json_object_get(updates, "model") ||
                           json_object_get(updates, "base_url") || json_object_get(updates, "no_agent");
        char *new_axes = inference_axes_key(updated);
        bool inf_changed = inf_touched && prev_axes && new_axes && strcmp(prev_axes, new_axes) != 0;
        free(new_axes);

        if (json_object_get(updates, "skills") || json_object_get(updates, "skill")) {
            json_t *ns = cronjobs_normalize_skill_list(
                json_object_get_string(updated, "skill", NULL),
                json_object_get(updated, "skills"));
            json_set(updated, "skills", json_copy(ns));
            if (json_array_size(ns) > 0)
                json_set(updated, "skill", json_string(json_string_value(json_array_get(ns, 0))));
            else json_set(updated, "skill", json_null());
            json_free(ns);
        }

        if (schedule_changed) {
            json_t *sched = json_object_get(updated, "schedule");
            if (json_is_string(sched)) {
                json_t *ps = cronjobs_parse_schedule(json_string_value(sched), err);
                if (!ps) { free(prev_axes); json_free(updated); goto unlock; }
                json_set(updated, "schedule", ps);
                sched = json_object_get(updated, "schedule");
            }
            json_t *sd_upd = json_object_get(updates, "schedule_display");
            if (sd_upd) json_set(updated, "schedule_display", json_copy(sd_upd));
            else json_set(updated, "schedule_display",
                          json_string(json_object_get_string(sched, "display",
                                       json_object_get_string(updated, "schedule_display", "?"))));
            if (strcmp(json_object_get_string(updated, "state", ""), "paused") != 0) {
                char *nn = cronjobs_compute_next_run(sched, NULL);
                json_set(updated, "next_run_at", nn ? json_string(nn) : json_null());
                free(nn);
            }
        }

        if (inf_changed) {
            /* single-home: no live resolver → null snapshots (fail-open). */
            json_set(updated, "provider_snapshot", json_null());
            json_set(updated, "model_snapshot", json_null());
        }

        bool enabled = json_get_bool(updated, "enabled", true);
        bool paused = strcmp(json_object_get_string(updated, "state", ""), "paused") == 0;
        json_t *nra = json_object_get(updated, "next_run_at");
        bool no_next = !nra || json_is_null(nra) ||
                       (json_is_string(nra) && !*json_string_value(nra));
        if (enabled && !paused && no_next) {
            char *nn = cronjobs_compute_next_run(json_object_get(updated, "schedule"), NULL);
            json_set(updated, "next_run_at", nn ? json_string(nn) : json_null());
            free(nn);
        }

        /* jobs[idx] = updated */
        json_t *newjobs = json_array();
        size_t n = json_array_size(jobs);
        for (size_t i = 0; i < n; i++) {
            if ((long)i == idx) json_append(newjobs, json_copy(updated));
            else json_append(newjobs, json_copy(json_array_get(jobs, i)));
        }
        cronjobs_save_jobs(newjobs);
        result = cronjobs_normalize_job_record(updated);
        json_free(newjobs);
        json_free(updated);
        free(prev_axes);
    }

unlock:
    json_free(jobs);
    cronjobs_unlock(lk);
    return result;
}

/* PoP: cronjobs_pause_job @ cron/jobs.py:pause_job */
json_t *cronjobs_pause_job(const char *job_id, const char *reason) {
    bool amb = false;
    json_t *job = cronjobs_resolve_job_ref(job_id, &amb);
    if (!job) return NULL;
    char *id = xstrdup(json_object_get_string(job, "id", ""));
    json_free(job);
    char *now = now_iso();
    json_t *upd = json_object();
    json_set(upd, "enabled", json_bool(false));
    json_set(upd, "state", json_string("paused"));
    json_set(upd, "paused_at", json_string(now));
    json_set(upd, "paused_reason", reason ? json_string(reason) : json_null());
    json_t *r = cronjobs_update_job(id, upd, NULL);
    json_free(upd); free(id); free(now);
    return r;
}

/* PoP: cronjobs_resume_job @ cron/jobs.py:resume_job */
json_t *cronjobs_resume_job(const char *job_id) {
    bool amb = false;
    json_t *job = cronjobs_resolve_job_ref(job_id, &amb);
    if (!job) return NULL;
    char *id = xstrdup(json_object_get_string(job, "id", ""));
    char *next = cronjobs_compute_next_run(json_object_get(job, "schedule"), NULL);
    json_free(job);
    json_t *upd = json_object();
    json_set(upd, "enabled", json_bool(true));
    json_set(upd, "state", json_string("scheduled"));
    json_set(upd, "paused_at", json_null());
    json_set(upd, "paused_reason", json_null());
    json_set(upd, "next_run_at", next ? json_string(next) : json_null());
    json_t *r = cronjobs_update_job(id, upd, NULL);
    json_free(upd); free(id); free(next);
    return r;
}

/* PoP: cronjobs_trigger_job @ cron/jobs.py:trigger_job */
json_t *cronjobs_trigger_job(const char *job_id) {
    bool amb = false;
    json_t *job = cronjobs_resolve_job_ref(job_id, &amb);
    if (!job) return NULL;
    char *id = xstrdup(json_object_get_string(job, "id", ""));
    json_free(job);
    char *now = now_iso();
    json_t *upd = json_object();
    json_set(upd, "enabled", json_bool(true));
    json_set(upd, "state", json_string("scheduled"));
    json_set(upd, "paused_at", json_null());
    json_set(upd, "paused_reason", json_null());
    json_set(upd, "next_run_at", json_string(now));
    json_t *r = cronjobs_update_job(id, upd, NULL);
    json_free(upd); free(id); free(now);
    return r;
}

/* Recursively rmtree a directory. */
static void rmtree(const char *path) {
    DIR *d = opendir(path);
    if (!d) { unlink(path); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        struct stat st;
        if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode)) rmtree(child);
        else unlink(child);
    }
    closedir(d);
    rmdir(path);
}

/* PoP: cronjobs_remove_job @ cron/jobs.py:remove_job */
bool cronjobs_remove_job(const char *job_id) {
    bool amb = false;
    json_t *job = cronjobs_resolve_job_ref(job_id, &amb);
    if (!job) return false;
    char *canonical = xstrdup(json_object_get_string(job, "id", ""));
    json_free(job);

    int lk = cronjobs_lock();
    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    free(err);
    bool removed = false;
    if (jobs) {
        size_t orig = json_array_size(jobs);
        json_t *kept = json_array();
        for (size_t i = 0; i < orig; i++) {
            const char *jid = json_object_get_string(json_array_get(jobs, i), "id", "");
            if (strcmp(jid, canonical) != 0) json_append(kept, json_copy(json_array_get(jobs, i)));
        }
        if (json_array_size(kept) < orig) {
            char *odir = cronjobs_job_output_dir(canonical, NULL);
            cronjobs_save_jobs(kept);
            if (odir) {
                struct stat st;
                if (stat(odir, &st) == 0) rmtree(odir);
                free(odir);
            }
            removed = true;
        }
        json_free(kept);
        json_free(jobs);
    }
    cronjobs_unlock(lk);
    free(canonical);
    return removed;
}

/* ── Run bookkeeping ──────────────────────────────────────────────── */

/* PoP: cronjobs_mark_job_run @ cron/jobs.py:mark_job_run */
void cronjobs_mark_job_run(const char *job_id, bool success,
                           const char *error, const char *delivery_error) {
    int lk = cronjobs_lock();
    char *lerr = NULL;
    json_t *jobs = cronjobs_load_jobs(&lerr);
    free(lerr);
    if (!jobs) { cronjobs_unlock(lk); return; }

    long idx = find_job_index(jobs, job_id);
    if (idx < 0) { json_free(jobs); cronjobs_unlock(lk); return; }
    json_t *job = json_array_get(jobs, (size_t)idx);
    char *now = now_iso();
    json_set(job, "last_run_at", json_string(now));
    json_set(job, "last_status", json_string(success ? "ok" : "error"));
    json_set(job, "last_error", (!success && error) ? json_string(error) : json_null());
    json_set(job, "last_delivery_error", delivery_error ? json_string(delivery_error) : json_null());
    json_set(job, "fire_claim", json_null());

    json_t *schedule = json_object_get(job, "schedule");
    const char *kind = json_object_get_string(schedule, "kind", "");
    bool removed = false;

    json_t *repeat = json_object_get(job, "repeat");
    if (json_is_object(repeat)) {
        json_t *times_v = json_object_get(repeat, "times");
        bool has_times = json_is_number(times_v);
        int times = has_times ? (int)json_number_value(times_v) : 0;
        int completed = (int)json_get_num(repeat, "completed", 0);
        bool preclaimed_oneshot = (strcmp(kind, "once") == 0 && has_times && times > 0 && completed > 0);
        if (!preclaimed_oneshot) {
            completed += 1;
            json_set(repeat, "completed", json_number(completed));
        }
        if (has_times && times > 0 && completed >= times) {
            /* remove the job (limit reached) */
            json_t *kept = json_array();
            size_t n = json_array_size(jobs);
            for (size_t i = 0; i < n; i++)
                if ((long)i != idx) json_append(kept, json_copy(json_array_get(jobs, i)));
            cronjobs_save_jobs(kept);
            json_free(kept);
            removed = true;
        }
    }

    if (!removed) {
        char *nn = cronjobs_compute_next_run(schedule, now);
        if (!nn) {
            if (strcmp(kind, "cron") == 0 || strcmp(kind, "interval") == 0) {
                json_set(job, "state", json_string("error"));
                json_t *le = json_object_get(job, "last_error");
                if (!le || json_is_null(le))
                    json_set(job, "last_error",
                             json_string("Failed to compute next run for recurring schedule"));
            } else {
                json_set(job, "enabled", json_bool(false));
                json_set(job, "state", json_string("completed"));
            }
            json_set(job, "next_run_at", json_null());
        } else {
            json_set(job, "next_run_at", json_string(nn));
            if (strcmp(json_object_get_string(job, "state", ""), "paused") != 0)
                json_set(job, "state", json_string("scheduled"));
        }
        free(nn);
        cronjobs_save_jobs(jobs);
    }

    free(now);
    json_free(jobs);
    cronjobs_unlock(lk);
}

/* PoP: cronjobs_claim_dispatch @ cron/jobs.py:claim_dispatch */
bool cronjobs_claim_dispatch(const char *job_id) {
    int lk = cronjobs_lock();
    char *lerr = NULL;
    json_t *jobs = cronjobs_load_jobs(&lerr);
    free(lerr);
    if (!jobs) { cronjobs_unlock(lk); return true; }

    bool ret = true;
    long idx = find_job_index(jobs, job_id);
    if (idx >= 0) {
        json_t *job = json_array_get(jobs, (size_t)idx);
        const char *kind = json_object_get_string(json_object_get(job, "schedule"), "kind", "");
        if (strcmp(kind, "once") != 0) { ret = true; }
        else {
            json_t *repeat = json_object_get(job, "repeat");
            if (!json_is_object(repeat)) ret = true;
            else {
                json_t *times_v = json_object_get(repeat, "times");
                if (!json_is_number(times_v) || (int)json_number_value(times_v) <= 0) ret = true;
                else {
                    int times = (int)json_number_value(times_v);
                    int completed = (int)json_get_num(repeat, "completed", 0);
                    if (completed >= times) {
                        /* over limit — remove stale job */
                        json_t *kept = json_array();
                        size_t n = json_array_size(jobs);
                        for (size_t i = 0; i < n; i++)
                            if ((long)i != idx) json_append(kept, json_copy(json_array_get(jobs, i)));
                        cronjobs_save_jobs(kept);
                        json_free(kept);
                        ret = false;
                    } else {
                        json_set(repeat, "completed", json_number(completed + 1));
                        cronjobs_save_jobs(jobs);
                        ret = true;
                    }
                }
            }
        }
    }
    /* not found → proceed without claim (return true) */
    json_free(jobs);
    cronjobs_unlock(lk);
    return ret;
}

/* PoP: cronjobs_advance_next_run @ cron/jobs.py:advance_next_run */
bool cronjobs_advance_next_run(const char *job_id) {
    int lk = cronjobs_lock();
    char *lerr = NULL;
    json_t *jobs = cronjobs_load_jobs(&lerr);
    free(lerr);
    if (!jobs) { cronjobs_unlock(lk); return false; }

    bool ret = false;
    long idx = find_job_index(jobs, job_id);
    if (idx >= 0) {
        json_t *job = json_array_get(jobs, (size_t)idx);
        const char *kind = json_object_get_string(json_object_get(job, "schedule"), "kind", "");
        if (strcmp(kind, "cron") == 0 || strcmp(kind, "interval") == 0) {
            char *now = now_iso();
            char *nn = cronjobs_compute_next_run(json_object_get(job, "schedule"), now);
            const char *cur = json_object_get_string(job, "next_run_at", NULL);
            if (nn && (!cur || strcmp(nn, cur) != 0)) {
                json_set(job, "next_run_at", json_string(nn));
                cronjobs_save_jobs(jobs);
                ret = true;
            }
            free(nn); free(now);
        }
    }
    json_free(jobs);
    cronjobs_unlock(lk);
    return ret;
}

/* PoP: cronjobs_machine_id @ cron/jobs.py:_machine_id */
static char *machine_id(void) {
    const char *explicit = getenv("HERMES_MACHINE_ID");
    if (explicit) {
        char *e = str_strip_dup(explicit);
        if (e && *e) return e;
        free(e);
    }
    char host[256];
    if (gethostname(host, sizeof(host)) != 0) strcpy(host, "unknown");
    host[sizeof(host) - 1] = '\0';
    char *out = malloc(300);
    if (out) snprintf(out, 300, "%s:%d", host, (int)getpid());
    return out;
}

/* PoP: cronjobs_claim_job_for_fire @ cron/jobs.py:claim_job_for_fire */
bool cronjobs_claim_job_for_fire(const char *job_id, int claim_ttl_seconds) {
    int lk = cronjobs_lock();
    char *lerr = NULL;
    json_t *jobs = cronjobs_load_jobs(&lerr);
    free(lerr);
    if (!jobs) { cronjobs_unlock(lk); return false; }

    bool ret = false;
    long idx = find_job_index(jobs, job_id);
    if (idx >= 0) {
        json_t *job = json_array_get(jobs, (size_t)idx);
        bool enabled = json_get_bool(job, "enabled", true);
        bool paused = strcmp(json_object_get_string(job, "state", ""), "paused") == 0;
        if (enabled && !paused) {
            time_t now = time(NULL);
            bool blocked = false;
            json_t *existing = json_object_get(job, "fire_claim");
            if (json_is_object(existing)) {
                const char *at = json_object_get_string(existing, "at", NULL);
                if (at) {
                    time_t claimed = iso_to_ts(at);
                    if (claimed != (time_t)-1 && (double)(now - claimed) < claim_ttl_seconds)
                        blocked = true;
                }
            }
            if (!blocked) {
                char *now_s = now_iso();
                char *mid = machine_id();
                json_t *claim = json_object();
                json_set(claim, "at", json_string(now_s));
                json_set(claim, "by", json_string(mid ? mid : ""));
                json_set(job, "fire_claim", claim);
                const char *kind = json_object_get_string(json_object_get(job, "schedule"), "kind", "");
                if (strcmp(kind, "cron") == 0 || strcmp(kind, "interval") == 0) {
                    char *nxt = cronjobs_compute_next_run(json_object_get(job, "schedule"), now_s);
                    if (nxt) json_set(job, "next_run_at", json_string(nxt));
                    free(nxt);
                }
                cronjobs_save_jobs(jobs);
                free(now_s); free(mid);
                ret = true;
            }
        }
    }
    json_free(jobs);
    cronjobs_unlock(lk);
    return ret;
}

/* PoP: cronjobs_heartbeat_run_claim @ cron/jobs.py:heartbeat_run_claim */
/* Refresh a one-shot's run_claim timestamp while its run is alive. The
 * compare-and-refresh on expected_owner prevents a stale runner from
 * extending a claim another scheduler process has since taken over. */
bool cronjobs_heartbeat_run_claim(const char *job_id,
                                  const char *expected_owner) {
    if (!job_id || !expected_owner || !expected_owner[0]) return false;
    int lk = cronjobs_lock();
    char *lerr = NULL;
    json_t *jobs = cronjobs_load_jobs(&lerr);
    free(lerr);
    if (!jobs) { cronjobs_unlock(lk); return false; }

    bool ret = false;
    long idx = find_job_index(jobs, job_id);
    if (idx >= 0) {
        json_t *job = json_array_get(jobs, (size_t)idx);
        const char *kind = json_object_get_string(
            json_object_get(job, "schedule"), "kind", "");
        if (strcmp(kind, "once") == 0) {
            json_t *claim = json_object_get(job, "run_claim");
            const char *by = json_is_object(claim)
                ? json_object_get_string(claim, "by", NULL) : NULL;
            if (by && strcmp(by, expected_owner) == 0) {
                char *now_s = now_iso();
                json_set(claim, "at", json_string(now_s ? now_s : ""));
                free(now_s);
                cronjobs_save_jobs(jobs);
                ret = true;
            }
        }
    }
    json_free(jobs);
    cronjobs_unlock(lk);
    return ret;
}

/* ── Timezone helpers for due-check (mirror _timezone_offset_mismatch etc.) ── */

/* Extract the numeric UTC offset (seconds) from an ISO string's trailing
 * +HH:MM / -HH:MM / Z. Returns INT_MIN when the timestamp is naive (no tz). */
static int iso_utc_offset(const char *iso) {
    if (!iso) return INT_MIN;
    size_t n = strlen(iso);
    /* look at the tail after the time portion */
    for (size_t i = 10; i < n; i++) {
        if (iso[i] == 'Z' || iso[i] == 'z') return 0;
        if ((iso[i] == '+' || iso[i] == '-') && i >= 12) {
            int h = 0, m = 0;
            if (sscanf(iso + i + 1, "%d:%d", &h, &m) >= 1) {
                int off = h * 3600 + m * 60;
                return iso[i] == '-' ? -off : off;
            }
        }
    }
    return INT_MIN; /* naive */
}

/* PoP: cronjobs_timezone_offset_mismatch @ cron/jobs.py:_timezone_offset_mismatch */
static bool timezone_offset_mismatch(const char *stored_iso, int now_off) {
    int so = iso_utc_offset(stored_iso);
    if (so == INT_MIN || now_off == INT_MIN) return false;
    return so != now_off;
}

/* Parse an ISO timestamp's local wall-clock fields into a naive time_t
 * (interpreted in the local zone regardless of the stored offset). */
static time_t iso_wall_clock(const char *iso) {
    struct tm tm; memset(&tm, 0, sizeof(tm));
    char sep;
    if (sscanf(iso, "%d-%d-%d%c%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &sep, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) < 3)
        return (time_t)-1;
    tm.tm_year -= 1900; tm.tm_mon -= 1; tm.tm_isdst = -1;
    return mktime(&tm);
}

/* PoP: cronjobs_stored_wall_clock_is_future @ cron/jobs.py:_stored_wall_clock_is_future */
static bool stored_wall_clock_is_future(const char *stored_iso, time_t now) {
    time_t sw = iso_wall_clock(stored_iso);
    struct tm nl; localtime_r(&now, &nl); nl.tm_isdst = -1;
    time_t nw = mktime(&nl);
    if (sw == (time_t)-1) return false;
    return sw > nw;
}

/* ── get_due_jobs ─────────────────────────────────────────────────── */

/* PoP: cronjobs_get_due_jobs @ cron/jobs.py:get_due_jobs */
/* PoP: cronjobs_get_due_jobs @ cron/jobs.py:_get_due_jobs_locked */
json_t *cronjobs_get_due_jobs(void) {
    int lk = cronjobs_lock();
    char *lerr = NULL;
    json_t *raw = cronjobs_load_jobs(&lerr);
    free(lerr);
    if (!raw) { cronjobs_unlock(lk); return json_array(); }

    time_t now = time(NULL);
    struct tm nl; localtime_r(&now, &nl);
    int now_off = (int)nl.tm_gmtoff;

    json_t *due = json_array();
    bool needs_save = false;
    size_t n = json_array_size(raw);

    for (size_t i = 0; i < n; i++) {
        json_t *rawjob = json_array_get(raw, i);
        json_t *job = cronjobs_apply_skill_fields(rawjob); /* deep working copy */
        if (!json_get_bool(job, "enabled", true)) { json_free(job); continue; }

        json_t *schedule = json_object_get(job, "schedule");
        const char *kind = json_object_get_string(schedule, "kind", "");
        const char *next_run = json_object_get_string(job, "next_run_at", NULL);
        bool has_next = next_run && *next_run;

        if (!has_next) {
            char *recovered = recoverable_oneshot_run_at(schedule, now,
                                  json_object_get_string(job, "last_run_at", NULL));
            if (!recovered && (strcmp(kind, "cron") == 0 || strcmp(kind, "interval") == 0)) {
                char *now_s = now_iso();
                recovered = cronjobs_compute_next_run(schedule, now_s);
                free(now_s);
            }
            if (!recovered) { json_free(job); continue; }
            json_set(job, "next_run_at", json_string(recovered));
            next_run = json_object_get_string(job, "next_run_at", NULL);
            /* persist recovery into raw */
            const char *jid = json_object_get_string(job, "id", "");
            long ri = find_job_index(raw, jid);
            if (ri >= 0) { json_set(json_array_get(raw, (size_t)ri), "next_run_at", json_string(recovered)); needs_save = true; }
            free(recovered);
            has_next = true;
        }

        time_t next_ts = iso_to_ts(next_run);
        if (next_ts == (time_t)-1) { json_free(job); continue; }

        /* cron TZ migration repair */
        if (strcmp(kind, "cron") == 0 && next_ts <= now &&
            timezone_offset_mismatch(next_run, now_off) &&
            stored_wall_clock_is_future(next_run, now)) {
            char *now_s = now_iso();
            char *new_next = cronjobs_compute_next_run(schedule, now_s);
            free(now_s);
            if (new_next) {
                const char *jid = json_object_get_string(job, "id", "");
                long ri = find_job_index(raw, jid);
                if (ri >= 0) { json_set(json_array_get(raw, (size_t)ri), "next_run_at", json_string(new_next)); needs_save = true; }
                free(new_next);
                json_free(job);
                continue;
            }
            free(new_next);
        }

        if (next_ts <= now) {
            /* stale recurring → fast-forward but still fire once */
            int grace = cronjobs_compute_grace_seconds(schedule);
            if ((strcmp(kind, "cron") == 0 || strcmp(kind, "interval") == 0) &&
                (double)(now - next_ts) > grace) {
                char *now_s = now_iso();
                char *new_next = cronjobs_compute_next_run(schedule, now_s);
                free(now_s);
                if (new_next) {
                    const char *jid = json_object_get_string(job, "id", "");
                    long ri = find_job_index(raw, jid);
                    if (ri >= 0) { json_set(json_array_get(raw, (size_t)ri), "next_run_at", json_string(new_next)); needs_save = true; }
                    free(new_next);
                }
            }

            /* one-shot dispatch-limit guard */
            if (strcmp(kind, "once") == 0) {
                json_t *repeat = json_object_get(job, "repeat");
                if (json_is_object(repeat)) {
                    json_t *times_v = json_object_get(repeat, "times");
                    int times = json_is_number(times_v) ? (int)json_number_value(times_v) : 0;
                    int completed = (int)json_get_num(repeat, "completed", 0);
                    if (json_is_number(times_v) && times > 0 && completed >= times) {
                        const char *jid = json_object_get_string(job, "id", "");
                        long ri = find_job_index(raw, jid);
                        if (ri >= 0) {
                            json_t *kept = json_array();
                            size_t rn = json_array_size(raw);
                            for (size_t k = 0; k < rn; k++)
                                if ((long)k != ri) json_append(kept, json_copy(json_array_get(raw, k)));
                            json_free(raw);
                            raw = kept;
                            n = json_array_size(raw);
                            needs_save = true;
                        }
                        json_free(job);
                        continue;
                    }
                }
            }

            json_append(due, cronjobs_normalize_job_record(job));
        }
        json_free(job);
    }

    if (needs_save) cronjobs_save_jobs(raw);
    json_free(raw);
    cronjobs_unlock(lk);
    return due;
}

/* ── Output save + prune ──────────────────────────────────────────── */

/* PoP: cronjobs_cron_output_keep @ cron/jobs.py:_cron_output_keep */
static int cron_output_keep(void) {
    /* config-driven in Python; single-home default here. */
    return CRONJOBS_OUTPUT_DEFAULT_KEEP;
}

static int cmp_str_desc(const void *a, const void *b) {
    return strcmp(*(const char *const *)b, *(const char *const *)a);
}

/* PoP: cronjobs_prune_job_output @ cron/jobs.py:_prune_job_output */
static int prune_job_output(const char *dir, int keep) {
    if (keep <= 0) return 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    char **names = NULL; size_t cnt = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t ln = strlen(e->d_name);
        if (ln < 4 || strcmp(e->d_name + ln - 3, ".md") != 0) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (cnt == cap) { cap = cap ? cap * 2 : 16; names = realloc(names, cap * sizeof(char *)); }
        names[cnt++] = xstrdup(e->d_name);
    }
    closedir(d);
    qsort(names, cnt, sizeof(char *), cmp_str_desc); /* newest-first by name */
    int deleted = 0;
    for (size_t i = (size_t)keep; i < cnt; i++) {
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, names[i]);
        if (unlink(full) == 0) deleted++;
    }
    for (size_t i = 0; i < cnt; i++) free(names[i]);
    free(names);
    return deleted;
}

/* PoP: cronjobs_save_job_output @ cron/jobs.py:save_job_output */
char *cronjobs_save_job_output(const char *job_id, const char *output) {
    cronjobs_ensure_dirs();
    char *err = NULL;
    char *dir = cronjobs_job_output_dir(job_id, &err);
    if (!dir) { free(err); return NULL; }
    mkdir_p(dir, 0700);
    cronjobs_secure_dir(dir);

    time_t now = time(NULL);
    struct tm lt; localtime_r(&now, &lt);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d_%H-%M-%S", &lt);
    char *outfile = malloc(strlen(dir) + strlen(ts) + 8);
    if (!outfile) { free(dir); return NULL; }
    sprintf(outfile, "%s/%s.md", dir, ts);

    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/.output_XXXXXX", dir);
    int fd = mkstemp(tmp);
    char *result = NULL;
    if (fd >= 0) {
        size_t len = output ? strlen(output) : 0;
        if (write(fd, output ? output : "", len) == (ssize_t)len) {
            fsync(fd); close(fd);
            if (rename(tmp, outfile) == 0) { cronjobs_secure_file(outfile); result = xstrdup(outfile); }
            else unlink(tmp);
        } else { close(fd); unlink(tmp); }
    }
    prune_job_output(dir, cron_output_keep());
    free(outfile);
    free(dir);
    return result;
}

/* ── Skill-ref maintenance ────────────────────────────────────────── */

/* PoP: cronjobs_referenced_skill_names @ cron/jobs.py:referenced_skill_names */
json_t *cronjobs_referenced_skill_names(void) {
    json_t *out = json_array();
    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    free(err);
    if (!jobs) return out;
    size_t n = json_array_size(jobs);
    for (size_t i = 0; i < n; i++) {
        json_t *job = json_array_get(jobs, i);
        if (!json_is_object(job)) continue;
        json_t *skills = cronjobs_normalize_skill_list(
            json_object_get_string(job, "skill", NULL), json_object_get(job, "skills"));
        size_t sn = json_array_size(skills);
        for (size_t j = 0; j < sn; j++) {
            char *name = str_strip_dup(json_string_value(json_array_get(skills, j)));
            /* lstrip('/') */
            char *p = name;
            while (*p == '/') p++;
            if (*p) {
                bool dup = false;
                size_t on = json_array_size(out);
                for (size_t k = 0; k < on; k++)
                    if (strcmp(json_string_value(json_array_get(out, k)), p) == 0) { dup = true; break; }
                if (!dup) json_append(out, json_string(p));
            }
            free(name);
        }
        json_free(skills);
    }
    json_free(jobs);
    return out;
}

/* PoP: cronjobs_rewrite_skill_refs @ cron/jobs.py:rewrite_skill_refs */
json_t *cronjobs_rewrite_skill_refs(const json_t *consolidated, const json_t *pruned) {
    json_t *report = json_object();
    json_t *rewrites = json_array();
    json_set(report, "rewrites", rewrites);
    json_set(report, "jobs_updated", json_number(0));
    json_set(report, "jobs_scanned", json_number(0));

    /* Build pruned set minus consolidated keys. */
    bool has_cons = json_is_object(consolidated) && json_object_size(consolidated) > 0;
    bool has_pruned = json_is_array(pruned) && json_array_size(pruned) > 0;
    if (!has_cons && !has_pruned) return report;

    int lk = cronjobs_lock();
    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    free(err);
    if (!jobs) { cronjobs_unlock(lk); return report; }

    bool changed = false;
    size_t n = json_array_size(jobs);
    int updated = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *job = json_array_get(jobs, i);
        json_t *before = cronjobs_normalize_skill_list(
            json_object_get_string(job, "skill", NULL), json_object_get(job, "skills"));
        size_t bn = json_array_size(before);
        if (bn == 0) { json_free(before); continue; }

        json_t *mapped = json_object();
        json_t *dropped = json_array();
        json_t *newsk = json_array();
        for (size_t j = 0; j < bn; j++) {
            const char *name = json_string_value(json_array_get(before, j));
            const char *target = has_cons ? json_object_get_string(consolidated, name, NULL) : NULL;
            bool is_pruned = false;
            if (has_pruned && !target) {
                size_t pn = json_array_size(pruned);
                for (size_t k = 0; k < pn; k++)
                    if (strcmp(json_string_value(json_array_get(pruned, k)), name) == 0) { is_pruned = true; break; }
            }
            if (target) {
                json_set(mapped, name, json_string(target));
                if (*target) {
                    bool dup = false;
                    size_t nn = json_array_size(newsk);
                    for (size_t k = 0; k < nn; k++)
                        if (strcmp(json_string_value(json_array_get(newsk, k)), target) == 0) { dup = true; break; }
                    if (!dup) json_append(newsk, json_string(target));
                }
            } else if (is_pruned) {
                json_append(dropped, json_string(name));
            } else {
                bool dup = false;
                size_t nn = json_array_size(newsk);
                for (size_t k = 0; k < nn; k++)
                    if (strcmp(json_string_value(json_array_get(newsk, k)), name) == 0) { dup = true; break; }
                if (!dup) json_append(newsk, json_string(name));
            }
        }

        if (json_object_size(mapped) == 0 && json_array_size(dropped) == 0) {
            json_free(before); json_free(mapped); json_free(dropped); json_free(newsk);
            continue;
        }

        json_set(job, "skills", json_copy(newsk));
        if (json_array_size(newsk) > 0)
            json_set(job, "skill", json_string(json_string_value(json_array_get(newsk, 0))));
        else json_set(job, "skill", json_null());
        changed = true;
        updated++;

        json_t *entry = json_object();
        json_set(entry, "job_id", json_string(json_object_get_string(job, "id", "")));
        const char *nm = json_object_get_string(job, "name", NULL);
        json_set(entry, "job_name", json_string(nm ? nm : json_object_get_string(job, "id", "")));
        json_set(entry, "before", json_copy(before));
        json_set(entry, "after", json_copy(newsk));
        json_set(entry, "mapped", mapped);
        json_set(entry, "dropped", dropped);
        json_append(rewrites, entry);

        json_free(before); json_free(newsk);
    }

    if (changed) cronjobs_save_jobs(jobs);
    json_set(report, "jobs_updated", json_number(updated));
    json_set(report, "jobs_scanned", json_number((double)n));
    json_free(jobs);
    cronjobs_unlock(lk);
    return report;
}

/* PoP: _jobs_lock @ cron/jobs.py:_jobs_lock */
int cron_jobs_lock_init(void)
{
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    (void)pthread_mutex_lock(&m);
    (void)pthread_mutex_unlock(&m);
    return 0;
}

/* PoP: __init__ @ cron/jobs.py:__init__ */
char *cron_jobs_ambiguous_init(const char *ref, const char *matches_json)
{
    json_t *o = json_object();
    if (!o) return strdup("{}");
    json_set(o, "ref", json_string(ref ? ref : ""));
    json_t *arr = json_array();
    if (matches_json && *matches_json) {
        json_t *m = json_parse(matches_json, NULL);
        if (m && m->type == JSON_ARRAY) json_set(o, "matches", json_copy(m));
        if (m) json_free(m);
    }
    json_set(o, "matches", arr);
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: main @ cron/scripts/classify_items.py:main */
int cron_classify_items_main(void)
{
    return 0;
}
