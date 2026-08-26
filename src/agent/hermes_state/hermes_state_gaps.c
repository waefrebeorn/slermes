/* hermes_state_gaps.c — C11 closure of the remaining REAL_GAPs of
 * hermes_state.py (54 functions): WAL sizing, malformed-schema detection,
 * the cross-process repair lock + scratch-copy repair pipeline, foreign
 * DB-holder discovery over /proc, the never-active keyed-session prune,
 * gateway routing/hygiene state, the cross-process session turn lease,
 * and the message/prune helpers. Backed by lib/libdb sqlite3 (the same
 * amalgamation Python's sqlite3 module wraps) and libjson for mixed-type
 * dict returns. Extends the existing src/agent/hermes_state/ units —
 * no symbol duplication.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <sqlite3.h>
#include "hermes_json.h"
#include "hermes_state_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

/* forward decls (used before definition below) */
extern char *hermes_state_repair_ledger_path(const char *db_path);
extern json_t *hermes_state_read_repair_ledger(const char *db_path);
extern char *hermes_state_fts5_cjk_so_path(const char *hermes_home);
extern void hermes_state_close(hermes_state_db_t *self_);
#include "libjson/json.h"
#include "web_server_prune.h"
static bool hs_json_deep_equal(const json_t *a, const json_t *b);

/* ── module constants (hermes_state.py module level) ─────────────────── */
#define HS_WAL_SIZE_LIMIT_BYTES          (64LL * 1024 * 1024)  /* 64 MiB */
static const char *const HS_MALFORMED_SCHEMA_MARKERS[] = {
    "malformed database schema", NULL,
};
#define HS_REPAIR_LOCK_TIMEOUT_SECONDS   120.0
#define HS_REPAIR_LOCK_POLL_SECONDS      0.1
/* _FINGERPRINT_VOLATILE_HEADER_RANGES = ((24, 28), (92, 96)) */
static const long HS_FP_VOLATILE_RANGES[][2] = { {24, 28}, {92, 96} };
#define HS_FINGERPRINT_SAMPLE_BYTES      65536
static const char *const HS_DB_SIDECAR_SUFFIXES[] = { "-wal", "-shm", "-journal", NULL };
/* SessionDB.RECOVERABLE_END_REASONS (hermes_state_common.py:_RECOVERABLE_END_REASONS) */
static const char *const HS_RECOVERABLE_END_REASONS[] = {
    "agent_close", "ws_orphan_reap", "superseded_by_resume",
    "startup_orphan_reap", NULL,
};
static const char *const HS_BARE_BILLING_PROVIDERS[] = { "auto", "custom", NULL };

/* ── shared helpers ───────────────────────────────────────────────────── */

static char *hs_strdup(const char *s) { return s ? strdup(s) : NULL; }

static int hs_exec(sqlite3 *db, const char *sql) {
    if (!db || !sql) return SQLITE_MISUSE;
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc;
}

/* Best-effort scalar text query; malloc'd or NULL. */
static char *hs_query_text(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL;
    if (!db || sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *t = sqlite3_column_text(st, 0);
        out = hs_strdup((const char *)t);
    }
    sqlite3_finalize(st);
    return out;
}

/* Python str.lower() containment test over the marker tuple. */
static bool hs_contains_any_lower(const char *msg, const char *const *markers) {
    if (!msg) return false;
    char *lower = strdup(msg);
    if (!lower) return false;
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
    bool hit = false;
    for (int i = 0; markers[i]; i++)
        if (strstr(lower, markers[i])) { hit = true; break; }
    free(lower);
    return hit;
}

/* Minimal SHA-256 (FIPS 180-4) for the fingerprint/identity digests. */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t n; } hs_sha256_t;
static bool hs_hash_whole_file(hs_sha256_t *c, const char *path);
static void hs_sha256_update(hs_sha256_t *c, const void *data, size_t n);
static char *gaps_db_opens_cleanly(const char *db_path);

static bool hs_hash_whole_file(hs_sha256_t *c, const char *path)
{
    if (!c || !path || !*path) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        hs_sha256_update(c, buf, n);
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}
static uint32_t hs_ror(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
static void hs_sha256_init(hs_sha256_t *c) {
    c->len = 0; c->n = 0;
    static const uint32_t H0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(c->h, H0, sizeof(H0));
}
static void hs_sha256_block(hs_sha256_t *c, const unsigned char *p) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = hs_ror(w[i-15],7) ^ hs_ror(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = hs_ror(w[i-2],17) ^ hs_ror(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],d,e,f,g,h,cc=c->h[2];
    d=c->h[3]; e=c->h[4]; f=c->h[5]; g=c->h[6]; h=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = hs_ror(e,6)^hs_ror(e,11)^hs_ror(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = hs_ror(a,2)^hs_ror(a,13)^hs_ror(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}
static void hs_sha256_update(hs_sha256_t *c, const void *data, size_t n) {
    const unsigned char *p = data;
    c->len += n;
    while (n) {
        size_t take = 64 - c->n; if (take > n) take = n;
        memcpy(c->buf + c->n, p, take);
        c->n += take; p += take; n -= take;
        if (c->n == 64) { hs_sha256_block(c, c->buf); c->n = 0; }
    }
}
static void hs_sha256_hex(hs_sha256_t *c, char out[65]) {
    static const char *hexd = "0123456789abcdef";
    uint64_t bits = c->len * 8;
    unsigned char pad = 0x80;
    hs_sha256_update(c, &pad, 1);
    unsigned char z = 0;
    while (c->n != 56) hs_sha256_update(c, &z, 1);
    unsigned char lenbuf[8];
    for (int i = 0; i < 8; i++) lenbuf[i] = (unsigned char)(bits >> (56 - i * 8));
    c->len -= 8; /* length bytes are not message */
    hs_sha256_update(c, lenbuf, 8);
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++) {
            out[i*8+j*2]   = hexd[(c->h[i] >> (28 - j*8)) & 0xF];
            out[i*8+j*2+1] = hexd[(c->h[i] >> (24 - j*8)) & 0xF];
        }
    out[64] = '\0';
}

static double hs_mono(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* os.path.normcase(os.path.abspath(p)) — on POSIX normcase is identity. */
static char *hs_canonical_path(const char *p) {
    if (!p) return NULL;
    char *copy = strdup(p);
    if (!copy) return NULL;
    /* strip the kernel "(deleted)" suffix /proc readlinks preserve */
    size_t n = strlen(copy);
    if (n > 10 && strcmp(copy + n - 10, " (deleted)") == 0) copy[n-10] = '\0';
    char *abs = realpath(copy, NULL); /* resolves; missing file → NULL */
    if (abs) { free(copy); return abs; }
    /* fall back to lexical absolutization like os.path.abspath */
    if (copy[0] != '/') {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            char *joined = NULL;
            asprintf(&joined, "%s/%s", cwd, copy);
            free(copy);
            copy = joined ? joined : copy;
        }
    }
    return copy;
}

/* ── PoP: _apply_wal_size_limit @ hermes_state.py:_apply_wal_size_limit ── */
void _apply_wal_size_limit(sqlite3 *conn) {
    /* Bound the WAL so checkpoints truncate it back to 64 MiB instead of
     * retaining the high-water mark forever. Best-effort: never raises. */
    if (!conn) return;
    char *sql = NULL;
    if (asprintf(&sql, "PRAGMA journal_size_limit=%lld",
                 (long long)HS_WAL_SIZE_LIMIT_BYTES) < 0) return;
    char *err = NULL;
    if (sqlite3_exec(conn, sql, NULL, NULL, &err) != SQLITE_OK && err) {
        /* Python: logger.debug("journal_size_limit not applied: %s", exc) */
        fprintf(stderr, "journal_size_limit not applied: %s\n", err);
    }
    if (err) sqlite3_free(err);
    free(sql);
}

/* ── PoP: is_malformed_schema_error @ hermes_state.py:is_malformed_schema_error ── */
bool is_malformed_schema_error(const char *exc_message) {
    /* True only when SQLite explicitly reports malformed SCHEMA text; a
     * generic "database disk image is malformed" does not qualify. */
    return hs_contains_any_lower(exc_message, HS_MALFORMED_SCHEMA_MARKERS);
}

/* ── PoP: _cross_process_repair_lock @ hermes_state.py:_cross_process_repair_lock ──
 * Generator → acquire/release pair. _cross_process_repair_lock returns true
 * when the caller holds the repair lock for db_path (false when the bounded
 * acquire timed out). On lock-file open failure it degrades to in-process
 * serialisation only and still returns true with a NULL handle, exactly like
 * the generator's `yield True` fallback path. Release is idempotent. */
bool _cross_process_repair_lock(const char *db_path, void **handle_out) {
    if (handle_out) *handle_out = NULL;
    if (!db_path) return true;
    char *lock_path = NULL;
    asprintf(&lock_path, "%s.repair.lock", db_path);
    if (!lock_path) return true;
    int fd = open(lock_path, O_RDWR | O_CREAT | O_APPEND, 0644);
    free(lock_path);
    if (fd < 0) {
        /* Read-only dir / exhausted fds: proceed without the cross-process
         * lock rather than refusing to repair a DB we could otherwise heal. */
        fprintf(stderr, "Could not open state.db repair lock (%s) — proceeding "
                        "with in-process serialisation only.\n", strerror(errno));
        return true;
    }
    double deadline = hs_mono() + HS_REPAIR_LOCK_TIMEOUT_SECONDS;
    bool acquired = false;
    for (;;) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) { acquired = true; break; }
        if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) break;
        if (hs_mono() >= deadline) break;
        usleep((useconds_t)(HS_REPAIR_LOCK_POLL_SECONDS * 1e6));
        if (hs_mono() >= deadline) {
            /* one last non-blocking try before giving up, matching the loop */
            if (flock(fd, LOCK_EX | LOCK_NB) == 0) acquired = true;
            break;
        }
    }
    if (!acquired) {
        fprintf(stderr, "state.db repair lock held by another process for more "
                        "than %.0fs — skipping schema surgery in this process to "
                        "avoid racing the repairer.\n", HS_REPAIR_LOCK_TIMEOUT_SECONDS);
        close(fd);
        return false;
    }
    if (handle_out) *handle_out = (void *)(intptr_t)fd;
    return true;
}
void _cross_process_repair_lock_release(void *handle) {
    /* finally: flock(LOCK_UN) then close — both best effort. */
    int fd = (int)(intptr_t)handle;
    if (fd <= 0) return;
    flock(fd, LOCK_UN);
    close(fd);
}

/* ── PoP: _bump_schema_cookie @ hermes_state.py:_bump_schema_cookie ── */
void _bump_schema_cookie(sqlite3 *conn) {
    /* Increment PRAGMA schema_version after direct sqlite_master surgery so
     * other connections discard their cached schema. Wraps at 32-bit signed
     * range. Best-effort and never raises. */
    if (!conn) return;
    char *cur = hs_query_text(conn, "PRAGMA schema_version");
    if (!cur) return;
    long v = strtol(cur, NULL, 10);
    free(cur);
    char *sql = NULL;
    if (asprintf(&sql, "PRAGMA schema_version=%ld", (long)((v + 1) & 0x7FFFFFFFL)) < 0)
        return;
    char *err = NULL;
    if (sqlite3_exec(conn, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Could not bump state.db schema cookie: %s\n",
                err ? err : "?");
    }
    if (err) sqlite3_free(err);
    free(sql);
}

/* ── PoP: _mask_volatile_header @ hermes_state.py:_mask_volatile_header ── */
void _mask_volatile_header(unsigned char *head, size_t len) {
    /* Zero the commit-counter fields (bytes 24..28 and 92..96 of the SQLite
     * header) so ordinary writes don't re-key the ledger. In-place; no-op
     * below 96 bytes exactly like Python returning head unchanged. */
    if (!head || len < 96) return;
    for (size_t i = 0; i < sizeof(HS_FP_VOLATILE_RANGES)/sizeof(HS_FP_VOLATILE_RANGES[0]); i++) {
        long start = HS_FP_VOLATILE_RANGES[i][0], end = HS_FP_VOLATILE_RANGES[i][1];
        memset(head + start, 0, (size_t)(end - start));
    }
}

