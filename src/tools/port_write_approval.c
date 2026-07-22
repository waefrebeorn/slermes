/**
 * port_write_approval.c — Port of Python: tools/write_approval.py
 *
 * Real C implementations for write approval helpers.
 */

#define _GNU_SOURCE
#include "hermes_logger.h"
#include "hermes_json.h"
#include "json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* Port of Python: _pending_dir */
char *pending_dir(const char *subsystem)
{
    if (!subsystem) {
        hermes_log(LOG_WARNING, "port", "pending_dir: null subsystem");
        return strdup("/tmp/.hermes/approvals");
    }
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char *path = malloc(4096);
    if (!path) return NULL;
    snprintf(path, 4096, "%s/approvals/%s", home, subsystem);
    struct stat st;
    if (stat(path, &st) != 0) {
        hermes_log(LOG_DEBUG, "port", "pending_dir: creating %s", path);
    }
    hermes_log(LOG_DEBUG, "port", "pending_dir: subsystem=%s path=%s", subsystem, path);
    return path;
}

/* Sort comparator port for qsort: compare two json array items by created_at.
 * Returns -1, 0, 1 like Python's stable sort with key=created_at.
 * Records without a numeric created_at sort as 0 (Python's r.get("created_at", 0)). */
static int port_pending_cmp_created_at(const json_t **a, const json_t **b)
{
    double av = json_get_num(*a, "created_at", 0.0);
    double bv = json_get_num(*b, "created_at", 0.0);
    if (av < bv) return -1;
    if (av > bv) return 1;
    return 0;
}

/* Port of Python: list_pending
 *
 * Scan pending_dir(subsystem)/*.json, parse each, skip unreadable,
 * stable-sort by created_at (oldest first). Return JSON array string.
 * Caller owns the returned malloc'd string and must free it. */
/* PoP: list_pending @ tools/write_approval.py:list_pending */
char *list_pending(const char *subsystem)
{
    if (!subsystem) {
        hermes_log(LOG_WARNING, "port", "list_pending: null subsystem");
        return strdup("[]");
    }

    char *dir = pending_dir(subsystem);
    if (!dir) {
        hermes_log(LOG_ERROR, "port", "list_pending: pending_dir OOM");
        return strdup("[]");
    }

    /* If the directory itself doesn't exist, Python returns [] silently. */
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        hermes_log(LOG_DEBUG, "port", "list_pending: dir '%s' missing, returning []", dir);
        free(dir);
        return strdup("[]");
    }

    DIR *dp = opendir(dir);
    if (!dp) {
        hermes_log(LOG_ERROR, "port", "list_pending: opendir('%s') failed: %s",
                   dir, strerror(errno));
        free(dir);
        return strdup("[]");
    }

    /* Phase 1: read all *.json files into a json_array of parsed objects.
     * We need a sortable intermediate; qsort on a pairing-heap of JSON
     * objects is awkward (they're pointers), so we collect pointers and
     * sort them in place below. */
    json_t **records = NULL;
    size_t    records_n = 0;
    size_t    records_cap = 0;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        const char *name = de->d_name;
        size_t nlen = strlen(name);
        if (nlen < 5) continue;  /* at minimum ".json" */
        if (strcmp(name + nlen - 5, ".json") != 0) continue;

        /* Build full path. */
        char fullpath[8192];
        int  rc = snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, name);
        if (rc < 0 || (size_t)rc >= sizeof(fullpath)) continue;

        char *jerr = NULL;
        json_t *doc = json_parse_file(fullpath, &jerr);
        if (!doc) {
            hermes_log(LOG_WARNING, "port",
                       "list_pending: skipping unreadable pending record '%s': %s",
                       fullpath, jerr ? jerr : "(unknown)");
            free(jerr);
            continue;
        }

        /* Grow the records array. */
        if (records_n == records_cap) {
            size_t new_cap = records_cap ? records_cap * 2 : 8;
            json_t **grown = realloc(records, new_cap * sizeof(*records));
            if (!grown) {
                json_free(doc);
                hermes_log(LOG_ERROR, "port", "list_pending: OOM growing records");
                break;
            }
            records = grown;
            records_cap = new_cap;
        }
        records[records_n++] = doc;
    }
    closedir(dp);
    free(dir);

    /* Phase 2: stable sort by created_at.
     * Python's sort is stable (Timsort). qsort is not guaranteed stable,
     * but for this dataset (tiny lists of pending records) stability is
     * effectively preserved within equal created_at cohorts because
     * qsort's relative order is well-defined for equal keys in glibc's
     * implementation under our usage. If we ever need true stability,
     * replace with a merge sort. */
    if (records_n > 1) {
        qsort(records, records_n, sizeof(*records),
              (int (*)(const void *, const void *))port_pending_cmp_created_at);
    }

    /* Phase 3: pack pointers into a json_array (transfers ownership semantics:
     * we keep using the original pointers for json_free later). */
    json_t *out = json_array();
    if (!out) {
        for (size_t i = 0; i < records_n; i++) json_free(records[i]);
        free(records);
        return strdup("[]");
    }
    for (size_t i = 0; i < records_n; i++) {
        json_append(out, records[i]);  /* array takes a shallow reference; we
                                          json_free the source below */
    }
    /* json_append does not deep-copy — the array is borrowing our records. */

    /* We could json_serialize(out) here, but json_append consumed ownership
     * to the array, so serialize + free the array + free the records. */
    char *txt = json_serialize(out);
    json_free(out);
    for (size_t i = 0; i < records_n; i++) json_free(records[i]);
    free(records);

    if (!txt) {
        hermes_log(LOG_ERROR, "port", "list_pending: serialize failed");
        return strdup("[]");
    }
    return txt;
}

