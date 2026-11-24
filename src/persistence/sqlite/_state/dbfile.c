/*
 * dbfile.c — C11 port of upstream Hermes persistence/sqlite/_state/dbfile.py.
 *
 * Gates (all against this one port file):
 *   w29_dbfile_fb85d9_p1 — file identity, SQLite header probes, WAL/SHM
 *                          sidecar path derivation, holder-PID classification.
 *   w29_dbfile_fb85d9_p2 — health decisions (zeroed / clobbered header),
 *                          quarantine naming, the read-only stats snapshot,
 *                          holder counting, the Desktop-holder reap predicate.
 *
 * Seams (deliberately NOT ported, answered by the caller):
 *   - the filesystem: every lstat/stat/exists/realpath answer arrives in the
 *     request.  Path arithmetic itself IS ported, with POSIX semantics;
 *   - sqlite3 connections: `_connect_tracked_db` is caller-supplied; the port
 *     receives its answers and reproduces the exact statement text and the
 *     derived fields;
 *   - /proc and libproc enumeration (`_iter_proc_fd_targets`,
 *     `_iter_darwin_fd_targets`) and `own_descriptors()`: caller-supplied lists;
 *   - byte I/O (`_pread_db_range`, `_copy_range`) and the manifest write.
 *
 * PoP tags name the upstream symbol each function ports.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w29_dbfile_fb85d9_p1.h"
#include "noc_w29_dbfile_fb85d9_p2.h"

#define DB_PATH_MAX 4096

/* ── shared helpers ─────────────────────────────────────────────────────────────────────────── */

/* PoP: db_hex @ persistence/sqlite/_state/dbfile.py:module (fixture codec)
 * Lowercase hex -> bytes; "" decodes to 0 bytes.  -1 on odd length / bad digit. */
static int db_hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int db_hex(const char *hex, unsigned char **out, size_t *len) {
    size_t n = hex ? strlen(hex) : 0, i;
    unsigned char *buf;
    *out = NULL;
    *len = 0;
    if (n % 2) return -1;
    if (!n) return 0;
    buf = (unsigned char *)malloc(n / 2 + 1);
    if (!buf) return -1;
    for (i = 0; i < n / 2; i++) {
        int hi = db_hexv(hex[2 * i]), lo = db_hexv(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) { free(buf); return -1; }
        buf[i] = (unsigned char)((hi << 4) | lo);
    }
    *out = buf;
    *len = n / 2;
    return 0;
}

static unsigned db_be16(const unsigned char *p) {
    return ((unsigned)p[0] << 8) | (unsigned)p[1];
}

