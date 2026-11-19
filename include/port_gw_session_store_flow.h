#ifndef PORT_GW_SESSION_STORE_FLOW_H
#define PORT_GW_SESSION_STORE_FLOW_H
/*
 * port_gw_session_store_flow.h — FLOW + leaf ports of
 * hermes_cli/gateway_session_store.py (GatewaySessionStore persistence flows).
 *
 * Decision sequences only: every live I/O (gateway RPCs, the read-only reader
 * probe, uuid minting, the owner-restart retry state, stderr warnings, debug
 * logs) enters through the universal seam below. The store is a JSON state
 * mirror mutated in place; the oracle compares the ordered seam trace, the
 * return value (or raise spec) and the final mirror state.
 *
 * Seam contract (single owner per value):
 *   io->call(ctx, name, args) returns a NEW json_t the CALLER frees.
 *   NULL == Python None. {"__raise__": cls, "message": msg} == an exception.
 *   `args` is BORROWED: the stub copies it for the trace; the caller frees it.
 *   KeyError specs carry the RAW key as message; the emitter reprs it once
 *   (str(KeyError(k)) == repr(k)).
 *   STRUCTURAL seam "connection.state" returns a BORROWED mirror state node
 *   (get-or-create, like GatewayConnection.state) — NEVER free it; mutate it
 *   in place (adopt_snapshot effects / title writes are silent state effects,
 *   not seam events, on both sides).
 *   SINK seams "logger.debug" and "print" record and return NULL (no script).
 *
 * Identity model: `self.current is snapshot` (end_session) is tracked by
 * gwss_store_t.current_ref — the sid whose snapshots entry current IS. The
 * fixture seeds current as {"ref": sid} for that identity, or an inline value
 * (a distinct object). Python object identity elsewhere is not observable in
 * the ported set.
 *
 * Fixture contract (modeling scope shared by the oracle): session/child ids
 * are strings; snapshot info/messages/pending are dict/list/null (dict()/list()
 * of exotic scalars is out of scope); the f-string slots (config_warning,
 * title, holder) are scalars (str/int/bool); float() string arguments use ASCII
 * whitespace. Scripted KeyError messages carry the raw key.
 */
#include "hermes_json.h"

typedef struct gwss_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gwss_io_t;

typedef struct gwss_store {
    json_t *m;          /* state mirror object (owned): snapshots, current,
                         * retry, connection{final,launch_api_key,
                         * launch_api_key_typed,states}, reader, handles,
                         * persistence, create_parameters */
    char *current_ref;  /* identity of `current` (see header note), or NULL */
} gwss_store_t;

/* Each port returns a NEW result (NULL == None). On a Python exception it
 * sets *raise_out to a NEW {"__raise__","message"} spec (caller frees) and
 * returns NULL. BaseException kinds (SystemExit/KeyboardInterrupt/
 * GeneratorExit/CancelledError) pass through every except-Exception site. */

/* PoP-target leaves */
json_t *gwss_session_label(const json_t *session_id, json_t **raise_out);
json_t *gwss_classic_only_policy(const json_t *launch, json_t **raise_out);
json_t *gwss_transcript_view(const json_t *include_inactive,
                             const json_t *include_compacted, json_t **raise_out);
json_t *gwss_window(const json_t *limit, const json_t *offset, json_t **raise_out);

/* Store flows (in = the case input object) */
json_t *gwss_set_session_yolo(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_close_reader(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_getattr(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_call_flow(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_adopt(gwss_store_t *st, const json_t *snapshot, gwss_io_t *io, json_t **raise_out);
json_t *gwss_attach(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_open_session(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_create(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_branch(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_mutate(gwss_store_t *st, const json_t *session_id, const json_t *operation,
                    const json_t *payload, const json_t *snapshot, gwss_io_t *io, json_t **raise_out);
json_t *gwss_set_session_title(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_delete_session(gwss_store_t *st, const json_t *session_id, gwss_io_t *io, json_t **raise_out);
json_t *gwss_end_session(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);
json_t *gwss_delete_session_if_empty(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out);

#endif /* PORT_GW_SESSION_STORE_FLOW_H */