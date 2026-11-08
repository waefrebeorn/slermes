/*
 * status.c — Slermes C11 port of gateway/status.py.
 *
 * Gateway runtime status: PID-file liveness detection, cross-process runtime
 * and scope locks, process fingerprinting (start-time + cmdline identity),
 * runtime health JSON, and the --replace takeover / planned-stop marker
 * protocol. Faithful port; POSIX-only (the slermes binary is native Linux),
 * so the Windows msvcrt/taskkill/psutil branches of the original map to their
 * /proc + kill(2) + flock(2) equivalents.
 *
 * See include/gateway_status.h for the public surface.
 */

#include "gateway_status.h"
#include "gateway_status_proc.h"
#include "gateway_status_paths.h"
#include "hermes_gateway_core.h"
#include "slermes_home.h"
#include "json.h"
#include "hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>   /* flock */
#include <sys/types.h>

/* ── Constants (mirror gateway/status.py) ────────────────────────────── */

#define TAKEOVER_MARKER_TTL_S      60
#define PLANNED_STOP_MARKER_TTL_S  60

/* Process-global runtime lock fd (mirrors _gateway_lock_handle). -1 = none. */
static int g_gateway_lock_fd = -1;

/* Forward declarations (defined later; referenced before their definition). */
static bool command_line_belongs_to_profile(const char *command,
                                            const char *profile_home);

/* PoP: gwstatus_get_process_start_time @ gateway/status.py:_get_process_start_time */
/* PoP: gwstatus_get_process_start_time @ gateway/status.py:get_process_start_time */
/* Python's public get_process_start_time is a thin wrapper over the private
 * _get_process_start_time; both collapse to this one C entry point.
 * Linux: field 22 of /proc/<pid>/stat (clock ticks since boot). The comm
 * field (2) can contain spaces/parens, so scan from the closing ')'. */
long gwstatus_get_process_start_time(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char buf[4096];
    size_t got = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (got == 0) return -1;
    buf[got] = '\0';
    /* comm is enclosed in parens and may contain spaces/parens; the last ')'
     * ends field 2. Fields after it are space-separated; start_time is
     * field 22, i.e. the 20th token after the closing paren. */
    char *rparen = strrchr(buf, ')');
    if (!rparen) return -1;
    char *p = rparen + 1;
    /* Token indices after the paren: field 3 = state, ... field 22 = start.
     * That's the 20th whitespace-delimited token starting at field 3. */
    int field = 2; /* we are positioned right after field 2 (comm) */
    char *tok = strtok(p, " \t\n");
    while (tok) {
        field++;
        if (field == 22) {
            errno = 0;
            long v = strtol(tok, NULL, 10);
            if (errno) return -1;
            return v;
        }
        tok = strtok(NULL, " \t\n");
    }
    return -1;
}

/* ── JSON record helpers ─────────────────────────────────────────────── */

/* Read + parse a JSON object file. Returns json_t* (caller json_free) or NULL. */
static json_t *read_json_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    /* strip */
    char *s = buf;
    while (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') s++;
    if (!*s) { free(buf); return NULL; }
    json_t *node = json_parse(s, NULL);
    free(buf);
    if (!node) return NULL;
    if (node->type != JSON_OBJECT) { json_free(node); return NULL; }
    return node;
}

/* PoP: write_json_file @ gateway/status.py:_write_json_file */
static int write_json_file(const char *path, json_t *payload) {
    ensure_parent_dir(path);
    char *ser = json_serialize(payload);
    if (!ser) return -1;
    char tmp[1300];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(ser); return -1; }
    size_t len = strlen(ser);
    size_t wrote = fwrite(ser, 1, len, f);
    int fd = fileno(f);
    if (fd >= 0) fsync(fd);
    fclose(f);
    free(ser);
    if (wrote != len) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* PoP: build_pid_record @ gateway/status.py:_build_pid_record */
