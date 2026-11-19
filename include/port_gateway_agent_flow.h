/*
 * port_gateway_agent_flow.h — gateway agent-bridge FLOW cores of
 * hermes_cli/gateway_agent.py (LIVE authority: GatewayAgent + module leaves).
 *
 * Ports the DECISION sequences of the gateway-backed AIAgent stand-in: the
 * one-turn route pick / give-back state machine, off-session route staging,
 * reasoning commit (session.mutate generation fence), show-reasoning and
 * service-tier config.set dedup, steer/redirect correction verdicts, the
 * pre-submit attach/start/notice chain, submit-param assembly (attachments,
 * api_text, model_override) and the batch clarify reply.
 *
 * SEAM DOCTRINE: every live effect (connection.call / reattach / start,
 * state.runs_route, i18n, prints, uuid4, pending_lines, the clarify callback)
 * enters through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` borrowed JSON object;
 * result NEW json_t* (NULL == None); {"__raise__": cls, ...} == exception.
 * The harness records {"ev": name, "args": args, "ret": result} per call in
 * order; the oracle's recording stubs emit the identical shape driving LIVE
 * Python with the same scripts. The cores perform NO I/O.
 *
 * The agent self-model is an injected json_t* `st` mirroring instance
 * attributes; st["state"] is the shared session-state object (info, gaps,
 * attached_gaps, pending, service_tier_sent, ...). Mutations happen in place.
 * Ownership: returned json_t* / char* malloc'd (caller frees) unless noted.
 */
#ifndef PORT_GATEWAY_AGENT_FLOW_H
#define PORT_GATEWAY_AGENT_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gwa_trace { json_t *events; } gwa_trace_t;
void gwa_trace_add(gwa_trace_t *t, json_t *ev);
void gwa_trace_free(gwa_trace_t *t);

typedef struct gwa_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gwa_seams_t;

json_t *gwa_call(const gwa_seams_t *s, const char *name, const json_t *args);
const char *gwa_raise_class(const json_t *r);
bool gwa_caught(const json_t *r, const char *cls);

/* Seam name table (name -> live call):
 *   call               self.connection.call(method, **kwargs)  args{method,...}
 *   runs_route         state.runs_route(model, provider) -> bool
 *   reattach           self.connection.reattach(session_id, abandon=...) -> snapshot
 *   start              self.connection.start()
 *   pending_lines      gateway_chat_view.pending_lines(pending) -> [str]
 *   t                  agent.i18n.t(key) -> str
 *   print              print(text)  (via _emit_print; suppress honored in core)
 *   uuid4_hex          uuid.uuid4().hex -> str
 *   clarify_callback   clarify_callback(questions) -> reply
 *
 * Self-model `st` fields read/written:
 *   session_id, model, provider, connection_epoch,
 *   _generation, _turn_route, _turn_reasoning, _taken_turn_route,
 *   _reasoning_config, _reasoning_callback (null == absent),
 *   _show_reasoning_sent ([sid,show,epoch]|null), _service_tier,
 *   _voice_turn_pending, _pending_cli_user_message, _finite,
 *   clarify_callback (null == absent), _moa_preset, suppress_status_output,
 *   state {info, gaps, attached_gaps, pending, service_tier_sent}
 */

/* _effort_word(config) -> "none"|effort|NULL (caller frees). */
/* PoP: gwa_effort_word @ hermes_cli/gateway_agent.py:_effort_word */
char *gwa_effort_word(const json_t *config);

/* _route_off_session(model, provider, alias) -> {model,provider?}|NULL */
/* PoP: gwa_route_off_session @ hermes_cli/gateway_agent.py:_route_off_session */
json_t *gwa_route_off_session(json_t *st, const char *model, const char *provider,
                              const char *alias, const gwa_seams_t *s);

/* _set_reasoning(config, explicit) -> bool; mutates st._reasoning_config/state.info */
/* PoP: gwa_set_reasoning @ hermes_cli/gateway_agent.py:_set_reasoning */
bool gwa_set_reasoning(json_t *st, const json_t *config, bool explicit_, const gwa_seams_t *s);

/* _sync_show_reasoning(); mutates st._show_reasoning_sent */
/* PoP: gwa_sync_show_reasoning @ hermes_cli/gateway_agent.py:_sync_show_reasoning */
void gwa_sync_show_reasoning(json_t *st, const gwa_seams_t *s);

/* service_tier setter(tier); mutates st._service_tier / state.service_tier_sent */
/* PoP: gwa_service_tier_set @ hermes_cli/gateway_agent.py:service_tier */
void gwa_service_tier_set(json_t *st, const char *tier, const gwa_seams_t *s);

/* _correct(verb, text) -> json bool true/false or string "voice_stopped". */
/* PoP: gwa_correct @ hermes_cli/gateway_agent.py:_correct */
json_t *gwa_correct(json_t *st, const char *verb, const char *text, const gwa_seams_t *s);

/* _take_turn_route() -> {model,...,reasoning?}|NULL; mutates route fields. */
/* PoP: gwa_take_turn_route @ hermes_cli/gateway_agent.py:_take_turn_route */
json_t *gwa_take_turn_route(json_t *st);

/* _give_back_turn_route(); mutates route fields. */
/* PoP: gwa_give_back_turn_route @ hermes_cli/gateway_agent.py:_give_back_turn_route */
void gwa_give_back_turn_route(json_t *st);

/* _prepare_submit(turn_session_id) -> notice str (caller frees; "" when none). */
/* PoP: gwa_prepare_submit @ hermes_cli/gateway_agent.py:_prepare_submit */
char *gwa_prepare_submit(json_t *st, const char *turn_session_id, const gwa_seams_t *s);

/* _submit_params(user_message, persist_user_message, turn_author) -> params obj. */
/* PoP: gwa_submit_params @ hermes_cli/gateway_agent.py:_submit_params */
json_t *gwa_submit_params(json_t *st, const json_t *user_message,
                          const json_t *persist_user_message, const char *turn_author,
                          const gwa_seams_t *s);

/* clarify_reply(callback_present, params) -> ClarifyResult obj. */
/* PoP: gwa_clarify_reply @ hermes_cli/gateway_agent.py:clarify_reply */
json_t *gwa_clarify_reply(bool callback_present, const json_t *params, const gwa_seams_t *s);

#ifdef __cplusplus
}
#endif
#endif /* PORT_GATEWAY_AGENT_FLOW_H */