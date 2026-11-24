/*
 * repair.c - C11 port of upstream persistence/sqlite/_state/repair.py
 * One port file, three gates: w28_repair_1f5388_p1 / _p2 / _p3.
 *
 * PoP: w28_repair_1f5388_constants @ persistence/sqlite/_state/repair.py:<module constants>
 * PoP: w28_repair_1f5388_sidecars @ persistence/sqlite/_state/repair.py:_sidecars
 * PoP: w28_repair_1f5388_mask_header @ persistence/sqlite/_state/repair.py:_mask_volatile_header
 * PoP: w28_repair_1f5388_headroom @ persistence/sqlite/_state/repair.py:_repair_backup_headroom_bytes
 * PoP: w28_repair_1f5388_snapshot_timeout @ persistence/sqlite/_state/repair.py:_repair_snapshot_timeout_seconds
 * PoP: w28_repair_1f5388_disk_budget_error @ persistence/sqlite/_state/repair.py:_disk_budget (failure text)
 * PoP: w28_repair_1f5388_scratch_space_error @ persistence/sqlite/_state/repair.py:_repair_scratch_space_error
 * PoP: w28_repair_1f5388_backup_free_space_error @ persistence/sqlite/_state/repair.py:_backup_free_space_error
 * PoP: w28_repair_1f5388_manual_hint @ persistence/sqlite/_state/repair.py:_MANUAL_RECOVER_HINT
 * PoP: w28_repair_1f5388_failure_consumes_attempt @ persistence/sqlite/_state/repair.py:_repair_failure_consumes_attempt
 * PoP: w28_repair_1f5388_temporary_file_failure @ persistence/sqlite/_state/repair.py:_is_temporary_file_failure
 * PoP: w28_repair_1f5388_schema_not_built @ persistence/sqlite/_state/repair.py:_schema_not_built
 * PoP: w28_repair_1f5388_integrity_structural @ persistence/sqlite/_state/repair.py:integrity_damage_is_structural
 * PoP: w28_repair_1f5388_probe_reason @ persistence/sqlite/_state/repair.py:_db_opens_cleanly (problem summary)
 * PoP: w28_repair_1f5388_strategy_statements @ persistence/sqlite/_state/repair.py:_strategy_* bodies
 * PoP: w28_repair_1f5388_probe_plan @ persistence/sqlite/_state/repair.py:_db_opens_cleanly (statement plan)
 * PoP: w28_repair_1f5388_schema_version_sql @ persistence/sqlite/_state/repair.py:_bump_schema_cookie
 * PoP: w28_repair_1f5388_fingerprint_text @ persistence/sqlite/_state/repair.py:_db_fingerprint (key format)
 * PoP: w28_repair_1f5388_ledger_name @ persistence/sqlite/_state/repair.py:_repair_ledger_path
 * PoP: w28_repair_1f5388_scratch_name @ persistence/sqlite/_state/repair.py:_repair_state_db_schema_locked
 * PoP: w28_repair_1f5388_staging_name @ persistence/sqlite/_state/repair.py:_backup_db_file (staging name)
 * PoP: w28_repair_1f5388_backup_name @ persistence/sqlite/_state/repair.py:_backup_db_file (collision loop)
 * PoP: w28_repair_1f5388_existing_backups @ persistence/sqlite/_state/repair.py:_existing_malformed_backups
 * PoP: w28_repair_1f5388_prune_victims @ persistence/sqlite/_state/repair.py:_prune_malformed_backups
 * PoP: w28_repair_1f5388_attempts_exhausted @ persistence/sqlite/_state/repair.py:_persistent_repair_attempts_exhausted
 * PoP: w28_repair_1f5388_next_attempts @ persistence/sqlite/_state/repair.py:_record_repair_outcome
 * PoP: w28_repair_1f5388_initial_report @ persistence/sqlite/_state/repair.py:repair_state_db_schema (report shape)
 * PoP: w28_repair_1f5388_repair_skip @ persistence/sqlite/_state/repair.py:_repair_skip
 * PoP: w28_repair_1f5388_run_strategies @ persistence/sqlite/_state/repair.py:_run_repair_strategies
 * PoP: w28_repair_1f5388_repair_flow @ persistence/sqlite/_state/repair.py:repair_state_db_schema (pre-surgery branches)
 * PoP: w28_repair_1f5388_preflight_skip @ persistence/sqlite/_state/repair.py:preflight_db_writability (early return)
 * PoP: w28_repair_1f5388_rw_or_vanished @ persistence/sqlite/_state/repair.py:_rw_or_vanished
 * PoP: w28_repair_1f5388_preflight_error @ persistence/sqlite/_state/repair.py:preflight_db_writability (refusal)
 * PoP: w28_repair_1f5388_unlink_victims @ persistence/sqlite/_state/repair.py:_unlink_db_triple
 * PoP: w28_repair_1f5388_strategies @ persistence/sqlite/_state/repair.py:_REPAIR_STRATEGIES
 *
 * ---------------------------------------------------------------------------
 * WHAT IS A SEAM HERE
 * ---------------------------------------------------------------------------
 * repair.py is an I/O module: sqlite connections, flock/msvcrt locking, stat()
 * and disk_usage(), the ledger sidecar, hashing, the clock, the logger, the
 * startup watchdog.  None of those are ported.  What IS ported is every
 * decision that is a pure function of the values those seams produce:
 *
 *   * the pinned budgets every guard is sized with;
 *   * the volatile-header mask (what changes when a live writer commits, and
 *     therefore what the attempt ledger must NOT key on);
 *   * the three disk-guard verdicts and their exact operator-facing text;
 *   * the snapshot timeout and the two failure classifiers (deterministic
 *     corruption consumes the ledger; contention/IO/full does not);
 *   * integrity_check row classification: which damage an FTS rebuild can
 *     heal, and which is a canonical b-tree the repair ladder cannot touch;
 *   * the byte-exact statement stream of every repair strategy and of the
 *     health probe (nothing is executed here -- the strings are the contract);
 *   * the ledger/backup naming and the attempt arithmetic (including the
 *     "never mint a differently shaped key" rule);
 *   * the repair report shape, the _repair_skip mutation, the escalation
 *     ladder's outcome classification and the pre-surgery branch plan;
 *   * the writability preflight's skip rule and its refusal text.
 *
 * Sizes arrive as numbers, exception facts as flags, paths as already-resolved
 * strings (Path normalisation is a seam: fixtures use strings whose
 * str(Path(x)) is x, which is how the oracles feed the live module).
 */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w28_repair_1f5388_p1.h"