static json_t *build_pid_record(void) {
    json_t *o = json_object();
    json_set(o, "pid", json_number((double)getpid()));
    json_set(o, "kind", json_string(GATEWAY_KIND));
    /* argv: read from /proc/self/cmdline for faithful parity with sys.argv. */
    json_t *argv = json_array();
    char *cmd = read_process_cmdline(getpid());
    if (cmd) {
        char *toks[256];
        int n = split_command(cmd, toks, 256);
        for (int i = 0; i < n; i++) json_append(argv, json_string(toks[i]));
        free_tokens(toks, n);
        free(cmd);
    }
    json_set(o, "argv", argv);
    long st = gwstatus_get_process_start_time(getpid());
    if (st >= 0) json_set(o, "start_time", json_number((double)st));
    else json_set(o, "start_time", json_null());
    return o;
}

/* PoP: pid_from_record @ gateway/status.py:_pid_from_record */
static pid_t pid_from_record(json_t *record) {
    if (!record || record->type != JSON_OBJECT) return -1;
    json_t *p = json_obj_get(record, "pid");
    if (!p) return -1;
    if (p->type == JSON_NUMBER) return (pid_t)p->num_val;
    if (p->type == JSON_STRING) {
        char *end = NULL;
        long v = strtol(p->str_val, &end, 10);
        if (end && end != p->str_val) return (pid_t)v;
    }
    return -1;
}

/* PoP: read_pid_record @ gateway/status.py:_read_pid_record */
static json_t *read_pid_record(const char *pid_path) {
    char defbuf[1200];
    if (!pid_path) pid_path = get_pid_path(defbuf, sizeof(defbuf));
    FILE *f = fopen(pid_path, "rb");
    if (!f) return NULL;
    char raw[8192];
    size_t got = fread(raw, 1, sizeof(raw) - 1, f);
    fclose(f);
    if (got == 0) return NULL;
    raw[got] = '\0';
    char *s = raw;
    while (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') s++;
    size_t L = strlen(s);
    while (L > 0 && (s[L-1] == ' ' || s[L-1] == '\n' || s[L-1] == '\t' || s[L-1] == '\r')) s[--L] = '\0';
    if (!*s) return NULL;
    json_t *node = json_parse(s, NULL);
    if (node) {
        if (node->type == JSON_OBJECT) return node;
        if (node->type == JSON_NUMBER) {
            json_t *o = json_object();
            json_set(o, "pid", json_number(node->num_val));
            json_free(node);
            return o;
        }
        json_free(node);
        return NULL;
    }
    /* Not JSON: try bare integer. */
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end && end != s) {
        json_t *o = json_object();
        json_set(o, "pid", json_number((double)v));
        return o;
    }
    return NULL;
}

/* -----------------------------------------------------------------------
 * PoP: read_gateway_lock_record @ gateway/status.py:_read_gateway_lock_record
 * The gateway lock file carries the same record shape as the PID file.
 * --------------------------------------------------------------------- */
static json_t *read_gateway_lock_record(const char *lock_path) {
    char buf[1200];
    const char *path = lock_path;
    if (!path) { get_gateway_lock_path(NULL, buf, sizeof(buf)); path = buf; }
    return read_pid_record(path);
}

/* -----------------------------------------------------------------------
 * PoP: write_gateway_lock_record @ gateway/status.py:_write_gateway_lock_record
 * Rewrite an open lock fd from offset 0 with the current PID record + fsync.
 * --------------------------------------------------------------------- */
static void write_gateway_lock_record(int fd) {
    if (fd < 0) return;
    json_t *record = build_pid_record();
    char *ser = json_serialize(record);
    json_free(record);
    if (!ser) return;
    if (lseek(fd, 0, SEEK_SET) == 0 && ftruncate(fd, 0) == 0) {
        size_t len = strlen(ser);
        ssize_t w = write(fd, ser, len);
        (void)w;
        fsync(fd);
    }
    free(ser);
}

/* -----------------------------------------------------------------------
 * PoP: cleanup_invalid_pid_path @ gateway/status.py:_cleanup_invalid_pid_path
 * Force-unlink a stale PID file and its sibling lock metadata (called after
 * the runtime lock is confirmed inactive, so the metadata is known dead).
 * --------------------------------------------------------------------- */
