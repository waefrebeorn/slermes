/*
 * readpool.c - C11 port of upstream persistence/sqlite/_state/readpool.py
 * (gate w23_readpool_4f9a79).
 *
 * PoP: w23_readpool_4f9a79_consts @ persistence/sqlite/_state/readpool.py:_READ_POOL_MAX
 * PoP: w23_readpool_4f9a79_fd_headroom @ persistence/sqlite/_state/readpool.py:_fd_headroom_ok
 * PoP: w23_readpool_4f9a79_fd_soft_limit @ persistence/sqlite/_state/readpool.py:_fd_soft_limit
 * PoP: w23_readpool_4f9a79_fd_count @ persistence/sqlite/_state/readpool.py:_open_fd_count
 * PoP: w23_readpool_4f9a79_budget_key @ persistence/sqlite/_state/readpool.py:_read_budget_key
 * PoP: w23_readpool_4f9a79_acquire @ persistence/sqlite/_state/readpool.py:_PathReadBudget.acquire
 * PoP: w23_readpool_4f9a79_register @ persistence/sqlite/_state/readpool.py:_PathReadBudget.register
 * PoP: w23_readpool_4f9a79_note_exhausted @ persistence/sqlite/_state/readpool.py:note_read_pool_exhausted
 *
 * The OS seams (RLIMIT_NOFILE, the open-descriptor count, the monotonic clock,
 * the live holders on a path, the logger) are injected by the caller; this file
 * owns only the decision ladder, the permit arithmetic and the exact wording of
 * the two diagnostics.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_readpool_4f9a79.h"

/* ---------------------------------------------------------------- constants */

json_t *w23_readpool_4f9a79_consts(void) {
    json_t *o = json_object();
    json_set(o, "pool_max", json_number((double)W23_READPOOL_4F9A79_POOL_MAX));
    json_set(o, "process_max", json_number((double)W23_READPOOL_4F9A79_PROCESS_MAX));
    json_set(o, "handles_warn", json_number((double)W23_READPOOL_4F9A79_HANDLES_WARN));
    json_set(o, "fd_reserve", json_number((double)W23_READPOOL_4F9A79_FD_RESERVE));
    json_set(o, "fd_cache_seconds", json_number(W23_READPOOL_4F9A79_FD_CACHE_SECONDS));
    json_set(o, "pool_warn_seconds", json_number(W23_READPOOL_4F9A79_POOL_WARN_SECONDS));
    return o;
}

/* ------------------------------------------------------------- fd headroom */

static int rp_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

/* Python's None: a missing key or an explicit JSON null. */
static int rp_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* _fd_headroom_ok: fails OPEN when unmeasurable, CLOSED only on evidence. */
json_t *w23_readpool_4f9a79_fd_headroom(const json_t *req) {
    const json_t *soft_v = req ? json_obj_get(req, "soft") : NULL;
    const json_t *cached_v = req ? json_obj_get(req, "cached") : NULL;
    const json_t *stamp_v = req ? json_obj_get(req, "stamp") : NULL;
    const json_t *probe_v = req ? json_obj_get(req, "probe") : NULL;
    double now = json_get_num(req, "now", 0.0);
    double stamp = rp_is_num(stamp_v) ? stamp_v->num_val : 0.0;
    json_t *out = json_object();
    int ok;

    /* soft is None: no usable rlimit, so nothing can be measured -> True. */
    if (!rp_is_num(soft_v)) {
        json_set(out, "ok", json_bool(1));
        json_set(out, "cached", rp_is_null(cached_v) ? json_null() : json_copy((json_t *)cached_v));
        json_set(out, "probed", json_bool(0));
        return out;
    }

    if (rp_is_null(cached_v) || !((now - stamp) < W23_READPOOL_4F9A79_FD_CACHE_SECONDS)) {
        /* stale (or absent) count: probe once and restamp the cache. */
        if (rp_is_num(probe_v)) {
            long long probe = (long long)probe_v->num_val;
            ok = probe >= 0 &&
                 ((long long)soft_v->num_val - probe) > (long long)W23_READPOOL_4F9A79_FD_RESERVE;
            json_set(out, "ok", json_bool(ok));
            json_set(out, "cached", json_number((double)probe));
        } else {
            /* unmeasurable -> fail open */
            json_set(out, "ok", json_bool(1));
            json_set(out, "cached", json_null());
        }
        json_set(out, "probed", json_bool(1));
        return out;
    }

    /* fresh cache: decide on the remembered count. */
    {
        long long cached = (long long)cached_v->num_val;
        ok = cached >= 0 &&
             ((long long)soft_v->num_val - cached) > (long long)W23_READPOOL_4F9A79_FD_RESERVE;
    }
    json_set(out, "ok", json_bool(ok));
    json_set(out, "cached", json_copy((json_t *)cached_v));
    json_set(out, "probed", json_bool(0));
    return out;
}

