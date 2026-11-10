/*
 * port_update_receipt.c
 *
 * Pure helpers ported from hermes_cli/update_receipt.py (58 defs, previously 0
 * ported).
 *
 *   ur_str_records       <- _str_records
 *   ur_run_filename      <- _run_file
 *   ur_launcher_corr_id  <- _launcher_correlation_id
 *
 * The rest of the module is ContextVar plumbing and durable-filesystem
 * bookkeeping; those stay in Python.
 */

#include "port_update_receipt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ===========================================================================
 *  _str_records  (update_receipt.py:_str_records)
 *
 *      records = []
 *      for entry in entries:
 *          if not isinstance(entry, dict): continue
 *          record = {"pid": int(entry.get("pid", 0) or 0)} if pid else {}
 *          record.update({key: str(entry.get(key, "")) for key in keys})
 *          records.append(record)
 *
 *  Dict entries reduced to stringified `keys`. A non-dict entry is SKIPPED, not
 *  coerced. `int(entry.get("pid", 0) or 0)` means a pid of 0 / "" / None becomes
 *  0, and a non-numeric string would raise in Python - the C port clamps to 0.
 *
 *  `entries` is a flat [key, value, ...] array per entry (NULL-terminated); the
 *  entry list itself is NULL-terminated. `keys` is a NULL-terminated array of
 *  field names. Returns a malloc'd array of malloc'd records; each record is a
 *  NULL-terminated array of malloc'd strings, and `*out_count` counts them.
 *  The record field order is: pid (when requested), then the keys in order.
 * =========================================================================== */

/* PoP: _str_records @ hermes_cli/update_receipt.py:_str_records */
char ***ur_str_records(const char *const *const *entries,
                       const char *const *keys, int want_pid,
                       int *out_count)
{
    int nk = 0;
    if (keys) while (keys[nk]) nk++;

    int ne = 0;
    if (entries) while (entries[ne]) ne++;

    char ***records = (char ***)calloc((size_t)ne + 1, sizeof(char **));
    if (!records) { if (out_count) *out_count = 0; return NULL; }

    int nr = 0;
    for (int e = 0; e < ne; e++) {
        const char *const *entry = entries[e];
        if (!entry) continue;                 /* not a dict -> skipped */

        char **rec = (char **)calloc((size_t)nk + (want_pid ? 2 : 1), sizeof(char *));
        if (!rec) continue;
        int k = 0;

        if (want_pid) {
            const char *pv = NULL;
            for (int i = 0; entry[i]; i += 2)
                if (strcmp(entry[i], "pid") == 0) { pv = entry[i + 1]; break; }
            /* int(entry.get("pid", 0) or 0): falsy -> 0, non-numeric -> 0 */
            long pid = 0;
            if (pv && pv[0]) {
                char *end = NULL;
                long v = strtol(pv, &end, 10);
                if (end != pv && v != 0) pid = v;
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "%ld", pid);
            rec[k++] = strdup(buf);
        }

        for (int j = 0; j < nk; j++) {
            const char *val = "";
            for (int i = 0; entry[i]; i += 2)
                if (strcmp(entry[i], keys[j]) == 0) { val = entry[i + 1]; break; }
            rec[k++] = strdup((val && val[0]) ? val : "");
        }
        records[nr++] = rec;
    }

    if (out_count) *out_count = nr;
    return records;
}

/* ===========================================================================
 *  _run_file  (update_receipt.py:_run_file)
 *
 *      stamp = re.sub(r"[^0-9]", "", str(data.get("started_at") or ""))[:14] \
 *              or time.strftime("%Y%m%d%H%M%S")
 *      return directory / f"update_{stamp[:8]}_{stamp[8:]}_{data.get('pid') or os.getpid()}" \
 *                    f"_{data.get('update_id')}.json"
 *
 *  One archive file per run, named at BEGIN so the running and terminal records
 *  share it. `now` is the strftime() fallback (NULL uses the real clock).
 *  Caller frees.
 * =========================================================================== */

/* PoP: _run_file @ hermes_cli/update_receipt.py:_run_file */
char *ur_run_filename(const char *directory, const char *const *data,
                      const char *now, long default_pid)
{
    const char *started = NULL, *pid_s = NULL, *update_id = NULL;
    for (int i = 0; data && data[i]; i += 2) {
        if (strcmp(data[i], "started_at") == 0) started = data[i + 1];
        else if (strcmp(data[i], "pid") == 0) pid_s = data[i + 1];
        else if (strcmp(data[i], "update_id") == 0) update_id = data[i + 1];
    }

    /* re.sub(r"[^0-9]", "", str(started or ""))[:14] */
    char stamp[32];
    size_t k = 0;
    if (started && started[0]) {
        for (size_t i = 0; started[i] && k < 14; i++)
            if (started[i] >= '0' && started[i] <= '9') stamp[k++] = started[i];
    }
    stamp[k] = '\0';

    /* `or time.strftime("%Y%m%d%H%M%S")` */
    char fallback[32];
    if (k == 0) {
        if (now && now[0]) {
            snprintf(fallback, sizeof(fallback), "%s", now);
        } else {
            time_t tt = time(NULL);
            struct tm tmv;
            localtime_r(&tt, &tmv);
            strftime(fallback, sizeof(fallback), "%Y%m%d%H%M%S", &tmv);
        }
        snprintf(stamp, sizeof(stamp), "%s", fallback);
    }

    /* data.get('pid') or os.getpid() - the caller supplies the live pid. */
    long pid = 0;
    if (pid_s && pid_s[0]) {
        char *end = NULL;
        pid = strtol(pid_s, &end, 10);
        if (end == pid_s) pid = 0;
    }
    if (pid == 0) pid = default_pid;

    const char *uid = (update_id && update_id[0]) ? update_id : "";

    size_t cap = strlen(directory ? directory : "") + strlen(uid) + 64;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    /* Path(directory) / name: the directory's separators are normalized too,
     * so "C:/hermes/receipts" renders as "C:\hermes\receipts". */
    char *dir = strdup(directory ? directory : "");
    if (dir) for (size_t i = 0; dir[i]; i++) if (dir[i] == '/') dir[i] = '\\';
    snprintf(out, cap, "%s\\update_%.8s_%.6s_%ld_%s.json",
             dir ? dir : "", stamp, stamp + 8, pid, uid);
    free(dir);
    return out;
}

/* ===========================================================================
 *  _launcher_correlation_id  (update_receipt.py:_launcher_correlation_id)
 *
 *      return os.environ.get("HERMES_UPDATE_CORRELATION_ID", "").strip() or None
 *
 *  Correlation id handed in by an external launcher (Desktop's managed SSH
 *  update). Separate from update_id, which stays the CLI's own id for pm sync
 *  receipts. `env` is a flat [key, value, ...] array; NULL reads the live
 *  environment. Caller frees.
 * =========================================================================== */

/* PoP: _launcher_correlation_id @ hermes_cli/update_receipt.py:_launcher_correlation_id */
char *ur_launcher_corr_id(const char *const *env)
{
    const char *val = NULL;
    if (env) {
        for (int i = 0; env[i]; i += 2)
            if (strcmp(env[i], "HERMES_UPDATE_CORRELATION_ID") == 0) { val = env[i + 1]; break; }
    } else {
        val = getenv("HERMES_UPDATE_CORRELATION_ID");
    }
    if (!val) return NULL;

    /* .strip() or None */
    const char *p = val;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r' || p[n - 1] == '\n')) n--;
    if (n == 0) return NULL;

    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}
