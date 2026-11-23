#ifndef NOC_W15_CLISINGLE_3677F3_H
#define NOC_W15_CLISINGLE_3677F3_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/cli_single_query.py (gate w15_clisingle_3677f3).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_clisingle_3677f3_int_or @ hermes_cli/cli_single_query.py:_int_or
 * Python int(value) coercion: bool/number/str parse, else a copy of `dflt`. */
struct json_t *w15_clisingle_3677f3_int_or(const struct json_t *value,
                                           const struct json_t *dflt);

/* PoP: w15_clisingle_3677f3_sync_session @ hermes_cli/cli_single_query.py:_sync_cli_session_id_from_agent
 * Returns the resulting {"session_id": ...} after the conditional adoption. */
struct json_t *w15_clisingle_3677f3_sync_session(const struct json_t *cli);

/* PoP: w15_clisingle_3677f3_configure_quiet @ hermes_cli/cli_single_query.py:_configure_quiet_agent
 * Returns the agent object with all nine quiet-mode attributes neutralized. */
struct json_t *w15_clisingle_3677f3_configure_quiet(const struct json_t *agent);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_CLISINGLE_3677F3_H */