static unsigned long db_be32(const unsigned char *p) {
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

/* PoP: db_normpath @ posixpath.normpath (the port's os.path)
 * Lexical POSIX normalization, identical to posixpath.normpath: empty -> ".",
 * a leading "//" keeps two slashes, "." components dropped, ".." pops. */
static void db_normpath(const char *path, char *out, size_t cap) {
    const char *comp[512];
    size_t clen[512];
    const char *keep[512];
    size_t klen[512];
    int ncomp = 0, initial = 0, i, nk = 0;
    const char *p;
    size_t n, w = 0;

    if (!path || !*path) { snprintf(out, cap, "."); return; }
    n = strlen(path);
    if (path[0] == '/') {
        initial = 1;
        if (n >= 2 && path[1] == '/' && !(n >= 3 && path[2] == '/')) initial = 2;
    }
    p = path;
    while (*p && ncomp < 512) {
        const char *s;
        while (*p == '/') p++;
        if (!*p) break;
        s = p;
        while (*p && *p != '/') p++;
        comp[ncomp] = s;
        clen[ncomp] = (size_t)(p - s);
        ncomp++;
    }
    for (i = 0; i < ncomp; i++) {
        int is_dot = (clen[i] == 1 && comp[i][0] == '.');
        int is_dotdot = (clen[i] == 2 && comp[i][0] == '.' && comp[i][1] == '.');
        int top_dotdot = (nk > 0 && klen[nk - 1] == 2 &&
                          keep[nk - 1][0] == '.' && keep[nk - 1][1] == '.');
        if (is_dot) continue;
        if (!is_dotdot) {
            keep[nk] = comp[i]; klen[nk] = clen[i]; nk++;
        } else if ((!initial && nk == 0) || top_dotdot) {
            keep[nk] = comp[i]; klen[nk] = clen[i]; nk++;
        } else if (nk > 0) {
            nk--;
        }
    }
    for (i = 0; i < initial; i++) out[w++] = '/';
    for (i = 0; i < nk; i++) {
        if (i) out[w++] = '/';
        memcpy(out + w, keep[i], klen[i]);
        w += klen[i];
    }
    if (!initial && nk == 0) out[w++] = '.';
    if (w == 0) out[w++] = '.';
    out[w] = '\0';
}

/* PoP: db_dirname @ posixpath.dirname */
static void db_dirname(const char *p, char *out, size_t cap) {
    const char *slash = strrchr(p, '/');
    size_t i, h;
    int all = 1, k;
    if (!slash) { out[0] = '\0'; return; }
    i = (size_t)(slash - p) + 1;
    h = i;
    for (k = 0; k < (int)i; k++) if (p[k] != '/') { all = 0; break; }
    if (!all) { while (h > 0 && p[h - 1] == '/') h--; }
    if (h >= cap) h = cap - 1;
    memcpy(out, p, h);
    out[h] = '\0';
}

/* PoP: db_basename @ posixpath.basename */
static void db_basename(const char *p, char *out, size_t cap) {
    const char *slash = strrchr(p, '/');
    snprintf(out, cap, "%s", slash ? slash + 1 : p);
}

/* PoP: db_join @ posixpath.join(a, b) */
static void db_join(const char *a, const char *b, char *out, size_t cap) {
    size_t la = strlen(a), lb = b ? strlen(b) : 0;
    if (b && b[0] == '/') { snprintf(out, cap, "%s", b); return; }
    if (la + lb + 2 >= cap) { snprintf(out, cap, "%s", a); return; }
    memcpy(out, a, la);
    if (la == 0) { memcpy(out, b, lb); out[lb] = '\0'; return; }
    if (a[la - 1] == '/') { memcpy(out + la, b, lb); out[la + lb] = '\0'; return; }
    out[la] = '/';
    memcpy(out + la + 1, b, lb);
    out[la + 1 + lb] = '\0';
}

/* PoP: db_canon @ persistence/sqlite/_state/holders.py:canonical_sqlite_path
 * normcase(abspath(path.removesuffix(" (deleted)"))): POSIX normcase is the
 * identity and abspath of an absolute path is normpath. */
static void db_canon(const char *p, char *out, size_t cap) {
    char tmp[DB_PATH_MAX];
    size_t n = p ? strlen(p) : 0;
    static const char suffix[] = " (deleted)";
    if (n >= sizeof(suffix) - 1 && strcmp(p + n - (sizeof(suffix) - 1), suffix) == 0)
        n -= sizeof(suffix) - 1;
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    memcpy(tmp, p, n);
    tmp[n] = '\0';
    db_normpath(tmp, out, cap);
}

/* PoP: db_resolve @ persistence/sqlite/_state/dbfile.py:_watched_sqlite_sidecar_paths
 * (the os.path.realpath seam) — exact-match map; unlisted paths resolve to
 * themselves. */
static const char *db_resolve(const json_t *map, const char *p, char *buf, size_t cap) {
    if (map && map->type == JSON_OBJECT) {
        json_t *v = json_obj_get(map, p);
        if (v && v->type == JSON_STRING) { snprintf(buf, cap, "%s", v->str_val); return buf; }
    }
    snprintf(buf, cap, "%s", p);
    return buf;
}

/* Element-wise [dev, ino] comparison. */
static int db_ident_eq(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || !b || a->type != JSON_ARRAY || b->type != JSON_ARRAY) return 0;
    n = json_len((json_t *)a);
    if (n != json_len((json_t *)b)) return 0;
    for (i = 0; i < n; i++) {
        json_t *x = json_get((json_t *)a, i), *y = json_get((json_t *)b, i);
        if (!x || !y || x->type != JSON_NUMBER || y->type != JSON_NUMBER) return 0;
        if (x->num_val != y->num_val) return 0;
    }
    return 1;
}

static int db_is_ident_array(const json_t *v) {
    json_t *first;
    if (!v || v->type != JSON_ARRAY || json_len((json_t *)v) != 2) return 0;
    first = json_get((json_t *)v, 0);
    return first && first->type == JSON_NUMBER;
}

/* ── gate w29_dbfile_fb85d9_p1 ─────────────────────────────────────────────────────────────── */

/* PoP: w29_dbfile_fb85d9_p1_header @ persistence/sqlite/_state/dbfile.py:_parse_sqlite_header */
json_t *w29_dbfile_fb85d9_p1_header(const char *hex) {
    unsigned char *raw = NULL;
    size_t n = 0;
    json_t *out;
    if (db_hex(hex ? hex : "", &raw, &n) != 0) return json_null();
    if (n < W29_DBFILE_FB85D9_P1_HEADER_BYTES || memcmp(raw, "SQLite format 3\0", 16) != 0) {
        free(raw);
        out = json_object();
        json_set(out, "valid", json_bool(0));
        return out;
    }
    out = json_object();
    json_set(out, "valid", json_bool(1));
    /* A raw page size of 1 means 65536: 64 KiB does not fit the 16-bit field. */
    json_set(out, "page_size",
             json_number((double)(db_be16(raw + 16) == 1 ? 65536u : db_be16(raw + 16))));
    json_set(out, "change_counter", json_number((double)db_be32(raw + 24)));
    json_set(out, "page_count", json_number((double)db_be32(raw + 28)));
    json_set(out, "user_version", json_number((double)db_be32(raw + 60)));
    json_set(out, "application_id", json_number((double)db_be32(raw + 68)));
    json_set(out, "version_valid_for", json_number((double)db_be32(raw + 92)));
    free(raw);
    return out;
}