static void cleanup_invalid_pid_path(const char *pid_path, bool cleanup_stale) {
    if (!cleanup_stale || !pid_path) return;
    unlink(pid_path);
    char lock[1200];
    get_gateway_lock_path(pid_path, lock, sizeof(lock));
    unlink(lock);
}

/* Port of Python: acquire_gateway_runtime_lock */
bool gwstatus_acquire_gateway_runtime_lock(void) {
    if (g_gateway_lock_fd >= 0) return true;  /* already held */
    char path[1200];
    get_gateway_lock_path(NULL, path, sizeof(path));
    ensure_parent_dir(path);
    int fd = open(path, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return false;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) { close(fd); return false; }
    /* Write the full PID record (kind/argv/start_time) so a fallback reader
     * can validate gateway identity from the lock file. */
    write_gateway_lock_record(fd);
    g_gateway_lock_fd = fd;
    return true;
}

/* Port of Python: release_gateway_runtime_lock */
void gwstatus_release_gateway_runtime_lock(void) {
    if (g_gateway_lock_fd < 0) return;
    flock(g_gateway_lock_fd, LOCK_UN);
    close(g_gateway_lock_fd);
    g_gateway_lock_fd = -1;
}

/* PoP: gwstatus_is_gateway_runtime_lock_active @ gateway/status.py:is_gateway_runtime_lock_active */
/* PoP: gwstatus_is_gateway_runtime_lock_active @ gateway/status.py:_try_acquire_file_lock */
/* PoP: gwstatus_is_gateway_runtime_lock_active @ gateway/status.py:_release_file_lock */
/* flock LOCK_EX|LOCK_NB acquire + LOCK_UN release are inlined here and in
 * acquire/release_gateway_runtime_lock — POSIX has no separate handle type. */
bool gwstatus_is_gateway_runtime_lock_active(const char *lock_path) {
    char buf[1200];
    const char *path = lock_path;
    if (!path) { get_gateway_lock_path(NULL, buf, sizeof(buf)); path = buf; }
    /* If we hold it in-process, it is active. */
    if (g_gateway_lock_fd >= 0) return true;
    int fd = open(path, O_RDWR, 0600);
    if (fd < 0) {
        if (errno == ENOENT) return false;
        /* Cannot open but exists: assume active (conservative). */
        struct stat st;
        return stat(path, &st) == 0;
    }
    /* Try to grab it non-blocking; success => nobody held it => not active. */
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        flock(fd, LOCK_UN);
        close(fd);
        return false;
    }
    close(fd);
    return true;  /* held by someone else */
}

/* ── PID file ────────────────────────────────────────────────────────── */

/* Port of Python: write_pid_file */
int gwstatus_write_pid_file(void) {
    char path[1200];
    get_pid_path(path, sizeof(path));
    ensure_parent_dir(path);
    json_t *record = build_pid_record();
    int rc = write_json_file(path, record);
    json_free(record);
    return rc;
}

/* Port of Python: remove_pid_file — only if it belongs to this process. */
void gwstatus_remove_pid_file(void) {
    char path[1200];
    get_pid_path(path, sizeof(path));
    json_t *record = read_pid_record(path);
    if (!record) { unlink(path); return; }
    pid_t rec_pid = pid_from_record(record);
    json_free(record);
    if (rec_pid == getpid() || rec_pid <= 0)
        unlink(path);
}

