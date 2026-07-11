/*
 * cron_suggestions.c — Port of Python: cron/suggestions.py
 *
 * Suggested cron jobs: a JSON file store (~/.hermes/cron/suggestions.json)
 * mirroring cron/jobs.py. A suggestion is a ready-to-run cron job spec the
 * user accepts (creates the real cron job via cron_add_job) or dismisses
 * (latched by dedup_key so it is never re-offered).
 *
 * Faithful 1:1 port. Storage: {"suggestions": [...], "updated_at": <iso>},
 * atomic write + 0600 perms, in-process mutex protecting load->modify->save.
 *
 * Ownership note (libjson uses aliasing, no refcount): json_set/json_append
 * STEAL the child pointer; json_get/json_obj_get return BORROWED refs. So any
 * value handed to json_set must be freshly built (or json_copy'd), and any
 * value returned to a caller must be json_copy'd before the parent is freed.
 *
 * PoP mappings:
 *  _secure_file              -> cron_sugg_secure_file
 *  _ensure_dir               -> cron_sugg_ensure_dir
 *  _load_raw                 -> cron_sugg_load_raw
 *  _save_raw                 -> cron_sugg_save_raw
 *  load_suggestions          -> cron_sugg_load_suggestions
 *  list_pending              -> cron_sugg_list_pending
 *  add_suggestion            -> cron_sugg_add
 *  get_suggestion            -> cron_sugg_get
 *  _set_status               -> cron_sugg_set_status
 *  dismiss_suggestion        -> cron_sugg_dismiss
 *  accept_suggestion         -> cron_sugg_accept
 *  clear_resolved            -> cron_sugg_clear_resolved
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include "uuid.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <libgen.h>
#include <strings.h>

/* ---- constants (mirror cron/suggestions.py) ---- */
#define CRON_SUGG_MAX_PENDING 5
#define CRON_SUGG_STATUS_PENDING "pending"
#define CRON_SUGG_STATUS_ACCEPTED "accepted"
#define CRON_SUGG_STATUS_DISMISSED "dismissed"

/* Forward decl for the job-creation integration (defined in scheduler.c). */
extern bool cron_add_job(const char *name, const char *schedule_expr,
                         const char *command);

/* ---- in-process lock (mirror threading.Lock) ---- */
static pthread_mutex_t g_sugg_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---- path helpers ---- */
static void sugg_dir(char *buf, size_t sz)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    snprintf(buf, sz, "%s/cron", home);
}

static void sugg_path(char *buf, size_t sz)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    snprintf(buf, sz, "%s/cron/suggestions.json", home);
}

static void now_iso(char *buf, size_t sz)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, sz, "%Y-%m-%dT%H:%M:%S", &tm);
}

/* PoP: cron_sugg_secure_file @ cron/suggestions.py:_secure_file */
void cron_sugg_secure_file(const char *path)
{
    chmod(path, 0600); /* ignore any OSError like Python */
}

/* PoP: cron_sugg_ensure_dir @ cron/suggestions.py:_ensure_dir */
void cron_sugg_ensure_dir(void)
{
    char dir[4096];
    sugg_dir(dir, sizeof(dir));
    mkdir(dir, 0755); /* Path.mkdir(parents=True, exist_ok=True) */
}

/* Build an empty {"suggestions":[]} store. Caller frees. */
static json_t *sugg_empty(void)
{
    json_t *e = json_object();
    json_set(e, "suggestions", json_array());
    return e;
}

/* PoP: cron_sugg_load_raw @ cron/suggestions.py:_load_raw */
json_t *cron_sugg_load_raw(void)
{
    char path[4096];
    sugg_path(path, sizeof(path));

    if (access(path, F_OK) != 0)
        return sugg_empty();

    char *err = NULL;
    json_t *data = json_parse_file(path, &err);
    if (err) free(err);
    if (!data) {
        hermes_log(LOG_WARNING, "cron_sugg",
                   "suggestions.json unreadable; starting empty");
        return sugg_empty();
    }
    /* dict with "suggestions" list -> return as-is */
    json_t *sugg = json_obj_get(data, "suggestions");
    if (json_is_object(data) && json_is_array(sugg))
        return data;
    /* a bare list at top level -> wrap (data now aliased into wrapper) */
    if (json_is_array(data)) {
        json_t *wrapped = json_object();
        json_set(wrapped, "suggestions", data); /* steals data */
        return wrapped;
    }
    /* malformed -> empty */
    hermes_log(LOG_WARNING, "cron_sugg",
               "suggestions.json malformed; starting empty");
    json_free(data);
    return sugg_empty();
}

/* PoP: cron_sugg_save_raw @ cron/suggestions.py:_save_raw */
/* Copies the list so the caller's list is NOT consumed/freed. Returns true on
 * success. */
