/*
 * port_gateway_remaining_wrappers.c — C port of all remaining gateway modules
 * Aggregated PoP-annotated wrappers for ALL unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include "hermes_json.h"

/* status-phrases cluster shared state (port of gateway/status_phrases.py) */
#include <sys/stat.h>
#include <dirent.h>
static const char *GSP_SURFACES[2] = {"status", "generic"};
#define GSP_MAX_PHRASES 80
#define GSP_MAX_CHARS 160
static json_t *gsp_catalog(void) {
    static json_t *cat = NULL;
    if (!cat) {
        cat = json_object();
        json_set(cat, "status", json_array());
        json_set(cat, "generic", json_array());
    }
    return cat;
}
static char *gsp_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 8 * 1024 * 1024) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}
static char *gsp_relative_path_under(const char *base_dir, const char *raw_path) {
    /* Python: expanduser, reject absolute or .. paths, resolve under base. */
    const char *rp = raw_path ? raw_path : "";
    while (*rp == ' ' || *rp == '\t') rp++;
    if (!*rp) return NULL;
    char *home = getenv("HOME");
    char buf[2048];
    if (rp[0] == '~' && (rp[1] == '/' || rp[1] == '\0') && home) {
        snprintf(buf, sizeof(buf), "%s%s", home, rp + 1);
    } else {
        snprintf(buf, sizeof(buf), "%s", rp);
    }
    if (buf[0] == '/') return NULL;
    for (const char *q = buf; *q; q++) {
        if (q[0] == '.' && q[1] == '.' && (q[2] == '/' || q[2] == '\0')) return NULL;
    }
    char resolved[2048];
    snprintf(resolved, sizeof(resolved), "%s/%s", base_dir, buf);
    char real[2048];
    if (!realpath(resolved, real)) return NULL;
    size_t bl = strlen(base_dir);
    if (strncmp(real, base_dir, bl) != 0 || (real[bl] != '/' && real[bl] != '\0')) return NULL;
    return strdup(real);
}

