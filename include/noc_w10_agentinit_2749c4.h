#ifndef NOC_W10_AGENTINIT_2749C4_H
#define NOC_W10_AGENTINIT_2749C4_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_agentinit_2749c4_writer_interface @ agent/agent_init_session.py:writer_interface
 * Returns a copy of the persistence handle, or NULL (Python None). */
struct json_t *w10_agentinit_2749c4_writer_interface(const struct json_t *persistence,
                                                      const struct json_t *store);
/* PoP: w10_agentinit_2749c4_agent_interface @ agent/agent_init_session.py:agent_interface
 * writer_interface(agent.persistence, agent._session_db); NULL for None. */
struct json_t *w10_agentinit_2749c4_agent_interface(const struct json_t *agent);
/* PoP: w10_agentinit_2749c4_stand_in_interface @ agent/agent_init_session.py:stand_in_interface
 * registered/is_session_db are explicit seams for `hermes_state` in sys.modules and the
 * isinstance(session_db, hermes_state._SessionDB) check. NULL for None. */
struct json_t *w10_agentinit_2749c4_stand_in_interface(const struct json_t *session_db,
                                                        int registered, int is_session_db);
/* PoP: w10_agentinit_2749c4_publish_session_id @ agent/agent_init_session.py:_publish_session_id
 * bridge_ok/delegated_child are explicit seams for the ContextVar import and the delegated-child
 * probe. Returns one of "contextvar" / "skip" / "env"; the string is static (do not free). */
const char *w10_agentinit_2749c4_publish_session_id(const char *session_id,
                                                    int bridge_ok, int delegated_child);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_AGENTINIT_2749C4_H */