/* ── PoP: _db_fingerprint @ hermes_state.py:_db_fingerprint ── */
char *_db_fingerprint(const char *db_path) {
    /* Cheap identity: size + sha256(masked 64KiB head + up-to-64KiB tail),
     * first 32 hex chars. Deliberately EXCLUDES mtime. Returns NULL on any
     * stat/read failure ("identity unavailable"). The offline_file_access /
     * LiveConnectionError guard has no live-tracked connections in the C
     * engine, so the raw read is safe (the ImportError fallback path). */
    if (!db_path) return NULL;
    struct stat st;
    if (stat(db_path, &st) != 0) return NULL;
    FILE *fh = fopen(db_path, "rb");
    if (!fh) return NULL;
    unsigned char *head = malloc(HS_FINGERPRINT_SAMPLE_BYTES);
    if (!head) { fclose(fh); return NULL; }
    size_t head_n = fread(head, 1, HS_FINGERPRINT_SAMPLE_BYTES, fh);
    size_t tail_n = 0;
    unsigned char *tail = NULL;
    if ((unsigned long long)st.st_size > HS_FINGERPRINT_SAMPLE_BYTES) {
        if (fseek(fh, -(long)HS_FINGERPRINT_SAMPLE_BYTES, SEEK_END) == 0) {
            tail = malloc(HS_FINGERPRINT_SAMPLE_BYTES);
            if (tail) tail_n = fread(tail, 1, HS_FINGERPRINT_SAMPLE_BYTES, fh);
        }
    }
    fclose(fh);
    _mask_volatile_header(head, head_n);
    hs_sha256_t c;
    hs_sha256_init(&c);
    hs_sha256_update(&c, head, head_n);
    if (tail) hs_sha256_update(&c, tail, tail_n);
    char hex[65];
    hs_sha256_hex(&c, hex);
    free(head); free(tail);
    char *out = NULL;
    if (asprintf(&out, "%lld:%.32s", (long long)st.st_size, hex) < 0) return NULL;
    return out;
}

/* ── PoP: _backup_content_identity @ hermes_state.py:_backup_content_identity ── */
char *_backup_content_identity(const char *db_path) {
    /* Recovery-image identity for forensic-backup dedupe: length-delimited
     * digest of the ENTIRE main file plus every present sidecar. Different
     * equivalence relation from _db_fingerprint — byte identity, not epoch
     * identity. Returns NULL on error (caller declines to dedupe). */
    if (!db_path) return NULL;
    hs_sha256_t c;
    hs_sha256_init(&c);
    struct stat st;
    if (stat(db_path, &st) != 0) return NULL;
    /* Length-delimit every member (main file included) so the concatenation
     * is prefix-free: Python updates with f"\0{name}:{size}\0".encode(). */
    char frame[160];
    int fl = snprintf(frame + 1, sizeof(frame) - 2, "main:%lld", (long long)st.st_size);
    if (fl < 0) return NULL;
    frame[0] = '\0';
    frame[fl + 1] = '\0';
    hs_sha256_update(&c, frame, (size_t)fl + 2);
    if (!hs_hash_whole_file(&c, db_path)) return NULL;
    for (int i = 0; HS_DB_SIDECAR_SUFFIXES[i]; i++) {
        const char *suffix = HS_DB_SIDECAR_SUFFIXES[i];
        char *sidecar = NULL;
        asprintf(&sidecar, "%s%s", db_path, suffix);
        if (!sidecar) return NULL;
        struct stat sst;
        if (stat(sidecar, &sst) == 0 && S_ISREG(sst.st_mode)) {
            char sframe[192];
            int sl = snprintf(sframe + 1, sizeof(sframe) - 2, "%s:%lld",
                              suffix, (long long)sst.st_size);
            if (sl > 0) {
                sframe[0] = '\0';
                sframe[sl + 1] = '\0';
                hs_sha256_update(&c, sframe, (size_t)sl + 2);
            }
            if (!hs_hash_whole_file(&c, sidecar)) { free(sidecar); return NULL; }
        }
        free(sidecar);
    }
    char hex[65];
    hs_sha256_hex(&c, hex);
    return strdup(hex);
}

/* ── PoP: _record_repair_outcome @ hermes_state.py:_record_repair_outcome ── */
void _record_repair_outcome(const char *db_path, bool repaired,
                            const char *fingerprint) {
    /* Update the persistent attempt ledger after a repair pass. Never raises.
     * When the fingerprint is unavailable, keep the key already on record and
     * still increment (dropping the pass would let a peer reset the budget). */
    if (!db_path) return;
    char *ledger_path = hermes_state_repair_ledger_path(db_path);
    if (!ledger_path) return;
    do {
        if (repaired) { unlink(ledger_path); break; }
        json_t *ledger = hermes_state_read_repair_ledger(db_path);
        const char *recorded = ledger
            ? json_get_str(ledger, "fingerprint", NULL) : NULL;
        char *rec_copy = hs_strdup(recorded);
        const char *fp = fingerprint ? fingerprint : rec_copy;
        char *fp_fresh = NULL;
        if (!fingerprint) {
            fp_fresh = _db_fingerprint(db_path); /* post-attempt identity */
            fp = fp_fresh ? fp_fresh : rec_copy;
        }
        if (!fp) { free(fp_fresh); free(rec_copy);
                   if (ledger) json_free(ledger); break; }
        double prev = ledger ? json_get_num(ledger, "failed_attempts", 0) : 0;
        long attempts = (rec_copy && strcmp(rec_copy, fp) == 0)
                        ? (long)prev + 1 : 1;
        char iso[32];
        time_t now = time(NULL);
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);
        strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", &tm_buf);
        FILE *fh = fopen(ledger_path, "w");
        if (fh) {
            fprintf(fh, "{\"fingerprint\": \"%s\", \"failed_attempts\": %ld, "
                        "\"last_attempt\": \"%s\"}",
                    fp, attempts, iso);
            fclose(fh);
        }
        free(fp_fresh); free(rec_copy);
        if (ledger) json_free(ledger);
    } while (0);
    free(ledger_path);
}

/* ── PoP: _reapply_durability_barriers @ hermes_state.py:_reapply_durability_barriers ── */
bool _reapply_durability_barriers(sqlite3 *conn) {
    /* Best-effort (re)application of the macOS write barriers
     * (_apply_macos_checkpoint_barrier + _enforce_macos_synchronous_full).
     * Returns true when the pragmas were accepted. On Linux both pragmas
     * parse and stick; on an unparseable schema SQLite raises — mirrored by
     * the non-OK exec return. */
    if (!conn) return false;
    int rc1 = hs_exec(conn, "PRAGMA checkpoint_fullfsync=ON");
    int rc2 = hs_exec(conn, "PRAGMA synchronous=FULL");
    return rc1 == SQLITE_OK && rc2 == SQLITE_OK;
}

/* ── PoP: _connect_repair_durable @ hermes_state.py:_connect_repair_durable ── */
sqlite3 *_connect_repair_durable(const char *db_path, double timeout) {
    /* sqlite3.connect for the repair/probe paths with durability barriers.
     * Autocommit preserved (no implicit BEGIN). Barrier application is
     * best-effort by necessity: on a malformed schema even PRAGMA raises. */
    if (!db_path) return NULL;
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(db_path, &conn, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        /* Python's connect creates when missing; repair paths always target
         * existing files, but keep create semantics for parity. */
        if (conn) sqlite3_close(conn);
        conn = NULL;
        if (sqlite3_open(db_path, &conn) != SQLITE_OK) {
            if (conn) sqlite3_close(conn);
            return NULL;
        }
    }
    sqlite3_busy_timeout(conn, (int)(timeout * 1000.0));
    _reapply_durability_barriers(conn);
    return conn;
}

/* ── PoP: _exclusive_repair_db_guard @ hermes_state.py:_exclusive_repair_db_guard ──
 * Generator → acquire/release. On failure returns false and sets *err_out
 * (malloc'd, caller frees) to the exception text, mirroring `yield None, exc`. */
bool _exclusive_repair_db_guard(const char *db_path, sqlite3 **guard_out,
                                char **err_out) {
    if (guard_out) *guard_out = NULL;
    if (err_out) *err_out = NULL;
    sqlite3 *guard = _connect_repair_durable(db_path, 0.0);
    if (!guard) {
        if (err_out)
            asprintf(err_out, "could not open %s for exclusive repair guard",
                     db_path);
        return false;
    }
    /* locking_mode=EXCLUSIVE retains file-level exclusion after the short
     * BEGIN EXCLUSIVE is rolled back; rollback keeps the connection
     * transaction-free so it can serve as backup source AND destination. */
    int rc = hs_exec(guard, "PRAGMA locking_mode=EXCLUSIVE");
    if (rc == SQLITE_OK) rc = hs_exec(guard, "BEGIN EXCLUSIVE");
    if (rc == SQLITE_OK) rc = hs_exec(guard, "ROLLBACK");
    if (rc != SQLITE_OK) {
        const char *msg = sqlite3_errmsg(guard);
        if (err_out) *err_out = strdup(msg ? msg : "sqlite error");
        hs_exec(guard, "PRAGMA locking_mode=NORMAL"); /* best effort */
        sqlite3_close(guard);
        return false;
    }
    if (guard_out) *guard_out = guard;
    return true;
}
void _exclusive_repair_db_guard_release(sqlite3 *guard) {
    /* finally: drop exclusive locking mode before close so close's checkpoint
     * is not mistaken for a repair write. */
    if (!guard) return;
    hs_exec(guard, "PRAGMA locking_mode=NORMAL");
    sqlite3_close(guard);
}

/* ── PoP: _copy_database_snapshot @ hermes_state.py:_copy_database_snapshot ──
 * Returns 0 on success; on failure returns -1 and sets *err_out (malloc'd).
 * Uses the online backup API in 256-page steps under a deadline so neither
 * file's inode is replaced. */
int _copy_database_snapshot(const char *source_path, const char *destination_path,
                            sqlite3 *source_connection,
                            sqlite3 *destination_connection,
                            char **err_out) {
    if (err_out) *err_out = NULL;
    double deadline_seconds = -1.0;
    {
        extern char *hermes_state_repair_snapshot_timeout_seconds(const char *);
        char *t = hermes_state_repair_snapshot_timeout_seconds(source_path);
        if (t) { deadline_seconds = strtod(t, NULL); free(t); }
        else deadline_seconds = 300.0;
    }
    double deadline = hs_mono() + deadline_seconds;

    sqlite3 *src = source_connection ? source_connection
                                     : _connect_repair_durable(source_path, 5.0);
    if (!src) {
        if (err_out) *err_out = strdup("could not open snapshot source");
        return -1;
    }
    sqlite3 *dst = destination_connection;
    int own_src = source_connection == NULL;
    int own_dst = destination_connection == NULL;
    int failed = 0;
    if (!dst) {
        dst = _connect_repair_durable(destination_path, 5.0);
        if (!dst) {
            if (err_out) *err_out = strdup("could not open snapshot destination");
            if (own_src) sqlite3_close(src);
            return -1;
        }
    } else if (sqlite3_get_autocommit(dst) == 0) {
        /* sqlite3_backup requires a transaction-free destination. The
         * exclusive repair guard retains exclusion via locking_mode, not an
         * active transaction, so it satisfies this. */
        if (err_out)
            *err_out = strdup("SQLite repair backup destination has an active "
                              "transaction");
        if (own_src) sqlite3_close(src);
        return -1;
    }

    sqlite3_backup *bk = sqlite3_backup_init(dst, "main", src, "main");
    if (!bk) {
        if (err_out) *err_out = strdup(sqlite3_errmsg(dst));
        failed = 1;
    } else {
        for (;;) {
            int rc = sqlite3_backup_step(bk, 256);
            if (hs_mono() >= deadline) {
                if (err_out)
                    asprintf(err_out, "timed out copying SQLite repair snapshot "
                                      "after %.0fs", deadline_seconds);
                failed = 1;
                break;
            }
            if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
                usleep((useconds_t)(HS_REPAIR_LOCK_POLL_SECONDS * 1e6));
                continue;
            }
            if (rc != SQLITE_OK) {
                if (rc != SQLITE_DONE && err_out)
                    *err_out = strdup(sqlite3_errmsg(dst));
                if (rc != SQLITE_DONE) failed = 1;
                break;
            }
        }
        sqlite3_backup_finish(bk);
    }
    if (own_dst && dst) sqlite3_close(dst);
    if (own_src && src) sqlite3_close(src);
    return failed ? -1 : 0;
}

/* ── PoP: _live_writer_holds_db @ hermes_state.py:_live_writer_holds_db ── */
bool _live_writer_holds_db(const char *db_path) {
    /* True when another connection holds db_path open: probe via
     * locking_mode=EXCLUSIVE + BEGIN IMMEDIATE (fails SQLITE_BUSY against
     * any live WAL-mode holder). Fails OPEN (false) otherwise. */
    sqlite3 *probe = _connect_repair_durable(db_path, 0.0);
    if (!probe) return false; /* cannot even open → no evidence of holder */
    bool held = false;
    if (hs_exec(probe, "PRAGMA locking_mode=EXCLUSIVE") == SQLITE_OK &&
        hs_exec(probe, "BEGIN IMMEDIATE") != SQLITE_OK) {
        const char *msg = sqlite3_errmsg(probe);
        char *lower = strdup(msg ? msg : "");
        for (char *p = lower; p && *p; p++) *p = (char)tolower((unsigned char)*p);
        held = strstr(lower, "locked") != NULL || strstr(lower, "busy") != NULL;
        free(lower);
    } else {
        hs_exec(probe, "ROLLBACK");
    }
    hs_exec(probe, "PRAGMA locking_mode=NORMAL");
    sqlite3_close(probe);
    return held;
}

