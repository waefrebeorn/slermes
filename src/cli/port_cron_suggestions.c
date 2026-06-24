/*
 * port_cron_suggestions.c — C port of cron/suggestions.py
 *
 * Manages a suggestions JSON store (~/.hermes/cron/suggestions.json)
 * with add, get, dismiss, accept, and clear operations.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>


#define MAX_PENDING 5
#define VALID_SOURCES_COUNT 4

static const char *valid_sources[VALID_SOURCES_COUNT] = {
    "catalog", "blueprint", "usage", "integration"
};

#define STATUS_PENDING   "pending"
#define STATUS_ACCEPTED  "accepted"
#define STATUS_DISMISSED "dismissed"

/*
 * _secure_file: Set file permissions to 0600.
 * p1 = file path string.
 */
/* PoP: cli_cron_suggestions__secure_file @ cron/suggestions.py:_secure_file */
void* cli_cron_suggestions__secure_file(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;

    const char *path = (const char *)p1;
    if (!path || !path[0]) return (void *)(intptr_t)(-1);

#ifdef _WIN32
    /* Windows: chmod not applicable in the same way */
    hermes_log(LOG_DEBUG, "port",
               "secure_file: skipping chmod on Windows for %s", path);
#else
    if (chmod(path, 0600) != 0) {
        hermes_log(LOG_WARNING, "port",
                   "secure_file: chmod failed for %s: %s", path, strerror(errno));
        /* Non-fatal: continue */
    } else {
        hermes_log(LOG_DEBUG, "port",
                   "secure_file: set 0600 on %s", path);
    }
#endif

    return (void *)(intptr_t)0;
}

/*
 * _load_raw: Load the suggestions JSON file.
 * Returns: pointer to allocated JSON string (caller frees), or NULL.
 */
/* PoP: cli_cron_suggestions__load_raw @ cron/suggestions.py:_load_raw */
void* cli_cron_suggestions__load_raw(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char path[HERMES_PATH_MAX];
    snprintf(path, sizeof(path), "%s/.hermes/cron/suggestions.json", home);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        hermes_log(LOG_DEBUG, "port",
                   "load_raw: file not found, returning empty");
        return strdup("{\"suggestions\":[]}");
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(fp);
        return strdup("{\"suggestions\":[]}");
    }

    char *data = malloc(fsize + 1);
    if (!data) {
        fclose(fp);
        return strdup("{\"suggestions\":[]}");
    }

    size_t nread = fread(data, 1, fsize, fp);
    data[nread] = '\0';
    fclose(fp);

    /* Validate: must be a JSON object or array */
    char *p = data;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p != '{' && *p != '[') {
        hermes_log(LOG_WARNING, "port",
                   "load_raw: malformed JSON (starts with '%c'), returning empty", *p);
        free(data);
        return strdup("{\"suggestions\":[]}");
    }

    /* If it's a bare array, wrap it */
    if (*p == '[') {
        char *wrapped = malloc(nread + 32);
        if (wrapped) {
            snprintf(wrapped, nread + 32,
                     "{\"suggestions\":%s}", data);
            free(data);
            return wrapped;
        }
    }

    hermes_log(LOG_DEBUG, "port",
               "load_raw: loaded %zu bytes from %s", strlen(data), path);

    return data;
}

/*
 * _save_raw: Save suggestions data to JSON file atomically.
 * p1 = JSON string to save.
 * p2 = file path string (NULL = default).
 */
