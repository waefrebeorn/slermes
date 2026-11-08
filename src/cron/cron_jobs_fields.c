/*
 * cron_jobs_fields.c - cron job-record field normalisers.
 *
 * Split from port_cron_jobs.c (soul.md: self-contained modules). Pure
 * per-field coercion of a job record; no file-scope state.
 */
#include "cron_jobs_fields.h"
#include "cron_jobs.h"
#include "hermes_strutil.h"
#include "hermes_fs.h"
#include "slermes_home.h"
#include "datetime.h"
#include "cron.h"

#include "cron_jobs_paths.h"   /* xstrdup, set_err */

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
#include "uuid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



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