/* static analog of _db_opens_cleanly (ported as sr_db_opens_cleanly in the
 * session_recovery unit; this unit needs its own instance): returns NULL when
 * db_path opens and parses cleanly, else a malloc'd reason string. */
static char *gaps_db_opens_cleanly(const char *db_path) {
    sqlite3 *conn = _connect_repair_durable(db_path, 5.0);
    if (!conn) return strdup("database is locked or unreadable");
    char *reason = NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT count(*) FROM sqlite_master", -1,
                           &st, NULL) != SQLITE_OK) {
        const char *m = sqlite3_errmsg(conn);
        reason = strdup(m ? m : "unreadable schema");
    } else if (sqlite3_step(st) != SQLITE_ROW) {
        const char *m = sqlite3_errmsg(conn);
        reason = strdup(m ? m : "schema scan failed");
    }
    sqlite3_finalize(st);
    if (!reason) {
        /* integrity_check catches index/page damage a plain read misses. */
        st = NULL;
        if (sqlite3_prepare_v2(conn, "PRAGMA integrity_check(1)", -1, &st,
                               NULL) == SQLITE_OK &&
            sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *r = sqlite3_column_text(st, 0);
            if (!r || strcmp((const char *)r, "ok") != 0)
                reason = strdup(r ? (const char *)r : "integrity_check failed");
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(conn);
    return reason;
}

/* ── PoP: _unlink_db_triple @ hermes_state.py:_unlink_db_triple ── */
char *_unlink_db_triple(const char *path) {
    /* Remove path and every SQLite sidecar; return any cleanup failure as a
     * malloc'd "; "-joined string, or NULL. POSIX unlink has no Windows
     * PermissionError retry class, so each victim gets one attempt per the
     * FileNotFoundError/OSError branches. */
    if (!path) return NULL;
    char *failures = NULL;
    size_t flen = 0;
    const char *names[4] = { path, NULL, NULL, NULL };
    char *sidecars[3] = { NULL, NULL, NULL };
    for (int i = 0; i < 3; i++)
        asprintf(&sidecars[i], "%s%s", path, HS_DB_SIDECAR_SUFFIXES[i]);
    names[1] = sidecars[0]; names[2] = sidecars[1]; names[3] = sidecars[2];
    for (int i = 0; i < 4; i++) {
        if (!names[i]) continue;
        if (unlink(names[i]) == 0 || errno == ENOENT) continue;
        char piece[1024];
        snprintf(piece, sizeof(piece), "%s: %s", names[i], strerror(errno));
        size_t plen = strlen(piece);
        char *nf = realloc(failures, flen + plen + (flen ? 2 : 0) + 1);
        if (nf) {
            failures = nf;
            if (flen) { memcpy(failures + flen, "; ", 2); flen += 2; }
            memcpy(failures + flen, piece, plen + 1);
            flen += plen;
        }
    }
    for (int i = 0; i < 3; i++) free(sidecars[i]);
    return failures;
}

/* ── PoP: _run_repair_strategies @ hermes_state.py:_run_repair_strategies ── */
json_t *_run_repair_strategies(const char *db_path, json_t *report) {
    /* Escalating repair attempts applied IN PLACE to db_path (the caller runs
     * them on a scratch copy). Strategy 0: FTS 'rebuild'. 0.5: REINDEX.
     * 1: de-duplicate sqlite_master under writable_schema. 2: drop all FTS
     * schema + VACUUM. Sets report.repaired/strategy/error like Python. */
    extern int hermes_state_load_fts5_cjk_extension(const char *so_path);
    if (!report) report = json_object();

    /* ── Strategy 0: rebuild FTS indexes in place ── */
    do {
        char *so = NULL;
        {
            extern char *hermes_state_fts5_cjk_so_path(const char *home);
            char *home = getenv("HERMES_HOME");
            so = hermes_state_fts5_cjk_so_path(home);
        }
        sqlite3 *conn = _connect_repair_durable(db_path, 5.0);
        if (!conn) break;
        if (so) hermes_state_load_fts5_cjk_extension(so);
        free(so);
        const char *fts_tables[] = { "messages_fts", "messages_fts_trigram",
                                     "messages_fts_cjk" };
        for (int i = 0; i < 3; i++) {
            char *sql = NULL;
            if (asprintf(&sql, "INSERT INTO %s(%s) VALUES('rebuild')",
                         fts_tables[i], fts_tables[i]) == 0 && sql) {
                hs_exec(conn, sql); /* OperationalError → absent table: skip */
                free(sql);
            }
        }
        sqlite3_close(conn);
        char *reason = gaps_db_opens_cleanly(db_path);
        if (!reason) {
            json_set(report, "repaired", json_bool(true));
            json_set(report, "strategy", json_string("rebuild_fts"));
            fprintf(stderr, "state.db FTS indexes rebuilt in place "
                            "(schema preserved): %s\n", db_path);
            return report;
        }
        free(reason);
    } while (0);

    /* ── Strategy 0.5: rebuild stale B-tree indexes (#63386) ── */
    do {
        sqlite3 *conn = _connect_repair_durable(db_path, 5.0);
        if (!conn) break;
        _reapply_durability_barriers(conn);
        int rc = hs_exec(conn, "REINDEX");
        sqlite3_close(conn);
        if (rc != SQLITE_OK) break;
        char *reason = gaps_db_opens_cleanly(db_path);
        if (!reason) {
            json_set(report, "repaired", json_bool(true));
            json_set(report, "strategy", json_string("reindex_btree"));
            fprintf(stderr, "state.db B-tree indexes rebuilt via REINDEX: %s\n",
                    db_path);
            return report;
        }
        free(reason);
    } while (0);

    /* ── Strategy 1: de-duplicate sqlite_master (keeps FTS index) ── */
    do {
        sqlite3 *conn = _connect_repair_durable(db_path, 5.0);
        if (!conn) break;
        int ok = hs_exec(conn, "PRAGMA writable_schema=ON") == SQLITE_OK;
        sqlite3_stmt *dupes = NULL;
        if (ok && sqlite3_prepare_v2(conn,
                "SELECT type, name, COUNT(*) AS c, MIN(rowid) AS keep "
                "FROM sqlite_master GROUP BY type, name HAVING c > 1",
                -1, &dupes, NULL) == SQLITE_OK) {
            int had_dupes = 0;
            while (sqlite3_step(dupes) == SQLITE_ROW) {
                const unsigned char *type = sqlite3_column_text(dupes, 0);
                const unsigned char *name = sqlite3_column_text(dupes, 1);
                long long keep = sqlite3_column_int64(dupes, 3);
                sqlite3_stmt *del = NULL;
                (void)del;
                char *sql = NULL;
                if (asprintf(&sql,
                        "DELETE FROM sqlite_master WHERE type IS '%s' AND "
                        "name IS '%s' AND rowid <> %lld",
                        type ? (const char *)type : "",
                        name ? (const char *)name : "", keep) == 0 && sql) {
                    if (hs_exec(conn, sql) == SQLITE_OK) had_dupes = 1;
                    free(sql);
                }
            }
            sqlite3_finalize(dupes);
            if (had_dupes) _bump_schema_cookie(conn);
            hs_exec(conn, "PRAGMA writable_schema=OFF");
        }
        sqlite3_close(conn);
        if (!ok) break;
        char *reason = gaps_db_opens_cleanly(db_path);
        if (!reason) {
            json_set(report, "repaired", json_bool(true));
            json_set(report, "strategy", json_string("dedup_schema"));
            fprintf(stderr, "state.db schema repaired by de-duplicating "
                            "sqlite_master (FTS index preserved): %s\n", db_path);
            return report;
        }
        free(reason);
    } while (0);

    /* ── Strategy 2: drop all FTS schema, VACUUM, rebuild on next open ──
     * The destructive one — only ever safe because callers run it on a
     * scratch copy. */
    do {
        sqlite3 *conn = _connect_repair_durable(db_path, 5.0);
        if (!conn) break;
        int ok = hs_exec(conn, "PRAGMA writable_schema=ON") == SQLITE_OK &&
                 hs_exec(conn, "DELETE FROM sqlite_master WHERE name LIKE "
                               "'messages_fts%'") == SQLITE_OK;
        if (ok) {
            _bump_schema_cookie(conn);
            hs_exec(conn, "PRAGMA writable_schema=OFF");
            /* Schema parses now → barriers can finally stick before the
             * whole-file VACUUM. */
            _reapply_durability_barriers(conn);
            ok = hs_exec(conn, "VACUUM") == SQLITE_OK;
        }
        sqlite3_close(conn);
        char *reason = gaps_db_opens_cleanly(db_path);
        if (ok && !reason) {
            json_set(report, "repaired", json_bool(true));
            json_set(report, "strategy", json_string("drop_fts_rebuild"));
            fprintf(stderr, "state.db schema repaired by dropping FTS schema; "
                            "indexes will rebuild from messages on next open: "
                            "%s\n", db_path);
            return report;
        }
        if (reason) json_set(report, "error", json_string(reason));
        free(reason);
    } while (0);

    /* The "could not recover" log lives in the caller (it names the user DB). */
    return report;
}

/* ── PoP: _repair_state_db_schema_locked @ hermes_state.py:_repair_state_db_schema_locked ── */
json_t *_repair_state_db_schema_locked(const char *db_path, bool backup,
                                       json_t *report) {
    /* Repair strategies for repair_state_db_schema. Caller must hold the
     * cross-process repair lock. Strategies run on a SCRATCH COPY and the
     * result is copied back through the transactional backup API only once
     * proven to open cleanly — a failed repair cannot modify or lose
     * committed canonical data. */
    extern char *hermes_state_repair_scratch_space_error(const char *db_path);
    if (!report) report = json_object();

    char *scratch = NULL;
    asprintf(&scratch, "%s.repair-scratch", db_path);
    if (!scratch) return report;

    char *cleanup_error = _unlink_db_triple(scratch);
    if (cleanup_error) {
        char *msg = NULL;
        asprintf(&msg, "could not remove a stale repair snapshot before "
                       "probing state.db: %s", cleanup_error);
        json_set(report, "error", json_string(msg));
        fprintf(stderr, "state.db repair aborted: %s\n", msg);
        free(msg); free(cleanup_error); free(scratch);
        return report;
    }

    /* Re-probe under the lock: a queued-behind process may have just repaired */
    char *already = gaps_db_opens_cleanly(db_path);
    if (!already) {
        json_set(report, "repaired", json_bool(true));
        json_set(report, "strategy", json_string("already_healthy"));
        free(scratch);
        return report;
    }
    free(already);

    if (backup) {
        extern char *hermes_state_backup_db_file(const char *db_path);
        char *bpath = hermes_state_backup_db_file(db_path);
        if (bpath) json_set(report, "backup_path", json_string(bpath));
        else json_set(report, "backup_path", json_null());
        if (!bpath) {
            /* HARD STOP (#69603): never mutate the only copy of the DB. */
            json_set(report, "error",
                     json_string("pre-repair backup refused; aborting schema "
                                 "repair to avoid mutating the only copy of the "
                                 "damaged DB"));
            fprintf(stderr, "state.db repair aborted: %s\n",
                    json_get_str(report, "error", ""));
            free(scratch);
            return report;
        }
        free(bpath);
    }

    sqlite3 *live_guard = NULL;
    char *guard_error = NULL;
    if (!_exclusive_repair_db_guard(db_path, &live_guard, &guard_error)) {
        char *msg = NULL;
        asprintf(&msg, "could not acquire exclusive state.db repair ownership; "
                       "skipped schema surgery to avoid overwriting a "
                       "concurrent writer. Stop the gateway and retry: %s",
                 guard_error ? guard_error : "?");
        json_set(report, "error", json_string(msg));
        /* Python sets _repair_attempted when the failure consumes an attempt
         * (_repair_failure_consumes_attempt); lock/IO errors do. */
        json_set(report, "_repair_attempted", json_bool(true));
        fprintf(stderr, "state.db repair skipped: %s\n", msg);
        free(msg); free(guard_error); free(scratch);
        return report;
    }

    do {
        char *space_error = hermes_state_repair_scratch_space_error(db_path);
        if (space_error) {
            json_set(report, "error", json_string(space_error));
            fprintf(stderr, "state.db repair aborted: %s\n", space_error);
            free(space_error);
            break;
        }
        char *snap_err = NULL;
        if (_copy_database_snapshot(db_path, scratch, live_guard, NULL,
                                    &snap_err) != 0) {
            char *msg = NULL;
            asprintf(&msg, "could not stage a complete SQLite repair snapshot "
                           "of %s: %s", db_path,
                     snap_err ? snap_err : "?");
            json_set(report, "error", json_string(msg));
            json_set(report, "_repair_attempted", json_bool(true));
            fprintf(stderr, "state.db repair aborted: %s\n", msg);
            free(msg); free(snap_err);
            _unlink_db_triple(scratch);
            break;
        }
        /* A strategy failure is a genuine repair outcome and consumes the
         * persistent budget; promotion failures are classified separately. */
        json_set(report, "_repair_attempted", json_bool(true));
        _run_repair_strategies(scratch, report);
        if (json_get_bool(report, "repaired", false)) {
            char *promo_err = NULL;
            if (_copy_database_snapshot(scratch, db_path, NULL, live_guard,
                                        &promo_err) != 0) {
                char *msg = NULL;
                asprintf(&msg, "repaired snapshot could not be promoted "
                               "transactionally: %s",
                         promo_err ? promo_err : "?");
                json_set(report, "repaired", json_bool(false));
                json_obj_del(report, "strategy");
                json_set(report, "_repair_attempted",
                         json_bool(promo_err != NULL)); /* environmental aborts
                                                           don't consume */
                json_set(report, "error", json_string(msg));
                fprintf(stderr, "state.db repair promotion failed: %s\n",
                        promo_err ? promo_err : "?");
                free(msg); free(promo_err);
            } else {
                fprintf(stderr, "state.db repaired via '%s' and promoted "
                                "transactionally: %s\n",
                        json_get_str(report, "strategy", "?"), db_path);
            }
        }
        if (!json_get_bool(report, "repaired", false)) {
            /* Named against the USER's database, not the scratch copy. */
            fprintf(stderr, "state.db schema repair could not recover %s "
                            "automatically (no committed canonical data was "
                            "modified or lost; backup: %s); manual restore from "
                            "backup may be required.\n",
                    db_path, json_get_str(report, "backup_path", "none"));
        }
    } while (0);

    /* finally: never leave a half-repaired file beside the DB. */
    cleanup_error = _unlink_db_triple(scratch);
    if (cleanup_error) {
        fprintf(stderr, "Could not remove state.db repair snapshot after "
                        "repair: %s\n", cleanup_error);
        free(cleanup_error);
    }
    _exclusive_repair_db_guard_release(live_guard);
    free(guard_error);
    free(scratch);
    return report;
}

/* holder tuple: (pid, path) — mirrors List[Tuple[int, str]] */
typedef struct { long pid; char *path; } hs_holder_t;

/* ── PoP: _is_inactive_orphan_desktop_holder @ hermes_state.py:_is_inactive_orphan_desktop_holder ── */
bool _is_inactive_orphan_desktop_holder(long ppid, double age_seconds,
                                        double min_age_seconds,
                                        bool ephemeral_backend,
                                        const char *const *connection_statuses,
                                        int n_statuses) {
    /* Pure safety predicate for the narrow Desktop holder reap. */
    if (ppid != 0 && ppid != 1) return false;
    if (age_seconds < min_age_seconds) return false;
    if (!ephemeral_backend) return false;
    for (int i = 0; i < n_statuses; i++)
        if (connection_statuses[i] &&
            strcmp(connection_statuses[i], "ESTABLISHED") == 0)
            return false;
    return true;
}

/* ── PoP: _concrete_state_db_holder_pids @ hermes_state.py:_concrete_state_db_holder_pids ── */
long *_concrete_state_db_holder_pids(const char *db_path, hs_holder_t *holders,
                                     int n_holders, int *out_n) {
    /* Unique PIDs proven to hold this DB or one of its sidecars. Caller
     * frees the array; *out_n is 0 when none. */
    *out_n = 0;
    if (!db_path || !holders || n_holders <= 0) return NULL;
    char *canonical_db = hs_canonical_path(db_path);
    if (!canonical_db) return NULL;
    const int watched_n = 4;
    char **watched = malloc(sizeof(char *) * watched_n);
    watched[0] = canonical_db;
    for (int i = 1; i < 4; i++) {
        watched[i] = NULL;
        asprintf(&watched[i], "%s%s", canonical_db, HS_DB_SIDECAR_SUFFIXES[i - 1]);
    }
    long *pids = malloc(sizeof(long) * (size_t)(n_holders > 0 ? n_holders : 1));
    for (int i = 0; i < n_holders; i++) {
        long pid = holders[i].pid;
        char *cp = hs_canonical_path(holders[i].path);
        int match = cp != NULL;
        if (match)
            for (int w = 0; w < watched_n && match; w++)
                if (strcmp(cp, watched[w]) == 0) match = 2; /* found */
        if (match == 2 && pid > 0) {
            int seen = 0;
            for (int j = 0; j < *out_n; j++) if (pids[j] == pid) seen = 1;
            if (!seen) pids[(*out_n)++] = pid;
        }
        free(cp);
    }
    for (int i = 0; i < watched_n; i++) free(watched[i]);
    free(watched);
    return pids;
}

/* ── PoP: _close_connection_quietly @ hermes_state.py:_close_connection_quietly ── */
void _close_connection_quietly(sqlite3 *conn) {
    /* Close a partially initialized connection without masking its error. */
    if (!conn) return;
    sqlite3_close_v2(conn);
}

/* read /proc/<pid>/cmdline into a single spaced string; malloc'd or NULL */
static char *hs_read_proc_cmdline(long pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 256, len = 0;
    char *buf = malloc(cap);
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 2 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = c == '\0' ? ' ' : (char)c;
    }
    fclose(f);
    if (!len) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

/* _looks_like_hermes: argv mentions our binary markers */
static bool hs_looks_like_hermes(const char *cmdline) {
    return cmdline && (strstr(cmdline, "hermes") != NULL ||
                       strstr(cmdline, "slermes") != NULL);
}

/* ── PoP: _foreign_state_db_holders @ hermes_state.py:_foreign_state_db_holders ── */
hs_holder_t *_foreign_state_db_holders(const char *db_path, int *out_n) {
    /* Foreign processes holding this DB or its WAL sidecars. A scan failure
     * is represented as an unknown holder ((-1, "...")) — skipping optional
     * automatic maintenance is safer than assuming quiescence.
     * Linux path: read /proc/<pid>/fd symlinks directly (psutil's
     * isfile_strict filtering silently drops unlinked WAL sidecars). */
    *out_n = 0;
    if (!db_path) return NULL;
    hs_holder_t *holders = NULL;
    int n = 0, cap = 0;
#define HS_PUSH(p, pth) do { \
        if (n == cap) { cap = cap ? cap * 2 : 8; \
                        holders = realloc(holders, sizeof(hs_holder_t) * (size_t)cap); } \
        holders[n].pid = (p); holders[n].path = (pth); n++; } while (0)

    char *canon = hs_canonical_path(db_path);
    if (!canon) { *out_n = 0; return NULL; }
    char *watched[3];
    watched[0] = canon;
    for (int i = 1; i < 3; i++)
        asprintf(&watched[i], "%s%s", canon, HS_DB_SIDECAR_SUFFIXES[i - 1]);

    DIR *proc = opendir("/proc");
    if (!proc) {
        HS_PUSH(-1, strdup("open-file scan unavailable"));
        for (int i = 0; i < 3; i++) free(watched[i]);
        *out_n = n;
        return holders;
    }
    long own_pid = (long)getpid();
    struct dirent *de;
    bool scan_failed = false;
    char *scan_err = NULL;
    while ((de = readdir(proc)) != NULL) {
        if (!isdigit((unsigned char)de->d_name[0])) continue;
        long pid = strtol(de->d_name, NULL, 10);
        if (pid == own_pid) continue;
        char fd_dir[64];
        snprintf(fd_dir, sizeof(fd_dir), "/proc/%ld/fd", pid);
        DIR *fdd = opendir(fd_dir);
        if (!fdd) {
            /* Cannot read this fd table (different user). Only flag
             * uninspectable holders that look like another Hermes instance. */
            char *cmdline = hs_read_proc_cmdline(pid);
            if (cmdline && hs_looks_like_hermes(cmdline)) {
                char *short_cmd = strndup(cmdline, 80);
                char *label = NULL;
                asprintf(&label, "uninspectable holder: %s",
                         short_cmd ? short_cmd : "");
                HS_PUSH(pid, label);
                free(short_cmd);
            }
            free(cmdline);
            continue;
        }
        struct dirent *fd_de;
        while ((fd_de = readdir(fdd)) != NULL) {
            char target[4096];
            char fd_path[128];
            snprintf(fd_path, sizeof(fd_path), "%s/%s", fd_dir, fd_de->d_name);
            ssize_t rn = readlink(fd_path, target, sizeof(target) - 1);
            if (rn <= 0) continue;
            target[rn] = '\0';
            char *ct = hs_canonical_path(target);
            if (!ct) continue;
            for (int w = 0; w < 3; w++)
                if (strcmp(ct, watched[w]) == 0) HS_PUSH(pid, strdup(target));
            free(ct);
        }
        closedir(fdd);
    }
    closedir(proc);
    for (int i = 0; i < 3; i++) free(watched[i]);
    if (scan_failed && n == 0) HS_PUSH(-1, scan_err ? scan_err
                                                    : strdup("open-file scan failed"));
    else free(scan_err);
#undef HS_PUSH
    *out_n = n;
    return holders;
}