/* PoP: w29_dbfile_fb85d9_p1_appid @ persistence/sqlite/_state/dbfile.py:_read_sqlite_application_id */
json_t *w29_dbfile_fb85d9_p1_appid(const json_t *req) {
    json_t *h = json_obj_get((json_t *)req, "header");
    unsigned char *raw = NULL;
    size_t n = 0, end = (size_t)W29_DBFILE_FB85D9_P1_APPLICATION_ID_OFFSET + 4;
    json_t *out;
    if (!h || h->type != JSON_STRING) return json_null();
    if (db_hex(h->str_val, &raw, &n) != 0) return json_null();
    if (n < end || memcmp(raw, "SQLite format 3\0", 16) != 0) { free(raw); return json_null(); }
    out = json_number((double)db_be32(raw + W29_DBFILE_FB85D9_P1_APPLICATION_ID_OFFSET));
    free(raw);
    return out;
}

/* PoP: w29_dbfile_fb85d9_p1_sidecars @ persistence/sqlite/_state/dbfile.py:_stat_sqlite_sidecar_identity */
json_t *w29_dbfile_fb85d9_p1_sidecars(const json_t *req) {
    static const char *const suffixes[] = { "-wal", "-shm" };
    json_t *stat = json_obj_get((json_t *)req, "stat");
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        json_t *ident = (stat && stat->type == JSON_OBJECT) ? json_obj_get(stat, suffixes[i]) : NULL;
        if (!db_is_ident_array(ident)) continue; /* absent sidecar: no entry at all */
        json_set(out, suffixes[i], json_copy(ident));
    }
    return out;
}

/* PoP: w29_dbfile_fb85d9_p1_watched @ persistence/sqlite/_state/dbfile.py:_watched_sqlite_sidecar_paths */
json_t *w29_dbfile_fb85d9_p1_watched(const json_t *req) {
    static const char *const suffixes[] = { "-wal", "-shm" };
    const char *path = json_get_str((json_t *)req, "path", "");
    json_t *resolve = json_obj_get((json_t *)req, "resolve");
    char base[DB_PATH_MAX], lit[2 * DB_PATH_MAX], key[2 * DB_PATH_MAX];
    char dir[DB_PATH_MAX], name[DB_PATH_MAX], rdir[DB_PATH_MAX], rb[2][DB_PATH_MAX];
    json_t *out = json_object();
    size_t i;
    int bi;

    db_normpath(path, base, sizeof(base));
    for (i = 0; i < 2; i++) {
        snprintf(lit, sizeof(lit), "%s%s", base, suffixes[i]);
        db_canon(lit, key, sizeof(key));
        if (!json_obj_get(out, key)) json_set(out, key, json_string(lit));
    }
    db_dirname(base, dir, sizeof(dir));
    db_basename(base, name, sizeof(name));
    db_join(db_resolve(resolve, dir, rdir, sizeof(rdir)), name, rb[0], sizeof(rb[0]));
    db_resolve(resolve, base, rb[1], sizeof(rb[1]));
    for (bi = 0; bi < 2; bi++) {
        for (i = 0; i < 2; i++) {
            snprintf(lit, sizeof(lit), "%s%s", rb[bi], suffixes[i]);
            db_canon(lit, key, sizeof(key));
            if (!json_obj_get(out, key)) json_set(out, key, json_string(lit));
        }
    }
    return out;
}

/* PoP: w29_dbfile_fb85d9_p1_unlinked_identity @ persistence/sqlite/_state/dbfile.py:_identity_is_truly_unlinked */
json_t *w29_dbfile_fb85d9_p1_unlinked_identity(const json_t *req) {
    json_t *identity = json_obj_get((json_t *)req, "identity");
    json_t *stat = json_obj_get((json_t *)req, "stat");
    if (!db_is_ident_array(identity)) return json_bool(1);
    if (!db_is_ident_array(stat)) return json_bool(1); /* os.stat raised */
    return json_bool(db_ident_eq(identity, stat) ? 0 : 1);
}

