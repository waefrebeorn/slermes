/*
 * noc_w32_events_3805ad.h - C surface for upstream tui_gateway/contracts/events.py
 * (gate w32_events_3805ad).
 *
 * The event catalog itself is the pure value here: which ``event`` frames the
 * gateway may emit, which payload model each one carries, whether that model is
 * a closed set or an open one, the field names/types/requiredness of every
 * payload, and the wire enums the payloads use.  Every emitter (and the
 * registry that validates what an emitter produced) sits behind that table, so
 * the table -- plus the structural check the registry runs against it -- is
 * what this file owns.
 */
#ifndef NOC_W32_EVENTS_3805AD_H
#define NOC_W32_EVENTS_3805AD_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of events events.py declares, in declaration order. */
#define W32_EVENTS_3805AD_EVENT_COUNT 72

/* [{name, payload(null when the frame carries none), open, doc}, ...] in
 * declaration order. */
json_t *w32_events_3805ad_catalog(void);

/* The catalog with each payload's field schema:
 * [{name, payload, open, required[], optional[], types{field: annotation}}, ...]. */
json_t *w32_events_3805ad_schema_all(void);

/* One catalog entry of schema_all's shape, or null for an undeclared event. */
json_t *w32_events_3805ad_schema(const json_t *event);

/* Structural conformance of one frame's payload against the declared contract:
 * a list of "<loc>:<problem>" (missing / extra_forbidden / payload:*) with
 * ``missing`` in field order and ``extra_forbidden`` in payload-key order;
 * [] when the payload conforms, null when the event has no contract.  The
 * declared annotation is carried, not enforced: value/type validation is
 * pydantic's seam (events.py's schema is the value; this checks its shape). */
json_t *w32_events_3805ad_problems(const json_t *event, const json_t *payload);

/* The WireEnum sets declared in the module: {EnumName: [value, ...]}. */
json_t *w32_events_3805ad_enums(void);

/* Event names selected by an optional filter object
 * {open?: bool, payload?: str, doc_has?: str} (absent/null: no constraint). */
json_t *w32_events_3805ad_select(const json_t *filter);

/* Validate a batch of frames:
 * [{event, payload}, ...] -> [{index, problems}, ...] one per frame. */
json_t *w32_events_3805ad_frames(const json_t *frames);

#ifdef __cplusplus
}
#endif
#endif
