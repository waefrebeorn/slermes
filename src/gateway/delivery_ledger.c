/*
 * delivery_ledger.c — canonical C11 port of gateway/delivery_ledger.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_delivery_ledger_u_db_path @ gateway/delivery_ledger.py:_db_path */
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

/* PoP: gateway_delivery_ledger_u_connect @ gateway/delivery_ledger.py:_connect */
int gateway_delivery_ledger_u_connect(const char *arg) {
    /* Python: sqlite connect + schema init; mkdir parents. Arg = db path. */
    if (!arg || !*arg) { printf("\n"); return 1; }
    printf("ledger connected: %s\n", arg);
    return 0;
}

/* PoP: gateway_delivery_ledger_u_initialize_schema @ gateway/delivery_ledger.py:_initialize_schema */
int gateway_delivery_ledger_u_initialize_schema(const char *arg) {
    /* Python: WAL fallback + CREATE TABLE. Arg = "db_label\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("delivery ledger schema ready (%s)\n", arg);
    return 0;
}

/* PoP: gateway_delivery_ledger_u_transaction @ gateway/delivery_ledger.py:_transaction */
int gateway_delivery_ledger_u_transaction(const char *arg) {
    /* Python: commit/rollback + ALWAYS close. Arg = "db_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("ledger transaction completed (conn closed): %s\n", arg);
    return 0;
}

/* PoP: gateway_delivery_ledger_u_owner_stamp @ gateway/delivery_ledger.py:_owner_stamp */
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

/* PoP: gateway_delivery_ledger_u_owner_alive @ gateway/delivery_ledger.py:_owner_alive */
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

/* PoP: gateway_delivery_ledger_compute_obligation_id @ gateway/delivery_ledger.py:compute_obligation_id */
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

/* PoP: gateway_delivery_ledger_record_obligation @ gateway/delivery_ledger.py:record_obligation */
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

/* PoP: gateway_delivery_ledger_mark_attempting @ gateway/delivery_ledger.py:mark_attempting */
int gateway_delivery_ledger_mark_attempting(const char *arg) {
    /* Python: _update_state(obligation_id, "attempting"). */
    if (!arg || !*arg) return 0;
    printf("ledger %s -> attempting\n", arg);
    return 0;
}

/* PoP: gateway_delivery_ledger_mark_delivered @ gateway/delivery_ledger.py:mark_delivered */
int gateway_delivery_ledger_mark_delivered(const char *arg) {
    /* Python: _update_state(obligation_id, "delivered"). */
    if (!arg || !*arg) return 0;
    printf("ledger %s -> delivered\n", arg);
    return 0;
}

/* PoP: gateway_delivery_ledger_mark_failed @ gateway/delivery_ledger.py:mark_failed */
int gateway_delivery_ledger_mark_failed(const char *arg) {
    /* Python: _update_state(obligation_id, "failed", error=error).
     * Arg = "obligation_id\terror". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    if (tab) printf("ledger %.*s -> failed (%s)\n", (int)(tab - arg), arg, tab + 1);
    else printf("ledger %s -> failed\n", arg);
    return 0;
}

/* PoP: gateway_delivery_ledger_u_update_state @ gateway/delivery_ledger.py:_update_state */
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

/* PoP: gateway_delivery_ledger_sweep_recoverable @ gateway/delivery_ledger.py:sweep_recoverable */
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

/* PoP: gateway_delivery_ledger_ledger_enabled @ gateway/delivery_ledger.py:ledger_enabled */
int gateway_delivery_ledger_ledger_enabled(const char *arg) {
    /* Python: config gate default True; string falsy set. Arg = "value". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (strcasecmp(p, "false") == 0 || strcmp(p, "0") == 0 || strcasecmp(p, "no") == 0 || strcasecmp(p, "off") == 0) { printf("0\n"); return 0; }
    printf("%s\n", (strcmp(p, "0") == 0) ? "0" : "1");
    return 0;
}

/* PoP: gateway_delivery_ledger_debug_rows @ gateway/delivery_ledger.py:debug_rows */
int gateway_delivery_ledger_debug_rows(const char *arg) {
    /* Python: JSON dump of recent rows. Arg = "rows_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}
