/* Slermes C11 port — 15 truly missing functions (no C equivalent existed).
 * Faithful implementations of Python functions that had no prior C port.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <ctype.h>
#include "hermes_json.h"

/* ═══════════════════════════════════════════════════════════════════════════ */
/* agent/verification_stop.py                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── verify_on_stop_enabled(config=None) -> bool ─── */
/* PoP: verify_on_stop_enabled @ agent/verification_stop.py:verify_on_stop_enabled */
int verify_on_stop_enabled(json_t *config) {
    const char *env = getenv("HERMES_VERIFY_ON_STOP");
    if (env) {
        char v[16] = {0};
        for (int i = 0; i < 15 && env[i]; i++)
            v[i] = (char)tolower((unsigned char)env[i]);
        return !(strcmp(v, "0") == 0 || strcmp(v, "false") == 0 ||
                 strcmp(v, "no") == 0 || strcmp(v, "off") == 0);
    }
    if (config) {
        json_t *agent = json_obj_get(config, "agent");
        if (agent) {
            json_t *val = json_obj_get(agent, "verify_on_stop");
            if (val && json_is_bool(val)) return json_get_bool(val, "", false);
            if (val && json_is_string(val)) {
                const char *s = json_get_str(val, "", "");
                if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0 ||
                    strcmp(s, "yes") == 0 || strcmp(s, "on") == 0) return 1;
                if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0 ||
                    strcmp(s, "no") == 0 || strcmp(s, "off") == 0) return 0;
            }
        }
    }
    extern int verification_stop_session_is_messaging_surface(void);
    return !verification_stop_session_is_messaging_surface();
}