/* Port of Python: get_running_pid */
pid_t gwstatus_get_running_pid(const char *pid_path, bool cleanup_stale) {
    char defbuf[1200];
    bool default_home = (pid_path == NULL);
    if (!pid_path) pid_path = get_pid_path(defbuf, sizeof(defbuf));

    char lockbuf[1200];
    get_gateway_lock_path(pid_path, lockbuf, sizeof(lockbuf));

    /* If the runtime lock is not held by anyone, the on-disk metadata is
     * known dead: try the runtime-status fallback, then clean up. */
    if (!gwstatus_is_gateway_runtime_lock_active(lockbuf)) {
        if (default_home) {
            pid_t rp = gwstatus_get_runtime_status_running_pid(NULL, NULL);
            if (rp > 0) return rp;
        }
        cleanup_invalid_pid_path(pid_path, cleanup_stale);
        return -1;
    }

    json_t *primary = read_pid_record(pid_path);
    json_t *fallback = read_gateway_lock_record(lockbuf);

    json_t *records[2] = { primary, fallback };
    pid_t found = -1;
    for (int i = 0; i < 2 && found < 0; i++) {
        json_t *record = records[i];
        pid_t pid = pid_from_record(record);
        if (pid <= 0) continue;
        if (!gwstatus_pid_exists(pid)) continue;

        json_t *st = record ? json_obj_get(record, "start_time") : NULL;
        if (st && st->type == JSON_NUMBER) {
            long recorded = (long)st->num_val;
            long live = gwstatus_get_process_start_time(pid);
            if (live >= 0 && recorded >= 0 && live != recorded) continue;
        }
        if (record_matches_live_gateway_pid(record, pid, NULL))
            found = pid;
    }

    if (primary) json_free(primary);
    if (fallback) json_free(fallback);

    if (found > 0) return found;

    cleanup_invalid_pid_path(pid_path, cleanup_stale);
    if (default_home) {
        pid_t rp = gwstatus_get_runtime_status_running_pid(NULL, NULL);
        if (rp > 0) return rp;
    }
    return -1;
}

/* -----------------------------------------------------------------------
 * PoP: gwstatus_get_runtime_status_running_pid @ gateway/status.py:get_runtime_status_running_pid
 * Conservative fallback: validate the runtime status record against the OS.
 * --------------------------------------------------------------------- */
pid_t gwstatus_get_runtime_status_running_pid(const char *runtime_json,
                                              const char *expected_home) {
    char *owned = NULL;
    const char *ser = runtime_json;
    if (!ser) {
        owned = gwstatus_read_runtime_status(NULL);
        ser = owned;
    }
    if (!ser) return -1;

    json_t *payload = json_parse(ser, NULL);
    if (owned) free(owned);
    if (!payload || payload->type != JSON_OBJECT) {
        if (payload) json_free(payload);
        return -1;
    }

    /* gateway_state in {null, "stopped", "startup_failed"} => not running. */
    json_t *gs = json_obj_get(payload, "gateway_state");
    if (!gs || gs->type == JSON_NULL) { json_free(payload); return -1; }
    if (gs->type == JSON_STRING &&
        (strcmp(gs->str_val, "stopped") == 0 ||
         strcmp(gs->str_val, "startup_failed") == 0)) {
        json_free(payload);
        return -1;
    }

    pid_t pid = pid_from_record(payload);
    if (pid <= 0 || !gwstatus_pid_exists(pid)) { json_free(payload); return -1; }

    json_t *st = json_obj_get(payload, "start_time");
    if (st && st->type == JSON_NUMBER) {
        long recorded = (long)st->num_val;
        long live = gwstatus_get_process_start_time(pid);
        if (live >= 0 && recorded >= 0 && live != recorded) {
            json_free(payload);
            return -1;
        }
    }

    pid_t result = record_matches_live_gateway_pid(payload, pid, expected_home)
                   ? pid : -1;
    json_free(payload);
    return result;
}

/* Port of Python: is_gateway_running */
bool gwstatus_is_gateway_running(const char *pid_path, bool cleanup_stale) {
    return gwstatus_get_running_pid(pid_path, cleanup_stale) > 0;
}

/* ── Runtime health status JSON ──────────────────────────────────────── */

/* Port of Python: parse_active_agents (string form). */
int gwstatus_parse_active_agents_str(const char *raw) {
    if (!raw || !raw[0]) return 0;
    /* Python int(raw): accept leading/trailing whitespace, optional sign. */
    char *end = NULL;
    errno = 0;
    long v = strtol(raw, &end, 10);
    if (errno || end == raw) return 0;
    /* trailing non-space => ValueError in Python. */
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    if (*end != '\0') return 0;
    if (v < 0) return 0;
    if (v > 0x7fffffff) return 0x7fffffff;
    return (int)v;
}

/* Port of Python: derive_gateway_busy */
bool gwstatus_derive_gateway_busy(bool gateway_running,
                                  const char *gateway_state,
                                  int active_agents) {
    if (!gateway_running) return false;
    if (!gateway_state || strcmp(gateway_state, "running") != 0) return false;
    return active_agents > 0;
}

