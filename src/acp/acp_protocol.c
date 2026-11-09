/*
 * acp_protocol.c - ACP wire protocol helpers.
 *
 * Split from server.c (soul.md: self-contained modules). Message framing and
 * response/error builders; no file-scope state.
 *
 * alloc_session/find_session stay in server.c: they reach into the opaque
 * acp_server_t (deliberately opaque in include/acp/server.h), so moving them
 * would require either publishing the struct or adding accessors.
 */
#include "acp_protocol.h"
#include "acp/server.h"
#include "acp/edit_approval.h"
#include "acp/events.h"
#include "acp/permissions.h"
#include "acp/resource.h"
#include "hermes_json.h"
#include "hermes_agent.h"   /* init_agent, agent_state_t, tool registry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>


/* ================================================================
 *  JSON-RPC framing
 * ================================================================ */

json_node_t *acp_read_message(void) {
    char header[256];
    int content_length = -1;

    while (1) {
        if (!fgets(header, sizeof(header), stdin)) {
            if (feof(stdin)) return NULL;
            return NULL;
        }

        size_t hlen = strlen(header);
        while (hlen > 0 && (header[hlen-1] == '\n' || header[hlen-1] == '\r'))
            header[--hlen] = '\0';

        if (hlen == 0) break;

        if (strncasecmp(header, "Content-Length:", 15) == 0) {
            const char *val = header + 15;
            while (*val && isspace((unsigned char)*val)) val++;
            content_length = atoi(val);
        }
    }

    if (content_length < 0) return NULL;
    if (content_length > ACP_MAX_MESSAGE_BYTES) {
        fprintf(stderr, "[acp] Message too large: %d bytes\n", content_length);
        return NULL;
    }

    char *body = (char *)malloc((size_t)content_length + 1);
    if (!body) return NULL;

    size_t total = 0;
    while (total < (size_t)content_length) {
        ssize_t n = fread(body + total, 1, (size_t)content_length - total, stdin);
        if (n <= 0) { free(body); return NULL; }
        total += (size_t)n;
    }
    body[total] = '\0';

    char *err = NULL;
    json_node_t *json = json_parse(body, &err);
    if (err) {
        fprintf(stderr, "[acp] JSON parse error: %s\n", err);
        free(err);
    }
    free(body);
    return json;
}


bool acp_write_message(json_node_t *msg) {
    char *serialized = json_serialize(msg);
    if (!serialized) return false;

    size_t len = strlen(serialized);
    int hlen = fprintf(stdout, "Content-Length: %zu\r\n\r\n", len);
    if (hlen < 0) { free(serialized); return false; }

    size_t written = fwrite(serialized, 1, len, stdout);
    fflush(stdout);
    free(serialized);
    return written == len;
}


/* ================================================================
 *  JSON-RPC helpers
 * ================================================================ */

json_node_t *acp_make_response(const char *id, json_node_t *result) {
    json_node_t *resp = json_new_object();
    json_object_set(resp, "jsonrpc", json_new_string("2.0"));
    json_object_set(resp, "id", id ? json_new_string(id) : json_new_null());
    json_object_set(resp, "result", result ? json_copy(result) : json_new_null());
    return resp;
}


json_node_t *acp_make_error(const char *id, int code, const char *message, json_node_t *data) {
    json_node_t *err = json_new_object();
    json_object_set(err, "code", json_new_number((double)code));
    json_object_set(err, "message", json_new_string(message));
    if (data) json_object_set(err, "data", json_copy(data));

    json_node_t *resp = json_new_object();
    json_object_set(resp, "jsonrpc", json_new_string("2.0"));
    json_object_set(resp, "id", id ? json_new_string(id) : json_new_null());
    json_object_set(resp, "error", err);
    return resp;
}


const char *acp_get_method(json_node_t *req) {
    return json_object_get_string(req, "method", NULL);
}


const char *acp_get_id(json_node_t *req) {
    json_node_t *id_node = json_object_get(req, "id");
    if (!id_node) return NULL;
    static char buf[64];
    if (id_node->type == JSON_STRING && id_node->str_val)
        return id_node->str_val;
    if (id_node->type == JSON_NUMBER) {
        snprintf(buf, sizeof(buf), "%.0f", id_node->num_val);
        return buf;
    }
    return NULL;
}


json_node_t *acp_session_to_json(acp_session_t *sess) {
    json_node_t *info = json_new_object();
    json_object_set(info, "session_id", json_new_string(sess->id));
    json_object_set(info, "name", json_new_string(sess->name));
    json_object_set(info, "message_count", json_new_number((double)sess->message_count));
    json_object_set(info, "created_at", json_new_string(ctime(&sess->created_at)));
    if (sess->model[0])
        json_object_set(info, "model", json_new_string(sess->model));
    if (sess->mode[0])
        json_object_set(info, "mode", json_new_string(sess->mode));
    return info;
}
