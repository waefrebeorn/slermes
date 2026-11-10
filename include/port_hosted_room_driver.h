/*
 * port_hosted_room_driver.h -- pure helpers ported from
 * gateway/hosted_room_driver.py.
 *
 * The SQLite schema / transaction / row-mapping half stays in Python.
 */
#ifndef PORT_HOSTED_ROOM_DRIVER_H
#define PORT_HOSTED_ROOM_DRIVER_H

#include <stddef.h>

/* _task_update -- the fenced task UPDATE statement. Caller frees. */
char *hd_task_update(const char *set_clause, const char *fence);

/* _generation_update -- fenced on status AND both generations. Caller frees. */
char *hd_generation_update(const char *set_clause, const char *status);

/* _finite -- a finite float, optionally required positive. `candidate` is the
 * value as a string; `ok` reports success; `err` carries the validation message
 * (caller frees). */
double hd_finite(const char *candidate, int *ok, int positive,
                 const char *message, char **err);

/* _timestamp -- the clock callable check plus _finite. */
double hd_timestamp(const char *clock_value, int is_callable, int *ok, char **err);

/* _lease_window -- validate ttl then clock; the SUM is re-checked for
 * finiteness. Returns 1 on success, 0 when validation failed (err set). */
int hd_lease_window(const char *ttl_seconds, const char *clock_value,
                    int is_callable, double *now_out, double *expires_out,
                    char **err);

#endif /* PORT_HOSTED_ROOM_DRIVER_H */