/* ------------------------------------------------------------- fd soft limit */

/* _fd_soft_limit: None when there is no usable soft RLIMIT_NOFILE. */
json_t *w23_readpool_4f9a79_fd_soft_limit(const json_t *req) {
    const json_t *soft_v = req ? json_obj_get(req, "soft") : NULL;
    const char *raise_at = json_get_str(req, "raise", NULL);
    int have_module = json_get_bool(req, "module", 1) ? 1 : 0;
    int infinity = json_get_bool(req, "infinity", 0) ? 1 : 0;
    json_t *out = json_object();

    /* import resource -> ImportError */
    if (!have_module) { json_set(out, "soft", json_null()); return out; }
    /* resource has no getrlimit, or the call itself raised */
    if (raise_at && *raise_at) { json_set(out, "soft", json_null()); return out; }
    if (infinity || !rp_is_num(soft_v)) { json_set(out, "soft", json_null()); return out; }
    if ((long long)soft_v->num_val == -1) { json_set(out, "soft", json_null()); return out; }
    json_set(out, "soft", json_number((double)(long long)soft_v->num_val));
    return out;
}

/* ------------------------------------------------------------- open fd count */

/* _open_fd_count: len(listing), -1 when the probe itself starved, None when
 * every candidate directory is unmeasurable. */
json_t *w23_readpool_4f9a79_fd_count(const json_t *req) {
    const json_t *dirs = req ? json_obj_get(req, "dirs") : NULL;
    size_t i, n = (dirs && dirs->type == JSON_ARRAY) ? json_len(dirs) : 0;
    json_t *out = json_object();
    for (i = 0; i < n; i++) {
        json_t *d = json_get(dirs, i);
        const char *err = d ? json_get_str(d, "error", NULL) : NULL;
        if (err && (!strcmp(err, "emfile") || !strcmp(err, "enfile"))) {
            json_set(out, "count", json_number(-1.0));
            return out;
        }
        if (err && *err) continue; /* any other OSError: try the next dir */
        {
            json_t *entries = d ? json_obj_get(d, "entries") : NULL;
            size_t m = (entries && entries->type == JSON_ARRAY) ? json_len(entries) : 0;
            json_set(out, "count", json_number((double)m));
            return out;
        }
    }
    json_set(out, "count", json_null());
    return out;
}

/* ---------------------------------------------------------------- path key */

#define RP_MAXSEG 32
#define RP_SEGLEN 192

typedef struct {
    char seg[RP_MAXSEG][RP_SEGLEN];
    size_t n;
} rp_segs;

static void rp_split(const char *s, rp_segs *out, char *drive, int *has_drive, int *absolute) {
    size_t len = strlen(s), i = 0, bl = 0;
    char buf[RP_SEGLEN];
    out->n = 0;
    *has_drive = 0;
    *absolute = 0;
    drive[0] = '\0';
    drive[1] = '\0';
    if (len >= 2 && s[1] == ':') {
        *has_drive = 1;
        drive[0] = s[0];
        i = 2;
    }
    if (i < len && (s[i] == '\\' || s[i] == '/')) *absolute = 1;
    for (; i <= len; i++) {
        char c = (i < len) ? s[i] : '\0';
        if (c == '\\' || c == '/' || c == '\0') {
            buf[bl] = '\0';
            /* PureWindowsPath drops empty and "." components, keeps ".." */
            if (bl > 0 && !(bl == 1 && buf[0] == '.')) {
                if (out->n < RP_MAXSEG) {
                    memcpy(out->seg[out->n], buf, bl + 1);
                    out->n++;
                }
            }
            bl = 0;
            if (c == '\0') break;
            continue;
        }
        if (bl + 1 < RP_SEGLEN) buf[bl++] = c;
    }
}

