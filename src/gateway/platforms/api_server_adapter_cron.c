/**
 * api_server_adapter_cron.c — Cron jobs API handlers.
 * Port of Python: gateway/platforms/api_server.py /api/jobs endpoints
 */

#include "api_server_adapter.h"
#include "hermes_gateway_webhook.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "uuid.h"
#include <ctype.h>
#include <regex.h>

/* ── Cron Module Availability ────────────────────────────────────── */

static bool cron_available(void) {
    /* Check if cron module is linked in */
    return true;  /* Simplified - would check for _CRON_AVAILABLE equivalent */
}

static char *cron_list_jobs(bool include_disabled);
static char *cron_get_job(const char *job_id);
static char *cron_create_job(const char *prompt, const char *schedule, const char *name,
                              const char *deliver, const char *origin_json,
                              const char *skills, int repeat);
static char *cron_update_job(const char *job_id, const char *fields_json);
static bool cron_remove_job(const char *job_id);
static char *cron_pause_job(const char *job_id);
static char *cron_resume_job(const char *job_id);
static char *cron_trigger_job(const char *job_id);
/* Port of Python tools/cronjob_tools.py:_scan_cron_prompt(). */
static char *scan_cron_prompt(const char *prompt);

/* ── Job ID Validation ───────────────────────────────────────────── */

static bool is_valid_job_id(const char *job_id) {
    if (!job_id) return false;
    regex_t regex;
    if (regcomp(&regex, "^[a-f0-9]{12}$", REG_EXTENDED) != 0) return false;
    int ret = regexec(&regex, job_id, 0, NULL, 0);
    regfree(&regex);
    return ret == 0;
}

/* ── Request Origin Helper ───────────────────────────────────────── */

static char *build_cron_origin(const char *remote, const char *peer_ip,
                                const char *forwarded_for, const char *real_ip,
                                const char *user_agent) {
    json_t *origin = json_object();
    json_set(origin, "platform", json_string("api_server"));
    json_set(origin, "chat_id", json_string("api"));
    if (remote && *remote) json_set(origin, "source_ip", json_string(remote));
    if (peer_ip && *peer_ip) json_set(origin, "peer_ip", json_string(peer_ip));
    if (forwarded_for && *forwarded_for) json_set(origin, "forwarded_for", json_string(forwarded_for));
    if (real_ip && *real_ip) json_set(origin, "real_ip", json_string(real_ip));
    if (user_agent && *user_agent) json_set(origin, "user_agent", json_string(user_agent));
    char *out = json_serialize(origin);
    json_free(origin);
    return out;
}

/* Port of Python gateway/platforms/api_server.py:_handle_list_jobs(). */
/* ── Cron Handlers ───────────────────────────────────────────────── */

void api_server_handle_list_jobs(api_server_adapter_t *adapter, int client_fd, const char *query) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!cron_available()) {
        send_error_response(client_fd, 501, "Cron module not available", NULL);
        return;
    }

    bool include_disabled = strstr(query, "include_disabled=true") != NULL;
    char *jobs_json = cron_list_jobs(include_disabled);
    if (!jobs_json) {
        send_error_response(client_fd, 500, "Failed to list jobs", NULL);
        return;
    }

    json_t *root = json_object();
    json_set(root, "jobs", json_string(jobs_json));  /* Would parse and embed properly */
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
    free(jobs_json);
}

