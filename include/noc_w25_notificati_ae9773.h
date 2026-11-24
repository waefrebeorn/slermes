/*
 * noc_w25_notificati_ae9773.h - C surface for upstream
 * agent/notification_presentation.py (gate w25_notificati_ae9773).
 *
 * Turn-local presentation of diagnostic-only wakes.  The ContextVar, the agent
 * attribute save/restore and the config loader are seams; what is ported here
 * is what those values decide: whether one event is presented for the muted
 * surface, whether an event is an early failure/monitor diagnostic rather than
 * the explicitly requested final result, and the literal tables both checks
 * are written against.
 */
#ifndef NOC_W25_NOTIFICATI_AE9773_H
#define NOC_W25_NOTIFICATI_AE9773_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of freeform UI events and of presentation callbacks. */
#define W25_NOTIFICATI_AE9773_FREEFORM_COUNT 13
#define W25_NOTIFICATI_AE9773_CALLBACK_COUNT 9

/* muted_surface (the ContextVar's current value, null for its None default),
 * event, session_id -> bool.  True only when the muted surface is this session
 * (Python '==', so null==null and 1==True) and the event is freeform. */
json_t *w25_notificati_ae9773_event_muted(const json_t *muted_surface,
                                          const json_t *event,
                                          const json_t *session_id);

/* event dict -> bool: early failure/monitor diagnostics (task_failure_notice,
 * or type in watch_disabled/watch_overflow_tripped/watch_overflow_released). */
json_t *w25_notificati_ae9773_diagnostic_process_event(const json_t *event);

/* _FREEFORM_EVENTS (sorted), _PRESENTATION_CALLBACKS and the ContextVar name. */
json_t *w25_notificati_ae9773_constants(void);

#ifdef __cplusplus
}
#endif
#endif