/* Port of Python: derive_gateway_drainable */
bool gwstatus_derive_gateway_drainable(bool gateway_running,
                                       const char *gateway_state) {
    return gateway_running && gateway_state && strcmp(gateway_state, "running") == 0;
}

/* Port of Python: _build_runtime_status_record — seed record when file absent. */
static json_t *build_runtime_status_record(void) {
    json_t *o = build_pid_record();
    json_set(o, "gateway_state", json_string("starting"));
    json_set(o, "exit_reason", json_null());
    json_set(o, "restart_requested", json_bool(false));
    json_set(o, "active_agents", json_number(0));
    json_set(o, "platforms", json_object());
    char iso[64]; utc_now_iso_buf(iso, sizeof(iso));
    json_set(o, "updated_at", json_string(iso));
    return o;
}

/* Port of Python: write_runtime_status */
int gwstatus_write_runtime_status(const char *gateway_state,
                                  const char *exit_reason,
                                  int restart_requested,
                                  int active_agents,
                                  const char *platform,
                                  const char *platform_state,
                                  const char *error_code,
                                  const char *error_message) {
    char path[1200];
    get_runtime_status_path(path, sizeof(path));

    json_t *payload = read_json_file(path);
    if (!payload) payload = build_runtime_status_record();

    /* ensure platforms sub-object */
    json_t *plats = json_obj_get(payload, "platforms");
    if (!plats || plats->type != JSON_OBJECT) {
        json_set(payload, "platforms", json_object());
        plats = json_obj_get(payload, "platforms");
    }

    /* refresh identity fields from current process */
    json_t *cur = build_pid_record();
    json_set(payload, "kind", json_string(json_get_str(cur, "kind", GATEWAY_KIND)));
    json_set(payload, "pid", json_number(json_get_num(cur, "pid", (double)getpid())));
    json_t *cargv = json_obj_get(cur, "argv");
    json_set(payload, "argv", cargv ? json_copy(cargv) : json_array());
    json_t *cst = json_obj_get(cur, "start_time");
    json_set(payload, "start_time", cst ? json_copy(cst) : json_null());
    json_free(cur);

    char iso[64]; utc_now_iso_buf(iso, sizeof(iso));
    json_set(payload, "updated_at", json_string(iso));

    if (gateway_state) json_set(payload, "gateway_state", json_string(gateway_state));
    if (exit_reason) {
        if (exit_reason[0]) json_set(payload, "exit_reason", json_string(exit_reason));
        else json_set(payload, "exit_reason", json_null());
    }
    if (restart_requested >= 0)
        json_set(payload, "restart_requested", json_bool(restart_requested != 0));
    if (active_agents >= 0)
        json_set(payload, "active_agents", json_number(active_agents));

    if (platform && platform[0]) {
        json_t *pp = json_obj_get(plats, platform);
        json_t *platform_payload;
        if (pp && pp->type == JSON_OBJECT) {
            platform_payload = json_copy(pp);
        } else {
            platform_payload = json_object();
        }
        if (platform_state) json_set(platform_payload, "state", json_string(platform_state));
        if (error_code) json_set(platform_payload, "error_code", json_string(error_code));
        if (error_message) json_set(platform_payload, "error_message", json_string(error_message));
        char piso[64]; utc_now_iso_buf(piso, sizeof(piso));
        json_set(platform_payload, "updated_at", json_string(piso));
        json_set(plats, platform, platform_payload);
    }

    int rc = write_json_file(path, payload);
    json_free(payload);
    return rc;
}

/* Port of Python: read_runtime_status — serialized string or NULL. */
char *gwstatus_read_runtime_status(const char *path) {
    char defbuf[1200];
    if (!path) path = get_runtime_status_path(defbuf, sizeof(defbuf));
    json_t *node = read_json_file(path);
    if (!node) return NULL;
    char *out = json_serialize(node);
    json_free(node);
    return out;
}

