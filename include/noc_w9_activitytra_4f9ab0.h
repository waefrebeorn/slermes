#ifndef NOC_W9_ACTIVITYTRA_4F9AB0_H
#define NOC_W9_ACTIVITYTRA_4F9AB0_H

#include <hermes_json.h>

/* C11 ports of the observable state transitions in
 * agent/activity_tracking.py's ActivityTrackingMixin. External results
 * (clock, description binding, provenance normalisation, store availability)
 * are injected via the args object, so the C side performs no I/O.
 * Every symbol is prefixed w9_activitytra_4f9ab0_ and takes one borrowed
 * json_t *args object, returning a NEW json_t *. */
json_t *w9_activitytra_4f9ab0_touch(json_t *a);
json_t *w9_activitytra_4f9ab0_persist_if_due(json_t *a);
json_t *w9_activitytra_4f9ab0_reset_labels(json_t *a);

#endif /* NOC_W9_ACTIVITYTRA_4F9AB0_H */