static void rp_join(char *out, size_t cap, const char *drive, int has_drive,
                    int absolute, const rp_segs *segs) {
    size_t used = 0, i;
    out[0] = '\0';
    if (has_drive && used + 2 < cap) {
        out[used++] = drive[0];
        out[used++] = ':';
        out[used] = '\0';
    }
    if (absolute && used + 1 < cap) {
        out[used++] = '\\';
        out[used] = '\0';
    }
    for (i = 0; i < segs->n; i++) {
        size_t sl = strlen(segs->seg[i]);
        if (used && out[used - 1] != '\\' && used + 1 < cap) out[used++] = '\\';
        if (used + sl < cap) {
            memcpy(out + used, segs->seg[i], sl);
            used += sl;
        }
        out[used] = '\0';
    }
}

/* str(Path(p)) */
static void rp_path_str(const char *p, char *out, size_t cap) {
    rp_segs segs;
    char drive[2];
    int has_drive, absolute;
    rp_split(p ? p : "", &segs, drive, &has_drive, &absolute);
    rp_join(out, cap, drive, has_drive, absolute, &segs);
}

/* Path(p).resolve() for an absolute, non-symlinked path: normalise, collapse
 * "..", upper-case the drive letter. */
static void rp_path_resolve(const char *p, char *out, size_t cap) {
    rp_segs segs, kept;
    char drive[2];
    int has_drive, absolute;
    size_t i;
    rp_split(p ? p : "", &segs, drive, &has_drive, &absolute);
    kept.n = 0;
    for (i = 0; i < segs.n; i++) {
        if (!strcmp(segs.seg[i], "..")) {
            if (kept.n > 0 && strcmp(kept.seg[kept.n - 1], "..")) kept.n--;
            else if (!absolute && kept.n < RP_MAXSEG) {
                memcpy(kept.seg[kept.n], "..", 3);
                kept.n++;
            }
            continue;
        }
        if (kept.n < RP_MAXSEG) {
            size_t sl = strlen(segs.seg[i]);
            memcpy(kept.seg[kept.n], segs.seg[i], sl + 1);
            kept.n++;
        }
    }
    if (has_drive && drive[0] >= 'a' && drive[0] <= 'z')
        drive[0] = (char)(drive[0] - 'a' + 'A');
    rp_join(out, cap, drive, has_drive, absolute, &kept);
}

json_t *w23_readpool_4f9a79_budget_key(const json_t *req) {
    const char *path = json_get_str(req, "path", "");
    char buf[1024];
    json_t *out = json_object();
    rp_path_resolve(path ? path : "", buf, sizeof(buf));
    if (!buf[0]) rp_path_str(path ? path : "", buf, sizeof(buf));
    json_set(out, "key", json_string(buf));
    return out;
}

/* ------------------------------------------------------------------ permits */

/* _PathReadBudget.acquire ladder over an injected world: fd headroom, then the
 * process ceiling (reclaiming one idle read anywhere), then this file's ceiling
 * (reclaiming one idle read from a member that is not the requester). */
