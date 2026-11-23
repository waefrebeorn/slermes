/*
 * main_dashboard.c — C home for upstream Hermes hermes_cli/main_dashboard.py.
 *
 * PoP: w12_maindashbo_f6142f_dashboard_probe_host @ hermes_cli/main_dashboard.py:_dashboard_probe_host
 * PoP: w12_maindashbo_f6142f_extract_scope_from_cgroup @ hermes_cli/main_dashboard.py:_extract_scope_from_cgroup
 * PoP: w12_maindashbo_f6142f_systemd_restart_commands @ hermes_cli/main_dashboard.py:_systemd_restart_commands
 * PoP: w12_maindashbo_f6142f_format_restart_commands @ hermes_cli/main_dashboard.py:_format_restart_commands
 * PoP: w12_maindashbo_f6142f_explicit_endpoint_flags @ hermes_cli/main_dashboard.py:_explicit_endpoint_flags
 * PoP: w12_maindashbo_f6142f_is_electron_packaged_web_dist @ hermes_cli/main_dashboard.py:_is_electron_packaged_web_dist
 */
#include "noc_w12_maindashbo_f6142f.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str.strip() with no argument: strip leading/trailing ASCII whitespace. */
static char *py_strip(const char *s) {
    if (!s) return strdup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, a, n); out[n] = '\0';
    return out;
}

/* Python str.strip("[]"): strip leading/trailing '[' or ']' characters. */
static char *py_strip_brackets(const char *s) {
    if (!s) return strdup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && (*a == '[' || *a == ']')) a++;
    while (b > a && (*(b - 1) == '[' || *(b - 1) == ']')) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, a, n); out[n] = '\0';
    return out;
}

char *w12_maindashbo_f6142f_dashboard_probe_host(const char *host) {
    char *a = py_strip(host && *host ? host : "127.0.0.1");
    char *b = py_strip_brackets(a);
    free(a);
    if (!*b || !strcmp(b, "0.0.0.0") || !strcmp(b, "::")) { free(b); return strdup("127.0.0.1"); }
    return b;
}

char *w12_maindashbo_f6142f_extract_scope_from_cgroup(const char *entry) {
    if (!entry) return NULL;
    if (strstr(entry, "/system.slice/")) return strdup("system");
    if (strstr(entry, "/user.slice/")) return strdup("user");
    return NULL;
}

struct json_t *w12_maindashbo_f6142f_systemd_restart_commands(const char *unit, const char *scope) {
    const char *u = unit ? unit : "";
    char user_cmd[1024], system_cmd[1024];
    snprintf(user_cmd, sizeof(user_cmd), "systemctl --user restart %s", u);
    snprintf(system_cmd, sizeof(system_cmd), "sudo systemctl restart %s", u);
    json_t *out = json_array();
    if (scope && !strcmp(scope, "user")) {
        json_append(out, json_string(user_cmd));
    } else if (scope && !strcmp(scope, "system")) {
        json_append(out, json_string(system_cmd));
    } else {
        json_append(out, json_string(user_cmd));
        json_append(out, json_string(system_cmd));
    }
    return out;
}

char *w12_maindashbo_f6142f_format_restart_commands(const struct json_t *commands) {
    char buf[16384];
    size_t n = 0;
    buf[0] = '\0';
    int first = 1;
    if (commands && commands->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(commands); ++i) {
            json_t *c = json_get(commands, i);
            const char *s = (c && c->type == JSON_STRING) ? c->str_val : "";
            if (!first) { size_t add = (size_t)snprintf(buf + n, sizeof(buf) - n, " or "); n += add; }
            first = 0;
            n += (size_t)snprintf(buf + n, sizeof(buf) - n, "`%s`", s ? s : "");
        }
    }
    (void)n;
    return strdup(buf);
}

struct json_t *w12_maindashbo_f6142f_explicit_endpoint_flags(const struct json_t *argv) {
    int have_host = 0, have_port = 0;
    if (argv && argv->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(argv); ++i) {
            json_t *v = json_get(argv, i);
            const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
            const char *eq = s ? strchr(s, '=') : NULL;
            size_t namelen = eq ? (size_t)(eq - s) : strlen(s ? s : "");
            if (namelen == 6 && !strncmp(s, "--host", 6)) have_host = 1;
            else if (namelen == 6 && !strncmp(s, "--port", 6)) have_port = 1;
        }
    }
    /* Python returns a set; emit sorted for a stable wire form. */
    json_t *out = json_array();
    if (have_host) json_append(out, json_string("host"));
    if (have_port) json_append(out, json_string("port"));
    return out;
}

int w12_maindashbo_f6142f_is_electron_packaged_web_dist(const char *path) {
    if (!path || !*path) return 0;
    /* path.replace("\\", "/") does not change whether "app.asar" is a substring. */
    return strstr(path, "app.asar") ? 1 : 0;
}