/* PoP: w29_dbfile_fb85d9_p1_unlinked_fd @ persistence/sqlite/_state/dbfile.py:_fd_is_truly_unlinked */
json_t *w29_dbfile_fb85d9_p1_unlinked_fd(const json_t *req) {
    json_t *fd_stat = json_obj_get((json_t *)req, "fd_stat");
    json_t *watched = json_obj_get((json_t *)req, "watched_stat");
    if (fd_stat && fd_stat->type == JSON_OBJECT) {
        json_t *e = json_obj_get(fd_stat, "errno");
        double err = e ? e->num_val : 0;
        /* ENOENT / ESRCH: a descriptor closed after /proc was read, or a
         * process gone mid-scan, cannot keep a retired generation alive — the
         * scan race is not a refusal.  Any other error fails closed. */
        if (err == 2 || err == 3) return json_bool(0);
        return json_bool(1);
    }
    if (!db_is_ident_array(fd_stat)) return json_bool(1);
    if (!db_is_ident_array(watched)) return json_bool(1);
    return json_bool(db_ident_eq(fd_stat, watched) ? 0 : 1);
}

/* PoP: w29_dbfile_fb85d9_p1_own_fd @ persistence/sqlite/_state/dbfile.py:_own_descriptor_for_identity */
json_t *w29_dbfile_fb85d9_p1_own_fd(const json_t *req) {
    json_t *identity = json_obj_get((json_t *)req, "identity");
    json_t *fds = json_obj_get((json_t *)req, "fds");
    size_t i, n;
    if (!db_is_ident_array(identity)) return json_null();
    if (!fds || fds->type != JSON_ARRAY) return json_null();
    n = json_len(fds);
    for (i = 0; i < n; i++) {
        json_t *entry = json_get(fds, i);
        json_t *fd, *ident;
        if (!entry || entry->type != JSON_ARRAY || json_len(entry) < 2) continue;
        fd = json_get(entry, 0);
        ident = json_get(entry, 1);
        if (fd && ident && db_ident_eq(identity, ident)) return json_number(fd->num_val);
    }
    return json_null();
}

/* PoP: w29_dbfile_fb85d9_p1_oplink @ persistence/sqlite/_state/dbfile.py:_is_operator_store_link */
json_t *w29_dbfile_fb85d9_p1_oplink(const json_t *req) {
    double link_uid = json_get_num((json_t *)req, "link_uid", 0);
    double euid = json_get_num((json_t *)req, "euid", 0);
    json_t *target_uid = json_obj_get((json_t *)req, "target_uid");
    int exists = json_get_bool((json_t *)req, "target_exists", 0);
    if (!exists) return json_bool(0); /* dangling link: SQLite would create the store */
    if (!target_uid || target_uid->type != JSON_NUMBER) return json_bool(0);
    if (link_uid != euid) return json_bool(0);
    return json_bool(target_uid->num_val == euid ? 1 : 0);
}

/* PoP: w29_dbfile_fb85d9_p1_holder_pids @ persistence/sqlite/_state/dbfile.py:_concrete_state_db_holder_pids */
json_t *w29_dbfile_fb85d9_p1_holder_pids(const json_t *req) {
    const char *path = json_get_str((json_t *)req, "path", "");
    json_t *holders = json_obj_get((json_t *)req, "holders");
    char db[DB_PATH_MAX], wal[DB_PATH_MAX + 8], shm[DB_PATH_MAX + 8], canon[DB_PATH_MAX];
    json_t *out = json_array();
    long seen[512];
    int nseen = 0;
    size_t i, n;

    db_canon(path, db, sizeof(db));
    snprintf(wal, sizeof(wal), "%s-wal", db);
    snprintf(shm, sizeof(shm), "%s-shm", db);
    if (!holders || holders->type != JSON_ARRAY) return out;
    n = json_len(holders);
    for (i = 0; i < n && nseen < 512; i++) {
        json_t *entry = json_get(holders, i);
        json_t *pid, *p;
        long v;
        int dup = 0, k;
        if (!entry || entry->type != JSON_ARRAY || json_len(entry) < 2) continue;
        pid = json_get(entry, 0);
        p = json_get(entry, 1);
        if (!pid || pid->type != JSON_NUMBER || !p || p->type != JSON_STRING) continue;
        v = (long)pid->num_val;
        if (v <= 0) continue; /* PID 0 / negatives are no holder */
        db_canon(p->str_val, canon, sizeof(canon));
        if (strcmp(canon, db) && strcmp(canon, wal) && strcmp(canon, shm)) continue;
        for (k = 0; k < nseen; k++) if (seen[k] == v) { dup = 1; break; }
        if (dup) continue;
        seen[nseen++] = v;
        json_append(out, json_number((double)v));
    }
    return out;
}

/* PoP: w29_dbfile_fb85d9_p1_canon @ persistence/sqlite/_state/holders.py:canonical_sqlite_path */
json_t *w29_dbfile_fb85d9_p1_canon(const char *path) {
    char buf[DB_PATH_MAX];
    db_canon(path ? path : "", buf, sizeof(buf));
    return json_string(buf);
}

/* ── gate w29_dbfile_fb85d9_p2 ─────────────────────────────────────────────────────────────── */

/* PoP: DB_STATS_KEYS @ persistence/sqlite/_state/dbfile.py:collect_state_db_stats
 * dict.fromkeys order: every field exists from the start and stays null until a
 * probe answers for it. */