/* ── PoP: _reap_inactive_orphan_desktop_holders @ hermes_state.py:_reap_inactive_orphan_desktop_holders ── */
long *_reap_inactive_orphan_desktop_holders(const char *db_path,
                                            hs_holder_t *holders, int n_holders,
                                            double min_age_seconds,
                                            int *out_n) {
    /* Terminate old PPID-1 Desktop ephemeral backends with no client.
     * Inspection fails closed: anything whose parent, age, argv, or network
     * connections cannot be proved safe remains a repair-blocking holder.
     * C runtime reads /proc/<pid>/{stat,cmdline,net dev status} directly in
     * place of psutil. */
    *out_n = 0;
    int npids = 0;
    long *cands = _concrete_state_db_holder_pids(db_path, holders, n_holders,
                                                 &npids);
    if (!cands || npids == 0) { free(cands); return NULL; }
    time_t now = time(NULL);
    long *signalled = malloc(sizeof(long) * (size_t)npids);
    int nsig = 0;
    for (int i = 0; i < npids; i++) {
        long pid = cands[i];
        char stat_path[64];
        snprintf(stat_path, sizeof(stat_path), "/proc/%ld/stat", pid);
        FILE *f = fopen(stat_path, "r");
        if (!f) continue;
        char line[2048];
        if (!fgets(line, sizeof(line), f)) { fclose(f); continue; }
        fclose(f);
        /* fields after the comm in parens: state(1) ppid(2) starttime(21) */
        char *p = strrchr(line, ')');
        if (!p) continue;
        long ppid = -1, starttime_ticks = 0;
        if (sscanf(p + 1, " %*c %ld", &ppid) != 1) continue;
        /* field 22 overall → offset 20 after state */
        {
            long field = 1;
            char *q = strtok(p + 1, " ");
            q = strtok(NULL, " "); /* skip state? no: first token IS state */
            field = 1;
            q = p + 1;
            char *save = NULL;
            char *tok = strtok_r(q, " ", &save); /* state */
            (void)tok;
            while ((tok = strtok_r(NULL, " ", &save)) != NULL) {
                field++;
                if (field == 21) { starttime_ticks = strtol(tok, NULL, 10); break; }
            }
        }
        double age_seconds = 0.0;
        {
            long hz = sysconf(_SC_CLK_TCK);
            if (hz <= 0) hz = 100;
            unsigned long long uptime_s = 0;
            FILE *uf = fopen("/proc/uptime", "r");
            double up = 0.0, idle = 0.0;
            if (uf) {
                if (fscanf(uf, "%lf %lf", &up, &idle) == 2) uptime_s = (unsigned long long)up;
                fclose(uf);
            }
            age_seconds = (double)now - (up - ((double)starttime_ticks / (double)hz));
        }
        /* ephemeral backend probe: cmdline contains the port-zero backend marker */
        char *cmdline = hs_read_proc_cmdline(pid);
        bool ephemeral_backend = cmdline &&
            strstr(cmdline, "--port") != NULL && strstr(cmdline, "0") != NULL;
        /* network statuses: /proc/<pid>/net/tcp ESTABLISHED check (state 01) */
        const char *statuses[1] = { "ESTABLISHED" };
        int n_statuses = 0;
        {
            char net_path[64];
            snprintf(net_path, sizeof(net_path), "/proc/%ld/net/tcp", pid);
            FILE *nf = fopen(net_path, "r");
            if (nf) {
                char nl[512];
                while (fgets(nl, sizeof(nl), nf))
                    if (strstr(nl, " 01 ")) { n_statuses = 1; break; }
                fclose(nf);
            }
        }
        if (!_is_inactive_orphan_desktop_holder(ppid, age_seconds,
                                                min_age_seconds,
                                                ephemeral_backend, statuses,
                                                n_statuses))
            continue;
        if (kill((pid_t)pid, SIGTERM) == 0) signalled[nsig++] = pid;
        free(cmdline);
    }
    free(cands);
    if (nsig == 0) { free(signalled); *out_n = 0; return NULL; }
    /* wait up to 1.5s then escalate to SIGKILL for survivors */
    for (int attempt = 0; attempt < 15; attempt++) {
        usleep(100000);
        long *alive = malloc(sizeof(long) * (size_t)nsig);
        int nalive = 0;
        for (int i = 0; i < nsig; i++)
            if (kill((pid_t)signalled[i], 0) == 0) alive[nalive++] = signalled[i];
        if (nalive == 0) { free(alive); break; }
        if (attempt == 14) {
            for (int i = 0; i < nalive; i++) kill((pid_t)alive[i], SIGKILL);
        }
        free(alive);
    }
    *out_n = nsig;
    return signalled;
}

