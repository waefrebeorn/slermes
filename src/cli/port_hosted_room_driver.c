/*
 * port_hosted_room_driver.c
 *
 * Pure helpers ported from gateway/hosted_room_driver.py (57 defs, previously 0
 * ported).
 *
 *   hd_task_update     <- _task_update
 *   hd_generation_update <- _generation_update
 *   hd_finite          <- _finite
 *   hd_timestamp       <- _timestamp
 *   hd_lease_window    <- _lease_window
 *
 * The SQLite schema / transaction / row-mapping half stays in Python.
 */

#include "port_hosted_room_driver.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* gateway/hosted_room_driver.py:_GENERATION_FENCE */
static const char *const HD_GENERATION_FENCE =
    "execution_generation=? AND cancel_generation=?";

/* ===========================================================================
 *  _task_update  (hosted_room_driver.py:_task_update)
 *
 *      return f"UPDATE hosted_room_driver_tasks SET {set_clause} " \
 *             f"WHERE room_id=? AND task_id=? AND {fence}"
 *
 *  Caller frees.
 * =========================================================================== */

/* PoP: _task_update @ gateway/hosted_room_driver.py:_task_update */
char *hd_task_update(const char *set_clause, const char *fence)
{
    size_t cap = strlen(set_clause ? set_clause : "") + strlen(fence ? fence : "") + 96;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap,
             "UPDATE hosted_room_driver_tasks SET %s "
             "WHERE room_id=? AND task_id=? AND %s",
             set_clause ? set_clause : "", fence ? fence : "");
    return out;
}

/* ===========================================================================
 *  _generation_update  (hosted_room_driver.py:_generation_update)
 *
 *      return _task_update(set_clause, f"status='{status}' AND {_GENERATION_FENCE}")
 *
 *  Transition fenced on status AND both generations (terminal settlements and
 *  the recovery family). Caller frees.
 * =========================================================================== */

/* PoP: _generation_update @ gateway/hosted_room_driver.py:_generation_update */
char *hd_generation_update(const char *set_clause, const char *status)
{
    size_t cap = strlen(status ? status : "") + strlen(HD_GENERATION_FENCE) + 32;
    char *fence = (char *)malloc(cap);
    if (!fence) return NULL;
    snprintf(fence, cap, "status='%s' AND %s", status ? status : "", HD_GENERATION_FENCE);

    char *out = hd_task_update(set_clause, fence);
    free(fence);
    return out;
}

/* ===========================================================================
 *  _finite  (hosted_room_driver.py:_finite)
 *
 *      try: value = float(compute())
 *      except (TypeError, ValueError, OverflowError) as exc:
 *          raise DriverValidationError(message) from exc
 *      if not math.isfinite(value) or (positive and value <= 0):
 *          raise DriverValidationError(message)
 *      return value
 *
 *  `compute` returns the candidate as a string; `ok` reports whether the parse
 *  succeeded (the C port has no exceptions). `message` is the validation error,
 *  returned via `err` when the value is rejected.
 * =========================================================================== */

/* PoP: _finite @ gateway/hosted_room_driver.py:_finite */
double hd_finite(const char *candidate, int *ok, int positive,
                 const char *message, char **err)
{
    if (err) *err = NULL;
    *ok = 0;

    char *end = NULL;
    double v = strtod(candidate ? candidate : "", &end);
    if (end == (candidate ? candidate : "")) {
        if (err) *err = strdup(message ? message : "invalid number");
        return 0.0;
    }

    if (!isfinite(v) || (positive && v <= 0)) {
        if (err) *err = strdup(message ? message : "invalid number");
        return 0.0;
    }
    *ok = 1;
    return v;
}

/* ===========================================================================
 *  _timestamp  (hosted_room_driver.py:_timestamp)
 *
 *      if not callable(clock): raise DriverValidationError("clock must be callable")
 *      return _finite(clock, "clock must return a finite number")
 *
 *  `clock` returns the candidate; `is_callable` stands in for the callable
 *  check. Caller frees `err`.
 * =========================================================================== */

/* PoP: _timestamp @ gateway/hosted_room_driver.py:_timestamp */
double hd_timestamp(const char *clock_value, int is_callable, int *ok, char **err)
{
    if (!is_callable) {
        if (err) *err = strdup("clock must be callable");
        *ok = 0;
        return 0.0;
    }
    return hd_finite(clock_value, ok, 0, "clock must return a finite number", err);
}

/* ===========================================================================
 *  _lease_window  (hosted_room_driver.py:_lease_window)
 *
 *      ttl = _finite(lambda: ttl_seconds, "ttl_seconds must be a finite positive number",
 *                    positive=True)
 *      now = _timestamp(clock)
 *      if not math.isfinite(now + ttl):
 *          raise DriverValidationError("lease expiry must be finite")
 *      return now, now + ttl
 *
 *  Validate ttl FIRST, then the clock. The SUM is re-checked for finiteness -
 *  two finite values can still overflow. `now` and `expires_at` are out-params.
 *  Returns 1 on success, 0 when validation failed (err set).
 * =========================================================================== */

/* PoP: _lease_window @ gateway/hosted_room_driver.py:_lease_window */
int hd_lease_window(const char *ttl_seconds, const char *clock_value, int is_callable,
                    double *now_out, double *expires_out, char **err)
{
    if (err) *err = NULL;

    int ok = 0;
    double ttl = hd_finite(ttl_seconds, &ok, 1,
                           "ttl_seconds must be a finite positive number", err);
    if (!ok) return 0;

    double now = hd_timestamp(clock_value, is_callable, &ok, err);
    if (!ok) return 0;

    if (!isfinite(now + ttl)) {
        if (err) *err = strdup("lease expiry must be finite");
        return 0;
    }

    if (now_out) *now_out = now;
    if (expires_out) *expires_out = now + ttl;
    return 1;
}