static const char *const DB_STATS_KEYS[16] = {
    "page_count", "page_size", "freelist_count", "logical_size_bytes", "wal_size_bytes",
    "journal_mode", "messages", "sessions", "fts_tables", "fts_storage_version",
    "fts_storage_upgrade_needed", "fts_teardown_pending", "fts_rebuild_pending",
    "fts_rebuild_high_water", "fts_rebuild_progress", "fts_rebuild_deferral"
};

/* The statements collect_state_db_stats issues, byte for byte. */
#define DB_SQL_PAGE_COUNT "PRAGMA page_count"
#define DB_SQL_PAGE_SIZE "PRAGMA page_size"
#define DB_SQL_FREELIST "PRAGMA freelist_count"
#define DB_SQL_JOURNAL "PRAGMA journal_mode"
#define DB_SQL_MESSAGES "SELECT COUNT(*) FROM messages"
#define DB_SQL_SESSIONS "SELECT COUNT(*) FROM sessions"
#define DB_SQL_FTS "SELECT name FROM sqlite_master WHERE type = 'table' AND name IN (?, ?)"
#define DB_SQL_META "SELECT value FROM state_meta WHERE key = ?"
#define DB_SQL_META_ONE "SELECT value FROM state_meta WHERE key = ? LIMIT 1"
#define DB_SQL_TRASH \
    "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name LIKE 'fts\\_v22\\_trash\\_%' ESCAPE '\\' LIMIT 1"

static int db_val_same(const json_t *a, const json_t *b) {
    if (!a || !b) return 0;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val, b->str_val) == 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_NULL && b->type == JSON_NULL) return 1;
    if (a->type == JSON_STRING && b->type == JSON_NUMBER) {
        char nb[64];
        snprintf(nb, sizeof(nb), "%.17g", b->num_val);
        return strcmp(a->str_val, nb) == 0;
    }
    if (a->type == JSON_NUMBER && b->type == JSON_STRING) return db_val_same(b, a);
    return 0;
}

/* Execute-through-the-fixture: append {sql, params} to the log, then return the
 * scripted answer (NULL when the fixture has none, which the live code sees as
 * a raised sqlite3 error). */
static json_t *db_script(json_t *answers, const char *sql, const char *const *params,
                        size_t np, json_t *log) {
    json_t *entry = json_object(), *plist = json_array();
    size_t i, k;
    json_set(entry, "sql", json_string(sql));
    for (i = 0; i < np; i++) json_append(plist, json_string(params[i]));
    json_set(entry, "params", plist);
    json_append(log, entry);
    if (!answers || answers->type != JSON_ARRAY) return NULL;
    k = json_len(answers);
    for (i = 0; i < k; i++) {
        json_t *a = json_get(answers, i);
        json_t *ap;
        size_t anp, j;
        int same = 1;
        if (!a || a->type != JSON_OBJECT) continue;
        if (strcmp(json_get_str(a, "sql", ""), sql) != 0) continue;
        ap = json_obj_get(a, "params");
        anp = (ap && ap->type == JSON_ARRAY) ? json_len(ap) : 0;
        if (anp != np) continue;
        for (j = 0; j < np && same; j++) {
            json_t *x = json_get(ap, j), *y = json_string(params[j]);
            same = db_val_same(x, y);
            json_free(y);
        }
        if (same) return a;
    }
    return NULL;
}

/* fetchone(): NULL means "no row" (either the statement failed or the fixture
 * scripts no row).  A row holding SQL NULL returns the JSON null node. */
static json_t *db_fetch_one(json_t *answers, const char *sql, const char *const *params,
                            size_t np, json_t *log) {
    json_t *a = db_script(answers, sql, params, np, log);
    json_t *one, *all;
    if (!a) return NULL;
    if (json_get_bool(a, "none", 0)) return NULL;
    one = json_obj_get(a, "one");
    if (one) return one;
    all = json_obj_get(a, "all");
    if (all && all->type == JSON_ARRAY && json_len(all) > 0) {
        json_t *row = json_get(all, 0);
        return (row && row->type == JSON_ARRAY) ? json_get(row, 0) : row;
    }
    return NULL;
}

/* int(value) with Python's tolerance for numeric strings; 0 when the value is
 * None or cannot be coerced (callers decide whether that means None). */
static int db_as_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char *end = NULL;
        long long x;
        if (!s || !*s) return 0;
        x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
               *end == '\f' || *end == '\v') end++;
        if (*end) return 0;
        *out = x;
        return 1;
    }
    return 0;
}

