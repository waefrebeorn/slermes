#ifndef NOC_W9_COMPACTIONE_F03B0C_H
#define NOC_W9_COMPACTIONE_F03B0C_H

#include <hermes_json.h>

/* C11 ports of the content-free hermes.compaction v1 payload builders in
 * agent/compaction_events.py. Every symbol is prefixed w9_compactione_f03b0c_
 * and takes one borrowed json_t *args object, returning a NEW json_t *.
 * Unknown categorical values collapse to a closed-set default; no field ever
 * carries message, summary, error or path text. */
json_t *w9_compactione_f03b0c_closed(json_t *a);
json_t *w9_compactione_f03b0c_int(json_t *a);
json_t *w9_compactione_f03b0c_failure_class(json_t *a);
json_t *w9_compactione_f03b0c_event_name(json_t *a);
json_t *w9_compactione_f03b0c_attempt_outcome(json_t *a);
json_t *w9_compactione_f03b0c_attempt_trigger(json_t *a);
json_t *w9_compactione_f03b0c_payload(json_t *a);
json_t *w9_compactione_f03b0c_attempt_payload(json_t *a);
json_t *w9_compactione_f03b0c_micro_payload(json_t *a);
json_t *w9_compactione_f03b0c_prune_payload(json_t *a);
json_t *w9_compactione_f03b0c_provider_native_payload(json_t *a);

#endif /* NOC_W9_COMPACTIONE_F03B0C_H */
