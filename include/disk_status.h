/*
 * disk_status.h — C11 port of gateway/disk_status.py (NS-656).
 *
 * Disk-usage rollup for /api/status: classify free/total MB into
 * ok/elevated/critical/unknown, and build the public "disk" block from a
 * live statvfs sample. Best-effort and read-only; never raises.
 */
#ifndef GATEWAY_DISK_STATUS_H
#define GATEWAY_DISK_STATUS_H

#include <stddef.h>
#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _coerce_mb @ gateway/disk_status.py:_coerce_mb */
/* Python accepts bool-rejecting non-negative int only. Returns 1 and sets
 * *out when the JSON value coerces (integral number, not bool, >= 0). */
int _coerce_mb(const json_t *value, long long *out);

/* PoP: classify_disk_pressure @ gateway/disk_status.py:classify_disk_pressure */
/* Map free/total MB to "ok"/"elevated"/"critical"; "unknown" when missing or
 * malformed. Inputs are the raw JSON values so the Any-typed Python contract
 * (bools/floats/strings rejected) is preserved. Returns a static string. */
const char *classify_disk_pressure(const json_t *free_mb, const json_t *total_mb);

/* Test seam mirroring shutil.disk_usage's (total, free, used) triple — lets
 * the oracle inject deterministic stat values without faking the logic. */
json_t *disk_status_collect_from_usage(unsigned long long total_bytes,
                                       unsigned long long free_bytes,
                                       unsigned long long used_bytes);

/* PoP: collect_disk_status @ gateway/disk_status.py:collect_disk_status */
/* Build the "disk" block for /api/status via statvfs on `home` (NULL =>
 * process HERMES_HOME/HOME fallback). Always returns a dict; an unreadable
 * filesystem yields {"pressure":"unknown", ...}. Caller frees with json_free. */
json_t *collect_disk_status(const char *home);

#endif /* GATEWAY_DISK_STATUS_H */
