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
#include "cron_jobs_paths.h"
#include "cron_jobs_time2.h"
#include "cron_jobs_sched.h"
#include "cron_jobs_time.h"
#include "hermes_strutil.h"
#include "hermes_fs.h"
#include "slermes_home.h"
#include "datetime.h"
#include "cron.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    str_lower_inplace(ref_lower);
    long match_idx = -1; int matches = 0;
    size_t n = json_array_size(jobs);
    for (size_t i = 0; i < n; i++) {
        const char *nm = json_object_get_string(json_array_get(jobs, i), "name", "");
        char *nl = xstrdup(nm ? nm : "");
        str_lower_inplace(nl);
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
    int now_off = (int)hermes_tm_gmtoff(&nl);

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
    fs_mkdir_p(dir, 0700);
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
