#ifndef NOC_W9_RUNINBOUND_BCB6B7_H
#define NOC_W9_RUNINBOUND_BCB6B7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_runinbound_bcb6b7_moa_decision @ gateway/run_inbound_moa.py:_hm_cmd_moa
 *
 * Faithful port of the decision logic of the /moa inbound handler.  Every
 * collaborator (command args, preset, policy/sink/managed probes,
 * rebind/admit results and the i18n strings) is injected as a field of `in`
 * so the C function stays pure.  Returns:
 *   {"handled": bool, "reply": <string|null>, "event_text": <string|null>}
 * where `event_text` is the post-call value of event.text (null when the
 * handler returned without assigning it).  Caller frees. */
struct json_t *w9_runinbound_bcb6b7_moa_decision(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_RUNINBOUND_BCB6B7_H */