/* Is /proc/<pid> in a stopped state (T/t)? */
static bool proc_is_stopped(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char line[256];
    bool stopped = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "State:", 6) == 0) {
            /* State:\t<char> ... */
            char *p = line + 6;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == 'T' || *p == 't') stopped = true;
            break;
        }
    }
    fclose(f);
    return stopped;
}

/* Long value or LONG_MIN if key absent/null. */
static long record_start_time(json_t *rec) {
    json_t *st = json_obj_get(rec, "start_time");
    if (!st || st->type != JSON_NUMBER) return LONG_MIN;
    return (long)st->num_val;
}

/* Port of Python: acquire_scoped_lock */
bool gwstatus_acquire_scoped_lock(const char *scope, const char *identity,
                                  const char *metadata_json,
                                  char **out_existing) {
    if (out_existing) *out_existing = NULL;
    char lock_path[1200];
    get_scope_lock_path(scope, identity, lock_path, sizeof(lock_path));
    ensure_parent_dir(lock_path);

    /* Build our record. */
    char h[17]; scope_hash(identity, h);
    json_t *record = build_pid_record();
    json_set(record, "scope", json_string(scope));
    json_set(record, "identity_hash", json_string(h));
    if (metadata_json && metadata_json[0]) {
        json_t *md = json_parse(metadata_json, NULL);
        json_set(record, "metadata", (md && md->type == JSON_OBJECT) ? md : json_object());
        if (md && md->type != JSON_OBJECT) json_free(md);
    } else {
        json_set(record, "metadata", json_object());
    }
    char iso[64]; utc_now_iso_buf(iso, sizeof(iso));
    json_set(record, "updated_at", json_string(iso));

    json_t *existing = read_json_file(lock_path);
    struct stat stbuf;
    if (!existing && stat(lock_path, &stbuf) == 0) {
        /* File present but empty/invalid — treat as stale. */
        unlink(lock_path);
    }
    if (existing) {
        pid_t existing_pid = pid_from_record(existing);
        long our_start = record_start_time(record);
        long existing_start = record_start_time(existing);

        if (existing_pid == getpid() && existing_start == our_start) {
            /* Re-acquire our own lock; refresh. */
            write_json_file(lock_path, record);
            if (out_existing) *out_existing = json_serialize(existing);
            json_free(existing);
            json_free(record);
            return true;
        }

        bool stale = (existing_pid <= 0);
        if (!stale) {
            if (!gwstatus_pid_exists(existing_pid)) {
                stale = true;
            } else {
                long current_start = gwstatus_get_process_start_time(existing_pid);
                if (existing_start != LONG_MIN && current_start >= 0 &&
                    current_start != existing_start) {
                    stale = true;
                }
                /* start_time unknown on both sides: fall back to cmdline. */
                if (!stale && existing_start == LONG_MIN && current_start < 0 &&
                    !looks_like_gateway_process(existing_pid)) {
                    char *live = read_process_cmdline(existing_pid);
                    if (live != NULL || !record_looks_like_gateway(existing)) stale = true;
                    free(live);
                }
                /* PID+start_time collide but live process isn't a gateway. */
                if (!stale && existing_start != LONG_MIN && current_start >= 0 &&
                    !looks_like_gateway_process(existing_pid)) {
                    char *live = read_process_cmdline(existing_pid);
                    if (live != NULL) stale = true;
                    free(live);
                }
                /* Stopped (Ctrl+Z) process: treat as stale. */
                if (!stale && proc_is_stopped(existing_pid)) stale = true;
            }
        }

        if (stale) {
            unlink(lock_path);
        } else {
            if (out_existing) *out_existing = json_serialize(existing);
            json_free(existing);
            json_free(record);
            return false;
        }
        json_free(existing);
    }

    /* Atomic create. */
    int fd = open(lock_path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) {
        if (errno == EEXIST) {
            json_t *now = read_json_file(lock_path);
            if (out_existing && now) *out_existing = json_serialize(now);
            if (now) json_free(now);
        }
        json_free(record);
        return false;
    }
    char *ser = json_serialize(record);
    if (ser) {
        ssize_t w = write(fd, ser, strlen(ser));
        (void)w;
        free(ser);
    }
    close(fd);
    json_free(record);
    return true;
}

