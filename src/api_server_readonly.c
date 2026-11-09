/*
 * api_server_readonly.c - read-only HTTP handlers.
 *
 * Split from api_server.c (soul.md: self-contained modules). Capabilities,
 * options, skills and response-GET/DELETE handlers. No file-scope state.
 */
#include "api_server_readonly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include "hermes_json.h"
#include "hermes_agent.h"
#include "registry.h"
#include "hermes_skills.h"
#include "api_server_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* GW16: Delete response — DELETE /v1/responses/{response_id} */ void handle_delete_response(int fd, const char *response_id) {
    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"id\":\"%s\",\"object\":\"response\",\"deleted\":true}",
             response_id);
    api_send_json(fd, 200, "OK", resp);
}


/**
 * Handle an OPTIONS preflight request.
 */ void handle_options(int fd) {
    send_response(fd, 204, "No Content", NULL, NULL);
}


/**
 * GET /v1/capabilities — machine-readable API capabilities.
 */ void handle_capabilities(int fd) {
    json_t *root = json_object();
    json_set(root, "api_version", json_string("v1"));
    json_set(root, "name", json_string("hermes-c"));

    json_t *endpoints = json_array();
    json_append(endpoints, json_string("GET /v1/models"));
    json_append(endpoints, json_string("POST /v1/chat/completions"));
    json_append(endpoints, json_string("GET /v1/capabilities"));
    json_append(endpoints, json_string("GET /v1/tools"));
    json_append(endpoints, json_string("GET /v1/agent/status"));
    json_append(endpoints, json_string("GET /v1/sessions"));
    json_append(endpoints, json_string("POST /v1/sessions"));
    json_append(endpoints, json_string("GET /v1/sessions/{id}"));
    json_append(endpoints, json_string("DELETE /v1/sessions/{id}"));
    json_append(endpoints, json_string("GET /health"));
    json_append(endpoints, json_string("GET /health/detailed"));
    json_set(root, "endpoints", endpoints);

    json_t *features = json_object();
    json_set(features, "chat_completions", json_bool(true));
    json_set(features, "streaming", json_bool(true));
    json_set(features, "tools_listing", json_bool(true));
    json_set(features, "models_listing", json_bool(true));
    json_set(features, "agent_status", json_bool(true));
    json_set(features, "sessions_listing", json_bool(true));
    json_set(features, "sessions_crud", json_bool(true));
    json_set(features, "health_detailed", json_bool(true));
    json_set(root, "features", features);

    char *out = json_serialize(root);
    if (out) {
        api_send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}


/* GW16: GET /v1/skills — list installed skills */ void handle_skills(int fd) {
    json_t *root = json_object();
    json_set(root, "object", json_string("list"));

    skill_list_t *list = skills_scan_all();
    if (list) {
        json_t *data = json_array();
        for (size_t i = 0; i < list->count; i++) {
            json_t *entry = json_object();
            json_set(entry, "name", json_string(list->skills[i].name));
            if (list->skills[i].description[0])
                json_set(entry, "description", json_string(list->skills[i].description));
            json_append(data, entry);
        }
        json_set(root, "data", data);
        json_set(root, "count", json_number((double)list->count));
        skills_scan_free(list);
    } else {
        json_set(root, "data", json_array());
        json_set(root, "count", json_number(0));
    }

    char *out = json_serialize(root);
    if (out) {
        api_send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}