/* ── PoP: __enter__ @ hermes_state.py:__enter__ ── */
hermes_state_db_t *__enter__(hermes_state_db_t *self) {
    /* Enter a scope that closes this handle on the way out. Returns self so
     * `with SessionDB(path) as db:` binds the same object. Purely additive:
     * callers that already close() directly are unaffected. */
    return self;
}

/* ── PoP: __exit__ @ hermes_state.py:__exit__ ── */
bool __exit__(hermes_state_db_t *self) {
    /* Close the handle, then let any exception propagate. Returns False
     * (never suppressing): the scope only manages the descriptor lifetime. */
    hermes_state_close(self);
    return false;
}

/* ── PoP: __del__ @ hermes_state.py:__del__ ── */
void __del__(hermes_state_db_t *self) {
    /* Safety net: close the connection if the caller forgot. Delegates to
     * close() so pool/writer/atexit cleanup all run. Idempotent. */
    if (!self) return;
    hermes_state_close(self);
}

/* ── PoP: list_never_active_keyed_sessions @ hermes_state.py:list_never_active_keyed_sessions ── */
json_t *list_never_active_keyed_sessions(hermes_state_db_t *self,
                                         double older_than_days) {
    /* Keyed gateway rows opened and then never used at all: session_key set,
     * still open, no messages/tokens/tool/API calls, no activity, no title,
     * not pinned, not archived, started before the cutoff. Returns a JSON
     * array of row dicts (caller frees). */
    json_t *rows = json_array();
    if (!self || !self->db) return rows;
    double cutoff = (double)time(NULL) - older_than_days * 86400.0;
    const char *sql =
        "SELECT s.id, s.session_key, s.source, s.chat_id,"
        "       s.chat_type, s.user_id, s.started_at"
        "  FROM sessions s"
        " WHERE s.session_key IS NOT NULL"
        "   AND s.ended_at IS NULL"
        "   AND s.title IS NULL"
        "   AND s.last_activity_at IS NULL"
        "   AND COALESCE(s.message_count, 0) = 0"
        "   AND COALESCE(s.tool_call_count, 0) = 0"
        "   AND COALESCE(s.api_call_count, 0) = 0"
        "   AND COALESCE(s.input_tokens, 0) = 0"
        "   AND COALESCE(s.output_tokens, 0) = 0"
        "   AND COALESCE(s.pinned, 0) = 0"
        "   AND COALESCE(s.archived, 0) = 0"
        "   AND s.started_at IS NOT NULL"
        "   AND s.started_at < ?"
        "   AND NOT EXISTS (SELECT 1 FROM messages m WHERE m.session_id = s.id)"
        " ORDER BY s.started_at";
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, cutoff);
        while (sqlite3_step(st) == SQLITE_ROW) {
            json_t *row = json_object();
            static const char *cols[] = { "id", "session_key", "source",
                                          "chat_id", "chat_type", "user_id",
                                          "started_at" };
            for (int i = 0; i < 7; i++) {
                if (sqlite3_column_type(st, i) == SQLITE_NULL)
                    json_set(row, cols[i], json_null());
                else if (i == 6)
                    json_set(row, cols[i],
                             json_number((double)sqlite3_column_int64(st, i)));
                else
                    json_set(row, cols[i], json_string(
                        (const char *)sqlite3_column_text(st, i)));
            }
            json_append(rows, row);
        }
    }
    sqlite3_finalize(st);
    return rows;
}

/* ── PoP: _delete_routing_entries_for_sessions @ hermes_state.py:_delete_routing_entries_for_sessions ── */
int _delete_routing_entries_for_sessions(hermes_state_db_t *self,
                                         char **session_ids, int n_ids) {
    /* Drop gateway_routing rows whose entry_json names any of *session_ids*.
     * Routing entries are keyed by (scope, session_key); the target session
     * lives inside entry_json, so the match is done over all scopes here. */
    if (!self || !self->db || !session_ids || n_ids <= 0) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "SELECT scope, session_key, entry_json FROM gateway_routing",
            -1, &st, NULL) != SQLITE_OK)
        return 0;
    typedef struct { char *scope; char *key; } doomed_t;
    doomed_t *doomed = NULL;
    int ndoomed = 0, cap = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *entry_json = sqlite3_column_text(st, 2);
        const char *ej = entry_json ? (const char *)entry_json : "{}";
        bool bad = false;
        json_t *entry = json_parse(ej, NULL);
        if (!entry) continue;
        const char *sid = NULL;
        if (entry->type == JSON_OBJECT)
            sid = json_get_str(entry, "session_id", NULL);
        for (int i = 0; i < n_ids; i++)
            if (sid && strcmp(sid, session_ids[i]) == 0) { bad = true; break; }
        json_free(entry);
        if (!bad) continue;
        if (ndoomed == cap) {
            cap = cap ? cap * 2 : 8;
            doomed = realloc(doomed, sizeof(doomed_t) * (size_t)cap);
        }
        doomed[ndoomed].scope =
            strdup((const char *)sqlite3_column_text(st, 0));
        doomed[ndoomed].key =
            strdup((const char *)sqlite3_column_text(st, 1));
        ndoomed++;
    }
    sqlite3_finalize(st);
    if (!ndoomed) return 0;
    int deleted = 0;
    hs_exec(self->db, "BEGIN IMMEDIATE");
    sqlite3_stmt *del = NULL;
    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM gateway_routing WHERE scope = ? AND session_key = ?",
            -1, &del, NULL) == SQLITE_OK) {
        for (int i = 0; i < ndoomed; i++) {
            sqlite3_bind_text(del, 1, doomed[i].scope, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(del, 2, doomed[i].key, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(del) == SQLITE_DONE) deleted++;
            sqlite3_reset(del);
            sqlite3_clear_bindings(del);
        }
        sqlite3_finalize(del);
    }
    hs_exec(self->db, "COMMIT");
    for (int i = 0; i < ndoomed; i++) { free(doomed[i].scope); free(doomed[i].key); }
    free(doomed);
    return deleted;
}

/* ── PoP: prune_never_active_keyed_sessions @ hermes_state.py:prune_never_active_keyed_sessions ── */
void prune_never_active_keyed_sessions(hermes_state_db_t *self,
                                       double older_than_days,
                                       const char *sessions_dir,
                                       long *out_deleted,
                                       long *out_routing_deleted) {
    /* Delete never-active keyed rows and the routing entries naming them.
     * Routing entries go first: a stale entry that outlived its target would
     * leave the gateway resuming a session id that no longer exists. Deletion
     * goes through delete_session so cascade/FTS/transcript cleanup stays in
     * one implementation. Returns (sessions_deleted, routing_entries_deleted). */
    extern bool hermes_state_delete_session(hermes_state_db_t *, const char *,
                                            const char *);
    *out_deleted = 0;
    *out_routing_deleted = 0;
    json_t *candidates = list_never_active_keyed_sessions(self, older_than_days);
    int ncand = candidates ? (int)json_len(candidates) : 0;
    if (!ncand) { if (candidates) json_free(candidates); return; }
    char **ids = malloc(sizeof(char *) * (size_t)ncand);
    for (int i = 0; i < ncand; i++) {
        json_t *row = json_get(candidates, i);
        ids[i] = strdup(json_get_str(row, "id", ""));
    }
    json_free(candidates);
    *out_routing_deleted =
        _delete_routing_entries_for_sessions(self, ids, ncand);
    long deleted = 0;
    for (int i = 0; i < ncand; i++)
        if (hermes_state_delete_session(self, ids[i], sessions_dir))
            deleted++;
    *out_deleted = deleted;
    for (int i = 0; i < ncand; i++) free(ids[i]);
    free(ids);
}

/* ── PoP: publish_session_git_metadata @ hermes_state.py:publish_session_git_metadata ── */
bool publish_session_git_metadata(hermes_state_db_t *self,
                                  const char *session_id, const char *cwd,
                                  long generation, const char *git_branch,
                                  const char *git_repo_root) {
    /* Publish async Git enrichment only while its cwd claim is current. */
    if (!self || !self->db || !session_id || !*session_id || !cwd || !*cwd ||
        generation < 1)
        return false;
    char *branch = git_branch ? strdup(git_branch) : strdup("");
    char *repo_root = git_repo_root ? strdup(git_repo_root) : strdup("");
    if (branch) { char *s = branch; while (*s == ' ' || *s == '\t') s++; memmove(branch, s, strlen(s) + 1); }
    if (repo_root) { char *s = repo_root; while (*s == ' ' || *s == '\t') s++; memmove(repo_root, s, strlen(s) + 1); }
    bool has_branch = branch && *branch;
    bool has_root = repo_root && *repo_root;
    if (!has_branch && !has_root) { free(branch); free(repo_root); return false; }

    char sets[256];
    snprintf(sets, sizeof(sets), "%s%s%s%s",
             has_branch ? "git_branch = ?" : "",
             has_branch && has_root ? ", " : "",
             has_root ? "git_repo_root = ?" : "", "");
    char *sql = NULL;
    asprintf(&sql, "UPDATE sessions SET %s "
                   "WHERE id = ? AND cwd = ? AND git_metadata_generation = ?",
             sets);
    sqlite3_stmt *st = NULL;
    bool updated = false;
    if (sql && sqlite3_prepare_v2(self->db, sql, -1, &st, NULL) == SQLITE_OK) {
        int p = 1;
        if (has_branch) sqlite3_bind_text(st, p++, branch, -1, SQLITE_TRANSIENT);
        if (has_root) sqlite3_bind_text(st, p++, repo_root, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, p++, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, p++, cwd, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, p++, generation);
        updated = sqlite3_step(st) == SQLITE_DONE &&
                  sqlite3_changes(self->db) == 1;
    }
    sqlite3_finalize(st);
    free(sql); free(branch); free(repo_root);
    return updated;
}

/* ── PoP: increment_hygiene_failure_streak @ hermes_state.py:increment_hygiene_failure_streak ── */
int increment_hygiene_failure_streak(hermes_state_db_t *self,
                                     const char *session_key) {
    /* Atomically increment the session-hygiene failure streak for one chat. */
    if (!self || !self->db || !session_key || !*session_key) return 1;
    hs_exec(self->db, "BEGIN IMMEDIATE");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO gateway_hygiene_state (session_key, failure_streak)"
            " VALUES (?, 1)"
            " ON CONFLICT(session_key) DO UPDATE SET"
            "     failure_streak = gateway_hygiene_state.failure_streak + 1",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_key, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    int streak = 1;
    st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "SELECT failure_streak FROM gateway_hygiene_state"
            " WHERE session_key = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW)
            streak = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    hs_exec(self->db, "COMMIT");
    return streak;
}

/* ── PoP: reset_hygiene_failure_streak @ hermes_state.py:reset_hygiene_failure_streak ── */
void reset_hygiene_failure_streak(hermes_state_db_t *self,
                                  const char *session_key) {
    /* Clear the persisted session-hygiene failure streak for one chat. */
    if (!self || !self->db || !session_key || !*session_key) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM gateway_hygiene_state WHERE session_key = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_key, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
}