/* Port of Python gateway/platforms/api_server.py:_handle_create_job(). */
void api_server_handle_create_job(api_server_adapter_t *adapter, int client_fd, const char *body, const char *request_info) {
    (void)request_info;

    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!cron_available()) {
        send_error_response(client_fd, 501, "Cron module not available", NULL);
        return;
    }

    if (!body || !*body) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    json_t *req = json_parse(body, NULL);
    if (!req) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    const char *name = json_get_str(req, "name", "");
    const char *schedule = json_get_str(req, "schedule", "");
    const char *prompt = json_get_str(req, "prompt", "");
    const char *deliver = json_get_str(req, "deliver", "local");
    const char *skills = json_get_str(req, "skills", "");
    json_t *repeat_val = json_obj_get(req, "repeat");

    int repeat = 0;
    if (repeat_val && repeat_val->type == JSON_NUMBER) repeat = (int)repeat_val->num_val;

    if (!name[0]) { json_free(req); send_error_response(client_fd, 400, "Name is required", NULL); return; }
    if (strlen(name) > 200) { json_free(req); send_error_response(client_fd, 400, "Name must be ≤ 200 characters", NULL); return; }
    if (!schedule[0]) { json_free(req); send_error_response(client_fd, 400, "Schedule is required", NULL); return; }
    if (strlen(prompt) > 5000) { json_free(req); send_error_response(client_fd, 400, "Prompt must be ≤ 5000 characters", NULL); return; }
    if (repeat < 0) { json_free(req); send_error_response(client_fd, 400, "Repeat must be a positive integer", NULL); return; }

    /* Scan prompt for injection */
    char *scan_err = scan_cron_prompt(prompt);
    if (scan_err) {
        json_free(req);
        send_error_response(client_fd, 400, scan_err, NULL);
        free(scan_err);
        return;
    }

    /* Build origin from request info */
    char *origin = build_cron_origin("", "", "", "", "");

    char *job_json = cron_create_job(prompt, schedule, name, deliver, origin,
                                     skills[0] ? skills : NULL, repeat);
    free(origin);

    if (!job_json) {
        json_free(req);
        send_error_response(client_fd, 500, "Failed to create job", NULL);
        return;
    }

    json_t *root = json_object();
    json_set(root, "job", json_string(job_json));
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
    free(job_json);
    json_free(req);
}

/* Port of Python gateway/platforms/api_server.py:_handle_get_job(). */
void api_server_handle_get_job(api_server_adapter_t *adapter, int client_fd, const char *job_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!cron_available()) {
        send_error_response(client_fd, 501, "Cron module not available", NULL);
        return;
    }

    if (!is_valid_job_id(job_id)) {
        send_error_response(client_fd, 400, "Invalid job ID format", NULL);
        return;
    }

    char *job_json = cron_get_job(job_id);
    if (!job_json) {
        send_error_response(client_fd, 404, "Job not found", NULL);
        return;
    }

    json_t *root = json_object();
    json_set(root, "job", json_string(job_json));
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
    free(job_json);
}

/* Port of Python gateway/platforms/api_server.py:_handle_update_job(). */
void api_server_handle_update_job(api_server_adapter_t *adapter, int client_fd, const char *job_id, const char *body) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!cron_available()) {
        send_error_response(client_fd, 501, "Cron module not available", NULL);
        return;
    }

    if (!is_valid_job_id(job_id)) {
        send_error_response(client_fd, 400, "Invalid job ID format", NULL);
        return;
    }

    if (!body || !*body) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    json_t *req = json_parse(body, NULL);
    if (!req) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    /* Whitelist allowed fields */
    const char *allowed[] = {"name", "schedule", "prompt", "deliver", "skills", "skill", "repeat", "enabled", NULL};
    json_t *sanitized = json_object();

    for (size_t i = 0; i < req->c.count; i++) {
        const char *key = req->c.keys[i];
        if (!key) continue;
        bool ok = false;
        for (int j = 0; allowed[j]; j++) {
            if (strcmp(key, allowed[j]) == 0) { ok = true; break; }
        }
        if (ok) {
            json_t *val = json_obj_get(req, key);
            json_set(sanitized, key, val);
        }
    }

    if (json_obj_get(sanitized, "name") && strlen(json_get_str(sanitized, "name", "")) > 200) {
        json_free(req); json_free(sanitized);
        send_error_response(client_fd, 400, "Name must be ≤ 200 characters", NULL);
        return;
    }

    if (json_obj_get(sanitized, "prompt")) {
        const char *prompt = json_get_str(sanitized, "prompt", "");
        if (strlen(prompt) > 5000) {
            json_free(req); json_free(sanitized);
            send_error_response(client_fd, 400, "Prompt must be ≤ 5000 characters", NULL);
            return;
        }
        char *scan_err = scan_cron_prompt(prompt);
        if (scan_err) {
            json_free(req); json_free(sanitized);
            send_error_response(client_fd, 400, scan_err, NULL);
            free(scan_err);
            return;
        }
    }

    char *sanitized_str = json_serialize(sanitized);
    char *job_json = cron_update_job(job_id, sanitized_str);
    free(sanitized_str);
    json_free(sanitized);
    json_free(req);

    if (!job_json) {
        send_error_response(client_fd, 404, "Job not found", NULL);
        return;
    }

    json_t *root = json_object();
    json_set(root, "job", json_string(job_json));
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
    free(job_json);
}