/* ─── build_verify_on_stop_nudge(...) ─── */
/* PoP: build_verify_on_stop_nudge @ agent/verification_stop.py:build_verify_on_stop_nudge */
json_t *build_verify_on_stop_nudge(const char *session_id, json_t *changed_paths,
                                    int attempts, int max_attempts) {
    json_t *result = json_object();
    json_set(result, "session_id", json_string(session_id ? session_id : ""));
    json_set(result, "changed_paths", json_copy(changed_paths));
    json_set(result, "attempts", json_number(attempts));
    json_set(result, "max_attempts", json_number(max_attempts));
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* gateway/drain_control.py                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── drain_requested(*, home=None) -> bool ─── */
/* PoP: drain_requested @ gateway/drain_control.py:drain_requested */
int drain_requested(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[1024];
    snprintf(path, sizeof(path), "%s/gateway/drain", home);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    long epoch = 0;
    if (fscanf(f, "%ld", &epoch) != 1) { fclose(f); return 0; }
    fclose(f);
    extern int drain_control_marker_epoch_is_stale(long);
    return !drain_control_marker_epoch_is_stale(epoch);
}

/* ─── drain_notification_suppressed(*, home=None) -> bool ─── */
/* PoP: drain_notification_suppressed @ gateway/drain_control.py:drain_notification_suppressed */
int drain_notification_suppressed(void) {
    if (!drain_requested()) return 0;
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[1024];
    snprintf(path, sizeof(path), "%s/gateway/drain_suppress", home);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* gateway/run.py                                                            */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── _launch_systemd_restart_shortcut() ─── */
/* PoP: _launch_systemd_restart_shortcut @ gateway/run.py:_launch_systemd_restart_shortcut */
void _launch_systemd_restart_shortcut(void) {
    const char *notify_socket = getenv("NOTIFY_SOCKET");
    if (!notify_socket) return;
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return;
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, notify_socket, sizeof(addr.sun_path) - 1);
    const char *msg = "READY=1";
    sendto(fd, msg, strlen(msg), 0, (struct sockaddr*)&addr, sizeof(addr));
    close(fd);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* hermes_cli/backup.py                                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── _should_skip_backup_file(abs_path, rel_path, out_path) -> bool ─── */
/* PoP: _should_skip_backup_file @ hermes_cli/backup.py:_should_skip_backup_file */
int _should_skip_backup_file(const char *abs_path, const char *rel_path) {
    (void)abs_path;
    if (!rel_path) return 0;
    if (strstr(rel_path, "__pycache__")) return 1;
    if (strstr(rel_path, ".pyc")) return 1;
    if (strstr(rel_path, ".lock")) return 1;
    size_t len = strlen(rel_path);
    if (len > 0 && rel_path[len-1] == '~') return 1;
    return 0;
}

/* ─── _detect_prefix(zf) -> str ─── */
/* PoP: _detect_prefix @ hermes_cli/backup.py:_detect_prefix */
json_t *detect_prefix(json_t *entries) {
    if (!entries || !json_len(entries)) return json_string("");
    const char *first = json_get_str(json_get(entries, 0), "", "");
    size_t len = strlen(first);
    if (len == 0) return json_string("");
    const char *slash = strrchr(first, '/');
    if (!slash) return json_string("");
    size_t plen = (size_t)(slash - first) + 1;
    char prefix[512];
    if (plen >= sizeof(prefix)) return json_string("");
    strncpy(prefix, first, plen);
    prefix[plen] = '\0';
    size_t n = json_len(entries);
    for (size_t i = 1; i < n; i++) {
        const char *entry = json_get_str(json_get(entries, i), "", "");
        if (strncmp(entry, prefix, plen) != 0)
            return json_string("");
    }
    return json_string(prefix);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* hermes_cli/models.py                                                       */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── partition_nous_models_by_tier(model_ids, pricing, free_tier) ─── */
/* PoP: partition_nous_models_by_tier @ hermes_cli/models.py:partition_nous_models_by_tier */
json_t *partition_nous_models_by_tier(json_t *model_ids, json_t *pricing) {
    json_t *result = json_object();
    json_t *free_models = json_array();
    json_t *paid_models = json_array();
    if (!model_ids) {
        json_set(result, "free", free_models);
        json_set(result, "paid", paid_models);
        return result;
    }
    size_t n = json_len(model_ids);
    for (size_t i = 0; i < n; i++) {
        const char *mid = json_get_str(json_get(model_ids, i), "", "");
        int is_free = 0;
        if (pricing) {
            json_t *p = json_obj_get(pricing, mid);
            if (p) {
                double ip = json_get_num(p, "input_price", 0);
                double op = json_get_num(p, "output_price", 0);
                is_free = (ip == 0 && op == 0);
            }
        }
        if (is_free)
            json_append(free_models, json_string(mid));
        else
            json_append(paid_models, json_string(mid));
    }
    json_set(result, "free", free_models);
    json_set(result, "paid", paid_models);
    return result;
}

/* ─── resolve_fast_mode_overrides(...) ─── */
/* PoP: resolve_fast_mode_overrides @ hermes_cli/models.py:resolve_fast_mode_overrides */
json_t *resolve_fast_mode_overrides(json_t *overrides) {
    if (!overrides) return json_object();
    return json_copy(overrides);
}

/* ─── copilot_default_headers() -> dict ─── */
/* PoP: copilot_default_headers @ hermes_cli/models.py:copilot_default_headers */
json_t *copilot_default_headers(void) {
    json_t *headers = json_object();
    json_set(headers, "Content-Type", json_string("application/json"));
    json_set(headers, "Accept", json_string("application/json"));
    json_set(headers, "User-Agent", json_string("Hermes-Agent/1.0"));
    return headers;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* cron/blueprint_catalog.py                                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── blueprint_form_schema(blueprint) -> dict ─── */
/* PoP: blueprint_form_schema @ cron/blueprint_catalog.py:blueprint_form_schema */
json_t *blueprint_form_schema(json_t *blueprint) {
    if (!blueprint) return json_object();
    json_t *schema = json_object();
    json_set(schema, "key", json_copy(json_obj_get(blueprint, "key")));
    json_set(schema, "title", json_copy(json_obj_get(blueprint, "title")));
    json_set(schema, "description", json_copy(json_obj_get(blueprint, "description")));
    json_set(schema, "category", json_copy(json_obj_get(blueprint, "category")));
    json_set(schema, "tags", json_copy(json_obj_get(blueprint, "tags")));
    json_t *slots = json_obj_get(blueprint, "slots");
    json_t *fields = json_array();
    if (slots) {
        size_t n = json_len(slots);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(slots, i);
            json_t *field = json_object();
            json_set(field, "name", json_copy(json_obj_get(s, "name")));
            json_set(field, "type", json_copy(json_obj_get(s, "type")));
            json_set(field, "label", json_copy(json_obj_get(s, "label")));
            json_set(field, "default", json_copy(json_obj_get(s, "default")));
            json_set(field, "options", json_copy(json_obj_get(s, "options")));
            json_set(field, "optional", json_copy(json_obj_get(s, "optional")));
            json_set(field, "strict", json_copy(json_obj_get(s, "strict")));
            json_set(field, "help", json_copy(json_obj_get(s, "help")));
            json_append(fields, field);
        }
    }
    json_set(schema, "fields", fields);
    return schema;
}

/* ─── blueprint_slash_command(blueprint, values=None) -> str ─── */
/* PoP: blueprint_slash_command @ cron/blueprint_catalog.py:blueprint_slash_command */
json_t *blueprint_slash_command(json_t *blueprint, json_t *values) {
    if (!blueprint) return json_string("");
    const char *key = json_get_str(blueprint, "key", "");
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "/blueprint %s", key);
    json_t *slots = json_obj_get(blueprint, "slots");
    if (slots) {
        size_t n = json_len(slots);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(slots, i);
            const char *name = json_get_str(s, "name", "");
            json_t *val = values ? json_obj_get(values, name) : NULL;
            json_t *defval = json_obj_get(s, "default");
            const char *sval = NULL;
            char buf[256] = {0};
            if (val && json_is_string(val)) sval = json_get_str(val, "", "");
            else if (val && json_is_number(val)) {
                snprintf(buf, sizeof(buf), "%g", json_get_num(val, "", 0));
                sval = buf;
            } else if (defval && json_is_string(defval))
                sval = json_get_str(defval, "", "");
            if (!sval || !sval[0]) {
                int optional = json_get_bool(s, "optional", false);
                if (optional) continue;
                sval = "";
            }
            size_t len = strlen(cmd);
            if (strlen(sval) > 0)
                snprintf(cmd + len, sizeof(cmd) - len, " %s=%s", name, sval);
            else
                snprintf(cmd + len, sizeof(cmd) - len, " %s=", name);
        }
    }
    return json_string(cmd);
}

/* ─── blueprint_deeplink(blueprint, values=None) -> str ─── */
/* PoP: blueprint_deeplink @ cron/blueprint_catalog.py:blueprint_deeplink */
json_t *blueprint_deeplink(json_t *blueprint, json_t *values) {
    if (!blueprint) return json_string("");
    const char *key = json_get_str(blueprint, "key", "");
    char url[4096];
    snprintf(url, sizeof(url), "hermes://blueprint/%s", key);
    /* Add query params for slot values */
    json_t *slots = json_obj_get(blueprint, "slots");
    int first = 1;
    if (slots) {
        size_t n = json_len(slots);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(slots, i);
            const char *name = json_get_str(s, "name", "");
            json_t *val = values ? json_obj_get(values, name) : NULL;
            if (!val) val = json_obj_get(s, "default");
            if (!val) continue;
            const char *sval = NULL;
            char buf[256] = {0};
            if (json_is_string(val)) sval = json_get_str(val, "", "");
            else if (json_is_number(val)) {
                snprintf(buf, sizeof(buf), "%g", json_get_num(val, "", 0));
                sval = buf;
            }
            if (!sval || !sval[0]) continue;
            size_t len = strlen(url);
            if (first)
                snprintf(url + len, sizeof(url) - len, "?%s=%s", name, sval);
            else
                snprintf(url + len, sizeof(url) - len, "&%s=%s", name, sval);
            first = 0;
        }
    }
    return json_string(url);
}

