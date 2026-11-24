#ifndef NOC_W20_RUNDELIVER_C36151_H
#define NOC_W20_RUNDELIVER_C36151_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/run_delivery_queue_watch.py (gate w20_rundeliver_c36151).
 *
 * A stamp-compare watch over every served profile's cron/deliveries.db and its
 * -wal sibling, plus the housekeeping wait loop that drains the moment a stamp
 * moves.  Filesystem stats and the clock arrive as data so the comparison is
 * the watch's own logic. */

/* PoP: w20_rundeliver_c36151_watch_seconds @ gateway/run_delivery_queue_watch.py:DELIVERY_QUEUE_WATCH_SECONDS */
double w20_rundeliver_c36151_watch_seconds(void);

/* PoP: w20_rundeliver_c36151_drain_label @ gateway/run_delivery_queue_watch.py:DRAIN_LABEL */
const char *w20_rundeliver_c36151_drain_label(void);

/* PoP: w20_rundeliver_c36151_stamp @ gateway/run_delivery_queue_watch.py:DeliveryQueueWatch._stamp
 * homes: [str|null]; stats: [{"path","mtime_ns","size"} | {"path","errno"}];
 * null home reads default_home.  -> [[path, mtime_ns, size], ...] */
json_t *w20_rundeliver_c36151_stamp(const json_t *homes, const json_t *stats,
                                    const char *default_home);

/* PoP: w20_rundeliver_c36151_changed @ gateway/run_delivery_queue_watch.py:DeliveryQueueWatch.changed
 * -> {"changed": bool, "seen": <current triples>} (seen only advances when the
 * stamp moved, exactly like the watch). */
json_t *w20_rundeliver_c36151_changed(const json_t *seen, const json_t *homes,
                                       const json_t *stats, const char *default_home);

/* PoP: w20_rundeliver_c36151_wait_plan @ gateway/run_delivery_queue_watch.py:wait_for_next_tick
 * in = {"interval": num, "watch": null | {"homes","stats","default_home"},
 *       "clock": [num], "stop_results": [bool], "changed_flags": [bool]}
 * -> {"actions": [{"action":"refresh"} | {"action":"wait","timeout":x} |
 *                 {"action":"drain","label":...} |
 *                 {"action":"return","why":...}], "drains": n} */
json_t *w20_rundeliver_c36151_wait_plan(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_RUNDELIVER_C36151_H */
