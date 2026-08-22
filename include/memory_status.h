/*
 * memory_status.h — C11 port of gateway/memory_status.py (NS-656).
 *
 * Memory status rollup for /api/status: distills the already-persisted
 * gateway heartbeat + lifecycle sentinel into a compact public-safe block.
 * Best-effort and read-only; never raises.
 */
#ifndef GATEWAY_MEMORY_STATUS_H
#define GATEWAY_MEMORY_STATUS_H

#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _mb @ gateway/memory_status.py:_mb */
/* kib//1024 for a non-negative int (bools/floats/negatives rejected). */
int _mb(const json_t *kib, long long *out);

/* PoP: _parse_iso @ gateway/memory_status.py:_parse_iso */
/* Parse an ISO-8601 timestamp string; naive values are treated as UTC.
 * On success returns 1 and fills *epoch_s / *usec. */
int _parse_iso(const char *value, long long *epoch_s, int *usec);

/* PoP: classify_pressure @ gateway/memory_status.py:classify_pressure */
/* Map MemAvailable/MemTotal to ok/elevated/critical; unknown when malformed. */
const char *classify_pressure(const json_t *available_kib, const json_t *total_kib);

/* PoP: _read_heartbeat @ gateway/memory_status.py:_read_heartbeat */
json_t *_read_heartbeat(const char *home);

/* PoP: _read_sentinel @ gateway/memory_status.py:_read_sentinel */
json_t *_read_sentinel(const char *home);

/* PoP: collect_memory_status @ gateway/memory_status.py:collect_memory_status */
/* Build the "memory" block. home==NULL => process profile home;
 * now_epoch_s==0 means "use the current time" (Python's injectable `now`).
 * Caller frees with json_free. */
json_t *collect_memory_status(const char *home, double now_epoch_s);

#endif /* GATEWAY_MEMORY_STATUS_H */