/* Port of Python: release_scoped_lock */
void gwstatus_release_scoped_lock(const char *scope, const char *identity) {
    char lock_path[1200];
    get_scope_lock_path(scope, identity, lock_path, sizeof(lock_path));
    json_t *existing = read_json_file(lock_path);
    if (!existing) return;
    pid_t p = pid_from_record(existing);
    long est = record_start_time(existing);
    json_free(existing);
    if (p != getpid()) return;
    if (est != gwstatus_get_process_start_time(getpid())) return;
    unlink(lock_path);
}

/* Port of Python: release_all_scoped_locks */
int gwstatus_release_all_scoped_locks(pid_t owner_pid, long owner_start_time) {
    char dir[1100];
    get_lock_dir(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) return 0;
    int removed = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        size_t nl = strlen(name);
        if (nl < 5 || strcmp(name + nl - 5, ".lock") != 0) continue;
        char full[1300];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (owner_pid > 0) {
            json_t *rec = read_json_file(full);
            if (!rec) continue;
            pid_t rp = pid_from_record(rec);
            long rst = record_start_time(rec);
            json_free(rec);
            if (rp != owner_pid) continue;
            if (owner_start_time >= 0 && rst != owner_start_time) continue;
        }
        if (unlink(full) == 0) removed++;
    }
    closedir(d);
    return removed;
}

/* ── --replace takeover / planned-stop markers ───────────────────────── */