/* ── PoP: _session_turn_lease_key_on_conn @ hermes_state.py:_session_turn_lease_key_on_conn ── */
char *_session_turn_lease_key_on_conn(hermes_state_db_t *self,
                                      sqlite3 *conn, const char *session_id) {
    /* Walk compression parents on conn to the conversation lease key. Must
     * run on the same connection as the lease INSERT/UPDATE/DELETE. Markers
     * bind to parent_session_id + end_reason='compression' (the explicit-fork
     * filter: branched/delegate children break the walk via their model_config
     * markers). */
    if (!conn || !session_id || !*session_id) return hs_strdup(session_id);
    char *current = NULL;             /* owned row id */
    char *cur_parent = NULL, *cur_source = NULL,
         *cur_model_config = NULL, *cur_end = NULL;
    /* _is_explicit_fork_child_row equivalent (hermes_state.py):
     * model_config JSON carries a fork/delegate marker. */
    char *seen[64];
    int nseen = 0;
    seen[nseen++] = strdup(session_id);
    char result[256];

    for (;;) {
        free(cur_parent); free(cur_source); free(cur_model_config); free(cur_end);
        cur_parent = cur_source = cur_model_config = cur_end = NULL;
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT id, parent_session_id, source, model_config, end_reason"
                " FROM sessions WHERE id = ?",
                -1, &st, NULL) != SQLITE_OK)
            break;
        sqlite3_bind_text(st, 1, current ? current : session_id, -1,
                          SQLITE_TRANSIENT);
        if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); break; }
        const unsigned char *id = sqlite3_column_text(st, 0);
        free(current);
        current = strdup(id ? (const char *)id : "");
        const unsigned char *parent = sqlite3_column_text(st, 1);
        cur_parent = parent ? strdup((const char *)parent) : NULL;
        const unsigned char *mc = sqlite3_column_text(st, 3);
        cur_model_config = mc ? strdup((const char *)mc) : NULL;
        const unsigned char *er = sqlite3_column_text(st, 4);
        cur_end = er ? strdup((const char *)er) : NULL;
        sqlite3_finalize(st);

        bool in_seen = false;
        for (int i = 0; i < nseen; i++)
            if (cur_parent && strcmp(seen[i], cur_parent) == 0) in_seen = true;
        if (!cur_parent || !*cur_parent || in_seen ||
            (cur_model_config && strstr(cur_model_config, "_branched_from")) ||
            (cur_model_config && strstr(cur_model_config, "delegate")))
            break;
        /* parent must be a compression continuation to keep walking */
        sqlite3_stmt *pst = NULL;
        bool parent_is_compression_child = false;
        if (sqlite3_prepare_v2(conn,
                "SELECT end_reason FROM sessions WHERE id = ?",
                -1, &pst, NULL) == SQLITE_OK) {
            sqlite3_bind_text(pst, 1, cur_parent, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(pst) == SQLITE_ROW) {
                const unsigned char *per = sqlite3_column_text(pst, 0);
                parent_is_compression_child =
                    per && strcmp((const char *)per, "compression") == 0;
            }
            sqlite3_finalize(pst);
        }
        if (!parent_is_compression_child) break;
        if (nseen < 64) seen[nseen++] = strdup(cur_parent);
        free(current);
        current = strdup(cur_parent);
    }
    snprintf(result, sizeof(result), "%s", current ? current : session_id);
    for (int i = 0; i < nseen; i++) free(seen[i]);
    free(current); free(cur_parent); free(cur_source);
    free(cur_model_config); free(cur_end);
    return strdup(result);
}

/* ── PoP: _session_turn_lease_key @ hermes_state.py:_session_turn_lease_key ── */
char *_session_turn_lease_key(hermes_state_db_t *self, const char *session_id) {
    /* Stable serialization key for every compression segment (tests +
     * diagnostics). Does not swallow lock errors — the caller sees the raw
     * open failure. */
    if (!self || !self->db || !session_id || !*session_id)
        return hs_strdup(session_id);
    return _session_turn_lease_key_on_conn(self, self->db, session_id);
}

/* _compression_lock_holder_process_is_dead analog (ported in the locks unit):
 * holder strings look like "host:pid"; dead when the pid is not alive. */
static bool hs_holder_pid_is_dead(const char *holder) {
    if (!holder) return false;
    const char *colon = strrchr(holder, ':');
    if (!colon) return false;
    long pid = strtol(colon + 1, NULL, 10);
    if (pid <= 0) return false;
    return kill((pid_t)pid, 0) != 0 && errno == ESRCH;
}

/* ── PoP: try_acquire_session_turn_lease @ hermes_state.py:try_acquire_session_turn_lease ── */
bool try_acquire_session_turn_lease(hermes_state_db_t *self,
                                    const char *session_id, const char *holder,
                                    double ttl_seconds) {
    /* Atomically acquire the cross-process turn lease for a conversation.
     * The durable key is the lineage root; the walk and INSERT share one
     * write transaction. Expired leases and leases whose local holder PID is
     * dead are reclaimed inside that same transaction. */
    if (!self || !self->db || !session_id || !*session_id || !holder || !*holder)
        return false;
    double now = (double)time(NULL);
    double expires_at = now + (ttl_seconds > 0.1 ? ttl_seconds : 0.1);
    hs_exec(self->db, "BEGIN IMMEDIATE");
    char *conversation_id =
        _session_turn_lease_key_on_conn(self, self->db, session_id);
    sqlite3_stmt *st = NULL;
    char current_holder[512] = "";
    double current_expires = 0.0;
    bool have_row = false;
    if (sqlite3_prepare_v2(self->db,
            "SELECT holder, expires_at FROM session_turn_leases"
            " WHERE conversation_id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, conversation_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *h = sqlite3_column_text(st, 0);
            snprintf(current_holder, sizeof(current_holder), "%s",
                     h ? (const char *)h : "");
            current_expires = sqlite3_column_double(st, 1);
            have_row = true;
        }
    }
    sqlite3_finalize(st);
    if (have_row && (current_expires <= now ||
                     hs_holder_pid_is_dead(current_holder))) {
        st = NULL;
        if (sqlite3_prepare_v2(self->db,
                "DELETE FROM session_turn_leases"
                " WHERE conversation_id = ? AND holder = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, conversation_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, current_holder, -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }
    }
    st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "INSERT OR IGNORE INTO session_turn_leases"
            " (conversation_id, holder, acquired_at, expires_at)"
            " VALUES (?, ?, ?, ?)",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, conversation_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, holder, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 3, now);
        sqlite3_bind_double(st, 4, expires_at);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    bool acquired = false;
    st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "SELECT holder FROM session_turn_leases WHERE conversation_id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, conversation_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *h = sqlite3_column_text(st, 0);
            acquired = h && strcmp((const char *)h, holder) == 0;
        }
        sqlite3_finalize(st);
    }
    free(conversation_id);
    hs_exec(self->db, "COMMIT");
    return acquired;
}

/* ── PoP: acquire_session_turn_lease @ hermes_state.py:acquire_session_turn_lease ── */
bool acquire_session_turn_lease(hermes_state_db_t *self,
                                const char *session_id, const char *holder,
                                double ttl_seconds, double wait_seconds,
                                double poll_interval_seconds,
                                void (*on_wait)(double elapsed, void *ud),
                                void *on_wait_ud,
                                bool (*should_abort)(void *ud), void *abort_ud,
                                double acquire_patience_s) {
    /* Wait for a cross-process turn lease WITHOUT holding a SQLite lock.
     * on_wait fires on the first failure (~elapsed 0) and about every notice
     * interval while waiting. should_abort()==true stops immediately with
     * false. Non-"locked" persistence errors propagate as -1-style failure
     * (C returns false after closing, matching Python's raise at the call
     * boundary being fatal). */
    double deadline = hs_mono() + (wait_seconds > 0 ? wait_seconds : 0);
    double wait_started = -1.0, last_notice_at = -1.0;
    double notice_every = 15.0;
    for (;;) {
        if (should_abort && should_abort(abort_ud)) return false;
        if (try_acquire_session_turn_lease(self, session_id, holder,
                                           ttl_seconds))
            return true;
        /* Python re-raises non-locked classify_persistence_error results; the
         * C try_acquire surfaces those as false-with-open-failure, so poll
         * until wait_seconds or abort either way. */
        double now = hs_mono();
        double remaining = deadline - now;
        if (remaining <= 0) return false;
        if (wait_started < 0) wait_started = now;
        if (on_wait && (last_notice_at < 0 || notice_every <= 0 ||
                        (now - last_notice_at) >= notice_every)) {
            on_wait(now - wait_started > 0 ? now - wait_started : 0, on_wait_ud);
            last_notice_at = now;
        }
        double sleep_s = poll_interval_seconds < 0.01 ? 0.01
                                                      : poll_interval_seconds;
        if (sleep_s > remaining) sleep_s = remaining;
        usleep((useconds_t)(sleep_s * 1e6));
    }
}

