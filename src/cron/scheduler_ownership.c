/*
 * scheduler_ownership.c — C home for upstream Hermes cron/scheduler_ownership.py.
 *
 * PoP: w10_schedulero_e0c314_claim_dead @ cron/scheduler_ownership.py:_claim_owner_is_dead
 * PoP: w10_schedulero_e0c314_record_serves @ cron/scheduler_ownership.py:record_serves_profile
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_schedulero_e0c314.h"

static int all_digits(const char *s, size_t n) {
    if (n == 0) return 0;
    for (size_t i = 0; i < n; i++) if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* Mirrors _claim_owner_is_dead:
 *   parts = str(claim.get("by") or "").split(":")
 *   if len(parts) < 2 or not parts[1].isdigit(): return False
 *   if parts[0] != socket.gethostname(): return False
 *   return not _pid_exists(int(parts[1]))   (probe errors -> False)
 * The fixture injects the host name and the pid-liveness set. */
json_t *w10_schedulero_e0c314_claim_dead(const json_t *in) {
    const json_t *by = json_obj_get(in, "by");
    char nbuf[64];
    const char *bys = "";
    if (by && by->type == JSON_STRING && by->str_val) bys = by->str_val;
    else if (by && by->type == JSON_BOOL && by->bool_val) bys = "True";
    else if (by && by->type == JSON_NUMBER && by->num_val != 0.0) {
        snprintf(nbuf, sizeof(nbuf), "%.17g", by->num_val);
        bys = nbuf;
    }
    /* claim.get("by") or "" -> falsy numbers/bools/null collapse to "". */

    const char *colon = strchr(bys, ':');
    if (!colon) return json_bool(0);           /* len(parts) < 2 */
    const char *p2 = colon + 1;
    const char *colon2 = strchr(p2, ':');      /* parts[1] ends at the next ':' */
    size_t p1len = (size_t)(colon - bys);
    size_t p2len = colon2 ? (size_t)(colon2 - p2) : strlen(p2);
    if (!all_digits(p2, p2len)) return json_bool(0);

    const char *host = json_get_str(in, "hostname", "");
    if (strlen(host) != p1len || strncmp(bys, host, p1len) != 0) return json_bool(0);

    char pidbuf[64];
    size_t cp = p2len < sizeof(pidbuf) - 1 ? p2len : sizeof(pidbuf) - 1;
    memcpy(pidbuf, p2, cp);
    pidbuf[cp] = '\0';
    long long pid = strtoll(pidbuf, NULL, 10);

    const json_t *live_pids = json_obj_get(in, "live_pids");
    int alive = 0;
    if (live_pids && live_pids->type == JSON_ARRAY) {
        size_t m = json_len(live_pids);
        for (size_t j = 0; j < m; j++) {
            const json_t *p = json_get(live_pids, j);
            long long v;
            if (p && p->type == JSON_STRING && p->str_val) v = strtoll(p->str_val, NULL, 10);
            else if (p && p->type == JSON_NUMBER) v = (long long)p->num_val;
            else continue;
            if (v == pid) { alive = 1; break; }
        }
    }
    return json_bool(alive ? 0 : 1);           /* not _pid_exists(pid) */
}

/* Mirrors record_serves_profile given injected helper results:
 *   non-dict record -> False
 *   _same_hermes_home(record_home or process_home, target) -> True
 *   served_profiles not a list -> False
 *   label = _profile_label_for_home(target); label is not None and label in served */
json_t *w10_schedulero_e0c314_record_serves(const json_t *in) {
    const json_t *rec = json_obj_get(in, "record");
    if (!rec || rec->type != JSON_OBJECT) return json_bool(0);
    if (json_get_bool(in, "same", 0)) return json_bool(1);
    const json_t *served = json_obj_get(rec, "served_profiles");
    if (!served || served->type != JSON_ARRAY) return json_bool(0);
    const json_t *label = json_obj_get(in, "label");
    if (!label || label->type != JSON_STRING || !label->str_val) return json_bool(0);
    size_t m = json_len(served);
    for (size_t j = 0; j < m; j++) {
        const json_t *e = json_get(served, j);
        if (e && e->type == JSON_STRING && e->str_val &&
            !strcmp(e->str_val, label->str_val)) return json_bool(1);
    }
    return json_bool(0);
}
