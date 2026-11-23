#ifndef NOC_STORLIM_H
#define NOC_STORLIM_H
/*
 * Public C API for the port of storage_limits.py (gate storlim).
 * All exported symbols carry the storlim_ prefix.
 */
#include <stdint.h>
#include "hermes_json.h"
/* Python _MAX_SAFE_MESSAGES = 20_000 */
#define STORLIM_MAX_SAFE_MESSAGES 20000

/* sessions is config["sessions"] (a JSON object, null, or absent). Keys are
 * looked up exactly as Python's (load_config_readonly().get("sessions") or {}).get(key).
 * Returns fallback on a missing/None value, a negative parsed limit, or any
 * conversion error (Python's except Exception path). */
int64_t storlim_configured_transcript_limit(const char *key, const json_t *sessions,
                                            int64_t fallback);
int64_t storlim_resolved_max_resume_messages(const json_t *sessions);
int64_t storlim_resolved_max_export_messages(const json_t *sessions);

#endif /* NOC_STORLIM_H */
