#ifndef NOC_CREDSTAMP_H
#define NOC_CREDSTAMP_H
/*
 * Public C API for the port of agent/credential_pool_timestamps.py (gate credtstamp).
 * All exported symbols are module-prefixed to avoid collisions.
 */
#include "hermes_json.h"

/*
 * Best-effort parse of epoch seconds / epoch milliseconds / ISO-8601 into
 * epoch seconds.  Returns 1 and writes *out_seconds on success; returns 0
 * for None, empty, invalid or unsupported values (Python returns None).
 * Naive ISO datetimes use the host local timezone (Python .timestamp()).
 */
int credtstamp_parse_absolute_timestamp(const json_t *value, double *out_seconds);

#endif /* NOC_CREDSTAMP_H */
