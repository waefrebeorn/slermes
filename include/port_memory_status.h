#ifndef PORT_MEMORY_STATUS_H
#define PORT_MEMORY_STATUS_H

#include "hermes_json.h"

/* Port of Python: gateway/memory_status.py — the /api/status memory rollup.
 *
 * The pure read-side core is ported: value coercion (_nonneg_int, _mb),
 * ISO-8601 parsing with UTC normalisation (_parse_iso), the pressure
 * classifier, and the status-dict assembly. `_read_state_files` (two file
 * reads to heartbeat/sentinel JSON) is runtime disk I/O and is NOT ported:
 * ms_collect_memory_status takes those two dicts as JSON inputs plus `now`,
 * mirroring the Python function with its state injectable.
 * libjson stores numbers as doubles, so an integral float (e.g. `1.0`) is
 * accepted where Python's `isinstance(k,int)` would reject it — real
 * heartbeat writers emit integers, so this cannot arise from a live file.
 */

#define MS_CRITICAL_AVAILABLE_KIB (64 * 1024)
#define MS_CRITICAL_AVAILABLE_FRACTION 0.05
#define MS_ELEVATED_AVAILABLE_KIB (128 * 1024)
#define MS_ELEVATED_AVAILABLE_FRACTION 0.15
#define MS_HEARTBEAT_FRESH_TTL_S 150.0

/* Python _nonneg_int: the value if it is a real (non-bool) int >= 0 else
 * None. Returns -1 for None/invalid. */
long long ms_nonneg_int(const json_t *value);

/* Python _mb: None when invalid, else kib // 1024. Returns -1 for None. */
long long ms_mb(const json_t *kib);

/* Python _parse_iso then convert to epoch-seconds-UTC. Returns -1 when absent
 * or unparsable; naive input is normalised to UTC (Python replace(tzinfo=UTC)). */
double ms_parse_iso_epoch(const char *value);

/* Python classify_pressure: "ok"/"elevated"/"critical"/"unknown". malloc'd. */
char *ms_classify_pressure(const json_t *available_kib, const json_t *total_kib);

/* Python collect_memory_status with _read_state_files(home) pre-resolved:
 * heartbeat and sentinel are the two decoded dicts (may be NULL or null for a
 * missing file), now is epoch-seconds-UTC (Python datetime.now(UTC)), returned
 * as a JSON object. Malloc'd. */
json_t *ms_collect_memory_status(const char *heartbeat_json,
                                 const char *sentinel_json,
                                 double now_seconds);

#endif /* PORT_MEMORY_STATUS_H */