bool cron_sugg_save_raw(json_t *suggestions_list)
{
    if (!suggestions_list) return false;
    cron_sugg_ensure_dir();

    char path[4096];
    sugg_path(path, sizeof(path));
    char tmp[4200];
    snprintf(tmp, sizeof(tmp), "%s/cron/.sugg_XXXXXX",
             getenv("HERMES_HOME") ? getenv("HERMES_HOME") : "/tmp/.hermes");
    int fd = mkstemp(tmp);
    if (fd < 0) return false;

    json_t *doc = json_object();
    json_set(doc, "suggestions", json_copy(suggestions_list)); /* own copy */
    char now[64];
    now_iso(now, sizeof(now));
    json_set(doc, "updated_at", json_string(now));

    char *serialized = json_serialize_pretty(doc, 2);
    json_free(doc); /* frees the copy too */

    bool ok = true;
    size_t len = strlen(serialized);
    ssize_t written = 0;
    while (written < (ssize_t)len) {
        ssize_t w = write(fd, serialized + written, len - written);
        if (w < 0) { ok = false; break; }
        written += w;
    }
    fsync(fd);
    close(fd);
    free(serialized);

    if (!ok) { unlink(tmp); return false; }
    if (rename(tmp, path) != 0) { unlink(tmp); return false; }
    cron_sugg_secure_file(path);
    return true;
}

/* PoP: cron_sugg_load_suggestions @ cron/suggestions.py:load_suggestions */
json_t *cron_sugg_load_suggestions(void)
{
    json_t *raw = cron_sugg_load_raw();
    json_t *arr = json_obj_get(raw, "suggestions");
    json_t *copy = arr ? json_copy(arr) : json_array();
    json_free(raw);
    return copy;
}

/* PoP: cron_sugg_list_pending @ cron/suggestions.py:list_pending */
json_t *cron_sugg_list_pending(void)
{
    json_t *all = cron_sugg_load_suggestions();
    json_t *pending = json_array();
    size_t n = json_len(all);
    for (size_t i = 0; i < n; i++) {
        json_t *s = json_get(all, i);
        if (s && strcmp(json_get_str(s, "status", ""), CRON_SUGG_STATUS_PENDING) == 0)
            json_append(pending, json_copy(s));
    }
    json_free(all);
    return pending;
}

static bool sugg_valid_source(const char *source)
{
    static const char *valid[] = { "catalog", "blueprint", "usage", "integration" };
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
        if (strcmp(source, valid[i]) == 0) return true;
    return false;
}

/* PoP: cron_sugg_add @ cron/suggestions.py:add_suggestion */
json_t *cron_sugg_add(const char *title, const char *description,
                       const char *source, json_t *job_spec,
                       const char *dedup_key)
{
    if (!sugg_valid_source(source)) {
        hermes_log(LOG_ERROR, "cron_sugg", "unknown suggestion source: %s",
                   source ? source : "");
        return NULL;
    }
    if (!title || !title[0] || !dedup_key || !dedup_key[0]) {
        hermes_log(LOG_ERROR, "cron_sugg", "title and dedup_key are required");
        return NULL;
    }

    pthread_mutex_lock(&g_sugg_lock);

    json_t *raw = cron_sugg_load_raw();
    json_t *list = json_obj_get(raw, "suggestions");
    if (!list) { json_free(raw); pthread_mutex_unlock(&g_sugg_lock); return NULL; }

    /* never re-offer a dedup_key already resolved or still pending */
    bool skip = false;
    size_t n = json_len(list);
    for (size_t i = 0; i < n && !skip; i++) {
        json_t *e = json_get(list, i);
        if (e && strcmp(json_get_str(e, "dedup_key", ""), dedup_key) == 0)
            skip = true;
    }
    if (skip) { json_free(raw); pthread_mutex_unlock(&g_sugg_lock); return NULL; }

    /* cap pending */
    int pending_count = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(list, i);
        if (e && strcmp(json_get_str(e, "status", ""), CRON_SUGG_STATUS_PENDING) == 0)
            pending_count++;
    }
    if (pending_count >= CRON_SUGG_MAX_PENDING) {
        hermes_log(LOG_INFO, "cron_sugg", "Suggestion backlog full; dropping %s",
                   title);
        json_free(raw);
        pthread_mutex_unlock(&g_sugg_lock);
        return NULL;
    }

    /* build record */
    json_t *rec = json_object();
    char *id = uuid_v4();
    char now[64];
    now_iso(now, sizeof(now));
    json_set(rec, "id", json_string(id ? id : ""));
    json_set(rec, "title", json_string(title));
    json_set(rec, "description", json_string(description ? description : ""));
    json_set(rec, "source", json_string(source));
    json_set(rec, "job_spec", job_spec ? json_copy(job_spec) : json_object());
    json_set(rec, "dedup_key", json_string(dedup_key));
    json_set(rec, "status", json_string(CRON_SUGG_STATUS_PENDING));
    json_set(rec, "created_at", json_string(now));
    free(id);

    json_append(list, rec); /* list now owns rec */
    bool saved = cron_sugg_save_raw(list);
    /* Copy the record for return BEFORE freeing raw (raw owns rec). */
    json_t *ret = saved ? json_copy(rec) : NULL;
    json_free(raw); /* frees rec (appended into list, owned by raw) */
    pthread_mutex_unlock(&g_sugg_lock);
    return ret;    /* caller frees */
}