/* PoP: w29_dbfile_fb85d9_p2_invalid_header @ persistence/sqlite/_state/dbfile.py:has_invalid_sqlite_header_preopen */
json_t *w29_dbfile_fb85d9_p2_invalid_header(const json_t *req) {
    json_t *h = json_obj_get((json_t *)req, "head");
    unsigned char *raw = NULL;
    size_t n = 0;
    int invalid;
    if (!h || h->type != JSON_STRING) return json_bool(0); /* probe refused */
    if (db_hex(h->str_val, &raw, &n) != 0) return json_bool(0);
    invalid = !(n >= 15 && memcmp(raw, "SQLite format 3", 15) == 0);
    free(raw);
    return json_bool(invalid);
}

/* PoP: w29_dbfile_fb85d9_p2_zeroed @ persistence/sqlite/_state/dbfile.py:is_zeroed_state_db */
json_t *w29_dbfile_fb85d9_p2_zeroed(const json_t *req) {
    json_t *h = json_obj_get((json_t *)req, "head");
    json_t *probe = json_obj_get((json_t *)req, "probe");
    unsigned char *raw = NULL;
    size_t n = 0, i;
    int zeroed = 1;
    if (probe && !json_get_bool(probe, "raises", 0))
        return json_bool(json_get_bool(probe, "answer", 0));
    if (!h || h->type != JSON_STRING) return json_bool(0);
    if (db_hex(h->str_val, &raw, &n) != 0) return json_bool(0);
    if (n >= 15 && memcmp(raw, "SQLite format 3", 15) == 0) zeroed = 0;
    for (i = 0; i < n && zeroed; i++) if (raw[i] != 0) zeroed = 0;
    free(raw);
    return json_bool(zeroed);
}

/* PoP: w29_dbfile_fb85d9_p2_quarantine @ persistence/sqlite/_state/dbfile.py:quarantine_invalid_state_db */
json_t *w29_dbfile_fb85d9_p2_quarantine(const json_t *req) {
    const char *name = json_get_str((json_t *)req, "name", "");
    const char *ts = json_get_str((json_t *)req, "ts", "");
    json_t *pid = json_obj_get((json_t *)req, "pid");
    json_t *taken = json_obj_get((json_t *)req, "taken");
    char stem[DB_PATH_MAX], dest[DB_PATH_MAX + 32];
    int n = 0;
    if (!json_get_bool((json_t *)req, "locked", 1)) return json_null();
    if (!json_get_bool((json_t *)req, "exists", 0)) return json_null();
    if (!json_get_bool((json_t *)req, "invalid", 0)) return json_null();
    snprintf(stem, sizeof(stem), "%s.%s-%s-%lld", name,
             json_get_bool((json_t *)req, "zeroed", 0) ? "zeroed" : "notadb", ts,
             (long long)(pid && pid->type == JSON_NUMBER ? pid->num_val : 0));
    snprintf(dest, sizeof(dest), "%s.bak", stem);
    while (taken && taken->type == JSON_ARRAY) {
        int hit = 0;
        size_t i, m = json_len(taken);
        for (i = 0; i < m; i++) {
            json_t *t = json_get(taken, i);
            if (t && t->type == JSON_STRING && strcmp(t->str_val, dest) == 0) { hit = 1; break; }
        }
        if (!hit) break;
        n++;
        if (n > 4096) break; /* bounded: hostile collision lists */
        snprintf(dest, sizeof(dest), "%s-%d.bak", stem, n);
    }
    return json_string(dest);
}

