/*
 * process_registry_results.c -- real C11 port of the receipt retention,
 * ownership and restore-admission decisions of upstream Hermes
 * tools/process_registry_results.py.
 *
 * PoP: w22_processreg_606e65_field_names @ tools/process_registry_results.py:_RESULT_FIELDS
 * PoP: w22_processreg_606e65_retention_seconds @ tools/process_registry_results.py:RESULT_RETENTION_SECONDS
 * PoP: w22_processreg_606e65_max_retained @ tools/process_registry_results.py:MAX_RETAINED_RESULTS
 * PoP: w22_processreg_606e65_select_retained @ tools/process_registry_results.py:_result_paths
 * PoP: w22_processreg_606e65_owns_result @ tools/process_registry_results.py:_owns_result
 * PoP: w22_processreg_606e65_admissible @ tools/process_registry_results.py:load_completed_results
 *
 * Host seams, injected by the caller: the process-results directory listing,
 * the wall clock, the session env, the hosted-scope check and the persistence
 * lineage tips. `save_completed_result`'s redaction plus `atomic_json_write`,
 * and the live `ProcessSession` construction, stay upstream.
 */
#include "hermes_json.h"
#include "noc_w22_processreg_606e65.h"

#include <stdlib.h>
#include <string.h>

/* Module-level literals mirrored from the Python source. */
#define PR_RESULT_RETENTION_SECONDS (7LL * 24LL * 60LL * 60LL)
#define PR_MAX_RETAINED_RESULTS 64

static const char *const PR_RESULT_FIELDS[] = {
    "id", "command", "cwd", "task_id", "owner_task_id", "session_key",
    "parent_session_id", "started_at", "exit_code", "completion_reason",
    "termination_source", "notify_on_complete",
};

/* `directory.glob("proc_*.json")`: prefix, then anything, then the suffix. */
static int pr_is_receipt_name(const char *name) {
    size_t n;
    if (!name) return 0;
    n = strlen(name);
    if (n < 10) return 0;
    if (strncmp(name, "proc_", 5) != 0) return 0;
    return strcmp(name + n - 5, ".json") == 0;
}

json_t *w22_processreg_606e65_field_names(void) {
    json_t *out = json_array();
    size_t i;
    if (!out) return NULL;
    for (i = 0; i < sizeof(PR_RESULT_FIELDS) / sizeof(PR_RESULT_FIELDS[0]); i++)
        json_append(out, json_string(PR_RESULT_FIELDS[i]));
    return out;
}

long long w22_processreg_606e65_retention_seconds(void) {
    return PR_RESULT_RETENTION_SECONDS;
}

long long w22_processreg_606e65_max_retained(void) {
    return (long long)PR_MAX_RETAINED_RESULTS;
}

typedef struct {
    const char *name;
    long long mtime;
    int pruned;
} pr_row;

/* (mtime, name) descending, the module's `reverse=True` sort. */
static int pr_cmp_desc(const void *a, const void *b) {
    const pr_row *x = (const pr_row *)a;
    const pr_row *y = (const pr_row *)b;
    if (x->mtime != y->mtime) return x->mtime < y->mtime ? 1 : -1;
    return -strcmp(x->name, y->name);
}

static int pr_cmp_names(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w22_processreg_606e65_select_retained(const json_t *rows, const char *now) {
    long long stamp = now ? strtoll(now, NULL, 10) : 0;
    long long cutoff = stamp - PR_RESULT_RETENTION_SECONDS;
    size_t total = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    pr_row *live = NULL;
    size_t live_count = 0;
    const char **pruned = NULL;
    size_t pruned_count = 0;
    json_t *out = json_object();
    json_t *kept = json_array();
    json_t *dropped = json_array();
    size_t i;

    if (!out || !kept || !dropped) {
        json_free(out);
        json_free(kept);
        json_free(dropped);
        return NULL;
    }
    if (total) {
        live = (pr_row *)calloc(total, sizeof(pr_row));
        pruned = (const char **)calloc(total, sizeof(char *));
    }
    for (i = 0; i < total; i++) {
        json_t *row = json_get(rows, i);
        const char *name = json_get_str(row, "name", NULL);
        const char *stamp_text = json_get_str(row, "mtime", NULL);
        long long mtime;
        if (!row || row->type != JSON_OBJECT || !name || !pr_is_receipt_name(name)) continue;
        mtime = stamp_text ? strtoll(stamp_text, NULL, 10) : 0;
        if (mtime < cutoff) {
            if (pruned) pruned[pruned_count++] = name;  /* st_mtime < cutoff -> unlink */
        } else if (live) {
            live[live_count].name = name;
            live[live_count].mtime = mtime;
            live_count++;
        }
    }
    if (live && live_count > 1)
        qsort(live, live_count, sizeof(pr_row), pr_cmp_desc);
    for (i = 0; i < live_count; i++) {
        if (i < (size_t)PR_MAX_RETAINED_RESULTS) {
            json_append(kept, json_string(live[i].name));
        } else if (pruned) {
            pruned[pruned_count++] = live[i].name;  /* past MAX_RETAINED_RESULTS */
        }
    }
    if (pruned && pruned_count > 1)
        qsort(pruned, pruned_count, sizeof(char *), pr_cmp_names);
    for (i = 0; i < pruned_count; i++) json_append(dropped, json_string(pruned[i]));

    json_set(out, "retained", kept);
    json_set(out, "pruned", dropped);
    free(live);
    free(pruned);
    return out;
}

/* Python truthiness for a string id: absent/empty is None/"" -> False. */
static int pr_has_text(const char *s) {
    return s && s[0];
}

int w22_processreg_606e65_owns_result(const char *owner, const char *parent,
                                      const json_t *lineage) {
    if (!pr_has_text(parent)) return 0;
    if (owner && parent && !strcmp(owner, parent)) return 1;
    {
        json_t *tip = lineage ? json_obj_get(lineage, parent) : NULL;
        const char *tip_text = (tip && tip->type == JSON_STRING) ? tip->str_val : NULL;
        return (tip_text && owner && !strcmp(tip_text, owner)) ? 1 : 0;
    }
}

/* re.fullmatch(r"proc_[\w]+") over ASCII word characters. */
static int pr_receipt_id_ok(const char *id) {
    const char *p;
    if (!id || strncmp(id, "proc_", 5) != 0) return 0;
    p = id + 5;
    if (!*p) return 0;
    for (; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

int w22_processreg_606e65_admissible(const char *prefix, const char *stem,
                                     const char *record_id, const char *owner,
                                     const char *parent, const json_t *lineage) {
    /* `if not owner ...: return {}` -- a session-less read restores nothing. */
    if (!pr_has_text(owner)) return 0;
    if (!stem) return 0;
    if (prefix && strncmp(stem, prefix, strlen(prefix)) != 0) return 0;
    if (!record_id || strcmp(record_id, stem) != 0) return 0;
    if (!pr_receipt_id_ok(record_id)) return 0;
    return w22_processreg_606e65_owns_result(owner, parent, lineage);
}