#include "noc_w28_repair_1f5388_p2.h"
#include "noc_w28_repair_1f5388_p3.h"

/* --------------------------------------------------------------- utilities */

static char *rp_dup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static char *rp_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    int n;
    char *buf;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return rp_dup(""); }
    buf = (char *)malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static const char *rp_s(const char *s) { return s ? s : ""; }

/* Python truthiness of a JSON value. */
static int rp_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python str() over the scalar shapes repair.py interpolates. */
static char *rp_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return rp_dup("None");
    switch (v->type) {
    case JSON_NULL:   return rp_dup("None");
    case JSON_BOOL:   return rp_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return rp_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val && fabs(v->num_val) < 1e16)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return rp_dup(buf);
    case JSON_ARRAY:  return rp_dup("[]");
    case JSON_OBJECT: return rp_dup("{}");
    }
    return rp_dup("None");
}

/* Python int(): bool -> 0/1, number -> truncate toward zero, string -> parsed
 * int (surrounding ASCII whitespace allowed).  0 when the value has no int(). */
static int rp_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        if (!(v->num_val == v->num_val) || fabs(v->num_val) == INFINITY) return 0;
        *out = (long long)v->num_val;
        return 1;
    }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        char *end = NULL;
        long long parsed;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        parsed = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end != '\0') return 0;
        *out = parsed;
        return 1;
    }
    return 0;
}

/* Python '==' over scalars (None/str/num/bool; bool counts as a number). */
static int rp_eq(const json_t *a, const json_t *b) {
    int an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    return 0;
}

static char *rp_lower(const char *s) {
    char *p = rp_dup(s);
    size_t i;
    if (!p) return NULL;
    for (i = 0; p[i]; i++) p[i] = (char)tolower((unsigned char)p[i]);
    return p;
}

static int rp_contains(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}

/* Path.name: the last component. */
static char *rp_basename(const char *path) {
    const char *p = rp_s(path), *cut = NULL, *q;
    for (q = p; *q; q++) if (*q == '/' || *q == '\\') cut = q;
    return rp_dup(cut ? cut + 1 : p);
}

/* path.with_name(path.name + suffix): same directory, new final component.
 * Trailing separators are dropped first, as Path does. */
static char *rp_with_suffix(const char *path, const char *suffix) {
    char *base = rp_dup(rp_s(path)), *name, *out;
    size_t n, i;
    if (!base) return NULL;
    n = strlen(base);
    while (n > 1 && (base[n - 1] == '/' || base[n - 1] == '\\')) base[--n] = '\0';
    name = rp_basename(base);
    for (i = n; i > 0; i--) {
        if (base[i - 1] == '/' || base[i - 1] == '\\') break;
    }
    if (i == 0) {
        out = rp_fmt("%s%s", name, suffix);
    } else {
        base[i] = '\0';
        out = rp_fmt("%s%s%s", base, name, suffix);
    }
    free(base);
    free(name);
    return out;
}

static int rp_array_has_str(const json_t *arr, const char *s) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0) return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- constants */

static json_t *rp_pair(double a, double b) {
    json_t *p = json_array();
    json_append(p, json_number(a));
    json_append(p, json_number(b));
    return p;
}

