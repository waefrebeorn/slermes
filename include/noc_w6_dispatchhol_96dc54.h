#ifndef NOC_W6_DISPATCHHOL_96DC54_H
#define NOC_W6_DISPATCHHOL_96DC54_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C port of cron/dispatch_hold.py (gate w6_dispatchhol_96dc54).
 * `content` is the raw text of <cron dir>/dispatch_held.json (the utf-8-sig
 * BOM, if any, is ignored). JSON null == Python None. */

/* PoP: w6_dispatchhol_96dc54_read @ cron/dispatch_hold.py:read
 * Returns a NEW object {"since":..., "last":...} (both doubles), or NULL
 * when the text is missing/unreadable/malformed. */
struct json_t *w6_dispatchhol_96dc54_read(const char *content);

/* PoP: w6_dispatchhol_96dc54_grace_starts_at @ cron/dispatch_hold.py:grace_starts_at
 * Explicit seam: `held` is the dict returned by read() (NULL == None). */
double w6_dispatchhol_96dc54_grace_starts_at_ex(const struct json_t *held, double due_at, double grace);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_DISPATCHHOL_96DC54_H */
