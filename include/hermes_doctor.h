#ifndef HERMES_DOCTOR_H
#define HERMES_DOCTOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run doctor diagnostics.
 * Returns number of issues found.
 */
int doctor_run(bool fix_mode);

/**
 * Acknowledge a security advisory.
 */
int doctor_acknowledge_advisory(const char *advisory_id);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DOCTOR_H */