json_t *w28_repair_1f5388_constants(void) {
    json_t *o = json_object(), *arr;
    json_set(o, "repair_lock_timeout_seconds", json_number(120.0));
    json_set(o, "repair_lock_poll_seconds", json_number(0.1));
    json_set(o, "snapshot_min_throughput_bytes_per_second", json_number(10485760.0));
    json_set(o, "max_persistent_repair_attempts", json_number(3.0));
    json_set(o, "max_malformed_backups", json_number(3.0));
    arr = json_array();
    json_append(arr, json_string("-wal"));
    json_append(arr, json_string("-shm"));
    json_append(arr, json_string("-journal"));
    json_set(o, "db_sidecar_suffixes", arr);
    json_set(o, "fingerprint_sample_bytes", json_number(65536.0));
    arr = json_array();
    json_append(arr, rp_pair(24.0, 28.0));
    json_append(arr, rp_pair(92.0, 96.0));
    json_set(o, "fingerprint_volatile_header_ranges", arr);
    json_set(o, "repair_backup_min_free_bytes", json_number(268435456.0));
    json_set(o, "repair_backup_free_fraction", json_number(0.02));
    arr = json_array();
    json_append(arr, json_string("messages_fts"));
    json_append(arr, json_string("messages_fts_trigram"));
    json_set(o, "fts_tables", arr);
    arr = json_array();
    json_append(arr, json_number(3.0));
    json_append(arr, json_number(8.0));
    json_append(arr, json_number(10.0));
    json_append(arr, json_number(13.0));
    json_append(arr, json_number(14.0));
    json_set(o, "temporary_file_failure_codes", arr);
    return o;
}

json_t *w28_repair_1f5388_sidecars(const char *db_path) {
    static const char *const suffixes[3] = {"-wal", "-shm", "-journal"};
    json_t *out = json_array();
    int i;
    for (i = 0; i < 3; i++) {
        char *p = rp_with_suffix(db_path, suffixes[i]);
        json_append(out, json_string(p));
        free(p);
    }
    return out;
}

/* ------------------------------------------------------------------- masks */

json_t *w28_repair_1f5388_mask_header(const char *head_hex) {
    const char *h = rp_s(head_hex);
    size_t len = strlen(h), bytes, i;
    unsigned char *buf;
    char *hex;
    json_t *out;
    bytes = len / 2;
    if (bytes < 96) return json_string(h);
    buf = (unsigned char *)calloc(bytes, 1);
    if (!buf) return json_string(h);
    for (i = 0; i < bytes; i++) {
        int hi = isdigit((unsigned char)h[i * 2]) ? h[i * 2] - '0'
                 : (tolower((unsigned char)h[i * 2]) - 'a' + 10);
        int lo = isdigit((unsigned char)h[i * 2 + 1]) ? h[i * 2 + 1] - '0'
                 : (tolower((unsigned char)h[i * 2 + 1]) - 'a' + 10);
        buf[i] = (unsigned char)(((hi & 0xF) << 4) | (lo & 0xF));
    }
    /* (24, 28) file change counter, (92, 96) version-valid-for. */
    for (i = 24; i < 28; i++) buf[i] = 0;
    for (i = 92; i < 96; i++) buf[i] = 0;
    hex = (char *)malloc(bytes * 2 + 1);
    if (!hex) { free(buf); return json_string(h); }
    for (i = 0; i < bytes; i++) snprintf(hex + i * 2, 3, "%02x", buf[i]);
    hex[bytes * 2] = '\0';
    out = json_string(hex);
    free(hex);
    free(buf);
    return out;
}

/* ------------------------------------------------------------------ budgets */

json_t *w28_repair_1f5388_headroom(double total_bytes) {
    double frac = (double)(long long)(total_bytes * 0.02);
    return json_number(frac > 268435456.0 ? frac : 268435456.0);
}

json_t *w28_repair_1f5388_snapshot_timeout(const json_t *sizes) {
    double total = 0.0, t;
    size_t i, n = (sizes && sizes->type == JSON_ARRAY) ? json_len(sizes) : 0;
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(sizes, i);
        if (v && v->type == JSON_NUMBER) total += v->num_val;
    }
    t = total / 10485760.0;
    return json_number(t > 120.0 ? t : 120.0);
}

/* ---------------------------------------------------------------- disk guard */

json_t *w28_repair_1f5388_disk_budget_error(const char *parent, const char *exc, const char *refusal) {
    char *t = rp_fmt("could not determine free space on %s (%s); refusing the %s rather than risk "
                     "filling the volume", rp_s(parent), rp_s(exc), rp_s(refusal));
    json_t *r = json_string(t);
    free(t);
    return r;
}

json_t *w28_repair_1f5388_scratch_space_error(const char *parent, double snapshot_bytes,
                                              double free_bytes, double headroom_bytes) {
    char *t;
    json_t *r;
    if (free_bytes >= snapshot_bytes + (2 * snapshot_bytes) + headroom_bytes) return json_null();
    t = rp_fmt("only %.2fGB free on %s; the repair snapshot needs up to %.2fGB, VACUUM may need "
               "another %.2fGB, and %.2fGB must remain as headroom. Free disk space, then retry.",
               free_bytes / 1e9, rp_s(parent), snapshot_bytes / 1e9,
               (2 * snapshot_bytes) / 1e9, headroom_bytes / 1e9);
    r = json_string(t);
    free(t);
    return r;
}

json_t *w28_repair_1f5388_manual_hint(const char *db_path) {
    char *t = rp_fmt("Free disk space, then retry (or recover manually with "
                     "`hermes sessions recover --source %s --inspect-only` first).", rp_s(db_path));
    json_t *r = json_string(t);
    free(t);
    return r;
}