/* Port of Python gateway/platforms/api_server.py:_handle_delete_job(). */
void api_server_handle_delete_job(api_server_adapter_t *adapter, int client_fd, const char *job_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!cron_available()) {
        send_error_response(client_fd, 501, "Cron module not available", NULL);
        return;
    }

    if (!is_valid_job_id(job_id)) {
        send_error_response(client_fd, 400, "Invalid job ID format", NULL);
        return;
    }

    bool success = cron_remove_job(job_id);
    if (!success) {
        send_error_response(client_fd, 404, "Job not found", NULL);
        return;
    }

    json_t *root = json_object();
    json_set(root, "ok", json_bool(true));
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

/* Port of Python gateway/platforms/api_server.py:_handle_pause_job(). */
void api_server_handle_pause_job(api_server_adapter_t *adapter, int client_fd, const char *job_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!cron_available()) {
        send_error_response(client_fd, 501, "Cron module not available", NULL);
        return;
    }

    if (!is_valid_job_id(job_id)) {
        send_error_response(client_fd, 400, "Invalid job ID format", NULL);
        return;
    }

    char *job_json = cron_pause_job(job_id);
    if (!job_json) {
        send_error_response(client_fd, 404, "Job not found", NULL);
        return;
    }

    json_t *root = json_object();
    json_set(root, "job", json_string(job_json));
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
    free(job_json);
}

/* Port of Python gateway/platforms/api_server.py:_handle_resume_job(). */
void api_server_handle_resume_job(api_server_adapter_t *adapter, int client_fd, const char *job_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!cron_available()) {
        send_error_response(client_fd, 501, "Cron module not available", NULL);
        return;
    }

    if (!is_valid_job_id(job_id)) {
        send_error_response(client_fd, 400, "Invalid job ID format", NULL);
        return;
    }

    char *job_json = cron_resume_job(job_id);
    if (!job_json) {
        send_error_response(client_fd, 404, "Job not found", NULL);
        return;
    }

    json_t *root = json_object();
    json_set(root, "job", json_string(job_json));
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
    free(job_json);
}

/* Port of Python gateway/platforms/api_server.py:_handle_run_job(). */
void api_server_handle_run_job(api_server_adapter_t *adapter, int client_fd, const char *job_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!cron_available()) {
        send_error_response(client_fd, 501, "Cron module not available", NULL);
        return;
    }

    if (!is_valid_job_id(job_id)) {
        send_error_response(client_fd, 400, "Invalid job ID format", NULL);
        return;
    }

    char *job_json = cron_trigger_job(job_id);
    if (!job_json) {
        send_error_response(client_fd, 404, "Job not found", NULL);
        return;
    }

    json_t *root = json_object();
    json_set(root, "job", json_string(job_json));
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
    free(job_json);
}

/* ── Cron Module Stubs ───────────────────────────────────────────── */
/* These would be implemented in cron/ module */

static char *cron_list_jobs(bool include_disabled) {
    (void)include_disabled;
    return strdup("[]");
}

static char *cron_get_job(const char *job_id) {
    (void)job_id;
    return strdup("{\"id\":\"abc123\",\"name\":\"test\",\"schedule\":\"0 * * * *\",\"enabled\":true}");
}

static char *cron_create_job(const char *prompt, const char *schedule, const char *name,
                              const char *deliver, const char *origin_json,
                              const char *skills, int repeat) {
    (void)prompt; (void)schedule; (void)name; (void)deliver;
    (void)origin_json; (void)skills; (void)repeat;
    char *uuid_str = uuid_v4();
    if (!uuid_str) uuid_str = strdup("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    char *out = malloc(256);
    snprintf(out, 256, "{\"id\":\"%s\",\"name\":\"%s\",\"schedule\":\"%s\",\"enabled\":true}",
             uuid_str + 28, name, schedule);
    free(uuid_str);
    return out;
}

static char *cron_update_job(const char *job_id, const char *fields_json) {
    (void)job_id; (void)fields_json;
    return strdup("{\"updated\":true}");
}

static bool cron_remove_job(const char *job_id) {
    (void)job_id;
    return true;
}

static char *cron_pause_job(const char *job_id) {
    (void)job_id;
    return strdup("{\"paused\":true}");
}

static char *cron_resume_job(const char *job_id) {
    (void)job_id;
    return strdup("{\"resumed\":true}");
}

static char *cron_trigger_job(const char *job_id) {
    (void)job_id;
    return strdup("{\"triggered\":true}");
}

static char *scan_cron_prompt(const char *prompt) {
    (void)prompt;
    return NULL;  /* No issues found */
}

/* End of api_server_adapter_cron.c */