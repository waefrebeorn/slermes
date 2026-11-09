/*
 * acp_protocol.h - ACP wire protocol + session registry helpers.
 *
 * Module-internal header (server.c <-> acp_protocol.c).
 */
#ifndef ACP_PROTOCOL_H
#define ACP_PROTOCOL_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "acp/server.h"          /* acp_server_t (opaque) */
#include "hermes_core_types.h"   /* agent_state_t */

typedef struct {
    char            id[64];
    char            name[128];
    agent_state_t   agent;            /* Hermes agent state */
    bool            active;
    time_t          created_at;
    int             message_count;
    char            model[128];
    char            mode[32];         /* "plan" | "act" */
    char            auto_approve_policy[32]; /* "ask" | "session" | "workspace_session" */
} acp_session_t;

const char * acp_get_id(json_node_t *req);
const char * acp_get_method(json_node_t *req);
json_node_t * acp_make_error(const char *id, int code, const char *message, json_node_t *data);
json_node_t * acp_make_response(const char *id, json_node_t *result);
json_node_t * acp_read_message(void);
bool acp_write_message(json_node_t *msg);
json_node_t * acp_session_to_json(acp_session_t *sess);

#endif /* ACP_PROTOCOL_H */