json_t *w28_repair_1f5388_backup_free_space_error(const char *parent, double need_bytes,
                                                  double free_bytes, double headroom_bytes,
                                                  const char *db_path) {
    char *hint, *t;
    json_t *r;
    if (free_bytes - need_bytes >= headroom_bytes) return json_null();
    hint = rp_fmt("Free disk space, then retry (or recover manually with "
                  "`hermes sessions recover --source %s --inspect-only` first).", rp_s(db_path));
    t = rp_fmt("only %.2fGB free on %s; copying the damaged DB needs %.2fGB and must leave "
               "%.2fGB headroom. %s",
               free_bytes / 1e9, rp_s(parent), need_bytes / 1e9, headroom_bytes / 1e9, hint);
    r = json_string(t);
    free(t);
    free(hint);
    return r;
}

/* ------------------------------------------------------------- classifiers */

json_t *w28_repair_1f5388_failure_consumes_attempt(int is_db_error, int errorcode_present,
                                                   double errorcode, const char *message) {
    char *low;
    int hit;
    if (!is_db_error) return json_bool(0);
    if (errorcode_present) {
        long long primary = ((long long)errorcode) & 0xFF;
        return json_bool(primary == 11 || primary == 26); /* SQLITE_CORRUPT / SQLITE_NOTADB */
    }
    low = rp_lower(rp_s(message));
    hit = rp_contains(low, "file is not a database") ||
          rp_contains(low, "database disk image is malformed");
    free(low);
    return json_bool(hit);
}

json_t *w28_repair_1f5388_temporary_file_failure(int errorcode_present, double errorcode,
                                                 const char *message) {
    static const char *const markers[6] = {
        "no space left on device", "not enough space", "database or disk is full",
        "disk full", "full disk", "enospc",
    };
    char *low;
    int i, hit = 0;
    if (errorcode_present) {
        long long primary = ((long long)errorcode) & 0xFF;
        if (primary == 3 || primary == 8 || primary == 10 || primary == 13 || primary == 14)
            return json_bool(1);
    }
    low = rp_lower(rp_s(message));
    for (i = 0; i < 6 && !hit; i++) hit = rp_contains(low, markers[i]);
    free(low);
    return json_bool(hit);
}

json_t *w28_repair_1f5388_schema_not_built(const char *message) {
    char *low = rp_lower(rp_s(message));
    int hit = rp_contains(low, "no such table") || rp_contains(low, "no such column");
    free(low);
    return json_bool(hit);
}

/* --------------------------------------------------- integrity classification */

#define RP_WORD(ch) (isalnum((unsigned char)(ch)) || (ch) == '_')

/* _FTS_OBJECT_RE.fullmatch:
 * messages_fts(_trigram|_cjk)?(_data|_idx|_content|_docsize|_config|_segdir|_segments)? */
static int rp_fts_object(const char *name) {
    static const char *const shadows[7] = {
        "_data", "_idx", "_content", "_docsize", "_config", "_segdir", "_segments",
    };
    const char *p = rp_s(name), *rest;
    size_t i;
    if (strncmp(p, "messages_fts", 12) != 0) return 0;
    rest = p + 12;
    if (strncmp(rest, "_trigram", 8) == 0) rest += 8;
    else if (strncmp(rest, "_cjk", 4) == 0) rest += 4;
    if (*rest == '\0') return 1;
    for (i = 0; i < 7; i++)
        if (strcmp(rest, shadows[i]) == 0) return 1;
    return 0;
}

/* re.search(r"\bTree (\d+)\b", text) -> the number. */
static int rp_tree_number(const char *text, long long *out) {
    const char *p = rp_s(text), *q;
    for (q = p; *q; q++) {
        const char *d;
        long long v = 0;
        size_t k = 0;
        if (*q != 'T' || strncmp(q, "Tree ", 5) != 0) continue;
        if (q != p && RP_WORD(q[-1])) continue;
        d = q + 5;
        while (isdigit((unsigned char)*d)) { v = v * 10 + (*d - '0'); d++; k++; }
        if (k == 0) continue;
        if (*d && RP_WORD(*d)) continue;
        *out = v;
        return 1;
    }
    return 0;
}

/* re.search(r"missing from index (\S+)", text) -> the captured name. */
static int rp_missing_index(const char *text, char *buf, size_t cap) {
    static const char *const needle = "missing from index ";
    const char *p = strstr(rp_s(text), needle);
    size_t n = 0;
    if (!p) return 0;
    p += strlen(needle);
    while (p[n] && !isspace((unsigned char)p[n]) && n + 1 < cap) {
        buf[n] = p[n];
        n++;
    }
    if (n == 0) return 0;
    buf[n] = '\0';
    return 1;
}