/* PoP: w29_dbfile_fb85d9_p2_stats @ persistence/sqlite/_state/dbfile.py:collect_state_db_stats */
json_t *w29_dbfile_fb85d9_p2_stats(const json_t *req) {
    json_t *answers = json_obj_get((json_t *)req, "answers");
    json_t *log = json_array(), *stats = json_object(), *out;
    long long pc = 0, ps = 0, fw = 0, msgs = 0, sess = 0, hw = 0, prog = 0;
    int have_pc = 0, have_ps = 0, have_fw = 0, have_msgs = 0, have_sess = 0;
    int have_hw = 0, have_prog = 0;
    json_t *v;
    size_t i;

    for (i = 0; i < 16; i++) json_set(stats, DB_STATS_KEYS[i], json_null());
    /* The WAL sidecar size needs no connection at all, and is collected even
     * when the read-only open fails. */
    v = json_obj_get((json_t *)req, "wal_size");
    json_set(stats, "wal_size_bytes", json_number((v && v->type == JSON_NUMBER) ? v->num_val : 0));
    out = json_object();
    json_set(out, "stats", stats);
    json_set(out, "sql", log);
    if (strcmp(json_get_str((json_t *)req, "connect", "ok"), "raise") == 0) return out;

    v = db_fetch_one(answers, DB_SQL_PAGE_COUNT, NULL, 0, log);
    have_pc = db_as_int(v, &pc);
    if (have_pc) json_set(stats, "page_count", json_number((double)pc));
    v = db_fetch_one(answers, DB_SQL_PAGE_SIZE, NULL, 0, log);
    have_ps = db_as_int(v, &ps);
    if (have_ps) json_set(stats, "page_size", json_number((double)ps));
    if (have_pc && have_ps)
        json_set(stats, "logical_size_bytes", json_number((double)(pc * ps)));
    v = db_fetch_one(answers, DB_SQL_FREELIST, NULL, 0, log);
    have_fw = db_as_int(v, &fw);
    if (have_fw) json_set(stats, "freelist_count", json_number((double)fw));
    v = db_fetch_one(answers, DB_SQL_JOURNAL, NULL, 0, log);
    if (v && v->type != JSON_NULL) {
        char nb[64];
        if (v->type == JSON_STRING) {
            json_set(stats, "journal_mode", json_string(v->str_val));
        } else if (v->type == JSON_NUMBER) {
            double d = v->num_val;
            if (d == (double)(long long)d) snprintf(nb, sizeof(nb), "%lld", (long long)d);
            else snprintf(nb, sizeof(nb), "%.17g", d);
            json_set(stats, "journal_mode", json_string(nb));
        } else if (v->type == JSON_BOOL) {
            json_set(stats, "journal_mode", json_string(v->bool_val ? "True" : "False"));
        }
    }
    v = db_fetch_one(answers, DB_SQL_MESSAGES, NULL, 0, log);
    have_msgs = db_as_int(v, &msgs);
    if (have_msgs) json_set(stats, "messages", json_number((double)msgs));
    v = db_fetch_one(answers, DB_SQL_SESSIONS, NULL, 0, log);
    have_sess = db_as_int(v, &sess);
    if (have_sess) json_set(stats, "sessions", json_number((double)sess));

    /* FTS table presence via sqlite_master -- never a SELECT from the virtual
     * tables, so a corrupt index cannot fail the snapshot. */
    {
        static const char *const fts_names[2] = { "messages_fts", "messages_fts_trigram" };
        json_t *a = db_script(answers, DB_SQL_FTS, fts_names, 2, log);
        if (a) {
            json_t *seen[64];
            size_t nseen = 0, j, k;
            json_t *t;
            if (!json_get_bool(a, "none", 0)) {
                json_t *one = json_obj_get(a, "one");
                json_t *all = json_obj_get(a, "all");
                if (one) {
                    seen[nseen++] = one;
                } else if (all && all->type == JSON_ARRAY) {
                    k = json_len(all);
                    for (j = 0; j < k && nseen < 64; j++) {
                        json_t *row = json_get(all, j);
                        json_t *c = (row && row->type == JSON_ARRAY) ? json_get(row, 0) : row;
                        if (c) seen[nseen++] = c;
                    }
                }
            }
            t = json_object();
            for (j = 0; j < 2; j++) {
                int found = 0;
                for (k = 0; k < nseen; k++)
                    if (seen[k]->type == JSON_STRING && !strcmp(seen[k]->str_val, fts_names[j]))
                        found = 1;
                json_set(t, fts_names[j], json_bool(found));
            }
            json_set(stats, "fts_tables", t);
        }
    }
    /* state_meta reads: cheap and independent of the store holder.  A
     * non-numeric value must yield None, not fail the snapshot. */
    {
        const char *k1[1] = { "fts_storage_version" };
        v = db_fetch_one(answers, DB_SQL_META, k1, 1, log);
        if (db_as_int(v, &ps)) json_set(stats, "fts_storage_version", json_number((double)ps));
    }
    {
        json_t *d = json_obj_get((json_t *)req, "drift");
        int raises = (d && d->type == JSON_STRING && !strcmp(d->str_val, "raise"));
        if (!raises) {
            int flag = 0;
            if (d && d->type == JSON_BOOL) flag = d->bool_val;
            else if (d && d->type == JSON_NUMBER) flag = d->num_val != 0;
            json_set(stats, "fts_storage_upgrade_needed", json_bool(flag));
        }
    }
    /* Demoted old-index tables still awaiting teardown. */
    {
        json_t *a = db_script(answers, DB_SQL_TRASH, NULL, 0, log);
        if (a) {
            json_t *one = json_obj_get(a, "one"), *all = json_obj_get(a, "all");
            int row;
            if (json_get_bool(a, "none", 0)) row = 0;
            else if (one) row = 1;
            else row = (all && all->type == JSON_ARRAY && json_len(all) > 0) ? 1 : 0;
            json_set(stats, "fts_teardown_pending", json_bool(row));
        }
    }
    {
        const char *kh[1] = { "fts_rebuild_high_water" };
        const char *kp[1] = { "fts_rebuild_progress" };
        v = db_fetch_one(answers, DB_SQL_META, kh, 1, log);
        have_hw = db_as_int(v, &hw);
        if (have_hw) json_set(stats, "fts_rebuild_high_water", json_number((double)hw));
        v = db_fetch_one(answers, DB_SQL_META, kp, 1, log);
        have_prog = db_as_int(v, &prog);
        if (have_prog) json_set(stats, "fts_rebuild_progress", json_number((double)prog));
        if (have_hw)
            json_set(stats, "fts_rebuild_pending",
                     json_bool((have_prog ? prog : 0) < hw));
        else
            json_set(stats, "fts_rebuild_pending", json_bool(0));
    }
    {
        const char *kd[1] = { "fts_rebuild_deferral" };
        v = db_fetch_one(answers, DB_SQL_META_ONE, kd, 1, log);
        if (v && v->type == JSON_STRING) {
            json_t *parsed = json_parse(v->str_val, NULL);
            if (parsed) {
                if (parsed->type == JSON_OBJECT) json_set(stats, "fts_rebuild_deferral", parsed);
                else json_free(parsed);
            }
        }
    }
    return out;
}