/* PoP: cron_sugg_get @ cron/suggestions.py:get_suggestion */
json_t *cron_sugg_get(const char *ref)
{
    if (!ref) return NULL;
    json_t *all = cron_sugg_load_suggestions();
    json_t *found = NULL;

    size_t n = json_len(all);
    for (size_t i = 0; i < n && !found; i++) {
        json_t *s = json_get(all, i);
        if (s && strcmp(json_get_str(s, "id", ""), ref) == 0) found = s;
    }
    if (!found && ref[0] >= '0' && ref[0] <= '9') {
        json_t *pending = json_array();
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(all, i);
            if (s && strcmp(json_get_str(s, "status", ""),
                            CRON_SUGG_STATUS_PENDING) == 0)
                json_append(pending, json_copy(s));
        }
        int idx = atoi(ref) - 1;
        if (idx >= 0 && (size_t)idx < json_len(pending))
            found = json_get(pending, idx);
        /* keep pending alive until we copy found */
        if (found) found = json_copy(found);
        json_free(pending);
    }
    if (!found) {
        for (size_t i = 0; i < n && !found; i++) {
            json_t *s = json_get(all, i);
            if (s && strcasecmp(json_get_str(s, "title", ""), ref) == 0)
                found = s;
        }
    }
    if (found) found = json_copy(found); /* always return an independent copy */
    json_free(all);
    return found;
}

/* PoP: cron_sugg_set_status @ cron/suggestions.py:_set_status */
bool cron_sugg_set_status(const char *suggestion_id, const char *status)
{
    pthread_mutex_lock(&g_sugg_lock);
    json_t *raw = cron_sugg_load_raw();
    json_t *list = json_obj_get(raw, "suggestions");
    bool changed = false;
    if (list) {
        size_t n = json_len(list);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(list, i);
            if (s && strcmp(json_get_str(s, "id", ""), suggestion_id) == 0) {
                json_set(s, "status", json_string(status));
                char now[64];
                now_iso(now, sizeof(now));
                json_set(s, "resolved_at", json_string(now));
                changed = true;
                break;
            }
        }
    }
    if (changed) cron_sugg_save_raw(list);
    json_free(raw);
    pthread_mutex_unlock(&g_sugg_lock);
    return changed;
}

/* PoP: cron_sugg_dismiss @ cron/suggestions.py:dismiss_suggestion */
bool cron_sugg_dismiss(const char *ref)
{
    json_t *s = cron_sugg_get(ref);
    if (!s) return false;
    const char *id = json_get_str(s, "id", "");
    bool r = cron_sugg_set_status(id, CRON_SUGG_STATUS_DISMISSED);
    json_free(s);
    return r;
}

/* PoP: cron_sugg_accept @ cron/suggestions.py:accept_suggestion */
json_t *cron_sugg_accept(const char *ref, json_t *origin)
{
    json_t *s = cron_sugg_get(ref);
    if (!s) return NULL;
    if (strcmp(json_get_str(s, "status", ""), CRON_SUGG_STATUS_PENDING) != 0) {
        json_free(s);
        return NULL;
    }

    json_t *spec = json_obj_get(s, "job_spec");
    json_t *spec_copy = spec ? json_copy(spec) : json_object();
    if (origin && !json_obj_get(spec_copy, "origin"))
        json_set(spec_copy, "origin", json_copy(origin));

    const char *name = json_get_str(spec_copy, "name", NULL);
    const char *schedule = json_get_str(spec_copy, "schedule", NULL);
    const char *command = json_get_str(spec_copy, "prompt", NULL);

    bool created = false;
    if (name && schedule)
        created = cron_add_job(name, schedule, command);

    if (created) {
        const char *id = json_get_str(s, "id", "");
        cron_sugg_set_status(id, CRON_SUGG_STATUS_ACCEPTED);
    }
    json_free(s);

    if (!created) { json_free(spec_copy); return NULL; }
    return spec_copy; /* caller frees; mirrors Python create_job result surface */
}

/* PoP: cron_sugg_clear_resolved @ cron/suggestions.py:clear_resolved */
int cron_sugg_clear_resolved(void)
{
    pthread_mutex_lock(&g_sugg_lock);
    json_t *raw = cron_sugg_load_raw();
    json_t *list = json_obj_get(raw, "suggestions");
    int removed = 0;
    if (list) {
        size_t n = json_len(list);
        json_t *kept = json_array();
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(list, i);
            const char *st = s ? json_get_str(s, "status", "") : "";
            if (strcmp(st, CRON_SUGG_STATUS_ACCEPTED) == 0) {
                removed++;
            } else {
                json_append(kept, json_copy(s));
            }
        }
        json_set(raw, "suggestions", json_copy(kept)); /* raw owns a copy */
        if (removed) cron_sugg_save_raw(json_obj_get(raw, "suggestions"));
        json_free(kept);
    }
    json_free(raw);
    pthread_mutex_unlock(&g_sugg_lock);
    return removed;
}