json_t *w28_repair_1f5388_integrity_structural(const json_t *lines, const json_t *master_rows) {
    long long pages[512];
    char *names[512];
    size_t used = 0, i, nrows, nlines;
    int structural = 0;

    nrows = (master_rows && master_rows->type == JSON_ARRAY) ? json_len(master_rows) : 0;
    for (i = 0; i < nrows && used < 512; i++) {
        const json_t *row = json_get(master_rows, i);
        const json_t *rp = (row && row->type == JSON_ARRAY && json_len(row) > 0) ? json_get(row, 0) : NULL;
        const json_t *nm = (row && row->type == JSON_ARRAY && json_len(row) > 2) ? json_get(row, 2) : NULL;
        long long page;
        if (!rp || !rp_truthy(rp) || !rp_int(rp, &page)) continue;
        pages[used] = page;
        names[used] = nm ? rp_pystr(nm) : rp_dup("None");
        used++;
    }

    nlines = (lines && lines->type == JSON_ARRAY) ? json_len(lines) : 0;
    for (i = 0; i < nlines && !structural; i++) {
        const json_t *lv = json_get(lines, i);
        char *text = rp_pystr(lv);
        char missing[512];
        long long page;
        size_t k;
        if (strncmp(text, "Freelist", 8) == 0) { free(text); return json_bool(1); }
        if (rp_tree_number(text, &page)) {
            char *mapped = NULL;
            for (k = 0; k < used; k++) if (pages[k] == page) mapped = names[k]; /* last wins */
            if (mapped && mapped[0] && !rp_fts_object(mapped)) structural = 1;
        }
        if (!structural && rp_missing_index(text, missing, sizeof missing) && !rp_fts_object(missing))
            structural = 1;
        free(text);
    }
    for (i = 0; i < used; i++) free(names[i]);
    return json_bool(structural);
}

json_t *w28_repair_1f5388_probe_reason(const json_t *rows, int all_ok) {
    size_t i, n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    char *problems[3];
    size_t count = 0;
    char *joined;
    json_t *r;

    if (all_ok) return json_null();
    for (i = 0; i < n && count < 3; i++) {
        const json_t *row = json_get(rows, i);
        char *text, *low;
        if (!row) continue;
        if (row->type == JSON_ARRAY && json_len(row) == 0) continue;  /* `if r` */
        text = row->type == JSON_ARRAY ? rp_pystr(json_get(row, 0)) : rp_pystr(row);
        low = rp_lower(text);
        if (strcmp(low, "ok") != 0) {
            problems[count++] = text;
            free(low);
        } else {
            free(low);
            free(text);
        }
    }
    if (count == 0) return json_null();
    joined = rp_dup(problems[0]);
    for (i = 1; i < count; i++) {
        char *next = rp_fmt("%s; %s", joined, problems[i]);
        free(joined);
        joined = next;
    }
    r = json_string(joined);
    for (i = 0; i < count; i++) free(problems[i]);
    free(joined);
    return r;
}

/* -------------------------------------------------------- statement streams */

static json_t *rp_call(const char *sql, const json_t *params) {
    json_t *o = json_object();
    json_set(o, "sql", json_string(sql));
    json_set(o, "params", params ? json_copy(params) : json_array());
    return o;
}

static json_t *rp_calls1(const char *a) {
    json_t *arr = json_array();
    json_append(arr, rp_call(a, NULL));
    return arr;
}

json_t *w28_repair_1f5388_strategy_statements(const char *name, const json_t *dupes,
                                              double schema_version) {
    const char *n = rp_s(name);
    json_t *arr;
    long long cur = (long long)schema_version;
    long long next = (cur + 1) & 0x7FFFFFFF;
    char *bump = rp_fmt("PRAGMA schema_version=%lld", next);

    if (strcmp(n, "rebuild_fts") == 0) {
        arr = json_array();
        json_append(arr, rp_call("INSERT INTO messages_fts(messages_fts) VALUES('rebuild')", NULL));
        json_append(arr, rp_call("INSERT INTO messages_fts_trigram(messages_fts_trigram) VALUES('rebuild')", NULL));
        free(bump);
        return arr;
    }
    if (strcmp(n, "reindex_btree") == 0) {
        free(bump);
        return rp_calls1("REINDEX");
    }
    if (strcmp(n, "dedup_schema") == 0) {
        size_t i, nd = (dupes && dupes->type == JSON_ARRAY) ? json_len(dupes) : 0;
        arr = json_array();
        json_append(arr, rp_call("PRAGMA writable_schema=ON", NULL));
        json_append(arr, rp_call("SELECT type, name, COUNT(*) AS c, MIN(rowid) AS keep "
                                 "FROM sqlite_master GROUP BY type, name HAVING c > 1", NULL));
        for (i = 0; i < nd; i++) {
            const json_t *row = json_get(dupes, i);
            json_t *params = json_array();
            if (row && row->type == JSON_ARRAY && json_len(row) >= 4) {
                json_append(params, json_copy(json_get(row, 0)));
                json_append(params, json_copy(json_get(row, 1)));
                json_append(params, json_copy(json_get(row, 3)));
            }
            json_append(arr, rp_call("DELETE FROM sqlite_master WHERE type IS ? AND name IS ? "
                                     "AND rowid <> ?", params));
            json_free(params);
        }
        /* _edit_sqlite_master bumps the cookie only when edit() reports a
         * change, i.e. only when sqlite_master actually held duplicates. */
        if (nd > 0) {
            json_append(arr, rp_call("PRAGMA schema_version", NULL));
            json_append(arr, rp_call(bump, NULL));
        }
        json_append(arr, rp_call("PRAGMA writable_schema=OFF", NULL));
        free(bump);
        return arr;
    }
    if (strcmp(n, "drop_fts_rebuild") == 0) {
        arr = json_array();
        json_append(arr, rp_call("PRAGMA writable_schema=ON", NULL));
        json_append(arr, rp_call("DELETE FROM sqlite_master WHERE name LIKE 'messages_fts%'", NULL));
        json_append(arr, rp_call("PRAGMA schema_version", NULL));
        json_append(arr, rp_call(bump, NULL));
        json_append(arr, rp_call("PRAGMA writable_schema=OFF", NULL));
        json_append(arr, rp_call("VACUUM", NULL));
        free(bump);
        return arr;
    }
    free(bump);
    return json_null();
}