/* PoP: w29_dbfile_fb85d9_p2_count_holders @ persistence/sqlite/_state/dbfile.py:count_db_holders */
json_t *w29_dbfile_fb85d9_p2_count_holders(const json_t *req) {
    const char *platform = json_get_str((json_t *)req, "platform", "linux");
    const char *path = json_get_str((json_t *)req, "path", "");
    json_t *resolve = json_obj_get((json_t *)req, "resolve");
    json_t *fds = json_obj_get((json_t *)req, "fds");
    json_t *st = json_obj_get((json_t *)req, "stat");
    char tbuf[DB_PATH_MAX];
    const char *target = db_resolve(resolve, path, tbuf, sizeof(tbuf));
    long pids[256];
    int npids = 0;
    size_t i, n;
    int darwin = (strcmp(platform, "darwin") == 0);

    if (!darwin && strncmp(platform, "linux", 5) != 0) return json_null();
    if (darwin && !db_is_ident_array(st)) return json_null(); /* os.stat raised */
    if (!fds || fds->type != JSON_ARRAY) return json_number(0);
    n = json_len(fds);
    for (i = 0; i < n && npids < 256; i++) {
        json_t *e = json_get(fds, i);
        json_t *pid, *key;
        long v;
        int dup = 0, k;
        if (!e || e->type != JSON_OBJECT) continue;
        pid = json_obj_get(e, "pid");
        if (!pid || pid->type != JSON_NUMBER) continue;
        key = json_obj_get(e, darwin ? "identity" : "link");
        if (darwin) {
            if (!key || !db_ident_eq(st, key)) continue;
        } else if (!key || key->type != JSON_STRING || strcmp(key->str_val, target) != 0) {
            continue;
        }
        v = (long)pid->num_val;
        for (k = 0; k < npids; k++) if (pids[k] == v) { dup = 1; break; }
        if (dup) continue;
        pids[npids++] = v;
    }
    return json_number((double)npids);
}

/* PoP: w29_dbfile_fb85d9_p2_orphan @ persistence/sqlite/_state/dbfile.py:_is_inactive_orphan_desktop_holder */
json_t *w29_dbfile_fb85d9_p2_orphan(const json_t *req) {
    double ppid = json_get_num((json_t *)req, "ppid", -1);
    double age = json_get_num((json_t *)req, "age_seconds", 0);
    double min_age = json_get_num((json_t *)req, "min_age_seconds", 0);
    json_t *statuses = json_obj_get((json_t *)req, "connection_statuses");
    int established = 0;
    size_t i;
    if (statuses && statuses->type == JSON_ARRAY) {
        size_t n = json_len(statuses);
        for (i = 0; i < n; i++) {
            json_t *s = json_get(statuses, i);
            if (s && s->type == JSON_STRING && !strcmp(s->str_val, "ESTABLISHED")) established = 1;
        }
    }
    if (!(ppid == 0 || ppid == 1)) return json_bool(0);
    if (!(age >= min_age)) return json_bool(0);
    if (!json_get_bool((json_t *)req, "ephemeral_backend", 0)) return json_bool(0);
    return json_bool(established ? 0 : 1);
}

/* PoP: w29_dbfile_fb85d9_p2_constants @ persistence/sqlite/_state/dbfile.py:<module> */
json_t *w29_dbfile_fb85d9_p2_constants(void) {
    json_t *out = json_object(), *fts = json_array();
    json_append(fts, json_string("messages_fts"));
    json_append(fts, json_string("messages_fts_trigram"));
    json_set(out, "header_probe_lock_timeout_s", json_number(0.1));
    json_set(out, "retired_generation_main_image_max_bytes", json_number(512.0 * 1024.0 * 1024.0));
    json_set(out, "retired_generation_manifest_version", json_number(1));
    json_set(out, "retired_generation_dir_suffix", json_string(".retired-wal-"));
    json_set(out, "retired_generation_manifest", json_string("manifest.json"));
    json_set(out, "capture_chunk_bytes", json_number(8.0 * 1024.0 * 1024.0));
    json_set(out, "sqlite_header_bytes", json_number(100));
    json_set(out, "fts_table_names", fts);
    return out;
}