json_t *w23_readpool_4f9a79_acquire(const json_t *req) {
    int fd_ok = json_get_bool(req, "fd_ok", 1) ? 1 : 0;
    long process_used = (long)json_get_num(req, "process_used", 0.0);
    long path_used = (long)json_get_num(req, "path_used", 0.0);
    long requester = (long)json_get_num(req, "requester", -1.0);
    const json_t *members = req ? json_obj_get(req, "members") : NULL;
    const json_t *ops = req ? json_obj_get(req, "ops") : NULL;
    int evictable[RP_MAXSEG];
    size_t nm = 0, nops, i;
    long denied_fd = 0, evicted = 0;
    json_t *results = json_array();
    json_t *out = json_object();

    for (i = 0; i < RP_MAXSEG; i++) evictable[i] = 0;
    if (members && members->type == JSON_ARRAY) {
        nm = json_len(members);
        if (nm > RP_MAXSEG) nm = RP_MAXSEG;
        for (i = 0; i < nm; i++) {
            json_t *m = json_get(members, i);
            evictable[i] = (m && json_get_bool(m, "evictable", 0)) ? 1 : 0;
        }
    }

    nops = (ops && ops->type == JSON_ARRAY) ? json_len(ops) : 0;
    for (i = 0; i < nops; i++) {
        json_t *opv = json_get(ops, i);
        const char *op = json_get_str(opv, "op", "");
        size_t j;
        if (op && !strcmp(op, "release")) {
            /* budget.release(): both permits go back */
            if (path_used > 0) path_used--;
            if (process_used > 0) process_used--;
            json_append(results, json_bool(1));
            continue;
        }
        if (!fd_ok) {
            denied_fd++;
            json_append(results, json_bool(0));
            continue;
        }
        if (process_used >= W23_READPOOL_4F9A79_PROCESS_MAX) {
            /* _reclaim_idle_read_conn_anywhere(): any budget, no exclusion */
            int freed = 0;
            for (j = 0; j < nm; j++) {
                if (evictable[j]) {
                    evictable[j] = 0;
                    if (path_used > 0) path_used--;
                    if (process_used > 0) process_used--;
                    evicted++;
                    freed = 1;
                    break;
                }
            }
            if (!freed) {
                json_append(results, json_bool(0));
                continue;
            }
        }
        process_used++;
        if (path_used >= W23_READPOOL_4F9A79_POOL_MAX) {
            /* reclaim_idle(exclude=requester) */
            int freed = 0;
            for (j = 0; j < nm; j++) {
                if ((long)j == requester) continue;
                if (evictable[j]) {
                    evictable[j] = 0;
                    /* the peer's close released BOTH permits */
                    if (path_used > 0) path_used--;
                    if (process_used > 0) process_used--;
                    evicted++;
                    freed = 1;
                    break;
                }
            }
            if (!freed) {
                process_used--;
                json_append(results, json_bool(0));
                continue;
            }
        }
        path_used++;
        json_append(results, json_bool(1));
    }

    json_set(out, "results", results);
    json_set(out, "process_used", json_number((double)process_used));
    json_set(out, "path_used", json_number((double)path_used));
    json_set(out, "denied_fd", json_number((double)denied_fd));
    json_set(out, "evicted", json_number((double)evicted));
    return out;
}

/* -------------------------------------------------------- duplicate handles */

static int rp_site_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* _PathReadBudget.register: warn once when this path already carries more than
 * _HANDLES_PER_PATH_WARN live WRITER handles in this process. */
json_t *w23_readpool_4f9a79_register(const json_t *req) {
    const json_t *members = req ? json_obj_get(req, "members") : NULL;
    const char *db_path = json_get_str(req, "db_path", "");
    const char *sites[RP_MAXSEG];
    size_t i, n = 0, nsites = 0;
    long handles = 0;
    int warned;
    json_t *out = json_object();

    if (members && members->type == JSON_ARRAY) {
        n = json_len(members);
        if (n > RP_MAXSEG) n = RP_MAXSEG;
    }
    for (i = 0; i < n; i++) {
        json_t *m = json_get(members, i);
        int read_only = (m && json_get_bool(m, "read_only", 0)) ? 1 : 0;
        int has_conn = (m && json_get_bool(m, "has_conn", 0)) ? 1 : 0;
        if (!read_only && has_conn) handles++;
        /* the site list carries every write-capable member, closed or not */
        if (!read_only) sites[nsites++] = m ? json_get_str(m, "site", "") : "";
    }
    warned = handles > (long)W23_READPOOL_4F9A79_HANDLES_WARN;
    json_set(out, "warned", json_bool(warned));
    if (warned) {
        char joined[1024];
        char line[4096];
        size_t used = 0;
        memset(joined, 0, sizeof(joined));
        qsort(sites, nsites, sizeof(sites[0]), rp_site_cmp);
        for (i = 0; i < nsites; i++) {
            int wrote = snprintf(joined + used, sizeof(joined) - used, "%s%s",
                                 i ? ", " : "", sites[i] ? sites[i] : "");
            if (wrote < 0) break;
            used += (size_t)wrote;
            if (used >= sizeof(joined)) { joined[sizeof(joined) - 1] = '\0'; break; }
        }
        snprintf(line, sizeof(line),
                 "%ld live SessionDB handles on %s in this process; each holds "
                 "its own writer connection (read connections are capped at %d "
                 "for the file). A long-lived process should share one handle per path. "
                 "Created at: %s",
                 handles, db_path ? db_path : "", W23_READPOOL_4F9A79_POOL_MAX, joined);
        json_set(out, "msg", json_string(line));
    } else {
        json_set(out, "msg", json_null());
    }
    return out;
}