json_t *w28_repair_1f5388_probe_plan(void) {
    static const char *const plan[11] = {
        "PRAGMA journal_mode",
        "PRAGMA integrity_check",
        "SELECT COUNT(*) FROM sessions",
        "SELECT 1 FROM messages_fts WHERE messages_fts MATCH '\"\"' LIMIT 1",
        "SELECT 1 FROM messages_fts_trigram WHERE messages_fts_trigram MATCH '\"\"' LIMIT 1",
        "BEGIN IMMEDIATE",
        "INSERT INTO sessions (id, source, started_at) VALUES (?, ?, ?)",
        "INSERT INTO messages (session_id, role, content, timestamp) VALUES (?, ?, ?, ?)",
        "INSERT INTO messages_fts(messages_fts) VALUES('flush')",
        "INSERT INTO messages_fts_trigram(messages_fts_trigram) VALUES('flush')",
        "ROLLBACK",
    };
    json_t *arr = json_array();
    int i;
    for (i = 0; i < 11; i++) json_append(arr, json_string(plan[i]));
    return arr;
}

json_t *w28_repair_1f5388_schema_version_sql(double current) {
    long long cur = (long long)current;
    long long next = (cur + 1) & 0x7FFFFFFF;
    char *sql = rp_fmt("PRAGMA schema_version=%lld", next);
    json_t *r = json_string(sql);
    free(sql);
    return r;
}

/* ------------------------------------------------------ ledger / backup names */

json_t *w28_repair_1f5388_fingerprint_text(double size, const char *digest_hex) {
    const char *d = rp_s(digest_hex);
    char head[33];
    size_t i;
    char *t;
    json_t *r;
    for (i = 0; i < 32 && d[i]; i++) head[i] = d[i];
    head[i] = '\0';
    t = rp_fmt("%.0f:%s", size, head);
    r = json_string(t);
    free(t);
    return r;
}

json_t *w28_repair_1f5388_ledger_name(const char *db_name) {
    char *t = rp_fmt("%s.repair-attempts.json", rp_s(db_name));
    json_t *r = json_string(t);
    free(t);
    return r;
}

json_t *w28_repair_1f5388_scratch_name(const char *db_name) {
    char *t = rp_fmt("%s.repair-scratch", rp_s(db_name));
    json_t *r = json_string(t);
    free(t);
    return r;
}

json_t *w28_repair_1f5388_staging_name(const char *db_name, const char *stamp) {
    char *t = rp_fmt("%s.backup-staging-%s", rp_s(db_name), rp_s(stamp));
    json_t *r = json_string(t);
    free(t);
    return r;
}

json_t *w28_repair_1f5388_backup_name(const char *db_name, const char *stamp, const json_t *existing) {
    char *base = rp_fmt("%s.malformed-backup-%s", rp_s(db_name), rp_s(stamp));
    int seq;
    json_t *r;
    if (!rp_array_has_str(existing, base)) {
        r = json_string(base);
        free(base);
        return r;
    }
    for (seq = 1; seq < 100000; seq++) {
        char *cand = rp_fmt("%s_%d", base, seq);
        if (!rp_array_has_str(existing, cand)) {
            r = json_string(cand);
            free(cand);
            free(base);
            return r;
        }
        free(cand);
    }
    r = json_string(base);
    free(base);
    return r;
}

static int rp_name_cmp_desc(const void *a, const void *b) {
    const char *const *x = (const char *const *)a;
    const char *const *y = (const char *const *)b;
    return -strcmp(*x, *y);
}

