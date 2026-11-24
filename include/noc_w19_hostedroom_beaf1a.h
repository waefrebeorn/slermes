#ifndef NOC_W19_HOSTEDROOM_BEAF1A_H
#define NOC_W19_HOSTEDROOM_BEAF1A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_hostedroom_beaf1a_schema_ready @ persistence/sqlite/gateway/hosted_room_replicas.py:_replica_schema_ready
 * Both replica tables present. */
int w19_hostedroom_beaf1a_schema_ready(int replicas, int replica_events);

/* PoP: w19_hostedroom_beaf1a_schema_sql @ persistence/sqlite/gateway/hosted_room_replicas.py:_initialize_replica_schema
 * {"replicas":...,"events":...} - the two CREATE TABLE IF NOT EXISTS statements. */
json_t *w19_hostedroom_beaf1a_schema_sql(void);

/* PoP: w19_hostedroom_beaf1a_control_event @ persistence/sqlite/gateway/hosted_room_replicas.py:_append_control_event
 * {"event_id","kind","actor_json","payload_json"} of the system authority.<kind> event,
 * payload encoded as sorted-key compact JSON. */
json_t *w19_hostedroom_beaf1a_control_event(const char *kind, long long epoch, json_t *payload);

/* PoP: w19_hostedroom_beaf1a_event_bytes @ session_rules/hosted_rooms.py:_event_bytes
 * UTF-8 byte size of the event's id, kind, actor and payload as the replica accounts it. */
json_t *w19_hostedroom_beaf1a_event_bytes(json_t *event);

/* PoP: w19_hostedroom_beaf1a_store_values @ persistence/sqlite/gateway/hosted_room_replicas.py:_store_replica
 * The replica row values (latest_seq = max(latest_seq, new_last)) the INSERT/UPDATE binds. */
json_t *w19_hostedroom_beaf1a_store_values(const char *room_name, const char *members_json,
                                           const char *gateway_id, long long epoch,
                                           long long new_last, long long latest_seq);

/* PoP: w19_hostedroom_beaf1a_row_state @ persistence/sqlite/gateway/hosted_room_replicas.py:_replica_row_state
 * {"error":null|str,"found":bool,"stored_epoch","last_seq","event_bytes"}; a new room is
 * admitted only under the replica room cap. */
json_t *w19_hostedroom_beaf1a_row_state(int found, long long room_count, long long stored_epoch,
                                        long long last_seq, long long event_bytes);

/* PoP: w19_hostedroom_beaf1a_ingest_plan @ persistence/sqlite/gateway/hosted_room_replicas.py:ingest_page
 * {"error":null|str,"result":null|obj,"rows":[event-insert params...]} for one page. */
json_t *w19_hostedroom_beaf1a_ingest_plan(json_t *args);

/* PoP: w19_hostedroom_beaf1a_promote_plan @ persistence/sqlite/gateway/hosted_room_replicas.py:promote_replica
 * {"error","result","control_row","room_row"} for one takeover. */
json_t *w19_hostedroom_beaf1a_promote_plan(json_t *args);

/* PoP: w19_hostedroom_beaf1a_demote_plan @ persistence/sqlite/gateway/hosted_room_replicas.py:demote_room
 * {"error","result","control_row","update_params"} for one demotion. */
json_t *w19_hostedroom_beaf1a_demote_plan(json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_HOSTEDROOM_BEAF1A_H */