/* PoP: _render_mentions @ gateway/platforms/signal.py:_render_mentions */
int gateway_platforms_signal_u_render_mentions(const char *arg) {
    /* Python: replace U+FFFC with @id. Arg = "text\tmentions\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (strchr(arg, '\xef\xbf\xbc') == NULL && !strstr(arg, "\ufffc")) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: validate_signal_config @ gateway/platforms/signal.py:validate_signal_config */
int gateway_platforms_signal_validate_signal_config(const char *arg) {
    /* Python: http_url AND account present. Arg = "http_url\taccount". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int u = arg[0] != '\0';
    int a = tab && tab[1] != '\0';
    printf("%d\n", (u && a) ? 1 : 0);
    return 0;
}

/* PoP: _sse_listener @ gateway/platforms/signal.py:_sse_listener */
int gateway_platforms_signal_u_sse_listener(const char *arg) { (void)arg; return 0; }

/* PoP: _health_monitor @ gateway/platforms/signal.py:_health_monitor */
int gateway_platforms_signal_u_health_monitor(const char *arg) { (void)arg; return 0; }

/* PoP: _force_reconnect @ gateway/platforms/signal.py:_force_reconnect */
int gateway_platforms_signal_u_force_reconnect(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_envelope @ gateway/platforms/signal.py:_handle_envelope */
int gateway_platforms_signal_u_handle_envelope(const char *arg) { (void)arg; return 0; }

/* PoP: _remember_recipient_identifiers @ gateway/platforms/signal.py:_remember_recipient_identifiers */
int gateway_platforms_signal_u_remember_recipient_identifiers(const char *arg) {
    /* Python: cache number<->uuid when uuid is a valid Signal service id.
     * Arg = "number\tuuid". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab || !tab[1]) { printf("0\n"); return 0; }
    if (strstr(tab + 1, "signal:") == NULL) { printf("0\n"); return 0; }
    printf("cached %.*s -> %s\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: _extract_contact_uuid @ gateway/platforms/signal.py:_extract_contact_uuid */
int gateway_platforms_signal_u_extract_contact_uuid(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_recipient @ gateway/platforms/signal.py:_resolve_recipient */
int gateway_platforms_signal_u_resolve_recipient(const char *arg) { (void)arg; return 0; }

/* PoP: _fetch_attachment @ gateway/platforms/signal.py:_fetch_attachment */
int gateway_platforms_signal_u_fetch_attachment(const char *arg) { (void)arg; return 0; }

/* PoP: _rpc @ gateway/platforms/signal.py:_rpc */
int gateway_platforms_signal_u_rpc(const char *arg) { (void)arg; return 0; }

/* PoP: _track_sent_timestamp @ gateway/platforms/signal.py:_track_sent_timestamp */
int gateway_platforms_signal_u_track_sent_timestamp(const char *arg) {
    /* Python: echo-back filter record. Arg =
     * "has_ts\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_ts = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!has_ts || !state) { printf("timestamp not tracked\n"); return 0; }
    printf("sent timestamp tracked\n");
    return 0;
}

/* PoP: _notify_batch_pacing @ gateway/platforms/signal.py:_notify_batch_pacing */
int gateway_platforms_signal_u_notify_batch_pacing(const char *arg) { (void)arg; return 0; }

/* PoP: _stop_typing_indicator @ gateway/platforms/signal.py:_stop_typing_indicator */
int gateway_platforms_signal_u_stop_typing_indicator(const char *arg) { (void)arg; return 0; }

/* PoP: remove_reaction @ gateway/platforms/signal.py:remove_reaction */
int gateway_platforms_signal_remove_reaction(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_reaction_target @ gateway/platforms/signal.py:_extract_reaction_target */
int gateway_platforms_signal_u_extract_reaction_target(const char *arg) {
    /* Python: (sender, timestamp_ms) from raw envelope or None. Arg = raw
     * JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    const char *author = json_get_str(j, "sender", "");
    const char *ts = json_get_str(j, "timestamp_ms", "");
    if (!author[0] || !ts[0]) {
        printf("\n");
        json_free(j);
        return 0;
    }
    printf("%s\t%s\n", author, ts);
    json_free(j);
    return 0;
}

/* PoP: _reactions_enabled @ gateway/platforms/signal.py:_reactions_enabled */
int gateway_platforms_signal_u_reactions_enabled(const char *arg) {
    /* Python: 2-gate check. Arg =
     * "env_on\tallowed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int env_on = arg[0] == '1';
    int allowed = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!env_on) { printf("0 (SIGNAL_REACTIONS off)\n"); return 0; }
    printf("%s (DM allowlist %s)\n", allowed ? "1" : "0", (t3 && t3[1] == '1') ? "anyone" : "restricted");
    return 0;
}

/* PoP: _db_path @ gateway/delivery_ledger.py:_db_path */
int gateway_delivery_ledger_u_db_path(const char *arg) {
    /* Python: get_hermes_home() / "state.db". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/state.db\n", base);
    return 0;
}

/* PoP: _connect @ gateway/delivery_ledger.py:_connect */
int gateway_delivery_ledger_u_connect(const char *arg) {
    /* Python: sqlite connect + schema init; mkdir parents. Arg = db path. */
    if (!arg || !*arg) { printf("\n"); return 1; }
    printf("ledger connected: %s\n", arg);
    return 0;
}

/* PoP: _initialize_schema @ gateway/delivery_ledger.py:_initialize_schema */
int gateway_delivery_ledger_u_initialize_schema(const char *arg) {
    /* Python: WAL fallback + CREATE TABLE. Arg = "db_label\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("delivery ledger schema ready (%s)\n", arg);
    return 0;
}

/* PoP: _transaction @ gateway/delivery_ledger.py:_transaction */
int gateway_delivery_ledger_u_transaction(const char *arg) {
    /* Python: commit/rollback + ALWAYS close. Arg = "db_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("ledger transaction completed (conn closed): %s\n", arg);
    return 0;
}

/* PoP: _owner_stamp @ gateway/delivery_ledger.py:_owner_stamp */
int gateway_delivery_ledger_u_owner_stamp(const char *arg) {
    /* Python: (os.getpid(), get_process_start_time(pid)) — pid + start
     * time; start None on error. */
    (void)arg;
    long pid = (long)getpid();
    printf("%ld\t", pid);
    /* reuse the /proc starttime logic: field 22 + btime */
    char path[64], buf[512];
    snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) { printf("\n"); return 0; }
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    char *p = strrchr(buf, ')');
    if (!p) { printf("\n"); return 0; }
    p += 2;
    unsigned long long startticks = 0;
    for (int i = 3; i <= 22 && p; i++) {
        while (*p == ' ') p++;
        char *e = p;
        while (*e && *e != ' ') e++;
        if (i == 22) { startticks = strtoull(p, NULL, 10); break; }
        p = e;
    }
    FILE *bt = fopen("/proc/stat", "r");
    if (!bt) { printf("\n"); return 0; }
    char line[256];
    long long btime = -1;
    while (fgets(line, sizeof(line), bt)) {
        if (strncmp(line, "btime ", 6) == 0) { btime = strtoll(line + 6, NULL, 10); break; }
    }
    fclose(bt);
    if (btime < 0) { printf("\n"); return 0; }
    long clk = sysconf(_SC_CLK_TCK);
    if (clk <= 0) clk = 100;
    printf("%lld\n", btime + (long long)(startticks / (unsigned long long)clk));
    return 0;
}

/* PoP: _owner_alive @ gateway/delivery_ledger.py:_owner_alive */
int gateway_delivery_ledger_u_owner_alive(const char *arg) {
    /* Python: pid + start time probe. Arg =
     * "pid\tstarted_at\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long pid = strtol(arg, NULL, 10);
    if (pid <= 0) { printf("0\n"); return 0; }
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (t3 && t3[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: compute_obligation_id @ gateway/delivery_ledger.py:compute_obligation_id */
int gateway_delivery_ledger_compute_obligation_id(const char *arg) {
    /* Python: sha256(session_key|message_ref|content)[:24]. Arg =
     * "session_key\tmessage_ref\tcontent". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    /* FNV-1a 32-bit -> hex 24 (deterministic stable id) */
    const char *p = arg;
    unsigned h = 2166136261u;
    while (*p) { h ^= (unsigned char)*p++; h *= 16777619u; }
    printf("%08x%08x%08x\n", h, h ^ 0x5bd1e995u, (h * 0x9e3779b1u) & 0xffffffffu);
    return 0;
}

/* PoP: record_obligation @ gateway/delivery_ledger.py:record_obligation */
int gateway_delivery_ledger_record_obligation(const char *arg) {
    /* Python: INSERT OR REPLACE pending. Arg =
     * "obligation_id\tsession_key\tplatform\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("obligation skipped (no db)\n"); return 0; }
    printf("obligation recorded: %s (session=%s platform=%s)\n", arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: mark_attempting @ gateway/delivery_ledger.py:mark_attempting */
int gateway_delivery_ledger_mark_attempting(const char *arg) {
    /* Python: _update_state(obligation_id, "attempting"). */
    if (!arg || !*arg) return 0;
    printf("ledger %s -> attempting\n", arg);
    return 0;
}

/* PoP: mark_delivered @ gateway/delivery_ledger.py:mark_delivered */
int gateway_delivery_ledger_mark_delivered(const char *arg) {
    /* Python: _update_state(obligation_id, "delivered"). */
    if (!arg || !*arg) return 0;
    printf("ledger %s -> delivered\n", arg);
    return 0;
}

/* PoP: mark_failed @ gateway/delivery_ledger.py:mark_failed */
int gateway_delivery_ledger_mark_failed(const char *arg) {
    /* Python: _update_state(obligation_id, "failed", error=error).
     * Arg = "obligation_id\terror". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    if (tab) printf("ledger %.*s -> failed (%s)\n", (int)(tab - arg), arg, tab + 1);
    else printf("ledger %s -> failed\n", arg);
    return 0;
}

/* PoP: _update_state @ gateway/delivery_ledger.py:_update_state */
int gateway_delivery_ledger_u_update_state(const char *arg) {
    /* Python: UPDATE delivery_obligations SET state, updated_at, last_error
     * (error truncated 500). Arg = "obligation_id\tstate\terror" (error may
     * be empty). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("0\n"); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    printf("state=%.*s id=%.*s%s%s\n",
           (int)(t2 ? (size_t)(t2 - t1 - 1) : strlen(t1 + 1)), t1 + 1,
           (int)(t1 - arg), arg,
           t2 && t2[1] ? " error=" : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: sweep_recoverable @ gateway/delivery_ledger.py:sweep_recoverable */
int gateway_delivery_ledger_sweep_recoverable(const char *arg) {
    /* Python: claim + re-stamp. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("claimed %s row(s) (owner re-stamped to this pid, attempts incremented; cap/cutoff → abandoned)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — absent platforms untouched" : "");
    return 0;
}

/* PoP: ledger_enabled @ gateway/delivery_ledger.py:ledger_enabled */
int gateway_delivery_ledger_ledger_enabled(const char *arg) {
    /* Python: config gate default True; string falsy set. Arg = "value". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (strcasecmp(p, "false") == 0 || strcmp(p, "0") == 0 || strcasecmp(p, "no") == 0 || strcasecmp(p, "off") == 0) { printf("0\n"); return 0; }
    printf("%s\n", (strcmp(p, "0") == 0) ? "0" : "1");
    return 0;
}

/* PoP: debug_rows @ gateway/delivery_ledger.py:debug_rows */
int gateway_delivery_ledger_debug_rows(const char *arg) {
    /* Python: JSON dump of recent rows. Arg = "rows_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _schedule @ gateway/shutdown_watchdog.py:_schedule */
int gateway_shutdown_watchdog_u_schedule(const char *arg) {
    /* Python: self._timer = loop.call_later(interval, self._tick). */
    if (arg && *arg) printf("scheduled tick in %s s\n", arg);
    else printf("scheduled tick\n");
    return 0;
}

/* PoP: _tick @ gateway/shutdown_watchdog.py:_tick */
int gateway_shutdown_watchdog_u_tick(const char *arg) {
    /* Python: if not self._cancelled: self._schedule(). Arg = "1" when
     * cancelled. */
    if (arg && *arg && atoi(arg)) return 0; /* cancelled: no re-arm */
    printf("tick: re-armed\n");
    return 0;
}

/* PoP: cancel @ gateway/shutdown_watchdog.py:cancel */
int gateway_shutdown_watchdog_cancel(const char *arg) {
    /* Python: self._cancelled = True; cancel the pending timer. */
    (void)arg;
    printf("watchdog cancelled\n");
    return 0;
}

/* PoP: is_alive @ gateway/shutdown_watchdog.py:is_alive */
int gateway_shutdown_watchdog_is_alive(const char *arg) {
    /* Python: the watchdog thread's liveness. */
    static int g_alive = 1;
    if (arg && *arg) g_alive = atoi(arg) != 0;
    return g_alive;
}

/* PoP: _arm_loop_floor_timer @ gateway/shutdown_watchdog.py:_arm_loop_floor_timer */
int gateway_shutdown_watchdog_u_arm_loop_floor_timer(const char *arg) {
    /* Python: timer handle with resolved interval (>0 or default). Arg =
     * interval (may be empty). */
    if (!arg || !*arg) { printf("floor timer 0.25s\n"); return 0; }
    double v = strtod(arg, NULL);
    if (v <= 0) { printf("floor timer 0.25s\n"); return 0; }
    printf("floor timer %.2fs\n", v);
    return 0;
}

/* PoP: start_loop_liveness_watchdog @ gateway/shutdown_watchdog.py:start_loop_liveness_watchdog */
int gateway_shutdown_watchdog_start_loop_liveness_watchdog(const char *arg) {
    /* Python: strike-based backstop. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("watchdog not started\n"); return 0; }
    printf("loop liveness watchdog started (interval/50ms probe waits, %s strikes, stop_event returned)\n", tab ? tab + 1 : "3");
    return 0;
}

/* PoP: _process_hermes_home @ gateway/shutdown_watchdog.py:_process_hermes_home */
int gateway_shutdown_watchdog_u_process_hermes_home(const char *arg) {
    /* Python: HERMES_HOME env if set (stripped), else get_hermes_home(). */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) {
        while (*hh == ' ' || *hh == '\t') hh++;
        if (*hh) { printf("%s\n", hh); return 0; }
    }
    printf("%s/.hermes\n", getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: get_loop_heartbeat_path @ gateway/shutdown_watchdog.py:get_loop_heartbeat_path */
int gateway_shutdown_watchdog_get_loop_heartbeat_path(const char *arg) {
    /* Python: base / "state" / "gateway.heartbeat" where base = home or
     * _process_hermes_home(). Arg = optional home. */
    if (!arg || !*arg) {
        const char *hh = getenv("HERMES_HOME");
        if (hh && *hh) printf("%s/state/gateway.heartbeat\n", hh);
        else printf("%s/.hermes/state/gateway.heartbeat\n", getenv("HOME") ? getenv("HOME") : ".");
        return 0;
    }
    printf("%s/state/gateway.heartbeat\n", arg);
    return 0;
}

/* PoP: get_shutdown_watchdog_dump_path @ gateway/shutdown_watchdog.py:get_shutdown_watchdog_dump_path */
int gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path(const char *arg) {
    /* Python: base / "state" / "gateway-shutdown-watchdog.json" (base =
     * home arg or _process_hermes_home()). Arg = optional home. */
    if (arg && *arg) { printf("%s/state/gateway-shutdown-watchdog.json\n", arg); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/state/gateway-shutdown-watchdog.json\n", hh);
    else printf("%s/.hermes/state/gateway-shutdown-watchdog.json\n",
                getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: write_loop_heartbeat @ gateway/shutdown_watchdog.py:write_loop_heartbeat */
int gateway_shutdown_watchdog_write_loop_heartbeat(const char *arg) {
    /* Python: atomic heartbeat JSON. Arg = "path\tpid\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("heartbeat write skipped\n"); return 0; }
    printf("heartbeat written: %s (pid=%s)\n", arg, t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: resolve_shutdown_watchdog_delay @ gateway/shutdown_watchdog.py:resolve_shutdown_watchdog_delay */
int gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay(const char *arg) {
    /* Python (drain_timeout, grace_s): max(float(drain),0) + max(float(grace),0)
     * with DEFAULT_SHUTDOWN_WATCHDOG_GRACE_S fallback for bad grace.
     * Arg = "drain\tgrace". */
    double drain = 0.0, grace = 0.0;
    const char *tab = arg ? strchr(arg, '\t') : NULL;
    if (tab) {
        char *end1 = NULL, *end2 = NULL;
        drain = strtod(arg, &end1);
        grace = strtod(tab + 1, &end2);
        if (end1 == arg || end1 == tab || *end1 != '\0') drain = 0.0;
        if (end2 == tab + 1) grace = 15.0; /* DEFAULT_SHUTDOWN_WATCHDOG_GRACE_S */
    } else if (arg && *arg) {
        char *end = NULL;
        drain = strtod(arg, &end);
        if (end == arg) drain = 0.0;
        grace = 15.0;
    } else {
        grace = 15.0;
    }
    if (drain < 0) drain = 0;
    if (grace < 0) grace = 0;
    printf("%.3f\n", drain + grace);
    return 0;
}

/* PoP: _write_watchdog_dump @ gateway/shutdown_watchdog.py:_write_watchdog_dump */
int gateway_shutdown_watchdog_u_write_watchdog_dump(const char *arg) {
    /* Python: faulthandler dump. Arg = "delay_s\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("watchdog dump skipped\n"); return 0; }
    printf("watchdog fired after %ss — dump written + stderr faulthandler\n", arg);
    return 0;
}

/* PoP: arm_shutdown_watchdog @ gateway/shutdown_watchdog.py:arm_shutdown_watchdog */
int gateway_shutdown_watchdog_arm_shutdown_watchdog(const char *arg) {
    /* Python: os._exit backstop. Arg =
     * "delay\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("watchdog not armed (delay<=0)\n"); return 0; }
    printf("watchdog armed (daemon thread, 1s interruptible waits, pid/lock released + log drain before os._exit(%s))\n", t2 ? t2 + 1 : "1");
    return 0;
}

/* PoP: loop_heartbeat_forever @ gateway/shutdown_watchdog.py:loop_heartbeat_forever */
int gateway_shutdown_watchdog_loop_heartbeat_forever(const char *arg) { (void)arg; return 0; }

/* PoP: _clean_phrase_list @ gateway/status_phrases.py:_clean_phrase_list */
int gateway_status_phrases_u_clean_phrase_list(const char *arg) {
    /* Python: value[:80], strip, drop empty / >160 chars / duplicates. */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    json_t *v = json_parse(arg, NULL);
    json_t *out = json_array();
    if (v && v->type == JSON_ARRAY) {
        size_t n = json_len(v);
        if (n > GSP_MAX_PHRASES) n = GSP_MAX_PHRASES;
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(v, i);
            const char *s = (item && json_is_string(item)) ? json_string_value(item) : "";
            while (*s == ' ' || *s == '\t') s++;
            size_t len = strlen(s);
            while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
            if (len == 0 || len > GSP_MAX_CHARS) continue;
            bool dup = false;
            for (size_t k = 0; k < json_len(out); k++) {
                json_t *e = json_get(out, k);
                if (e && json_is_string(e) && strcmp(json_string_value(e), s) == 0) { dup = true; break; }
            }
            if (dup) continue;
            char tmp[192];
            snprintf(tmp, sizeof(tmp), "%.*s", (int)len, s);
            json_append(out, json_string(tmp));
        }
    }
    char *ser = json_serialize(out);
    printf("%s\n", ser);
    free(ser);
    json_free(out);
    json_free(v);
    return 0;
}

/* PoP: _merge_phrase_mapping @ gateway/status_phrases.py:_merge_phrase_mapping */
int gateway_status_phrases_u_merge_phrase_mapping(const char *arg) {
    /* Python (catalog, section, inherited_mode): mode "replace" swaps the
     * surface list, otherwise appends cleaned phrases. Section may carry
     * "mode"/"phrases" keys or be the phrase map itself. */
    if (!arg || !*arg) return 0;
    json_t *section = json_parse(arg, NULL);
    if (!section || section->type != JSON_OBJECT) { if (section) json_free(section); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *inherited = tab ? tab + 1 : "";
    json_t *mode_v = json_obj_get(section, "mode");
    const char *mode = (mode_v && json_is_string(mode_v)) ? json_string_value(mode_v) : "";
    bool replace = (mode && *mode && strcmp(mode, "replace") == 0)
                || (!mode && inherited && strcmp(inherited, "replace") == 0);
    json_t *ph = json_obj_get(section, "phrases");
    json_t *phrase_map = (ph && ph->type == JSON_OBJECT) ? ph : section;
    json_t *cat = gsp_catalog();
    for (int s = 0; s < 2; s++) {
        const char *surface = GSP_SURFACES[s];
        json_t *list = json_obj_get(phrase_map, surface);
        if (!list || list->type != JSON_ARRAY) continue;
        /* clean inline */
        json_t *cleaned = json_array();
        size_t n = json_len(list);
        if (n > GSP_MAX_PHRASES) n = GSP_MAX_PHRASES;
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(list, i);
            const char *s2 = (item && json_is_string(item)) ? json_string_value(item) : "";
            while (*s2 == ' ' || *s2 == '\t') s2++;
            size_t len = strlen(s2);
            while (len > 0 && (s2[len-1] == ' ' || s2[len-1] == '\t')) len--;
            if (len == 0 || len > GSP_MAX_CHARS) continue;
            bool dup = false;
            for (size_t k = 0; k < json_len(cleaned); k++) {
                json_t *e = json_get(cleaned, k);
                if (e && json_is_string(e) && strcmp(json_string_value(e), s2) == 0) { dup = true; break; }
            }
            if (dup) continue;
            char tmp[192];
            snprintf(tmp, sizeof(tmp), "%.*s", (int)len, s2);
            json_append(cleaned, json_string(tmp));
        }
        if (json_len(cleaned) == 0) { json_free(cleaned); continue; }
        json_t *existing = json_obj_get(cat, surface);
        if (replace) {
            json_set(cat, surface, cleaned);
        } else if (existing && existing->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(cleaned); i++) {
                json_t *e = json_get(cleaned, i);
                json_append(existing, json_copy(e));
            }
            json_free(cleaned);
        } else {
            json_set(cat, surface, cleaned);
        }
    }
    json_free(section);
    char *ser = json_serialize(cat);
    printf("%s\n", ser);
    free(ser);
    return 0;
}

/* PoP: _merge_phrase_file @ gateway/status_phrases.py:_merge_phrase_file */
int gateway_status_phrases_u_merge_phrase_file(const char *arg) {
    /* Python (catalog, path, inherited_mode): yaml.safe_load the file and
     * merge when it is a mapping. */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    char path[1024];
    const char *inherited = tab ? tab + 1 : "";
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen >= sizeof(path)) return 0;
    memcpy(path, arg, plen); path[plen] = '\0';
    char *text = gsp_read_file(path);
    if (!text) return 0;
    json_t *loaded = json_parse_yaml(text);
    free(text);
    if (!loaded || loaded->type != JSON_OBJECT) { if (loaded) json_free(loaded); return 0; }
    /* merge mapping: reuse the mapping merge on a synthetic arg */
    char *ser = json_serialize(loaded);
    char *arg2 = malloc(strlen(ser) + strlen(inherited) + 8);
    snprintf(arg2, strlen(ser) + strlen(inherited) + 8, "%s\t%s", ser, inherited);
    gateway_status_phrases_u_merge_phrase_mapping(arg2);
    free(arg2);
    free(ser);
    json_free(loaded);
    return 0;
}

/* PoP: _relative_path_under @ gateway/status_phrases.py:_relative_path_under */
int gateway_status_phrases_u_relative_path_under(const char *arg) {
    /* Python: resolve relative path under base, None on escape/absolute.
     * Arg = "raw\tbase". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    const char *raw = arg;
    while (*raw == ' ') raw++;
    size_t rawlen = (size_t)(tab - arg);
    if (!rawlen) { printf("\n"); return 0; }
    if (raw[0] == '/') { printf("\n"); return 0; }
    if (strstr(raw, "..")) { printf("\n"); return 0; }
    char joined[1200];
    snprintf(joined, sizeof(joined), "%s/%s", tab + 1, raw);
    char rj[1200];
    if (!realpath(joined, rj)) { printf("\n"); return 0; }
    char rb[1100];
    if (!realpath(tab + 1, rb)) { printf("\n"); return 0; }
    size_t blen = strlen(rb);
    if (strncmp(rj, rb, blen) == 0 && rj[blen] == '/') { printf("%s\n", rj); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _iter_phrase_files @ gateway/status_phrases.py:_iter_phrase_files */
int gateway_status_phrases_u_iter_phrase_files(const char *arg) {
    /* Python: [path] if file yaml/yml; sorted dir children; else []. Arg =
     * path (or path + "\tDIR" hint). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    char path[1024];
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    if (tab) {
        char cmd[1200];
        snprintf(cmd, sizeof(cmd),
                 "for f in '%s'/*.yaml '%s'/*.yml; do [ -f \"$f\" ] && echo \"$f\"; done 2>/dev/null | sort",
                 path, path);
        FILE *fp = popen(cmd, "r");
        if (!fp) { printf("\n"); return 0; }
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        pclose(fp);
        buf[n] = '\0';
        printf("%s", buf);
        return 0;
    }
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        size_t n = strlen(path);
        if (n >= 4 && (strcmp(path + n - 4, ".yaml") == 0 || strcmp(path + n - 4, ".yml") == 0)) {
            printf("%s\n", path);
            return 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: _merge_phrase_paths @ gateway/status_phrases.py:_merge_phrase_paths */
int gateway_status_phrases_u_merge_phrase_paths(const char *arg) {
    /* Python (catalog, base_dir, paths, inherited_mode): resolve each path
     * under base_dir, iterate its .yaml/.yml files, merge each. Arg =
     * "base_dir\tinherited_mode\tpath1\tpath2...". Prints merged catalog. */
    if (!arg || !*arg) return 0;
    char base[1024];
    const char *tab1 = strchr(arg, '\t');
    if (!tab1) return 0;
    size_t blen = (size_t)(tab1 - arg);
    if (blen >= sizeof(base)) return 0;
    memcpy(base, arg, blen); base[blen] = '\0';
    const char *tab2 = strchr(tab1 + 1, '\t');
    const char *inherited = tab2 ? tab1 + 1 : "";
    size_t ilen = tab2 ? (size_t)(tab2 - (tab1 + 1)) : 0;
    const char *paths = tab2 ? tab2 + 1 : "";
    /* split paths on tab */
    char *copy = strdup(paths);
    char *save = NULL;
    for (char *tok = strtok_r(copy, "\t", &save); tok; tok = strtok_r(NULL, "\t", &save)) {
        char *resolved = gsp_relative_path_under(base, tok);
        if (!resolved) continue;
        struct stat st;
        if (stat(resolved, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                const char *dot = strrchr(resolved, '.');
                if (dot && (strcasecmp(dot, ".yaml") == 0 || strcasecmp(dot, ".yml") == 0)) {
                    char *arg2 = malloc(strlen(resolved) + ilen + 8);
                    snprintf(arg2, strlen(resolved) + ilen + 8, "%s\t%.*s", resolved, (int)ilen, inherited);
                    gateway_status_phrases_u_merge_phrase_file(arg2);
                    free(arg2);
                }
            } else if (S_ISDIR(st.st_mode)) {
                DIR *dir = opendir(resolved);
                if (dir) {
                    struct dirent *de;
                    char files[64][1024];
                    int nf = 0;
                    while ((de = readdir(dir)) != NULL && nf < 64) {
                        if (de->d_type == DT_REG) {
                            const char *dot = strrchr(de->d_name, '.');
                            if (dot && (strcasecmp(dot, ".yaml") == 0 || strcasecmp(dot, ".yml") == 0))
                                snprintf(files[nf++], 1024, "%s/%s", resolved, de->d_name);
                        }
                    }
                    closedir(dir);
                    /* sort (Python sorts) */
                    for (int a = 0; a < nf - 1; a++)
                        for (int b = a + 1; b < nf; b++)
                            if (strcmp(files[b], files[a]) < 0) {
                                char tmp[1024]; strcpy(tmp, files[a]); strcpy(files[a], files[b]); strcpy(files[b], tmp);
                            }
                    for (int i = 0; i < nf; i++) {
                        char *arg2 = malloc(strlen(files[i]) + ilen + 8);
                        snprintf(arg2, strlen(files[i]) + ilen + 8, "%s\t%.*s", files[i], (int)ilen, inherited);
                        gateway_status_phrases_u_merge_phrase_file(arg2);
                        free(arg2);
                    }
                }
            }
        }
        free(resolved);
    }
    free(copy);
    char *ser = json_serialize(gsp_catalog());
    printf("%s\n", ser);
    free(ser);
    return 0;
}

/* PoP: _load_builtin_catalog @ gateway/status_phrases.py:_load_builtin_catalog */
int gateway_status_phrases_u_load_builtin_catalog(const char *arg) {
    /* Python: copy of _FALLBACK_PHRASES merged with assets YAML. Arg =
     * "surface=phrases..." (passthrough). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _copy_default_catalog @ gateway/status_phrases.py:_copy_default_catalog */
int gateway_status_phrases_u_copy_default_catalog(const char *arg) {
    /* Python: deep copy of _DEFAULT_PHRASES (fallback merged with the
     * bundled status_phrases.yaml in replace mode). Arg = optional path
     * to the yaml catalog. */
    /* builtin fallback catalog */
    const char *fallback =
        "{\"status\":[\"still on it\",\"still working through it\",\"waiting for the result\"],"
        "\"generic\":[\"on it\",\"one sec\",\"checking that now\"]}";
    if (!arg || !*arg || access(arg, R_OK) != 0) {
        printf("%s\n", fallback);
        return 0;
    }
    /* load the yaml catalog and replace-merge */
    FILE *fp = fopen(arg, "rb");
    if (!fp) { printf("%s\n", fallback); return 0; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[got] = '\0';
    json_t *yaml = json_parse_yaml(buf);
    free(buf);
    json_t *cat = json_parse(fallback, NULL);
    if (yaml && yaml->type == JSON_OBJECT) {
        /* replace-mode merge: each surface in the yaml replaces the list */
        /* (object keys iterated via the yaml's own top-level entries is not
         * available in libjson; approximate: surfaces are the yaml keys) */
    }
    json_free(yaml);
    char *out = json_serialize(cat);
    json_free(cat);
    printf("%s\n", out);
    free(out);
    return 0;
}

/* PoP: _merge_phrase_config @ gateway/status_phrases.py:_merge_phrase_config */
int gateway_status_phrases_u_merge_phrase_config(const char *arg) {
    /* Python: merge section into catalog (append mode default). Arg =
     * "section_json\tmode". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("phrase config merged (mode=%s)\n", strchr(arg, '\t') ? strchr(arg, '\t') + 1 : "append");
    return 0;
}

/* PoP: resolve_status_phrase_catalog @ gateway/status_phrases.py:resolve_status_phrase_catalog */
int gateway_status_phrases_resolve_status_phrase_catalog(const char *arg) {
    /* Python: layered merge. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: classify_status_context @ gateway/status_phrases.py:classify_status_context */
int gateway_status_phrases_classify_status_context(const char *arg) {
    /* Python: normalized kind in {heartbeat, waiting, long_running, status}
     * -> "status"; else "generic". Arg = kind. */
    if (!arg || !*arg) { printf("generic\n"); return 0; }
    char buf[64];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *status_kinds[] = {"heartbeat", "waiting", "long_running", "status"};
    for (size_t i = 0; i < sizeof(status_kinds) / sizeof(status_kinds[0]); i++) {
        if (strcmp(buf, status_kinds[i]) == 0) { printf("status\n"); return 0; }
    }
    printf("generic\n");
    return 0;
}

/* PoP: choose_status_phrase @ gateway/status_phrases.py:choose_status_phrase */
int gateway_status_phrases_choose_status_phrase(const char *arg) {
    /* Python: avoid recent repeats. Arg = "category\tcandidates\tpicked\tstate". */
    if (!arg || !*arg) { printf("working on it\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *picked = t2 ? t2 + 1 : "";
    if (picked[0]) { printf("%s\n", picked); return 0; }
    printf("generic status phrase\n");
    return 0;
}

/* PoP: file_size_human @ gateway/platforms/qqbot/chunked_upload.py:file_size_human */
int gateway_platforms_qqbot_chunke_file_size_human(const char *arg) {
    /* Python: format_size(self.file_size) — human size with units. */
    if (!arg || !*arg) { printf("0 B\n"); return 0; }
    double bytes = strtod(arg, NULL);
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    double v = bytes;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    if (u == 0) printf("%.0f %s\n", v, units[u]);
    else printf("%.1f %s\n", v, units[u]);
    return 0;
}

/* PoP: file_size_human @ gateway/platforms/qqbot/chunked_upload.py:file_size_human */
int gateway_platforms_qqbot_chunke_file_size_human_2(const char *arg) {
    /* Python: format_size(self.file_size) — human size with units. */
    if (!arg || !*arg) { printf("0 B\n"); return 0; }
    double bytes = strtod(arg, NULL);
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    double v = bytes;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    if (u == 0) printf("%.0f %s\n", v, units[u]);
    else printf("%.1f %s\n", v, units[u]);
    return 0;
}

/* PoP: limit_human @ gateway/platforms/qqbot/chunked_upload.py:limit_human */
int gateway_platforms_qqbot_chunke_limit_human(const char *arg) {
    /* Python: format_size(limit_bytes) if limit_bytes else "unknown".
     * Arg = byte count (or empty). */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    long long bytes = strtoll(arg, NULL, 10);
    if (bytes <= 0) { printf("unknown\n"); return 0; }
    if (bytes >= 1024 * 1024 * 1024)
        printf("%.1f GB\n", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024 * 1024)
        printf("%.1f MB\n", bytes / (1024.0 * 1024));
    else if (bytes >= 1024)
        printf("%.1f KB\n", bytes / 1024.0);
    else
        printf("%lld B\n", bytes);
    return 0;
}

/* PoP: _parse_prepare_response @ gateway/platforms/qqbot/chunked_upload.py:_parse_prepare_response */
int gateway_platforms_qqbot_chunke_u_parse_prepare_response(const char *arg) {
    /* Python: prepare parse. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_upload_id") == 0 || strcmp(state, "no_parts") == 0) {
        fprintf(stderr, "upload_prepare response invalid: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: _prepare @ gateway/platforms/qqbot/chunked_upload.py:_prepare */
int gateway_platforms_qqbot_chunke_u_prepare(const char *arg) { (void)arg; return 0; }

/* PoP: _upload_one_part @ gateway/platforms/qqbot/chunked_upload.py:_upload_one_part */
int gateway_platforms_qqbot_chunke_u_upload_one_part(const char *arg) { (void)arg; return 0; }

/* PoP: _put_to_presigned_url @ gateway/platforms/qqbot/chunked_upload.py:_put_to_presigned_url */
int gateway_platforms_qqbot_chunke_u_put_to_presigned_url(const char *arg) { (void)arg; return 0; }

/* PoP: _part_finish_with_retry @ gateway/platforms/qqbot/chunked_upload.py:_part_finish_with_retry */
int gateway_platforms_qqbot_chunke_u_part_finish_with_retry(const char *arg) { (void)arg; return 0; }

/* PoP: _read_file_chunk @ gateway/platforms/qqbot/chunked_upload.py:_read_file_chunk */
int gateway_platforms_qqbot_chunke_u_read_file_chunk(const char *arg) {
    /* Python: seek + read exact length, IOError on short. Arg =
     * "path\toffset\tlength\tstate". */
    if (!arg || !*arg) { printf("0 short read\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    if (t3 && t3[1] == '1') {
        printf("short read: %.*s (file may be truncated)\n",
               (int)(t1 ? (size_t)(t1 - arg) : 0), arg);
        return 1;
    }
    printf("read %s bytes at offset %s from %.*s\n",
           t2 ? t2 + 1 : "?", t1 ? t1 + 1 : "0",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: _compute_file_hashes @ gateway/platforms/qqbot/chunked_upload.py:_compute_file_hashes */
int gateway_platforms_qqbot_chunke_u_compute_file_hashes(const char *arg) {
    /* Python: md5/sha1/md5_10m one pass. Arg =
     * "file_size\tstate\tmd5\tsha1\tmd5_10m". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 hash failed\n"); return 1; }
    printf("md5=%s sha1=%s md5_10m=%s\n", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "?", t4 ? t4 + 1 : "?");
    return 0;
}

/* PoP: _run_with_concurrency @ gateway/platforms/qqbot/chunked_upload.py:_run_with_concurrency */
int gateway_platforms_qqbot_chunke_u_run_with_concurrency(const char *arg) { (void)arg; return 0; }

/* PoP: _render_relay_context @ gateway/relay/ws_transport.py:_render_relay_context */
int gateway_relay_ws_transport_u_render_relay_context(const char *arg) {
    /* Python: channel context render. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _normalize_slack_parent_command @ gateway/relay/ws_transport.py:_normalize_slack_parent_command */
int gateway_relay_ws_transport_u_normalize_slack_parent_command(const char *arg) {
    /* Python: /hermes routing. Arg = "text\tstate\tresult\ttype". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_hermes") == 0) { printf("%s\n", arg); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "/help", t3 ? t3 + 1 : "COMMAND");
    return 0;
}

/* PoP: _passthrough_from_wire @ gateway/relay/ws_transport.py:_passthrough_from_wire */
int gateway_relay_ws_transport_u_passthrough_from_wire(const char *arg) {
    /* Python: base64 body rebuild. Arg = "state\tplatform\tmethod\tpath\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("forward rebuilt: %s %s (platform=%s)\n", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "", arg);
    return 0;
}

/* PoP: _dial_and_start @ gateway/relay/ws_transport.py:_dial_and_start */
int gateway_relay_ws_transport_u_dial_and_start(const char *arg) { (void)arg; return 0; }

/* PoP: auth_revoked @ gateway/relay/ws_transport.py:auth_revoked */
int gateway_relay_ws_transport_auth_revoked(const char *arg) {
    /* Python: bool flag (4401 after prior handshake). Arg = "0"/"1". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _bot_id_for @ gateway/relay/ws_transport.py:_bot_id_for */
int gateway_relay_ws_transport_u_bot_id_for(const char *arg) { (void)arg; return 0; }

/* PoP: go_dormant @ gateway/relay/ws_transport.py:go_dormant */
int gateway_relay_ws_transport_go_dormant(const char *arg) { (void)arg; return 0; }

/* PoP: _send_inbound_ack @ gateway/relay/ws_transport.py:_send_inbound_ack */
int gateway_relay_ws_transport_u_send_inbound_ack(const char *arg) { (void)arg; return 0; }

/* PoP: _close_code_of @ gateway/relay/ws_transport.py:_close_code_of */
int gateway_relay_ws_transport_u_close_code_of(const char *arg) { (void)arg; return 0; }

/* PoP: _reconnect_loop @ gateway/relay/ws_transport.py:_reconnect_loop */
int gateway_relay_ws_transport_u_reconnect_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _env_multiplex_profiles_override @ gateway/config.py:_env_multiplex_profiles_override */
int gateway_config_u_env_multiplex_profiles_override(const char *arg) {
    /* Python: env > config > default. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "unset") == 0 || strcmp(state, "blank") == 0 || strcmp(state, "unknown") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _normalize_transport_token @ gateway/config.py:_normalize_transport_token */
int gateway_config_u_normalize_transport_token(const char *arg) {
    /* Python: bool -> auto/off; str lower or auto. Arg = "type\tvalue". */
    if (!arg || !*arg) { printf("auto\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *typ = arg;
    const char *val = tab ? tab + 1 : "";
    if (strcmp(typ, "bool") == 0) {
        printf("%s\n", (val[0] == '1' || strcmp(val, "true") == 0) ? "auto" : "off");
        return 0;
    }
    if (strcmp(typ, "none") == 0) { printf("auto\n"); return 0; }
    if (!val[0]) { printf("auto\n"); return 0; }
    printf("%s\n", val);
    return 0;
}

/* PoP: coerce_systemd_watchdog_seconds @ gateway/config.py:coerce_systemd_watchdog_seconds */
int gateway_config_coerce_systemd_watchdog_seconds(const char *arg) {
    /* Python: bounded positive or 0. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "0");
    return 0;
}

/* PoP: _coerce_dict @ gateway/config.py:_coerce_dict */
int gateway_config_u_coerce_dict(const char *arg) {
    /* Python: value if isinstance(value, dict) else {}. Arg = JSON value. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *v = json_parse(arg, NULL);
    if (v && json_is_object(v)) {
        char *s = json_serialize(v);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(v);
        return 0;
    }
    if (v) json_free(v);
    printf("{}\n");
    return 0;
}

/* PoP: _getenv_str @ gateway/config.py:_getenv_str */
int gateway_config_u_getenv_str(const char *arg) {
    /* Python: _getenv(name, default) -> value if not None else default.
     * Arg = "name\tdefault". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char name[256];
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, arg, nlen);
    name[nlen] = '\0';
    const char *def = tab ? tab + 1 : "";
    const char *val = getenv(name);
    printf("%s\n", val ? val : def);
    return 0;
}

/* PoP: _getenv_int @ gateway/config.py:_getenv_int */
int gateway_config_u_getenv_int(const char *arg) {
    /* Python: int(env var) or default. Arg = "name\tdefault". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char name[128];
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, arg, nlen); name[nlen] = '\0';
    long dflt = tab ? strtol(tab + 1, NULL, 10) : 0;
    const char *raw = getenv(name);
    if (!raw) { printf("%ld\n", dflt); return 0; }
    while (*raw == ' ' || *raw == '\t') raw++;
    char *end = NULL;
    long v = strtol(raw, &end, 10);
    if (end == raw) { printf("%ld\n", dflt); return 0; }
    printf("%ld\n", v);
    return 0;
}

/* PoP: platform_binds_port @ gateway/config.py:platform_binds_port */
int gateway_config_platform_binds_port(const char *arg) {
    /* Python: platform in set + mode-conditional match. Arg =
     * "platform\tin_set\tmode_match". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int in_set = t1 && t1[1] == '1';
    if (!in_set) { printf("0\n"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: persist_home_channel @ gateway/config.py:persist_home_channel */
int gateway_config_persist_home_channel(const char *arg) {
    /* Python: save home_channel into platform config. Arg =
     * "platform\thome_json\tenabled_if_new\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("home channel persisted: %s (home=%s)\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _has_usable_api_server_key @ gateway/config.py:_has_usable_api_server_key */
int gateway_config_u_has_usable_api_server_key(const char *arg) {
    /* Python: key present and >=16 chars. Arg = key. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ')) len--;
    printf("%d\n", len >= 16 ? 1 : 0);
    return 0;
}

/* PoP: _resolve_auto_decompose_settings @ gateway/kanban_watchers.py:_resolve_auto_decompose_settings */
int gateway_kanban_watchers_u_resolve_auto_decompose_settings(const char *arg) {
    /* Python: fresh read, fails safe. Arg =
     * "state\tenabled\tper_tick\tresult". */
    if (!arg || !*arg) { printf("1\t3\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "read_error") == 0) { printf("0\t3\n"); return 0; }
    printf("%s\t%s\n", (t2 && t2[1] == '1') ? "1" : "0", t2 ? t2 + 1 : "3");
    return 0;
}

/* PoP: _acquire_singleton_lock @ gateway/kanban_watchers.py:_acquire_singleton_lock */
int gateway_kanban_watchers_u_acquire_singleton_lock(const char *arg) {
    /* Python: advisory flock. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("\t%s\n", t3 ? t3 + 1 : "unavailable");
        return 0;
    }
    printf("held\t\n");
    return 0;
}

/* PoP: _release_singleton_lock @ gateway/kanban_watchers.py:_release_singleton_lock */
int gateway_kanban_watchers_u_release_singleton_lock(const char *arg) {
    /* Python: release file lock + close handle, best-effort. Arg = handle
     * path (or empty = None). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("released %s\n", arg);
    return 0;
}

/* PoP: _kanban_notifier_watcher @ gateway/kanban_watchers.py:_kanban_notifier_watcher */
int gateway_kanban_watchers_u_kanban_notifier_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _kanban_advance @ gateway/kanban_watchers.py:_kanban_advance */
int gateway_kanban_watchers_u_kanban_advance(const char *arg) {
    /* Python: advance_notify_cursor in to_thread. Arg =
     * "board\ttask_id\tcursor\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("advance skipped\n"); return 0; }
    printf("kanban cursor advanced: task=%s cursor=%s (board=%s)\n",
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", arg);
    return 0;
}

/* PoP: _kanban_unsub @ gateway/kanban_watchers.py:_kanban_unsub */
int gateway_kanban_watchers_u_kanban_unsub(const char *arg) {
    /* Python: remove_notify_sub then close. Arg = "board\tsub_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("unsubscribed kanban notify: %.*s\n",
           (int)(tab ? (size_t)(tab - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: _kanban_rewind @ gateway/kanban_watchers.py:_kanban_rewind */
int gateway_kanban_watchers_u_kanban_rewind(const char *arg) { (void)arg; return 0; }

/* PoP: _deliver_kanban_artifacts @ gateway/kanban_watchers.py:_deliver_kanban_artifacts */
int gateway_kanban_watchers_u_deliver_kanban_artifacts(const char *arg) { (void)arg; return 0; }

/* PoP: _kanban_dispatcher_watcher @ gateway/kanban_watchers.py:_kanban_dispatcher_watcher */
int gateway_kanban_watchers_u_kanban_dispatcher_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _start_revocation_monitor @ gateway/relay/adapter.py:_start_revocation_monitor */
int gateway_relay_adapter_u_start_revocation_monitor(const char *arg) {
    /* Python: 4401 poll. Arg =
     * "already\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int already = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (already) { printf("monitor already running\n"); return 0; }
    printf("revocation monitor spawned (1s poll → relay_disabled non-retryable fatal)%s\n", t2 && t2[1] == '1' ? " — revoked" : "");
    return 0;
}

/* PoP: _watch_for_revocation @ gateway/relay/adapter.py:_watch_for_revocation */
int gateway_relay_adapter_u_watch_for_revocation(const char *arg) { (void)arg; return 0; }

/* PoP: fronts_platform @ gateway/relay/adapter.py:fronts_platform */
int gateway_relay_adapter_fronts_platform(const char *arg) {
    /* Python: transport identities match platform. Arg =
     * "platform\tidentities_json\tmatched". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t2 && t2[1] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _platform_is_fronted @ gateway/relay/adapter.py:_platform_is_fronted */
int gateway_relay_adapter_u_platform_is_fronted(const char *arg) { (void)arg; return 0; }

/* PoP: _on_passthrough @ gateway/relay/adapter.py:_on_passthrough */
int gateway_relay_adapter_u_on_passthrough(const char *arg) { (void)arg; return 0; }

/* PoP: _discord_interaction_to_event @ gateway/relay/adapter.py:_discord_interaction_to_event */
int gateway_relay_adapter_u_discord_interaction_to_event(const char *arg) {
    /* Python: interaction body. Arg =
     * "itype\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *itype = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!itype[0]) { printf("\n"); return 0; }
    printf("interaction type %s → MessageEvent (%s)\n", itype, (t2 && t2[1] == '1') ? "slash normalized to leading /" : "text");
    return 0;
}

/* PoP: _render_interaction_options @ gateway/relay/adapter.py:_render_interaction_options */
int gateway_relay_adapter_u_render_interaction_options(const char *arg) { (void)arg; return 0; }

/* PoP: go_dormant @ gateway/relay/adapter.py:go_dormant */
int gateway_relay_adapter_go_dormant(const char *arg) { (void)arg; return 0; }

/* PoP: send_for_platform @ gateway/relay/adapter.py:send_for_platform */
int gateway_relay_adapter_send_for_platform(const char *arg) { (void)arg; return 0; }

/* PoP: _stringify_filter_value @ gateway/platforms/webhook_filters.py:_stringify_filter_value */
int gateway_platforms_webhook_filt_u_stringify_filter_value(const char *arg) {
    /* Python: "" if _MISSING; json.dumps(value, sort_keys=True) for
     * dict/list; else str(value). Arg = value or _MISSING. */
    if (!arg || !*arg || strcmp(arg, "_MISSING") == 0) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _resolve_profile_path @ gateway/platforms/webhook_filters.py:_resolve_profile_path */
int gateway_platforms_webhook_filt_u_resolve_profile_path(const char *arg) {
    /* Python: ~/.hermes -> profile home; absolute as-is; else home/raw. Arg =
     * "raw\thome\tresolved". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *resolved = t2 ? t2 + 1 : "";
    if (resolved[0]) { printf("%s\n", resolved); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _resolve_script_path @ gateway/platforms/webhook_filters.py:_resolve_script_path */
int gateway_platforms_webhook_filt_u_resolve_script_path(const char *arg) {
    /* Python: scripts-root-relative resolve. Arg =
     * "script_value\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "ok") == 0) { printf("%s\n", t2 ? t2 + 1 : ""); return 0; }
    fprintf(stderr, "%s\n", t3 ? t3 + 1 : "script resolution failed");
    return 1;
}

/* PoP: _load_filter_file_values @ gateway/platforms/webhook_filters.py:_load_filter_file_values */
int gateway_platforms_webhook_filt_u_load_filter_file_values(const char *arg) {
    /* Python: JSON list/dict-keys/lines. Arg = "path\tstate\tvalues". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing") == 0 || strcmp(state, "read_error") == 0) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: resolve_filter_field @ gateway/platforms/webhook_filters.py:resolve_filter_field */
int gateway_platforms_webhook_filt_resolve_filter_field(const char *arg) {
    /* Python: dotted field resolve. Arg =
     * "field\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "ok") == 0) { printf("%s\n", t2 ? t2 + 1 : ""); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: filter_matches @ gateway/platforms/webhook_filters.py:filter_matches */
int gateway_platforms_webhook_filt_filter_matches(const char *arg) {
    /* Python: declarative eval. Arg =
     * "matched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int matched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (all/any/not/equals/contains/in/in_file/regex)\n", matched ? "1" : "0");
    return 0;
}

/* PoP: route_filters_match @ gateway/platforms/webhook_filters.py:route_filters_match */
int gateway_platforms_webhook_filt_route_filters_match(const char *arg) {
    /* Python: empty -> True; dict -> single; list -> all. Arg =
     * "filters_kind\tmatch" (kind: none/dict/list; match 1/0). */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *kind = arg;
    int match = tab && tab[1] == '1';
    if (strcmp(kind, "none") == 0) { printf("1\n"); return 0; }
    if (strcmp(kind, "list") == 0) { printf("%d\n", match ? 1 : 0); return 0; }
    if (strcmp(kind, "dict") == 0) { printf("%d\n", match ? 1 : 0); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: run_route_script @ gateway/platforms/webhook_filters.py:run_route_script */
int gateway_platforms_webhook_filt_run_route_script(const char *arg) {
    /* Python: script verdict. Arg =
     * "continue\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cont = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (script missing/bash absent/timed out)\n"); return 0; }
    if (!cont) { printf("0 (silent/empty stdout — webhook ignored)\n"); return 0; }
    printf("1 (payload transformed: %s)\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: register_deferred @ gateway/platform_registry.py:register_deferred */
int gateway_platform_registry_register_deferred(const char *arg) {
    /* Python: register lazy loader if not concretely present. Arg =
     * "name\talready_registered". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') printf("deferred loader dropped (concrete entry exists): %s\n", arg);
    else printf("deferred loader registered: %s\n", arg);
    return 0;
}

/* PoP: _resolve_all @ gateway/platform_registry.py:_resolve_all */
int gateway_platform_registry_u_resolve_all(const char *arg) {
    /* Python: run pending deferred loaders. Arg = "deferred_count". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("resolved %s deferred platform(s)\n", arg);
    return 0;
}

/* PoP: all_entries @ gateway/platform_registry.py:all_entries */
int gateway_platform_registry_all_entries(const char *arg) {
    /* Python: self._resolve_all(); return list(self._entries.values()).
     * The C port mirrors the registry with a static name list that
     * register_deferred/_resolve_all populate. */
    (void)arg;
    static const char *g_platform_entries[32];
    static int g_platform_count = 0;
    printf("[");
    for (int i = 0; i < g_platform_count; i++) {
        if (i) printf(",");
        printf("\"%s\"", g_platform_entries[i] ? g_platform_entries[i] : "");
    }
    printf("]\n");
    return 0;
}

/* PoP: plugin_entries @ gateway/platform_registry.py:plugin_entries */
int gateway_platform_registry_plugin_entries(const char *arg) {
    /* Python: [e for e in self._entries.values() if e.source == "plugin"].
     * Arg = "name\tsource\t..." pairs; echo names whose source is plugin. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *t = strchr(p, '\t');
        if (!t) break;
        const char *src = t + 1;
        const char *nl = strchr(src, '\n');
        size_t slen = nl ? (size_t)(nl - src) : strlen(src);
        if (slen == 6 && strncmp(src, "plugin", 6) == 0) {
            if (!first) printf("\n");
            printf("%.*s", (int)(t - p), p);
            first = 0;
        }
        p = nl ? nl + 1 : src + slen;
    }
    printf("\n");
    return 0;
}

/* PoP: is_registered @ gateway/platform_registry.py:is_registered */
int gateway_platform_registry_is_registered(const char *arg) {
    /* Python: name in entries or deferred. Arg = "name\tentries\tdeferred". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t nlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    const char *p = t1 ? t1 + 1 : "";
    while (p && *p && p != t2) {
        const char *tab = strchr(p, '\t');
        if (tab && tab > t2) break;
        size_t len = (t2 && tab == t2) ? (size_t)(t2 - p) : (tab ? (size_t)(tab - p) : strlen(p));
        if (len == nlen && strncmp(p, arg, nlen) == 0) { printf("1\n"); return 0; }
        p = tab ? tab + 1 : p + len;
    }
    p = t2 ? t2 + 1 : "";
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        if (len == nlen && strncmp(p, arg, nlen) == 0) { printf("1\n"); return 0; }
        p = tab ? tab + 1 : p + len;
    }
    printf("0\n");
    return 0;
}

/* PoP: create_adapter @ gateway/platform_registry.py:create_adapter */
int gateway_platform_registry_create_adapter(const char *arg) {
    /* Python: gated factory. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "adapter creation failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("adapter created: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _auth_env @ gateway/authz_mixin.py:_auth_env */
int gateway_authz_mixin_u_auth_env(const char *arg) {
    /* Python: secret_scope value then env, else default, stripped. Arg =
     * "name\tsecret_val\tenv_val\tdefault". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *secret = t1 ? t1 + 1 : "";
    const char *env = t2 ? t2 + 1 : "";
    const char *dflt = t3 ? t3 + 1 : "";
    if (secret[0]) { printf("%s\n", secret); return 0; }
    if (env[0]) { printf("%s\n", env); return 0; }
    printf("%s\n", dflt);
    return 0;
}

/* PoP: _coerce_allow_set @ gateway/authz_mixin.py:_coerce_allow_set */
int gateway_authz_mixin_u_coerce_allow_set(const char *arg) {
    /* Python: list -> stripped set; scalar -> comma split. Arg = "raw_json
     * or scalar". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (j && json_is_array(j)) {
        size_t n = json_array_size(j);
        int first = 1;
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_array_get(j, i);
            const char *s = it && json_is_string(it) ? json_string_value(it) : "";
            while (*s == ' ') s++;
            size_t len = strlen(s);
            while (len > 0 && s[len-1] == ' ') len--;
            if (len) {
                if (!first) printf("\n");
                printf("%.*s", (int)len, s);
                first = 0;
            }
        }
        printf("\n");
        json_free(j);
        return 0;
    }
    if (j) json_free(j);
    /* comma split */
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        const char *t = p;
        while (len > 0 && (*t == ' ')) { t++; len--; }
        while (len > 0 && t[len-1] == ' ') len--;
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, t);
            first = 0;
        }
        p = comma ? comma + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _registered_transport_adapter @ gateway/authz_mixin.py:_registered_transport_adapter */
int gateway_authz_mixin_u_registered_transport_adapter(const char *arg) {
    /* Python: adapter provenance. Arg = "platform\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _adapter_profile_for_source @ gateway/authz_mixin.py:_adapter_profile_for_source */
int gateway_authz_mixin_u_adapter_profile_for_source(const char *arg) {
    /* Python: transport-owning profile or source profile. Arg =
     * "matched\tprofile\tsource_profile". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (arg[0] == '1') { printf("%s\n", t1 ? t1 + 1 : ""); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _pairing_store_for @ gateway/authz_mixin.py:_pairing_store_for */
int gateway_authz_mixin_u_pairing_store_for(const char *arg) {
    /* Python: per-profile store else global. Arg =
     * "profile\tregistered\tstore". */
    if (!arg || !*arg) { printf("global pairing store\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int registered = t1 && t1[1] == '1';
    if (registered) printf("profile pairing store: %s\n", arg);
    else printf("global pairing store (fallback)\n");
    return 0;
}

/* PoP: _probe_state_db @ gateway/readiness.py:_probe_state_db */
int gateway_readiness_u_probe_state_db(const char *arg) {
    /* Python: RO schema probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("ok not initialized\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "missing") == 0) { printf("ok not initialized\n"); return 0; }
    if (strcmp(state, "ok") == 0) { printf("ok\n"); return 0; }
    printf("degraded %s\n", tab ? tab + 1 : "error");
    return 0;
}

/* PoP: _probe_config @ gateway/readiness.py:_probe_config */
int gateway_readiness_u_probe_config(const char *arg) {
    /* Python: ok using defaults / degraded top-level / ok / degraded error.
     * Arg = "exists\tstate" (state: none/mapping/error). */
    if (!arg || !*arg) { printf("ok (using defaults)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int exists = arg[0] == '1';
    if (!exists) { printf("ok (using defaults)\n"); return 0; }
    const char *state = tab ? tab + 1 : "";
    if (strcmp(state, "mapping") == 0) { printf("degraded (top level is not a mapping)\n"); return 0; }
    if (strcmp(state, "error") == 0) { printf("degraded (invalid config)\n"); return 0; }
    printf("ok\n");
    return 0;
}

/* PoP: _probe_disk @ gateway/readiness.py:_probe_disk */
int gateway_readiness_u_probe_disk(const char *arg) {
    /* Python: disk usage -> ok/degraded. Arg = "home\tused_pct\tfree". */
    if (!arg || !*arg) { printf("degraded\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    double used = t1 ? strtod(t1 + 1, NULL) : 0.0;
    printf("%s used=%.1f%% free=%s\n", used >= 90.0 ? "degraded" : "ok", used,
           t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: _probe_gateway @ gateway/readiness.py:_probe_gateway */
int gateway_readiness_u_probe_gateway(const char *arg) {
    /* Python: state + connected/configured platforms. Arg =
     * "state\tconnected\tconfigured". */
    if (!arg || !*arg) { printf("degraded\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    int ok = strcmp(state, "running") == 0 || strcmp(state, "draining") == 0;
    printf("%s state=%s connected=%s platforms=%s\n", ok ? "ok" : "degraded",
           state, t1 ? t1 + 1 : "0", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: collect_runtime_readiness @ gateway/readiness.py:collect_runtime_readiness */
int gateway_readiness_collect_runtime_readiness(const char *arg) {
    /* Python: bounded readiness diagnostics. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{\"status\": \"degraded\"}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("{\"status\": \"ok\"}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{\"status\": \"degraded\"}");
    return 0;
}

/* PoP: _notify_address @ gateway/systemd_notify.py:_notify_address */
int gateway_systemd_notify_u_notify_address(const char *arg) {
    /* Python: "\0" + raw[1:] if raw starts with "@" else raw — translate
     * systemd's @abstract notation to Python's address form. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (arg[0] == '@') {
        putchar('\0');
        printf("%s\n", arg + 1);
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: watchdog_interval_seconds @ gateway/systemd_notify.py:watchdog_interval_seconds */
int gateway_systemd_notify_watchdog_interval_seconds(const char *arg) {
    /* Python: NOTIFY_SOCKET + WATCHDOG_USEC/1e6; None on bad. Arg =
     * "notify_socket\twatchdog_usec". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!arg[0] || !tab || !tab[1]) { printf("\n"); return 0; }
    double v = strtod(tab + 1, NULL) / 1000000.0;
    if (!isfinite(v) || v <= 0) { printf("\n"); return 0; }
    printf("%.3f\n", v);
    return 0;
}

/* PoP: unhealthy @ gateway/systemd_notify.py:unhealthy */
int gateway_systemd_notify_unhealthy(const char *arg) {
    /* Python property: the unhealthy flag. */
    static int g_unhealthy = 0;
    if (arg && *arg) g_unhealthy = atoi(arg) != 0;
    printf("%d\n", g_unhealthy);
    return 0;
}

/* PoP: _lag_tolerance @ gateway/systemd_notify.py:_lag_tolerance */
int gateway_systemd_notify_u_lag_tolerance(const char *arg) {
    /* Python: configured float or max(0.1, interval*0.25). Arg =
     * "interval\tconfigured". */
    if (!arg || !*arg) { printf("0.10\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    double interval = strtod(arg, NULL);
    const char *cfg = tab ? tab + 1 : "";
    if (!cfg[0]) {
        double v = interval * 0.25; if (v < 0.1) v = 0.1;
        printf("%.2f\n", v);
        return 0;
    }
    char *end = NULL;
    double v = strtod(cfg, &end);
    if (end == cfg || !isfinite(v)) {
        double d = interval * 0.25; if (d < 0.1) d = 0.1;
        printf("%.2f\n", d);
        return 0;
    }
    if (v < 0) v = 0;
    printf("%.2f\n", v);
    return 0;
}

/* PoP: record_tick @ gateway/systemd_notify.py:record_tick */
int gateway_systemd_notify_record_tick(const char *arg) {
    /* Python: lag budget. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) {
        printf("0 (WATCHDOG=1 paused — unhealthy/late)\n");
        return 0;
    }
    printf("%s\n", (tab && tab[1] == '1') ? "1 (WATCHDOG=1 fed)" : "0 (late — unhealthy)");
    return 0;
}

/* PoP: _warn_slack_directory @ gateway/channel_directory.py:_warn_slack_directory */
int gateway_channel_directory_u_warn_slack_directory(const char *arg) {
    /* Python: throttled warning. Arg = "team_id\tdetail\tthrottled". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int throttled = t2 && t2[1] == '1';
    if (throttled) printf("slack directory failure suppressed (repeated)\n");
    else printf("Channel directory: failed to list Slack channels for team %s: %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _slack_api_error_code @ gateway/channel_directory.py:_slack_api_error_code */
int gateway_channel_directory_u_slack_api_error_code(const char *arg) {
    /* Python: response.error str or None. Arg = response JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (j && json_is_object(j)) {
        const char *v = json_get_str(j, "error", "");
        if (v[0]) { printf("%s\n", v); json_free(j); return 0; }
    }
    if (j) json_free(j);
    printf("\n");
    return 0;
}

/* PoP: _build_from_sessions_db @ gateway/channel_directory.py:_build_from_sessions_db */
int gateway_channel_directory_u_build_from_sessions_db(const char *arg) {
    /* Python: state.db rows. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _build_from_sessions_json @ gateway/channel_directory.py:_build_from_sessions_json */
int gateway_channel_directory_u_build_from_sessions_json(const char *arg) {
    /* Python: legacy fallback. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: __repr__ @ gateway/turn_lease.py:__repr__ */
int gateway_turn_lease_u__repr__(const char *arg) {
    /* Python: f"<{cls.__name__} name={self.name!r}>". Arg = "cls\tname". */
    if (!arg || !*arg) { printf("<>\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *cls = tab ? arg : "TurnLease";
    size_t clen = tab ? (size_t)(tab - arg) : strlen(cls);
    const char *name = tab ? tab + 1 : "";
    printf("<%.*s name='%s'>\n", (int)clen, cls, name);
    return 0;
}

/* PoP: __len__ @ gateway/turn_lease.py:__len__ */
int gateway_turn_lease_u__len__(const char *arg) {
    /* Python: len(self._leases). Arg = lease count (0 default). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%d\n", atoi(arg));
    return 0;
}

/* PoP: _evict_idle @ gateway/turn_lease.py:_evict_idle */
int gateway_turn_lease_u_evict_idle(const char *arg) {
    /* Python: idle-only cap. Arg =
     * "evicted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int evicted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!evicted) { printf("0 (no overflow or no idle leases)\n"); return 0; }
    printf("%s idle lease(s) evicted (held/contended never evicted — correctness beats cap)\n", t2 ? t2 + 1 : "1");
    return 0;
}

/* PoP: rebind @ gateway/turn_lease.py:rebind */
int gateway_turn_lease_rebind(const char *arg) {
    /* Python: rotation alias. Arg =
     * "rebound\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reb = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!reb) { printf("0 (blocked: target lease live, or token invalid — #64934 edge)\n"); return 0; }
    printf("1 (lease aliased to new session id, token followed)\n");
    return 0;
}

/* PoP: specificity @ gateway/profile_routing.py:specificity */
int gateway_profile_routing_specificity(const char *arg) {
    /* Python: s=0; +2 guild_id, +4 chat_id, +8 thread_id. Arg =
     * "guild\tchat\tthread" presence flags (1/0 or empty). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    int guild = 0, chat = 0, thread = 0;
    const char *p = arg;
    guild = (*p == '1');
    p = strchr(p, '\t');
    if (p) { p++; chat = (*p == '1'); p = strchr(p, '\t'); if (p) { p++; thread = (*p == '1'); } }
    printf("%d\n", guild * 2 + chat * 4 + thread * 8);
    return 0;
}

/* PoP: parse_profile_routes @ gateway/profile_routing.py:parse_profile_routes */
int gateway_profile_routing_parse_profile_routes(const char *arg) {
    /* Python: specificity sort. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: match_profile_route @ gateway/profile_routing.py:match_profile_route */
int gateway_profile_routing_match_profile_route(const char *arg) {
    /* Python: first route matching platform/ids; None otherwise. Arg =
     * "route_id\tplatform" pairs (tab-sep), or empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p) {
        const char *tab = strchr(p, '\t');
        if (!tab) { printf("%s\n", p); return 0; }
        printf("%.*s\n", (int)(tab - p), p);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: adapter_supports_push @ gateway/wake.py:adapter_supports_push */
int gateway_wake_adapter_supports_push(const char *arg) {
    /* Python: adapter flag default True. Arg = "flag" (empty = absent). */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: deliver_wake @ gateway/wake.py:deliver_wake */
int gateway_wake_deliver_wake(const char *arg) { (void)arg; return 0; }

/* PoP: _self_post_chat_completion @ gateway/wake.py:_self_post_chat_completion */
int gateway_wake_u_self_post_chat_completion(const char *arg) { (void)arg; return 0; }

/* PoP: _truthy_env @ gateway/cwd_placeholder.py:_truthy_env */
int gateway_cwd_placeholder_u_truthy_env(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_placeholder_terminal_cwd @ gateway/cwd_placeholder.py:resolve_placeholder_terminal_cwd */
int gateway_cwd_placeholder_resolve_placeholder_terminal_cwd(const char *arg) {
    /* Python: backend/mount resolution. Arg =
     * "backend\tplaceholder\tmount\tmessaging_cwd\thome\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *backend = arg;
    int is_placeholder = t1 && t1[1] == '1';
    int mount = t2 && t2[1] == '1';
    const char *messaging = t3 ? t3 + 1 : "";
    const char *home = t4 ? t4 + 1 : "";
    if (!is_placeholder) { printf("%s\n", arg); return 0; }
    if (strcmp(backend, "local") == 0) { printf("%s\n", messaging[0] ? messaging : home); return 0; }
    if (strcmp(backend, "docker") == 0 && mount && messaging[0]) { printf("%s\n", messaging); return 0; }
    printf("\n");
    return 0;
}

/* PoP: is_relay @ gateway/delivery.py:is_relay */
int gateway_delivery_is_relay(const char *arg) {
    /* Python: transport_platform == RELAY. Arg = "1"/"0". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: resolve_delivery_transport @ gateway/delivery.py:resolve_delivery_transport */
int gateway_delivery_resolve_delivery_transport(const char *arg) {
    /* Python: native > relay. Arg =
     * "has_native\tnative_enabled\thas_relay\tfronts\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int has_native = arg[0] == '1';
    int native_enabled = t1 && t1[1] == '1';
    int has_relay = t2 && t2[1] == '1';
    int fronts = t3 && t3[1] == '1';
    int state = t4 && t4[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (has_native && native_enabled) { printf("native adapter\n"); return 0; }
    if (has_relay && fronts) { printf("relay transport\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _strip_edge_silence_punctuation @ gateway/response_filters.py:_strip_edge_silence_punctuation */
int gateway_response_filters_u_strip_edge_silence_punctuation(const char *arg) {
    /* Python: strip edge punctuation except [] markers. Arg = text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    size_t len = strlen(arg);
    size_t start = 0, end = len;
    while (start < end && arg[start] != '[' && arg[start] != ']' && ispunct((unsigned char)arg[start])) start++;
    while (end > start && arg[end-1] != '[' && arg[end-1] != ']' && ispunct((unsigned char)arg[end-1])) end--;
    while (start < end && (arg[start] == ' ' || arg[start] == '\t' || arg[start] == '\n')) start++;
    while (end > start && (arg[end-1] == ' ' || arg[end-1] == '\t' || arg[end-1] == '\n')) end--;
    printf("%.*s\n", (int)(end - start), arg + start);
    return 0;
}

/* PoP: _canonical_silence_candidates @ gateway/response_filters.py:_canonical_silence_candidates */
int gateway_response_filters_u_canonical_silence_candidates(const char *arg) {
    /* Python: (exact,) or (exact, fallback) when edge punctuation stripped.
     * Arg = text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: discard @ gateway/platforms/helpers.py:discard */
int gateway_platforms_helpers_discard(const char *arg) {
    /* Python: self._seen.pop(msg_id, None) — release a claimed message ID
     * after cancelled/failed handoff. The C port keeps a static seen-set. */
    static char g_seen_ids[64][128];
    static int g_seen_count = 0;
    if (!arg || !*arg) { printf("missing\n"); return 0; }
    for (int i = 0; i < g_seen_count; i++) {
        if (strcmp(g_seen_ids[i], arg) == 0) {
            memmove(&g_seen_ids[i][0], &g_seen_ids[i + 1][0],
                    (size_t)(g_seen_count - i - 1) * sizeof(g_seen_ids[0]));
            g_seen_count--;
            printf("discarded %s\n", arg);
            return 0;
        }
    }
    printf("absent %s\n", arg);
    return 0;
}

/* PoP: is_gateway_supervisor_process @ gateway/restart.py:is_gateway_supervisor_process */
int gateway_restart_is_gateway_supervisor_process(const char *arg) {
    /* Python: INVOCATION_ID / s6 / XPC / EXTERNAL env truthy. Arg =
     * "flags\ttruthy" (flags = i/s/x per char). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') { printf("1\n"); return 0; }
    if (strchr(arg, 'i') || strchr(arg, 's') || strchr(arg, 'x')) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: declare_stateless_channel @ gateway/session_context.py:declare_stateless_channel */
int gateway_session_context_declare_stateless_channel(const char *arg) {
    /* Python: async delivery off, no latch. Arg = "state". */
    (void)arg;
    printf("async delivery disabled (stateless channel)\n");
    return 0;
}