static int rp_ends_with(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

/* _existing_malformed_backups over an explicit name list: prefix match, not a
 * sidecar of one, newest name first. */
static char **rp_collect_backups(const char *db_name, const json_t *names, size_t *out_n) {
    char *prefix = rp_fmt("%s.malformed-backup-", rp_s(db_name));
    size_t plen = strlen(prefix);
    char **keep = NULL;
    size_t n = 0, cap = 0, i, len = (names && names->type == JSON_ARRAY) ? json_len(names) : 0;
    for (i = 0; i < len; i++) {
        const json_t *v = json_get(names, i);
        const char *nm;
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        nm = v->str_val;
        if (strncmp(nm, prefix, plen) != 0) continue;
        if (rp_ends_with(nm, "-wal") || rp_ends_with(nm, "-shm") || rp_ends_with(nm, "-journal"))
            continue;
        if (n == cap) {
            size_t ncap = cap ? cap * 2 : 8;
            char **grown = (char **)realloc(keep, ncap * sizeof(char *));
            if (!grown) break;
            keep = grown;
            cap = ncap;
        }
        keep[n++] = rp_dup(nm);
    }
    free(prefix);
    if (n > 1) qsort(keep, n, sizeof(char *), rp_name_cmp_desc);
    *out_n = n;
    return keep;
}

json_t *w28_repair_1f5388_existing_backups(const char *db_name, const json_t *names) {
    json_t *out = json_array();
    size_t n = 0, i;
    char **keep = rp_collect_backups(db_name, names, &n);
    for (i = 0; i < n; i++) {
        json_append(out, json_string(keep[i]));
        free(keep[i]);
    }
    free(keep);
    return out;
}

json_t *w28_repair_1f5388_prune_victims(const char *db_name, const json_t *names, double keep) {
    static const char *const suffixes[3] = {"-wal", "-shm", "-journal"};
    json_t *out = json_array();
    size_t n = 0, i;
    long long cap = (long long)keep;
    char **found = rp_collect_backups(db_name, names, &n);
    if (cap < 0) cap = 0;
    for (i = (size_t)cap; i < n; i++) {
        int s;
        json_append(out, json_string(found[i]));
        for (s = 0; s < 3; s++) {
            char *sc = rp_with_suffix(found[i], suffixes[s]);
            json_append(out, json_string(sc));
            free(sc);
        }
    }
    for (i = 0; i < n; i++) free(found[i]);
    free(found);
    return out;
}

/* -------------------------------------------------------- ledger arithmetic */

json_t *w28_repair_1f5388_attempts_exhausted(const json_t *recorded, const json_t *fp,
                                             double db_size, const json_t *failed_attempts) {
    int same;
    long long attempts = 0;

    if (fp && fp->type != JSON_NULL) {
        same = rp_eq(recorded, fp);
    } else {
        char *prefix = rp_fmt("%.0f:", db_size);
        same = recorded && recorded->type == JSON_STRING && recorded->str_val &&
               strncmp(recorded->str_val, prefix, strlen(prefix)) == 0;
        free(prefix);
    }
    if (!same) return json_bool(0);
    if (!rp_int(failed_attempts, &attempts)) return json_bool(0);
    return json_bool(attempts >= 3);
}

json_t *w28_repair_1f5388_next_attempts(const json_t *recorded, const json_t *fp,
                                        const json_t *failed_attempts) {
    int have_recorded = recorded && recorded->type == JSON_STRING;
    int have_fp = fp && fp->type != JSON_NULL;
    long long attempts = 0;
    int same;

    if (!have_fp) {
        /* No safe way to mint a key: extend only a key the ledger already holds. */
        if (!have_recorded) return json_null();
        same = 1;  /* fp = recorded */
    } else {
        same = rp_eq(recorded, fp);
    }
    if (same) {
        rp_int(failed_attempts, &attempts);
        return json_number((double)(attempts + 1));
    }
    return json_number(1.0);
}

/* ------------------------------------------------------------------ report */

json_t *w28_repair_1f5388_initial_report(void) {
    json_t *o = json_object();
    json_set(o, "repaired", json_bool(0));
    json_set(o, "strategy", json_null());
    json_set(o, "backup_path", json_null());
    json_set(o, "error", json_null());
    return o;
}

json_t *w28_repair_1f5388_repair_skip(const json_t *report, int has_exc, int is_db_error,
                                      int errorcode_present, double errorcode,
                                      const char *message, const char *error) {
    json_t *out = (report && report->type == JSON_OBJECT) ? json_copy(report) : json_object();
    if (has_exc && is_db_error) {
        int consumes;
        if (errorcode_present) {
            long long primary = ((long long)errorcode) & 0xFF;
            consumes = (primary == 11 || primary == 26);
        } else {
            char *low = rp_lower(rp_s(message));
            consumes = rp_contains(low, "file is not a database") ||
                       rp_contains(low, "database disk image is malformed");
            free(low);
        }
        if (consumes) json_set(out, "_repair_attempted", json_bool(1));
    }
    json_set(out, "error", json_string(rp_s(error)));
    return out;
}

json_t *w28_repair_1f5388_run_strategies(const json_t *reasons) {
    static const char *const names[4] = {
        "rebuild_fts", "reindex_btree", "dedup_schema", "drop_fts_rebuild",
    };
    json_t *rep = json_object();
    size_t i;

    json_set(rep, "_repair_attempted", json_bool(1));
    for (i = 0; i < 4; i++) {
        const json_t *reason = json_get(reasons, i);
        if (reason && reason->type == JSON_OBJECT) {
            char *text = rp_pystr(json_obj_get(reason, "unavailable"));
            char *msg = rp_fmt("could not check repaired snapshot: %s", text);
            json_set(rep, "_repair_attempted", json_bool(0));
            json_set(rep, "error", json_string(msg));
            free(msg);
            free(text);
            return rep;
        }
        if (!reason || reason->type == JSON_NULL) {
            json_set(rep, "repaired", json_bool(1));
            json_set(rep, "strategy", json_string(names[i]));
            return rep;
        }
        /* A probe reason: the strategy failed.  Only the final strategy's
         * failure becomes report["error"]; the rest are logged and skipped. */
        if (i == 3) json_set(rep, "error", json_copy(reason));
    }
    return rep;
}

json_t *w28_repair_1f5388_repair_flow(const char *scenario, const char *db_path,
                                      const char *profile_arg) {
    const char *s = rp_s(scenario);
    json_t *rep = json_object();

    json_set(rep, "repaired", json_bool(0));
    json_set(rep, "strategy", json_null());
    json_set(rep, "backup_path", json_null());
    json_set(rep, "error", json_null());

    if (strcmp(s, "missing") == 0) {
        char *t = rp_fmt("%s does not exist", rp_s(db_path));
        json_set(rep, "error", json_string(t));
        free(t);
        return rep;
    }
    if (strcmp(s, "exhausted") == 0) {
        char *ledger = rp_fmt("%s.repair-attempts.json", rp_s(db_path));
        char *t = rp_fmt(
            "automatic repair has already failed 3 times on this exact file \xe2\x80\x94 the "
            "corruption is beyond the schema/FTS repair strategies (likely b-tree page damage). Manual recovery "
            "required: restore a backup, or salvage with `hermes %ssessions recover --source %s "
            "--inspect-only`, then (if it reports recoverable) `hermes %ssessions recover --source %s "
            "--output recovered-state.db` (recovery snapshots the damaged file first, then runs the page-level "
            "`.recover` lane on the copy; do NOT point a raw `sqlite3` shell at the live database). "
            "Delete %s to force another automatic attempt.",
            rp_s(profile_arg), rp_s(db_path), rp_s(profile_arg), rp_s(db_path), ledger);
        json_set(rep, "error", json_string(t));
        free(t);
        free(ledger);
        return rep;
    }
    if (strcmp(s, "lock_lost_healed") == 0) {
        json_set(rep, "repaired", json_bool(1));
        json_set(rep, "strategy", json_string("repaired_by_other_process"));
        return rep;
    }
    if (strcmp(s, "lock_lost") == 0) {
        json_set(rep, "error", json_string(
            "could not obtain the state.db repair lock (held by another process, or the lock "
            "file was unopenable); skipped schema surgery to avoid racing a concurrent repairer"));
        return rep;
    }
    if (strcmp(s, "live_writer") == 0) {
        json_set(rep, "error", json_string(
            "a live writer still holds state.db; skipped schema surgery to avoid tearing "
            "b-tree pages under a concurrent writer. Stop the gateway (hermes gateway stop) and retry."));
        return rep;
    }
    return rep;
}

json_t *w28_repair_1f5388_preflight_skip(const char *db_path) {
    const char *p = rp_s(db_path);
    return json_bool(strcmp(p, ":memory:") == 0 || strncmp(p, "file:", 5) == 0);
}

json_t *w28_repair_1f5388_rw_or_vanished(int rw_first, int exists, int rw_second) {
    return json_bool(rw_first || !exists || rw_second);
}

json_t *w28_repair_1f5388_preflight_error(const char *db_label, const char *path, int is_dir) {
    char *name = rp_basename(path);
    size_t nl = strlen(name);
    int wal = nl >= 4 && strcmp(name + nl - 4, "-wal") == 0;
    const char *wal_note = wal
        ? " Do NOT delete the -wal file \xe2\x80\x94 it contains committed data that "
          "will be merged into the database once it is writable."
        : "";
    char *t = rp_fmt("%s is not writable: %s %s is read-only for this user. Hermes "
                     "needs read-write access to open the database. Fix with: chmod u+rw%s '%s' "
                     "(files owned by another user may need sudo/chown).%s",
                     rp_s(db_label), is_dir ? "directory" : "file", rp_s(path),
                     is_dir ? "x" : "", rp_s(path), wal_note);
    json_t *r = json_string(t);
    free(t);
    free(name);
    return r;
}

json_t *w28_repair_1f5388_unlink_victims(const char *db_path) {
    static const char *const suffixes[3] = {"-wal", "-shm", "-journal"};
    json_t *out = json_array();
    int i;
    json_append(out, json_string(rp_s(db_path)));
    for (i = 0; i < 3; i++) {
        char *p = rp_with_suffix(db_path, suffixes[i]);
        json_append(out, json_string(p));
        free(p);
    }
    return out;
}

json_t *w28_repair_1f5388_strategies(void) {
    static const char *const names[4] = {
        "rebuild_fts", "reindex_btree", "dedup_schema", "drop_fts_rebuild",
    };
    static const char *const success[4] = {
        "state.db FTS indexes rebuilt in place (schema preserved): %s",
        "state.db B-tree indexes rebuilt via REINDEX: %s",
        "state.db schema repaired by de-duplicating sqlite_master (FTS index preserved): %s",
        "state.db schema repaired by dropping FTS schema; indexes will rebuild from messages on next open: %s",
    };
    static const char *const failure[4] = {
        "state.db FTS in-place rebuild pass failed: %s",
        "state.db REINDEX pass failed: %s",
        "state.db dedup repair pass failed: %s",
        NULL,
    };
    json_t *arr = json_array();
    int i;
    for (i = 0; i < 4; i++) {
        json_t *o = json_object();
        json_set(o, "name", json_string(names[i]));
        json_set(o, "success", json_string(success[i]));
        json_set(o, "failure", failure[i] ? json_string(failure[i]) : json_null());
        json_append(arr, o);
    }
    return arr;
}