/* ── PoP: refresh_session_turn_lease @ hermes_state.py:refresh_session_turn_lease ── */
bool refresh_session_turn_lease(hermes_state_db_t *self,
                                const char *session_id, const char *holder,
                                double ttl_seconds) {
    /* Extend a turn lease only while holder still owns it. */
    if (!self || !self->db || !session_id || !*session_id || !holder || !*holder)
        return false;
    double expires_at = (double)time(NULL) +
                        (ttl_seconds > 0.1 ? ttl_seconds : 0.1);
    char *conversation_id =
        _session_turn_lease_key_on_conn(self, self->db, session_id);
    sqlite3_stmt *st = NULL;
    bool refreshed = false;
    if (sqlite3_prepare_v2(self->db,
            "UPDATE session_turn_leases SET expires_at = ?"
            " WHERE conversation_id = ? AND holder = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, expires_at);
        sqlite3_bind_text(st, 2, conversation_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, holder, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        refreshed = sqlite3_changes(self->db) > 0;
    }
    sqlite3_finalize(st);
    free(conversation_id);
    return refreshed;
}

/* ── PoP: release_session_turn_lease @ hermes_state.py:release_session_turn_lease ── */
void release_session_turn_lease(hermes_state_db_t *self,
                                const char *session_id, const char *holder) {
    /* Release a turn lease iff holder still owns it; idempotent. */
    if (!self || !self->db || !session_id || !*session_id || !holder || !*holder)
        return;
    char *conversation_id =
        _session_turn_lease_key_on_conn(self, self->db, session_id);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM session_turn_leases"
            " WHERE conversation_id = ? AND holder = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, conversation_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, holder, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    free(conversation_id);
}

/* ── PoP: session_gateway_runtime @ hermes_state.py:session_gateway_runtime ── */
json_t *session_gateway_runtime(const json_t *session_meta) {
    /* Read the persisted runtime route off a session row dict. Prefers the
     * nested gateway_runtime key, falls back to top-level provider/base_url/
     * api_mode, then to the billing_provider column (bare billing buckets
     * "auto"/"custom" are not routable and are filtered out). Empty object on
     * any parse failure. */
    json_t *out = json_object();
    const json_t *raw = session_meta ? json_obj_get(session_meta, "model_config")
                                     : NULL;
    json_t *cfg = NULL;
    if (raw && raw->type == JSON_STRING) {
        cfg = json_parse(raw->str_val, NULL);
    } else if (raw && raw->type == JSON_OBJECT) {
        cfg = json_copy(raw);
    }
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return out; /* {} on any parse failure */
    }
    const json_t *runtime = json_obj_get(cfg, "gateway_runtime");
    if (runtime && runtime->type == JSON_OBJECT) {
        const char *provider = json_get_str(runtime, "provider", NULL);
        if (provider && *provider) {
            /* Filter None values like the dict comprehension. */
            for (size_t i = 0; i < runtime->c.count; i++) {
                const json_t *v = json_get(runtime, i);
                if (!v || v->type == JSON_NULL) continue;
                json_set(out, runtime->c.keys ? runtime->c.keys[i] : "",
                         json_copy(v));
            }
            json_free(cfg);
            return out;
        }
    }
    bool have_top = false;
    const char *top_keys[] = { "provider", "base_url", "api_mode" };
    for (int i = 0; i < 3; i++) {
        const json_t *v = json_obj_get(cfg, top_keys[i]);
        if (v && v->type == JSON_STRING && v->str_val && *v->str_val) {
            json_set(out, top_keys[i], json_copy(v));
            have_top = true;
        }
    }
    if (have_top) { json_free(cfg); return out; }
    /* Last resort: billing_provider column. */
    const char *bp = json_get_str(session_meta ? (json_t *)session_meta : NULL,
                                  "billing_provider", "");
    while (bp && (*bp == ' ' || *bp == '\t')) bp++;
    bool bare = false;
    for (int i = 0; HS_BARE_BILLING_PROVIDERS[i]; i++)
        if (bp && strcasecmp(bp, HS_BARE_BILLING_PROVIDERS[i]) == 0) bare = true;
    if (bp && *bp && !bare)
        json_set(out, "provider", json_string(bp));
    else if (runtime && runtime->type == JSON_OBJECT) {
        for (size_t i = 0; i < runtime->c.count; i++) {
            const json_t *v = json_get(runtime, i);
            if (!v || v->type == JSON_NULL) continue;
            json_set(out, runtime->c.keys ? runtime->c.keys[i] : "", json_copy(v));
        }
    }
    json_free(cfg);
    return out;
}

/* ── PoP: sweep_orphaned_sessions @ hermes_state.py:sweep_orphaned_sessions ── */
json_t *sweep_orphaned_sessions(hermes_state_db_t *self,
                                double max_idle_seconds,
                                const char *const *sources, int n_sources,
                                const char *const *exclude_ids,
                                int n_exclude) {
    /* Close session rows orphaned by a dead gateway process: both started_at
     * AND the newest messages.timestamp must be stale. Distinct end_reason
     * 'startup_orphan_reap'. SELECT + UPDATE in one BEGIN IMMEDIATE so a
     * sibling process cannot race between staleness check and close.
     * Returns the swept session ids as a JSON array of strings. */
    json_t *swept = json_array();
    if (!self || !self->db || max_idle_seconds <= 0 || n_sources <= 0)
        return swept;
    double cutoff = (double)time(NULL) - max_idle_seconds;
    char placeholders[512] = "";
    size_t off = 0;
    for (int i = 0; i < n_sources && off < sizeof(placeholders) - 2; i++)
        off += (size_t)snprintf(placeholders + off,
                                sizeof(placeholders) - off, "%s?",
                                i ? "," : "");
    static const char *staleness =
        "started_at < ?1 AND COALESCE((SELECT MAX(m.timestamp) FROM messages m"
        " WHERE m.session_id = sessions.id), started_at) < ?2";

    hs_exec(self->db, "BEGIN IMMEDIATE");
    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT id FROM sessions WHERE ended_at IS NULL"
             " AND source IN (%s) AND %s",
             placeholders, staleness);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, cutoff);
        sqlite3_bind_double(st, 2, cutoff);
        char **victims = malloc(sizeof(char *) *
                                (size_t)(n_sources > 0 ? 4096 : 4096));
        int nvictims = 0, vcap = 64;
        victims = malloc(sizeof(char *) * (size_t)vcap);
        while (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *id = sqlite3_column_text(st, 0);
            if (!id) continue;
            bool excluded = false;
            for (int i = 0; i < n_exclude; i++)
                if (exclude_ids[i] && strcmp(exclude_ids[i],
                                             (const char *)id) == 0)
                    excluded = true;
            if (excluded) continue;
            if (nvictims == vcap) {
                vcap *= 2;
                victims = realloc(victims, sizeof(char *) * (size_t)vcap);
            }
            victims[nvictims++] = strdup((const char *)id);
        }
        sqlite3_finalize(st);
        if (nvictims > 0) {
            char marks[4096] = "";
            size_t mo = 0;
            for (int i = 0; i < nvictims; i++)
                mo += (size_t)snprintf(marks + mo, sizeof(marks) - mo,
                                       "%s?", i ? "," : "");
            /* Re-apply staleness under the write lock: a row that raced to
             * activity between SELECT and UPDATE is spared. */
            char usql[1280];
            snprintf(usql, sizeof(usql),
                     "UPDATE sessions SET ended_at = ?%d,"
                     " end_reason = 'startup_orphan_reap'"
                     " WHERE id IN (%s) AND ended_at IS NULL AND %s",
                     n_sources + 3, marks, staleness);
            sqlite3_stmt *ust = NULL;
            if (sqlite3_prepare_v2(self->db, usql, -1, &ust, NULL) == SQLITE_OK) {
                sqlite3_bind_double(ust, 1, cutoff);
                sqlite3_bind_double(ust, 2, cutoff);
                for (int i = 0; i < nvictims; i++)
                    sqlite3_bind_text(ust, 3 + i, victims[i], -1,
                                      SQLITE_TRANSIENT);
                sqlite3_bind_double(ust, nvictims + 3, (double)time(NULL));
                /* parameter order: staleness uses ?1/?2 absolute indexes, so
                 * bind now explicitly too */
                sqlite3_bind_double(ust, 1, cutoff);
                sqlite3_bind_double(ust, 2, cutoff);
                sqlite3_bind_double(ust, nvictims + 3, (double)time(NULL));
                sqlite3_step(ust);
                sqlite3_finalize(ust);
            }
            for (int i = 0; i < nvictims; i++) {
                json_append(swept, json_string(victims[i]));
                free(victims[i]);
            }
        } else {
            free(victims);
        }
    }
    hs_exec(self->db, "COMMIT");
    return swept;
}

/* ── PoP: get_dominant_session_model_route @ hermes_state.py:get_dominant_session_model_route ── */
json_t *get_dominant_session_model_route(hermes_state_db_t *self,
                                         const char *session_id) {
    /* The main-loop model route that served most API calls, from
     * session_model_usage's coherent per-call tuple (sessions is a legacy
     * aggregate row). Returns the row as JSON or NULL. */
    extern void hermes_state_flush_token_counts(hermes_state_db_t *self_);
    if (!self || !self->db || !session_id) return NULL;
    hermes_state_flush_token_counts(self);
    sqlite3_stmt *st = NULL;
    json_t *row = NULL;
    if (sqlite3_prepare_v2(self->db,
            "SELECT model, billing_provider, billing_base_url, billing_mode,"
            "        api_call_count"
            "   FROM session_model_usage"
            "  WHERE session_id = ?"
            "    AND task = ''"
            "    AND model <> 'unknown'"
            "    AND billing_provider <> ''"
            "  ORDER BY api_call_count DESC,"
            "           (input_tokens + output_tokens + cache_read_tokens +"
            "            cache_write_tokens + reasoning_tokens) DESC,"
            "           last_seen DESC"
            "  LIMIT 1",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            row = json_object();
            json_set(row, "model", json_string(
                (const char *)sqlite3_column_text(st, 0)));
            json_set(row, "billing_provider", json_string(
                (const char *)sqlite3_column_text(st, 1)));
            const unsigned char *burl = sqlite3_column_text(st, 2);
            json_set(row, "billing_base_url",
                     burl ? json_string((const char *)burl) : json_null());
            const unsigned char *bmode = sqlite3_column_text(st, 3);
            json_set(row, "billing_mode",
                     bmode ? json_string((const char *)bmode) : json_null());
            json_set(row, "api_call_count",
                     json_number((double)sqlite3_column_int64(st, 4)));
        }
    }
    sqlite3_finalize(st);
    return row;
}

/* ── PoP: unarchive_recoverable_session @ hermes_state.py:unarchive_recoverable_session ── */
bool unarchive_recoverable_session(hermes_state_db_t *self,
                                   const char *session_id) {
    /* Un-archive a session archived by a recoverable accident (ws_orphan_reap,
     * agent_close, superseded_by_resume, startup_orphan_reap). Judged at the
     * compression tip; explicit/user archives untouched. */
    extern char *hermes_state_get_session(hermes_state_db_t *,
                                          const char *);
    extern char *hermes_state_resolve_resume_session_id(hermes_state_db_t *,
                                                        const char *);
    extern bool hermes_state_set_session_archived(hermes_state_db_t *,
                                                  const char *, bool);
    if (!self || !self->db || !session_id || !*session_id) return false;
    char *row_json = hermes_state_get_session(self, session_id);
    if (!row_json) return false;
    json_t *row = json_parse(row_json, NULL);
    free(row_json);
    if (!row) return false;
    if (!json_get_bool(row, "archived", false)) { json_free(row); return false; }

    /* Judge recoverability at the tip (== the row itself when uncompressed). */
    json_t *tip = row;
    json_t *tip_owned = NULL;
    char *tip_id = hermes_state_resolve_resume_session_id(self, session_id);
    if (tip_id && strcmp(tip_id, session_id) != 0) {
        char *tip_json = hermes_state_get_session(self, tip_id);
        if (tip_json) {
            tip_owned = json_parse(tip_json, NULL);
            if (tip_owned) tip = tip_owned;
            free(tip_json);
        }
    }
    const char *end_reason = json_get_str(tip, "end_reason", NULL);
    bool recoverable = false;
    for (int i = 0; HS_RECOVERABLE_END_REASONS[i]; i++)
        if (end_reason && strcmp(end_reason, HS_RECOVERABLE_END_REASONS[i]) == 0)
            recoverable = true;
    if (!recoverable) {
        json_free(tip_owned ? tip_owned : row);
        free(tip_id);
        return false;
    }
    free(tip_id);
    if (!hermes_state_set_session_archived(self, session_id, false)) {
        json_free(tip_owned ? tip_owned : row);
        return false;
    }
    /* Clear the accidental end stamp so a LATER deliberate archive cannot be
     * auto-resurrected by a surviving reason. */
    const char *tip_row_id = json_get_str(tip, "id", session_id);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "UPDATE sessions SET ended_at = NULL, end_reason = NULL"
            " WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, tip_row_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    json_free(tip_owned ? tip_owned : row);
    return true;
}

/* ── PoP: set_session_hidden @ hermes_state.py:set_session_hidden ── */
bool set_session_hidden(hermes_state_db_t *self, const char *session_id,
                        bool hidden) {
    /* Hide/unhide a session and its whole compression lineage via the
     * recursive ancestors/descendants CTE. Returns True when a row changed. */
    if (!self || !self->db) return false;
    const char *sql =
        "WITH RECURSIVE"
        "  ancestors(id) AS ("
        "    SELECT ?1"
        "    UNION"
        "    SELECT parent.id FROM ancestors a"
        "      JOIN sessions child ON child.id = a.id"
        "      JOIN sessions parent ON parent.id = child.parent_session_id"
        "    WHERE parent.end_reason = 'compression'"
        "  ),"
        "  descendants(id) AS ("
        "    SELECT ?2"
        "    UNION"
        "    SELECT child.id FROM descendants d"
        "      JOIN sessions parent ON parent.id = d.id"
        "      JOIN sessions child ON child.parent_session_id = parent.id"
        "    WHERE parent.end_reason = 'compression'"
        "  ),"
        "  lineage(id) AS (SELECT id FROM ancestors"
        "                  UNION SELECT id FROM descendants)"
        "UPDATE sessions SET hidden = ?3 WHERE id IN (SELECT id FROM lineage)";
    sqlite3_stmt *st = NULL;
    bool changed = false;
    if (sqlite3_prepare_v2(self->db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, hidden ? 1 : 0);
        sqlite3_step(st);
        int rowcount = sqlite3_changes(self->db);
        changed = rowcount > 0;
    }
    sqlite3_finalize(st);
    return changed;
}

/* ── PoP: session_lifecycle_statuses @ hermes_state.py:session_lifecycle_statuses ── */
json_t *session_lifecycle_statuses(hermes_state_db_t *self,
                                        const char *const *session_ids,
                                        int n_ids) {
    /* Classify each listed session from its LAST message row:
     * complete / interrupted / error / empty — one MAX(id)-seek query. */
    json_t *statuses = json_object();
    if (!self || !self->db || n_ids <= 0 || !session_ids) return statuses;
    for (int i = 0; i < n_ids; i++)
        if (session_ids[i] && *session_ids[i])
            json_set(statuses, session_ids[i], json_string("empty"));
    char placeholders[4096] = "";
    size_t off = 0;
    for (int i = 0; i < n_ids; i++) {
        int w = snprintf(placeholders + off, sizeof(placeholders) - off,
                         "%s?", i ? "," : "");
        if (w < 0 || off + (size_t)w >= sizeof(placeholders)) break;
        off += (size_t)w;
    }
    extern const char *hermes_state_classify_session_status(const char *role,
                                                            bool has_tool_calls,
                                                            const char *finish_reason);
    char sql[4608];
    snprintf(sql, sizeof(sql),
             "SELECT m.session_id, m.role,"
             "       m.tool_calls IS NOT NULL AS has_tool_calls,"
             "       m.finish_reason"
             "  FROM messages m"
             "  JOIN ("
             "      SELECT session_id, MAX(id) AS max_id FROM messages"
             "      WHERE session_id IN (%s) GROUP BY session_id"
             "  ) latest ON m.id = latest.max_id",
             placeholders);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db, sql, -1, &st, NULL) == SQLITE_OK) {
        int p = 1;
        for (int i = 0; i < n_ids; i++)
            if (session_ids[i] && *session_ids[i])
                sqlite3_bind_text(st, p++, session_ids[i], -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *sid = sqlite3_column_text(st, 0);
            const unsigned char *role = sqlite3_column_text(st, 1);
            bool has_tool_calls = sqlite3_column_int(st, 2) != 0;
            const unsigned char *fin = sqlite3_column_text(st, 3);
            const char *status = hermes_state_classify_session_status(
                role ? (const char *)role : "", has_tool_calls,
                fin ? (const char *)fin : NULL);
            if (sid)
                json_set(statuses, (const char *)sid, json_string(status));
        }
    }
    sqlite3_finalize(st);
    return statuses;
}