/* ------------------------------------------------------ exhaustion warnings */

/* note_read_pool_exhausted: one warning per database per window, counting the
 * reads it swallowed and reporting that count on the next warning. */
#define RP_WARN_SLOTS 32

json_t *w23_readpool_4f9a79_note_exhausted(const json_t *req) {
    const json_t *ops = req ? json_obj_get(req, "ops") : NULL;
    char keys[RP_WARN_SLOTS][256];
    double at[RP_WARN_SLOTS];
    int seen[RP_WARN_SLOTS];
    long suppressed[RP_WARN_SLOTS];
    int nk = 0;
    size_t i, nops = (ops && ops->type == JSON_ARRAY) ? json_len(ops) : 0;
    json_t *warned_out = json_array();
    json_t *msgs_out = json_array();
    json_t *out = json_object();

    for (i = 0; i < RP_WARN_SLOTS; i++) { seen[i] = 0; suppressed[i] = 0; at[i] = 0.0; }

    for (i = 0; i < nops; i++) {
        json_t *op = json_get(ops, i);
        const char *db = json_get_str(op, "db", "");
        double now = json_get_num(op, "t", 0.0);
        int slot = -1, j;
        char msg[4096];

        if (!db) db = "";
        for (j = 0; j < nk; j++) {
            if (!strcmp(keys[j], db)) { slot = j; break; }
        }
        if (slot < 0) {
            if (nk >= RP_WARN_SLOTS) {
                json_append(warned_out, json_bool(0));
                json_append(msgs_out, json_null());
                continue;
            }
            slot = nk++;
            snprintf(keys[slot], sizeof(keys[slot]), "%s", db);
            seen[slot] = 0;
            suppressed[slot] = 0;
        }
        if (seen[slot] && (now - at[slot]) < W23_READPOOL_4F9A79_POOL_WARN_SECONDS) {
            suppressed[slot]++;
            json_append(warned_out, json_bool(0));
            json_append(msgs_out, json_null());
            continue;
        }
        {
            int had_previous = seen[slot];
            char suffix[128];
            suffix[0] = '\0';
            if (had_previous)
                snprintf(suffix, sizeof(suffix), " (%ld more reads since the last warning)",
                         suppressed[slot]);
            at[slot] = now;
            seen[slot] = 1;
            suppressed[slot] = 0;
            snprintf(msg, sizeof(msg),
                     "read pool at capacity (%d) for %s; serving this read from the locked "
                     "writer connection, so it waits behind every in-flight write%s. Sustained, "
                     "this means long writes or slow storage (a network filesystem) are holding "
                     "readers too. Logged at most once per %.0fs per database.",
                     W23_READPOOL_4F9A79_POOL_MAX, db, suffix,
                     W23_READPOOL_4F9A79_POOL_WARN_SECONDS);
            json_append(warned_out, json_bool(1));
            json_append(msgs_out, json_string(msg));
        }
    }

    json_set(out, "warned", warned_out);
    json_set(out, "msgs", msgs_out);
    return out;
}