/* PoP: cli_cron_suggestions__save_raw @ cron/suggestions.py:_save_raw */
void* cli_cron_suggestions__save_raw(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    const char *data = (const char *)p1;
    const char *path = (const char *)p2;

    if (!data) return (void *)(intptr_t)(-1);

    char target[HERMES_PATH_MAX];
    if (path && path[0]) {
        strncpy(target, path, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
    } else {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(target, sizeof(target), "%s/.hermes/cron/suggestions.json", home);
    }

    /* Ensure directory exists */
    char dir[HERMES_PATH_MAX];
    strncpy(dir, target, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        char mkdir_cmd[HERMES_PATH_MAX + 20];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s 2>/dev/null", dir);
        system(mkdir_cmd);
    }

    /* Write to temp file, then rename (atomic on POSIX) */
    char tmp_path[HERMES_PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", target);

    FILE *fp = fopen(tmp_path, "w");
    if (!fp) {
        hermes_log(LOG_ERROR, "port",
                   "save_raw: cannot open temp file %s: %s", tmp_path, strerror(errno));
        return (void *)(intptr_t)(-1);
    }

    size_t data_len = strlen(data);
    size_t written = fwrite(data, 1, data_len, fp);
    fflush(fp);
    int fd = fileno(fp);
    if (fd >= 0) {
#ifdef _WIN32
        _commit(fd);
#else
        fsync(fd);
#endif
    }
    fclose(fp);

    if (written != data_len) {
        hermes_log(LOG_ERROR, "port",
                   "save_raw: short write (%zu/%zu)", written, data_len);
        remove(tmp_path);
        return (void *)(intptr_t)(-1);
    }

    /* Atomic rename */
    if (rename(tmp_path, target) != 0) {
        hermes_log(LOG_ERROR, "port",
                   "save_raw: rename failed: %s", strerror(errno));
        remove(tmp_path);
        return (void *)(intptr_t)(-1);
    }

    /* Set permissions */
    cli_cron_suggestions__secure_file((void *)target, NULL, NULL, NULL, NULL);

    hermes_log(LOG_DEBUG, "port",
               "save_raw: saved %zu bytes to %s", data_len, target);

    return (void *)(intptr_t)0;
}

/*
 * load_suggestions: Return all suggestion records.
 * Returns: pointer to JSON string of suggestions array.
 */
/* PoP: cli_cron_suggestions_load_suggestions @ cron/suggestions.py:load_suggestions */
void* cli_cron_suggestions_load_suggestions(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    char *raw = (char *)cli_cron_suggestions__load_raw(NULL, NULL, NULL, NULL, NULL);
    if (!raw) return strdup("[]");

    /* Extract the suggestions array from {"suggestions":[...]} */
    char *arr_start = strstr(raw, "\"suggestions\"");
    if (!arr_start) {
        free(raw);
        return strdup("[]");
    }

    arr_start = strchr(arr_start, '[');
    if (!arr_start) {
        free(raw);
        return strdup("[]");
    }

    /* Find matching ] */
    int depth = 0;
    char *arr_end = arr_start;
    do {
        if (*arr_end == '[') depth++;
        else if (*arr_end == ']') depth--;
        arr_end++;
    } while (*arr_end && depth > 0);

    if (depth != 0) {
        free(raw);
        return strdup("[]");
    }

    size_t arr_len = arr_end - arr_start;
    char *result = malloc(arr_len + 1);
    if (!result) {
        free(raw);
        return strdup("[]");
    }

    memcpy(result, arr_start, arr_len);
    result[arr_len] = '\0';
    free(raw);

    hermes_log(LOG_DEBUG, "port",
               "load_suggestions: returned %zu bytes", arr_len);

    return result;
}

/*
 * list_pending: Return pending suggestions.
 * Returns: pointer to JSON string of pending suggestions array.
 */
/* PoP: cli_cron_suggestions_list_pending @ cron/suggestions.py:list_pending */
void* cli_cron_suggestions_list_pending(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    char *all = (char *)cli_cron_suggestions_load_suggestions(NULL, NULL, NULL, NULL, NULL);
    if (!all) return strdup("[]");

    /* Parse and filter pending items */
    /* Simple approach: scan for objects with "status":"pending" */
    char *result = malloc(strlen(all) + 32);
    if (!result) {
        free(all);
        return strdup("[]");
    }

    strcpy(result, "[");
    size_t result_len = 1;
    int found = 0;

    /* Find each {"id":...} block and check its status */
    char *p = all;
    while (*p) {
        /* Find start of an object */
        p = strchr(p, '{');
        if (!p) break;

        /* Find end of this object */
        int depth = 0;
        char *obj_end = p;
        do {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') depth--;
            obj_end++;
        } while (*obj_end && depth > 0);

        if (depth != 0) break;

        /* Check if this object has "status":"pending" */
        /* Temporarily null-terminate for searching */
        char saved = *obj_end;
        *obj_end = '\0';

        if (strstr(p, "\"status\":\"pending\"") ||
            strstr(p, "\"status\": \"pending\"")) {
            if (found > 0) {
                result[result_len++] = ',';
            }
            size_t obj_len = obj_end - p;
            memcpy(result + result_len, p, obj_len);
            result_len += obj_len;
            found++;
        }

        *obj_end = saved;
        p = obj_end;
    }

    result[result_len++] = ']';
    result[result_len] = '\0';
    free(all);

    hermes_log(LOG_DEBUG, "port",
               "list_pending: found %d pending suggestions", found);

    return result;
}

/*
 * add_suggestion: Register a pending suggestion.
 *
 * Parameters (passed as void* array in p1):
 *   p1[0] = title (char*)
 *   p1[1] = description (char*)
 *   p1[2] = source (char*)
 *   p1[3] = job_spec_json (char*)
 *   p1[4] = dedup_key (char*)
 *   p2 = out_record (char* buffer, size 4096)
 *
 * Returns: pointer to out_record with JSON, or NULL if skipped.
 */
/* PoP: cli_cron_suggestions_add_suggestion @ cron/suggestions.py:add_suggestion */
void* cli_cron_suggestions_add_suggestion(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    char **args = (char **)p1;
    char *out = (char *)p2;

    if (!args || !out) return NULL;

    const char *title = args[0];
    const char *description = args[1];
    const char *source = args[2];
    const char *job_spec_json = args[3];
    const char *dedup_key = args[4];

    if (!title || !title[0] || !dedup_key || !dedup_key[0]) {
        hermes_log(LOG_ERROR, "port",
                   "add_suggestion: title and dedup_key are required");
        return NULL;
    }

    /* Validate source */
    int source_valid = 0;
    for (int i = 0; i < VALID_SOURCES_COUNT; i++) {
        if (strcmp(source, valid_sources[i]) == 0) {
            source_valid = 1;
            break;
        }
    }
    if (!source_valid) {
        hermes_log(LOG_ERROR, "port",
                   "add_suggestion: unknown source '%s'", source);
        return NULL;
    }

    /* Load existing suggestions */
    char *raw = (char *)cli_cron_suggestions__load_raw(NULL, NULL, NULL, NULL, NULL);
    if (!raw) raw = strdup("{\"suggestions\":[]}");

    /* Check for existing dedup_key */
    if (strstr(raw, dedup_key)) {
        /* Check if it's dismissed/accepted/pending */
        char *key_pos = strstr(raw, dedup_key);
        /* Look backwards for "status" */
        char *status_pos = key_pos;
        while (status_pos > raw && strncmp(status_pos - 7, "\"status\"", 8) != 0 &&
               strncmp(status_pos - 8, "\"status\"", 8) != 0) {
            status_pos--;
        }
        if (status_pos > raw) {
            /* Found a status field near this dedup_key — skip */
            hermes_log(LOG_DEBUG, "port",
                       "add_suggestion: dedup_key '%s' already exists, skipping", dedup_key);
            free(raw);
            return NULL;
        }
    }

    /* Count pending */
    int pending_count = 0;
    char *scan = raw;
    while ((scan = strstr(scan, "\"status\":\"pending\"")) != NULL ||
           (scan = strstr(scan, "\"status\": \"pending\"")) != NULL) {
        pending_count++;
        scan++;
    }

    if (pending_count >= MAX_PENDING) {
        hermes_log(LOG_INFO, "port",
                   "add_suggestion: backlog full (%d), dropping '%s'",
                   MAX_PENDING, title);
        free(raw);
        return NULL;
    }

    /* Generate a simple ID (first 12 chars of a hash) */
    unsigned long hash = 5381;
    for (const char *c = title; *c; c++)
        hash = ((hash << 5) + hash) + *c;
    for (const char *c = dedup_key; *c; c++)
        hash = ((hash << 5) + hash) + *c;
    hash ^= (unsigned long)time(NULL);

    char id[13];
    snprintf(id, sizeof(id), "%012lx", hash & 0xFFFFFFFFFFFF);

    /* Get current timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    /* Build the new record */
    char record[4096];
    snprintf(record, sizeof(record),
             "{\"id\":\"%s\",\"title\":\"%s\",\"description\":\"%s\","
             "\"source\":\"%s\",\"job_spec\":%s,\"dedup_key\":\"%s\","
             "\"status\":\"pending\",\"created_at\":\"%s\"}",
             id, title, description, source,
             job_spec_json ? job_spec_json : "{}",
             dedup_key, timestamp);

    /* Insert into suggestions array */
    /* Find the ] that closes the suggestions array */
    char *arr_end = strstr(raw, "\"suggestions\"");
    if (arr_end) {
        arr_end = strchr(arr_end, '[');
        if (arr_end) {
            /* Find the last ] at depth 0 */
            int depth = 0;
            char *p = arr_end;
            do {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                p++;
            } while (*p && depth > 0);

            /* Insert before the closing ] */
            size_t prefix_len = (arr_end - raw) + 1;  /* include [ */
            size_t suffix_len = strlen(p - 1);  /* from ] onwards */
            size_t record_len = strlen(record);

            char *new_raw = malloc(prefix_len + suffix_len + record_len + 16);
            if (new_raw) {
                memcpy(new_raw, raw, prefix_len);
                new_raw[prefix_len] = '\0';
                if (prefix_len > 1) {
                    /* Check if array is non-empty */
                    char *check = arr_end + 1;
                    while (*check == ' ' || *check == '\n' || *check == '\t') check++;
                    if (*check != ']') {
                        strcat(new_raw, ",");
                    }
                }
                strcat(new_raw, record);
                strcat(new_raw, p - 1);  /* include ] and rest */
                free(raw);
                raw = new_raw;
            }
        }
    }

    /* Save */
    cli_cron_suggestions__save_raw(raw, NULL, NULL, NULL, NULL);
    free(raw);

    /* Return the record */
    strncpy(out, record, 4095);
    out[4095] = '\0';

    hermes_log(LOG_INFO, "port",
               "add_suggestion: created '%s' (id=%s, source=%s)", title, id, source);

    return out;
}

/*
 * get_suggestion: Resolve a suggestion by id, 1-based pending index, or title.
 * p1 = ref string (id, index, or title).
 * p2 = out buffer for result JSON.
 * Returns: pointer to out buffer, or NULL if not found.
 */
/* PoP: cli_cron_suggestions_get_suggestion @ cron/suggestions.py:get_suggestion */
void* cli_cron_suggestions_get_suggestion(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    const char *ref = (const char *)p1;
    char *out = (char *)p2;

    if (!ref || !out) return NULL;

    char *all = (char *)cli_cron_suggestions_load_suggestions(NULL, NULL, NULL, NULL, NULL);
    if (!all) return NULL;

    /* Try matching by id first */
    char *match = NULL;
    char *p = all;

    while ((p = strchr(p, '{')) != NULL) {
        /* Find end of object */
        int depth = 0;
        char *obj_end = p;
        do {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') depth--;
            obj_end++;
        } while (*obj_end && depth > 0);

        if (depth != 0) break;

        /* Null-terminate temporarily */
        char saved = *obj_end;
        *obj_end = '\0';

        /* Check id match */
        char *id_pos = strstr(p, "\"id\"");
        if (id_pos) {
            char *val = strchr(id_pos + 3, '"');
            if (val) {
                val++;  /* skip opening quote */
                char *val_end = strchr(val, '"');
                if (val_end) {
                    size_t id_len = val_end - val;
                    if (strncmp(val, ref, id_len) == 0 && ref[id_len] == '\0') {
                        match = p;
                        *obj_end = saved;
                        break;
                    }
                }
            }
        }

        /* Check title match (case-insensitive) */
        char *title_pos = strstr(p, "\"title\"");
        if (title_pos) {
            char *val = strchr(title_pos + 7, '"');
            if (val) {
                val++;
                char *val_end = strchr(val, '"');
                if (val_end) {
                    size_t title_len = val_end - val;
                    if (strncasecmp(val, ref, title_len) == 0 &&
                        (ref[title_len] == '\0')) {
                        match = p;
                        *obj_end = saved;
                        break;
                    }
                }
            }
        }

        *obj_end = saved;
        p = obj_end;
    }

    /* Try 1-based pending index */
    if (!match) {
        int all_digits = 1;
        for (const char *c = ref; *c; c++) {
            if (*c < '0' || *c > '9') { all_digits = 0; break; }
        }

        if (all_digits) {
            int target_idx = atoi(ref) - 1;
            if (target_idx >= 0) {
                int pending_idx = 0;
                p = all;
                while ((p = strchr(p, '{')) != NULL) {
                    int depth = 0;
                    char *obj_end = p;
                    do {
                        if (*obj_end == '{') depth++;
                        else if (*obj_end == '}') depth--;
                        obj_end++;
                    } while (*obj_end && depth > 0);
                    if (depth != 0) break;

                    char saved = *obj_end;
                    *obj_end = '\0';

                    if (strstr(p, "\"status\":\"pending\"") ||
                        strstr(p, "\"status\": \"pending\"")) {
                        if (pending_idx == target_idx) {
                            match = p;
                            *obj_end = saved;
                            break;
                        }
                        pending_idx++;
                    }

                    *obj_end = saved;
                    p = obj_end;
                }
            }
        }
    }

    if (match) {
        strncpy(out, match, 4095);
        out[4095] = '\0';
        free(all);
        hermes_log(LOG_DEBUG, "port",
                   "get_suggestion: found match for ref '%s'", ref);
        return out;
    }

    free(all);
    hermes_log(LOG_DEBUG, "port",
               "get_suggestion: no match for ref '%s'", ref);
    return NULL;
}

/*
 * _set_status: Update a suggestion's status by id.
 * p1 = suggestion_id string.
 * p2 = new status string.
 * Returns: 0 on success, -1 if not found.
 */
/* PoP: cli_cron_suggestions__set_status @ cron/suggestions.py:_set_status */
void* cli_cron_suggestions__set_status(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    const char *suggestion_id = (const char *)p1;
    const char *new_status = (const char *)p2;

    if (!suggestion_id || !new_status) return (void *)(intptr_t)(-1);

    char *raw = (char *)cli_cron_suggestions__load_raw(NULL, NULL, NULL, NULL, NULL);
    if (!raw) return (void *)(intptr_t)(-1);

    /* Find the object with this id and update its status */
    char *p = raw;
    int found = 0;

    while ((p = strchr(p, '{')) != NULL) {
        int depth = 0;
        char *obj_end = p;
        do {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') depth--;
            obj_end++;
        } while (*obj_end && depth > 0);
        if (depth != 0) break;

        char saved = *obj_end;
        *obj_end = '\0';

        /* Check if this object has the matching id */
        char *id_pos = strstr(p, "\"id\"");
        if (id_pos) {
            char *val = strchr(id_pos + 3, '"');
            if (val) {
                val++;
                char *val_end = strchr(val, '"');
                if (val_end) {
                    size_t id_len = val_end - val;
                    if (strncmp(val, suggestion_id, id_len) == 0 &&
                        suggestion_id[id_len] == '\0') {
                        /* Found it — now update the status field */
                        char *status_pos = strstr(p, "\"status\"");
                        if (status_pos) {
                            char *sval = strchr(status_pos + 8, '"');
                            if (sval) {
                                sval++;
                                char *sval_end = strchr(sval, '"');
                                if (sval_end) {
                                    /* Replace status value */
                                    size_t prefix_len = sval - raw;
                                    size_t old_status_len = sval_end - sval;
                                    size_t suffix_len = strlen(sval_end);
                                    size_t new_status_len = strlen(new_status);

                                    char *new_raw = malloc(prefix_len + new_status_len + suffix_len + 32);
                                    if (new_raw) {
                                        memcpy(new_raw, raw, prefix_len);
                                        new_raw[prefix_len] = '\0';
                                        strcat(new_raw, new_status);
                                        strcat(new_raw, sval_end);

                                        /* Add resolved_at timestamp */
                                        time_t now = time(NULL);
                                        struct tm *tm_info = gmtime(&now);
                                        char ts[32];
                                        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm_info);

                                        /* Insert resolved_at before closing } */
                                        char *close_brace = strrchr(new_raw, '}');
                                        if (close_brace) {
                                            size_t clen = strlen(close_brace);
                                            char *with_ts = malloc(strlen(new_raw) + 64);
                                            if (with_ts) {
                                                *close_brace = '\0';
                                                snprintf(with_ts, strlen(new_raw) + 64,
                                                         "%s,\"resolved_at\":\"%s\"%s",
                                                         new_raw, ts, close_brace);
                                                free(new_raw);
                                                new_raw = with_ts;
                                            }
                                        }

                                        free(raw);
                                        raw = new_raw;
                                        found = 1;
                                    }
                                }
                            }
                        }
                        *obj_end = saved;
                        break;
                    }
                }
            }
        }

        *obj_end = saved;
        p = obj_end;
    }

    if (found) {
        cli_cron_suggestions__save_raw(raw, NULL, NULL, NULL, NULL);
        hermes_log(LOG_DEBUG, "port",
                   "_set_status: set id '%s' to '%s'", suggestion_id, new_status);
    }

    free(raw);
    return (void *)(intptr_t)(found ? 0 : -1);
}

/*
 * dismiss_suggestion: Dismiss a suggestion by ref.
 * p1 = ref string (id, index, or title).
 * Returns: 0 on success, -1 if not found.
 */
/* PoP: cli_cron_suggestions_dismiss_suggestion @ cron/suggestions.py:dismiss_suggestion */
void* cli_cron_suggestions_dismiss_suggestion(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;

    const char *ref = (const char *)p1;
    if (!ref) return (void *)(intptr_t)(-1);

    /* Get the suggestion to find its id */
    char suggestion_json[4096];
    void *result = cli_cron_suggestions_get_suggestion(
        (void *)ref, suggestion_json, NULL, NULL, NULL);

    if (!result) {
        hermes_log(LOG_DEBUG, "port",
                   "dismiss_suggestion: no suggestion found for ref '%s'", ref);
        return (void *)(intptr_t)(-1);
    }

    /* Extract id from the suggestion JSON */
    char *id_pos = strstr(suggestion_json, "\"id\"");
    if (!id_pos) return (void *)(intptr_t)(-1);

    char *val = strchr(id_pos + 3, '"');
    if (!val) return (void *)(intptr_t)(-1);
    val++;
    char *val_end = strchr(val, '"');
    if (!val_end) return (void *)(intptr_t)(-1);

    char id[64];
    size_t id_len = val_end - val;
    if (id_len >= sizeof(id)) id_len = sizeof(id) - 1;
    memcpy(id, val, id_len);
    id[id_len] = '\0';

    hermes_log(LOG_INFO, "port",
               "dismiss_suggestion: dismissing suggestion id='%s'", id);

    return cli_cron_suggestions__set_status(id, STATUS_DISMISSED, NULL, NULL, NULL);
}

/*
 * accept_suggestion: Accept a suggestion and create the cron job.
 * p1 = ref string.
 * p2 = origin JSON string (can be NULL).
 * p3 = out buffer for job result JSON.
 * Returns: pointer to out buffer with job spec, or NULL.
 */
/* PoP: cli_cron_suggestions_accept_suggestion @ cron/suggestions.py:accept_suggestion */
void* cli_cron_suggestions_accept_suggestion(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p4; (void)p5;

    const char *ref = (const char *)p1;
    const char *origin_json = (const char *)p2;
    char *out = (char *)p3;

    if (!ref || !out) return NULL;

    /* Get the suggestion */
    char suggestion_json[4096];
    void *result = cli_cron_suggestions_get_suggestion(
        (void *)ref, suggestion_json, NULL, NULL, NULL);

    if (!result) {
        hermes_log(LOG_DEBUG, "port",
                   "accept_suggestion: no suggestion found for ref '%s'", ref);
        return NULL;
    }

    /* Verify it's pending */
    if (!strstr(suggestion_json, "\"status\":\"pending\"") &&
        !strstr(suggestion_json, "\"status\": \"pending\"")) {
        hermes_log(LOG_DEBUG, "port",
                   "accept_suggestion: suggestion is not pending");
        return NULL;
    }

    /* Extract job_spec */
    char *js_pos = strstr(suggestion_json, "\"job_spec\"");
    if (!js_pos) {
        hermes_log(LOG_ERROR, "port",
                   "accept_suggestion: no job_spec in suggestion");
        return NULL;
    }

    char *js_start = strchr(js_pos + 10, ':');
    if (!js_start) return NULL;
    js_start++;

    /* Find end of job_spec value (could be a nested object) */
    int depth = 0;
    char *js_end = js_start;
    int in_string = 0;
    do {
        if (in_string) {
            if (*js_end == '\\') js_end++;
            else if (*js_end == '"') in_string = 0;
        } else {
            if (*js_end == '"') in_string = 1;
            else if (*js_end == '{') depth++;
            else if (*js_end == '}') { if (depth > 0) depth--; else break; }
        }
        js_end++;
    } while (*js_end);

    /* Copy job_spec */
    char job_spec[2048];
    size_t js_len = js_end - js_start;
    if (js_len >= sizeof(job_spec)) js_len = sizeof(job_spec) - 1;
    memcpy(job_spec, js_start, js_len);
    job_spec[js_len] = '\0';

    /* Add origin if provided */
    if (origin_json && origin_json[0] && !strstr(job_spec, "\"origin\"")) {
        /* Insert origin into job_spec */
        char enhanced[2048];
        char *brace = strchr(job_spec, '{');
        if (brace && js_len < sizeof(enhanced) - 64) {
            size_t prefix = brace - job_spec + 1;
            memcpy(enhanced, job_spec, prefix);
            enhanced[prefix] = '\0';
            snprintf(enhanced + prefix, sizeof(enhanced) - prefix,
                     "\"origin\":%s,%s", origin_json, brace + 1);
            strncpy(job_spec, enhanced, sizeof(job_spec) - 1);
            job_spec[sizeof(job_spec) - 1] = '\0';
        }
    }

    /* In the full implementation, this would call create_job().
     * For now, return the job_spec as the result. */
    snprintf(out, 4096, "{\"job_spec\":%s,\"status\":\"created\"}", job_spec);

    /* Set status to accepted */
    /* Extract id */
    char *id_pos = strstr(suggestion_json, "\"id\"");
    if (id_pos) {
        char *val = strchr(id_pos + 3, '"');
        if (val) {
            val++;
            char *val_end = strchr(val, '"');
            if (val_end) {
                char id[64];
                size_t id_len = val_end - val;
                if (id_len >= sizeof(id)) id_len = sizeof(id) - 1;
                memcpy(id, val, id_len);
                id[id_len] = '\0';
                cli_cron_suggestions__set_status(id, STATUS_ACCEPTED, NULL, NULL, NULL);
            }
        }
    }

    hermes_log(LOG_INFO, "port",
               "accept_suggestion: accepted suggestion, job created");

    return out;
}

/*
 * clear_resolved: Remove accepted records from the store.
 * Returns: number of records removed.
 */
/* PoP: cli_cron_suggestions_clear_resolved @ cron/suggestions.py:clear_resolved */
void* cli_cron_suggestions_clear_resolved(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    char *raw = (char *)cli_cron_suggestions__load_raw(NULL, NULL, NULL, NULL, NULL);
    if (!raw) return (void *)(intptr_t)0;

    /* Count total and accepted */
    int total = 0;
    int accepted = 0;
    char *p = raw;

    while ((p = strchr(p, '{')) != NULL) {
        int depth = 0;
        char *obj_end = p;
        do {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') depth--;
            obj_end++;
        } while (*obj_end && depth > 0);
        if (depth != 0) break;

        char saved = *obj_end;
        *obj_end = '\0';

        total++;
        if (strstr(p, "\"status\":\"accepted\"") ||
            strstr(p, "\"status\": \"accepted\"")) {
            accepted++;
        }

        *obj_end = saved;
        p = obj_end;
    }

    if (accepted == 0) {
        hermes_log(LOG_DEBUG, "port",
                   "clear_resolved: no accepted records to remove");
        free(raw);
        return (void *)(intptr_t)0;
    }

    /* Rebuild without accepted records */
    /* Find the suggestions array */
    char *arr_start = strstr(raw, "\"suggestions\"");
    if (!arr_start) {
        free(raw);
        return (void *)(intptr_t)0;
    }

    arr_start = strchr(arr_start, '[');
    if (!arr_start) {
        free(raw);
        return (void *)(intptr_t)0;
    }

    /* Build new array content */
    char *new_content = malloc(strlen(raw) + 32);
    if (!new_content) {
        free(raw);
        return (void *)(intptr_t)0;
    }
    strcpy(new_content, "[");

    int kept = 0;
    p = arr_start + 1;
    while (*p) {
        p = strchr(p, '{');
        if (!p) break;

        int depth = 0;
        char *obj_end = p;
        do {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') depth--;
            obj_end++;
        } while (*obj_end && depth > 0);
        if (depth != 0) break;

        char saved = *obj_end;
        *obj_end = '\0';

        if (!strstr(p, "\"status\":\"accepted\"") &&
            !strstr(p, "\"status\": \"accepted\"")) {
            if (kept > 0) strcat(new_content, ",");
            /* Trim leading whitespace */
            while (*p == ' ' || *p == '\n' || *p == '\t') p++;
            strcat(new_content, "{");
            /* Find the content between { and } */
            char *inner = p + 1;
            /* Find last } */
            char *last_brace = strrchr(inner, '}');
            if (last_brace) {
                *last_brace = '\0';
                strcat(new_content, inner);
                strcat(new_content, "}");
            }
            kept++;
        }

        *obj_end = saved;
        p = obj_end;
    }

    strcat(new_content, "]");

    /* Rebuild the full JSON */
    size_t prefix_len = (arr_start - raw);
    char *new_raw = malloc(prefix_len + strlen(new_content) + 32);
    if (new_raw) {
        memcpy(new_raw, raw, prefix_len);
        new_raw[prefix_len] = '\0';
        strcat(new_raw, new_content);
        /* Add any trailing content after the array */
        /* Find the closing ] of the array in the original */
        char *orig_arr_end = arr_start;
        int depth = 0;
        do {
            if (*orig_arr_end == '[') depth++;
            else if (*orig_arr_end == ']') depth--;
            orig_arr_end++;
        } while (*orig_arr_end && depth > 0);
        if (*orig_arr_end == ']') orig_arr_end++;
        /* Skip to end of object */
        char *obj_close = strchr(orig_arr_end, '}');
        if (obj_close) {
            char trailing = *(obj_close + 1);
            *(obj_close + 1) = '\0';
            /* Don't append trailing — we already have the array */
            *(obj_close + 1) = trailing;
        }

        cli_cron_suggestions__save_raw(new_raw, NULL, NULL, NULL, NULL);
        free(new_raw);

        hermes_log(LOG_INFO, "port",
                   "clear_resolved: removed %d accepted records, kept %d",
                   accepted, kept);
    }

    free(new_content);
    free(raw);

    return (void *)(intptr_t)accepted;
}