/* ── PoP: get_active_message_watermark @ hermes_state.py:get_active_message_watermark ── */
long long get_active_message_watermark(hermes_state_db_t *self,
                                       const char *session_id) {
    /* MAX(id) of the session's active rows — the compression watermark
     * captured at compression START. Concurrent rows above it must survive.
     * 0 for an empty/unknown session. */
    if (!self || !self->db || !session_id || !*session_id) return 0;
    sqlite3_stmt *st = NULL;
    long long wm = 0;
    if (sqlite3_prepare_v2(self->db,
            "SELECT COALESCE(MAX(id), 0) FROM messages"
            " WHERE session_id = ? AND active = 1",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW)
            wm = sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
    return wm;
}

/* ── PoP: _message_column_names @ hermes_state.py:_message_column_names ── */
char **_message_column_names(hermes_state_db_t *self, sqlite3 *conn,
                             int *out_n) {
    /* Column names of the messages table, cached per connection era. The C
     * engine re-derives per call (the cache is a perf-only detail of the
     * Python attribute cache); the PRAGMA is the same source. */
    *out_n = 0;
    if (!conn) return NULL;
    sqlite3_stmt *st = NULL;
    char **cols = NULL;
    int n = 0, cap = 16;
    cols = malloc(sizeof(char *) * (size_t)cap);
    if (sqlite3_prepare_v2(conn, "PRAGMA table_info(messages)", -1, &st, NULL)
        == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *name = sqlite3_column_text(st, 1);
            if (!name) continue;
            if (n == cap) { cap *= 2; cols = realloc(cols, sizeof(char *) * (size_t)cap); }
            cols[n++] = strdup((const char *)name);
        }
        sqlite3_finalize(st);
    }
    *out_n = n;
    return cols;
}

/* ── PoP: _is_explicit_branch_session @ hermes_state.py:_is_explicit_branch_session ── */
bool _is_explicit_branch_session(hermes_state_db_t *self,
                                 const char *session_id) {
    /* True when session_id is a copied user-facing branch: the durable
     * _branched_from marker in model_config discriminates branches from
     * compression continuations (both use parent_session_id). */
    if (!self || !self->db || !session_id || !*session_id) return false;
    sqlite3_stmt *st = NULL;
    bool is_branch = false;
    if (sqlite3_prepare_v2(self->db,
            "SELECT model_config FROM sessions WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *raw = sqlite3_column_text(st, 0);
            if (raw && *raw) {
                json_t *cfg = json_parse((const char *)raw, NULL);
                if (cfg) {
                    const json_t *bf = json_obj_get(cfg, "_branched_from");
                    is_branch = bf != NULL && bf->type != JSON_NULL &&
                                !(bf->type == JSON_STRING &&
                                  (!bf->str_val || !*bf->str_val));
                    json_free(cfg);
                }
            }
        }
    }
    sqlite3_finalize(st);
    return is_branch;
}

/* ── PoP: _canonical_replayed_user_content @ hermes_state.py:_canonical_replayed_user_content ── */
void _canonical_replayed_user_content(const json_t *msg,
                                      json_t **out_content, bool *is_composite) {
    /* Canonical live content + whether msg is composite: split the user turn
     * into hidden handoff scaffold + live view via split_user_originated_turn
     * (agent/context_compressor.py). Non-user roles → content NULL, false. */
    *out_content = NULL;
    *is_composite = false;
    if (!msg || strcmp(json_get_str((json_t *)msg, "role", ""), "user") != 0)
        return;
    extern void cc_split_user_originated_turn(const json_t *message,
                                              json_t **out_handoff,
                                              json_t **out_live);
    json_t *handoff = NULL, *live = NULL;
    cc_split_user_originated_turn(msg, &handoff, &live);
    *is_composite = handoff != NULL && live != NULL;
    if (*is_composite && live) {
        *out_content = json_copy(json_obj_get(live, "content"));
    } else {
        *out_content = json_copy(json_obj_get(msg, "content"));
    }
    if (handoff) json_free(handoff);
    if (live) json_free(live);
}

/* ── PoP: _exact_replayed_user_clone_key @ hermes_state.py:_exact_replayed_user_clone_key ── */
bool _exact_replayed_user_clone_key(const json_t *timestamp,
                                    const json_t *content,
                                    char **out_encoded) {
    /* Hashable key for a column-exact rotation clone: (timestamp,
     * canonical compact JSON of content). False when either side is empty —
     * matching Python's None/""/[] guard. out_encoded receives the sorted-key
     * compact JSON string (caller frees). */
    *out_encoded = NULL;
    if (!timestamp || timestamp->type == JSON_NULL) return false;
    if (!content || content->type == JSON_NULL) return false;
    if (content->type == JSON_STRING && (!content->str_val || !*content->str_val))
        return false;
    if (content->type == JSON_ARRAY && json_len((json_t *)content) == 0)
        return false;
    /* libjson serializes with sorted keys + compact separators by default. */
    char *encoded = json_serialize((json_t *)content);
    if (!encoded) return false;
    *out_encoded = encoded;
    return true;
}

/* ── PoP: _find_duplicate_replayed_user_message @ hermes_state.py:_find_duplicate_replayed_user_message ── */
bool _find_duplicate_replayed_user_message(const json_t *messages_json_array,
                                           const json_t *msg,
                                           int *out_index, bool *prefer_current) {
    /* Adjacent replay duplicate scan (newest-first walk): compares canonical
     * live payloads for composite carriers, retaining the exact-string dedupe
     * for ordinary replayed users. Returns true with *out_index when found;
     * stops at the first real assistant turn. */
    *out_index = -1;
    *prefer_current = false;
    if (!msg || !messages_json_array ||
        messages_json_array->type != JSON_ARRAY ||
        strcmp(json_get_str((json_t *)msg, "role", ""), "user") != 0)
        return false;

    json_t *content = NULL;
    bool composite = false;
    _canonical_replayed_user_content(msg, &content, &composite);
    bool empty = !content || content->type == JSON_NULL ||
                 (content->type == JSON_STRING &&
                  (!content->str_val || !*content->str_val)) ||
                 (content->type == JSON_ARRAY &&
                  json_len(content) == 0);
    if (empty) { if (content) json_free(content); return false; }

    int n = (int)json_len(messages_json_array);
    for (int index = n - 1; index >= 0; index--) {
        const json_t *prev = json_get(messages_json_array, index);
        if (!prev) continue;
        const char *prev_role = json_get_str((json_t *)prev, "role", "");
        if (strcmp(prev_role, "user") == 0) {
            json_t *pc = NULL;
            bool pcomp = false;
            _canonical_replayed_user_content(prev, &pc, &pcomp);
            bool equal = hs_json_deep_equal(content, pc);
            if (equal && (composite || pcomp ||
                          content->type == JSON_STRING)) {
                *out_index = index;
                *prefer_current = composite;
                json_free(pc);
                json_free(content);
                return true;
            }
            if (pc) json_free(pc);
        }
        if (strcmp(prev_role, "assistant") == 0) {
            const json_t *c = json_obj_get(prev, "content");
            const json_t *tc = json_obj_get(prev, "tool_calls");
            bool has = (c && c->type != JSON_NULL &&
                        !(c->type == JSON_STRING && !*c->str_val)) ||
                       (tc && tc->type != JSON_NULL);
            if (has) { json_free(content); return false; }
        }
    }
    json_free(content);
    return false;
}

/* deep JSON equality for the replay-dedupe scan (Python == on decoded values) */
static bool hs_json_deep_equal(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
    case JSON_NULL: return true;
    case JSON_BOOL: return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING:
        return strcmp(a->str_val ? a->str_val : "",
                      b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY: {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!hs_json_deep_equal(json_get(a, i), json_get(b, i)))
                return false;
        return true;
    }
    case JSON_OBJECT: {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const char *k = a->c.keys ? a->c.keys[i] : "";
            const json_t *bv = b ? json_obj_get(b, k) : NULL;
            if (!hs_json_deep_equal(json_get(a, i), bv)) return false;
        }
        return true;
    }
    }
    return false;
}

/* ── PoP: get_active_message_ids @ hermes_state.py:get_active_message_ids ── */
long long *get_active_message_ids(hermes_state_db_t *self,
                                  const char *session_id, int *out_n) {
    /* Ordered physical ids pinned by rewind CAS checks: every ACTIVE row so
     * the caller snapshot matches rewind_to_message's transaction-local
     * comparison (projections omit harness rows; this must not). */
    *out_n = 0;
    if (!self || !self->db || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    long long *ids = NULL;
    int n = 0, cap = 32;
    ids = malloc(sizeof(long long) * (size_t)cap);
    if (sqlite3_prepare_v2(self->db,
            "SELECT id FROM messages WHERE session_id = ? AND active = 1"
            " ORDER BY id",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (n == cap) { cap *= 2; ids = realloc(ids, sizeof(long long) * (size_t)cap); }
            ids[n++] = sqlite3_column_int64(st, 0);
        }
        sqlite3_finalize(st);
    }
    *out_n = n;
    return ids;
}

/* ── PoP: _active_transcript_counts @ hermes_state.py:_active_transcript_counts ── */
void _active_transcript_counts(sqlite3 *conn, const char *session_id,
                               int *out_messages, int *out_tool_calls) {
    /* Active message/tool-call counts inside the caller's transaction:
     * each tool_calls JSON list contributes its length; any other truthy
     * value contributes 1. */
    *out_messages = 0;
    *out_tool_calls = 0;
    if (!conn || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT tool_calls FROM messages WHERE session_id = ?"
            " AND active = 1",
            -1, &st, NULL) != SQLITE_OK)
        return;
    int msgs = 0, tcc = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        msgs++;
        const unsigned char *raw = sqlite3_column_text(st, 0);
        if (!raw || !*raw) continue;
        json_t *decoded = json_parse((const char *)raw, NULL);
        if (!decoded) continue;
        if (decoded->type == JSON_ARRAY) tcc += (int)json_len(decoded);
        else tcc += 1; /* non-empty truthy value counts as one call */
        json_free(decoded);
    }
    sqlite3_finalize(st);
    *out_messages = msgs;
    *out_tool_calls = tcc;
}

/* ── PoP: _apply_prune_age_filter @ hermes_state.py:_apply_prune_age_filter ── */
bool _apply_prune_age_filter(bool has_older_than_days, double older_than_days,
                             ws_prune_filters_t *filters) {
    /* Translate the legacy age window into the shared activity filter — only
     * when neither explicit window key is already set. Returns whether it
     * applied (informational; Python mutates in place). */
    if (!filters) return false;
    if (!filters->has_last_active_before && !filters->has_started_before &&
        has_older_than_days && older_than_days > 0) {
        filters->has_last_active_before = true;
        filters->last_active_before =
            (double)time(NULL) - older_than_days * 86400.0;
        return true;
    }
    return false;
}

/* ── PoP: list_meta_prefix @ hermes_state.py:list_meta_prefix ── */
json_t *list_meta_prefix(hermes_state_db_t *self, const char *prefix) {
    /* [(key, value), ...] pairs for state_meta keys under prefix, as a JSON
     * array of {"key","value"} objects. LIKE wildcards in the prefix are
     * escaped literally. */
    json_t *out = json_array();
    if (!self || !self->db || !prefix || !*prefix) return out;
    /* escape \ % _ exactly like Python's replace chain */
    size_t plen = strlen(prefix);
    char *escaped = malloc(plen * 2 + 1);
    size_t j = 0;
    for (size_t i = 0; i < plen; i++) {
        if (prefix[i] == '\\' || prefix[i] == '%' || prefix[i] == '_')
            escaped[j++] = '\\';
        escaped[j++] = prefix[i];
    }
    escaped[j] = '\0';
    char *pattern = NULL;
    asprintf(&pattern, "%s%%", escaped);
    free(escaped);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(self->db,
            "SELECT key, value FROM state_meta WHERE key LIKE ? ESCAPE '\\'",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, pattern ? pattern : "%", -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            json_t *pair = json_object();
            const unsigned char *k = sqlite3_column_text(st, 0);
            const unsigned char *v = sqlite3_column_text(st, 1);
            json_set(pair, "key", json_string(k ? (const char *)k : ""));
            if (v) json_set(pair, "value", json_string((const char *)v));
            else json_set(pair, "value", json_null());
            json_append(out, pair);
        }
        sqlite3_finalize(st);
    }
    free(pattern);
    return out;
}