/* Port of Python: _marker_is_stale — true if age > ttl or unparseable. */
static bool marker_is_stale(const char *written_at, int ttl_s) {
    if (!written_at || !written_at[0]) return true;
    /* Parse ISO 8601 (may carry +00:00 and fractional seconds). */
    struct tm tmv; memset(&tmv, 0, sizeof(tmv));
    int y, mo, d, h, mi, s;
    if (sscanf(written_at, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6)
        return true;
    tmv.tm_year = y - 1900; tmv.tm_mon = mo - 1; tmv.tm_mday = d;
    tmv.tm_hour = h; tmv.tm_min = mi; tmv.tm_sec = s;
    time_t written = timegm(&tmv);  /* value was UTC */
    if (written == (time_t)-1) return true;
    double age = difftime(time(NULL), written);
    return age > (double)ttl_s;
}

/* Port of Python: _consume_pid_marker_for_self. Always unlinks a matched or
 * stale/malformed marker. Returns true when the marker names this process. */
static bool consume_pid_marker_for_self(const char *path,
                                        const char *pid_field,
                                        const char *start_time_field,
                                        int ttl_s) {
    json_t *record = read_json_file(path);
    if (!record) return false;

    json_t *tp = json_obj_get(record, pid_field);
    if (!tp || (tp->type != JSON_NUMBER && tp->type != JSON_STRING)) {
        json_free(record);
        unlink(path);
        return false;
    }
    pid_t target_pid = (tp->type == JSON_NUMBER)
        ? (pid_t)tp->num_val : (pid_t)strtol(tp->str_val, NULL, 10);
    json_t *tst = json_obj_get(record, start_time_field);
    long target_start = (tst && tst->type == JSON_NUMBER) ? (long)tst->num_val : LONG_MIN;
    const char *written_at = json_get_str(record, "written_at", "");

    if (marker_is_stale(written_at, ttl_s)) {
        json_free(record);
        unlink(path);
        return false;
    }

    /* Cross-home guard: reject markers written under a different home. */
    json_t *rh = json_obj_get(record, "replacer_hermes_home");
    if (rh && rh->type == JSON_STRING) {
        if (strcmp(rh->str_val, slermes_home()) != 0) {
            json_free(record);
            return false;  /* leave marker in place for the correct home */
        }
    }

    pid_t our_pid = getpid();
    long our_start = gwstatus_get_process_start_time(our_pid);
    bool matches;
    if (target_pid != our_pid) {
        matches = false;
    } else if (target_start != LONG_MIN && our_start >= 0) {
        matches = (target_start == our_start);
    } else {
        matches = true;
    }

    json_free(record);
    unlink(path);
    return matches;
}

/* Port of Python: write_takeover_marker */
bool gwstatus_write_takeover_marker(pid_t target_pid) {
    long target_start = gwstatus_get_process_start_time(target_pid);
    json_t *record = json_object();
    json_set(record, "target_pid", json_number((double)target_pid));
    if (target_start >= 0) json_set(record, "target_start_time", json_number((double)target_start));
    else json_set(record, "target_start_time", json_null());
    json_set(record, "replacer_pid", json_number((double)getpid()));
    json_set(record, "replacer_hermes_home", json_string(slermes_home()));
    char iso[64]; utc_now_iso_buf(iso, sizeof(iso));
    json_set(record, "written_at", json_string(iso));
    char path[1200];
    get_takeover_marker_path(path, sizeof(path));
    int rc = write_json_file(path, record);
    json_free(record);
    return rc == 0;
}

/* Port of Python: consume_takeover_marker_for_self */
bool gwstatus_consume_takeover_marker_for_self(void) {
    char path[1200];
    get_takeover_marker_path(path, sizeof(path));
    return consume_pid_marker_for_self(path, "target_pid",
                                       "target_start_time", TAKEOVER_MARKER_TTL_S);
}

/* Port of Python: clear_takeover_marker */
void gwstatus_clear_takeover_marker(void) {
    char path[1200];
    get_takeover_marker_path(path, sizeof(path));
    unlink(path);
}

/* Port of Python: write_planned_stop_marker */
bool gwstatus_write_planned_stop_marker(pid_t target_pid) {
    long target_start = gwstatus_get_process_start_time(target_pid);
    json_t *record = json_object();
    json_set(record, "target_pid", json_number((double)target_pid));
    if (target_start >= 0) json_set(record, "target_start_time", json_number((double)target_start));
    else json_set(record, "target_start_time", json_null());
    json_set(record, "stopper_pid", json_number((double)getpid()));
    char iso[64]; utc_now_iso_buf(iso, sizeof(iso));
    json_set(record, "written_at", json_string(iso));
    char path[1200];
    get_planned_stop_marker_path(path, sizeof(path));
    int rc = write_json_file(path, record);
    json_free(record);
    return rc == 0;
}

/* Port of Python: consume_planned_stop_marker_for_self */
bool gwstatus_consume_planned_stop_marker_for_self(void) {
    char path[1200];
    get_planned_stop_marker_path(path, sizeof(path));
    return consume_pid_marker_for_self(path, "target_pid",
                                       "target_start_time", PLANNED_STOP_MARKER_TTL_S);
}

/* Port of Python: planned_stop_marker_targets_self — non-destructive probe. */
bool gwstatus_planned_stop_marker_targets_self(void) {
    char path[1200];
    get_planned_stop_marker_path(path, sizeof(path));
    json_t *record = read_json_file(path);
    if (!record) return false;

    json_t *tp = json_obj_get(record, "target_pid");
    if (!tp || (tp->type != JSON_NUMBER && tp->type != JSON_STRING)) {
        json_free(record);
        unlink(path);  /* malformed: drop */
        return false;
    }
    pid_t target_pid = (tp->type == JSON_NUMBER)
        ? (pid_t)tp->num_val : (pid_t)strtol(tp->str_val, NULL, 10);
    json_t *tst = json_obj_get(record, "target_start_time");
    long target_start = (tst && tst->type == JSON_NUMBER) ? (long)tst->num_val : LONG_MIN;
    const char *written_at = json_get_str(record, "written_at", "");

    if (marker_is_stale(written_at, PLANNED_STOP_MARKER_TTL_S)) {
        json_free(record);
        unlink(path);
        return false;
    }

    pid_t our_pid = getpid();
    if (target_pid != our_pid) { json_free(record); return false; }

    long our_start = gwstatus_get_process_start_time(our_pid);
    bool result;
    if (target_start != LONG_MIN && our_start >= 0)
        result = (target_start == our_start);
    else
        result = true;
    json_free(record);
    return result;  /* never unlink a matching marker (non-destructive) */
}

/* Port of Python: clear_planned_stop_marker */
void gwstatus_clear_planned_stop_marker(void) {
    char path[1200];
    get_planned_stop_marker_path(path, sizeof(path));
    unlink(path);
}