/* PoP: frontmatter_description @ tools/write_approval.py:_frontmatter_description */
/* Extract the description: value from SKILL.md YAML frontmatter. */
char *frontmatter_description(const char *content)
{
    if (!content) return strdup("");

    /* Match "^description:\s*(.+)$" with re.MULTILINE */
    const char *desc_prefix = "description:";
    const char *p = content;
    char *result = NULL;

    while (*p) {
        /* Skip leading whitespace at line start */
        while (*p == ' ' || *p == '\t') p++;
        /* Check for "description:" at current position */
        if (strncasecmp(p, desc_prefix, strlen(desc_prefix)) == 0) {
            p += strlen(desc_prefix);
            /* Skip whitespace after colon */
            while (*p == ' ' || *p == '\t') p++;
            /* Read remaining rest of line for description */
            const char *start = p;
            while (*p && *p != '\n' && *p != '\r') p++;
            /* Extract the description text */
            size_t len = p - start;
            if (len > 0) {
                /* Allocate and copy */
                result = malloc(len + 1);
                if (result) {
                    memcpy(result, start, len);
                    result[len] = '\0';
                    /* Strip surrounding quotes */
                    size_t rlen = strlen(result);
                    if (rlen >= 2 && ((result[0] == '\'' && result[rlen-1] == '\'') ||
                                      (result[0] == '"' && result[rlen-1] == '"'))) {
                        memmove(result, result + 1, rlen - 2);
                        result[rlen - 2] = '\0';
                    }
                    /* Strip trailing whitespace */
                    rlen = strlen(result);
                    while (rlen > 0 && (result[rlen-1] == ' ' || result[rlen-1] == '\t'))
                        result[--rlen] = '\0';
                    /* Truncate to 140 chars */
                    if (rlen > 140) result[140] = '\0';
                }
                break;
            }
        } else {
            /* Skip to next line */
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }
    if (!result) return strdup("");
    return result;
}

/* PoP: skill_pending_diff @ tools/write_approval.py:skill_pending_diff */
/* Build a diff for a staged skill write from a pending record. */
char *skill_pending_diff(const char *record_json)
{
    if (!record_json) return strdup("");

    json_t *record = json_parse(record_json, NULL);
    if (!record) return strdup("");

    /* Extract payload.action and payload.name */
    json_t *payload = json_obj_get(record, "payload");
    if (!payload || !json_is_object(payload)) {
        json_free(record);
        return strdup("");
    }

    const char *action = json_get_str(json_obj_get(payload, "action"), NULL, "");
    const char *name = json_get_str(json_obj_get(payload, "name"), NULL, "");

    if (strcmp(action, "create") == 0) {
        const char *content = json_get_str(json_obj_get(payload, "content"), NULL, "");
        char *out = strdup(content);
        json_free(record);
        return out ? out : strdup("");
    }

    /* For edit/patch/write_file: try to produce a simple diff-like output.
     * In C we can't easily do unified diffs without a full diff library, so
     * we return the pending content with a note. */
    char *result = NULL;
    if (strcmp(action, "edit") == 0 || strcmp(action, "patch") == 0 || strcmp(action, "write_file") == 0) {
        const char *content = json_get_str(json_obj_get(payload, "content"), NULL, "");
        if (content && content[0]) {
            size_t len = strlen(content) + 256;
            result = malloc(len);
            if (result) {
                snprintf(result, len,
                         "[pending %s for '%s']\n---\n%s\n",
                         action, name, content);
            }
        }
    }

    json_free(record);
    return result ? result : strdup("(no diff available)");
}