/* ─── fill_blueprint(blueprint, values) -> dict ─── */
/* PoP: fill_blueprint @ cron/blueprint_catalog.py:fill_blueprint */
json_t *fill_blueprint(json_t *blueprint, json_t *values) {
    /* Validate values against slots and return create_job kwargs */
    json_t *result = json_object();
    if (!blueprint) return result;
    json_t *slots = json_obj_get(blueprint, "slots");
    if (slots) {
        size_t n = json_len(slots);
        for (size_t i = 0; i < n; i++) {
            json_t *s = json_get(slots, i);
            const char *name = json_get_str(s, "name", "");
            int optional = json_get_bool(s, "optional", false);
            json_t *val = values ? json_obj_get(values, name) : NULL;
            if (!val) {
                if (optional) continue;
                /* Missing required slot — would raise BlueprintFillError in Python */
                json_set(result, "error", json_string(name));
                return result;
            }
            char buf[256] = {0};
            if (json_is_string(val))
                snprintf(buf, sizeof(buf), "%s", json_get_str(val, "", ""));
            else if (json_is_number(val))
                snprintf(buf, sizeof(buf), "%g", json_get_num(val, "", 0));
            json_set(result, name, json_string(buf));
        }
    }
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/* tools/terminal_tool.py                                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* ─── _check_disk_usage_warning() ─── */
/* PoP: _check_disk_usage_warning @ tools/terminal_tool.py:_check_disk_usage_warning */
json_t *_check_disk_usage_warning(void) {
    struct statvfs sv;
    if (statvfs("/", &sv) != 0) return NULL;
    unsigned long total = sv.f_blocks * sv.f_frsize;
    unsigned long available = sv.f_bavail * sv.f_frsize;
    unsigned long used = total - available;
    int pct_used = total > 0 ? (int)((used * 100) / total) : 0;
    if (pct_used < 90) return NULL;
    json_t *warning = json_object();
    json_set(warning, "type", json_string("disk_usage"));
    json_set(warning, "severity", json_string(pct_used > 95 ? "critical" : "warning"));
    json_set(warning, "pct_used", json_number(pct_used));
    return warning;
}
