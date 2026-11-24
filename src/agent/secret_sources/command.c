/*
 * command.c -- real C11 port of upstream Hermes
 *   agent/secret_sources/command.py (the ``secrets.command`` helper source).
 *
 * PoP: w20_command_79acbf_unquote_dotenv_value @ agent/secret_sources/command.py:unquote_dotenv_value
 * PoP: w20_command_79acbf_parse_dotenv_map @ agent/secret_sources/command.py:_parse_dotenv_map
 * PoP: w20_command_79acbf_fetch_plan @ agent/secret_sources/command.py:CommandSource.fetch
 * PoP: w20_command_79acbf_config_schema @ agent/secret_sources/command.py:CommandSource.config_schema
 *
 * fetch()'s spawn (Popen via /bin/sh -c, hard timeout, killpg, stderr
 * discard) needs a live process table and stays upstream; the port models the
 * decision shaping around it with the child's stdout injected.
 */
#include "hermes_json.h"
#include "noc_w20_command_79acbf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *cmd79_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int cmd79_is_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() for the ASCII whitespace the module's inputs carry. */
static char *cmd79_strip(const char *s) {
    size_t start = 0, end;
    if (!s) return cmd79_dup("");
    end = strlen(s);
    while (start < end && cmd79_is_space((unsigned char)s[start])) start++;
    while (end > start && cmd79_is_space((unsigned char)s[end - 1])) end--;
    {
        char *out = (char *)malloc(end - start + 1);
        if (!out) return NULL;
        memcpy(out, s + start, end - start);
        out[end - start] = '\0';
        return out;
    }
}

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int cmd79_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int cmd79_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* str() of the wire scalar shapes a config value can carry. */
static char *cmd79_str_scalar(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return cmd79_dup("");
    switch (v->type) {
    case JSON_BOOL:   return cmd79_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return cmd79_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (cmd79_is_py_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return cmd79_dup(buf);
    default:
        return cmd79_dup("");
    }
}

/* --- unquote_dotenv_value ------------------------------------------------- */

char *w20_command_79acbf_unquote_dotenv_value(const char *raw) {
    char *t = cmd79_strip(raw);
    size_t n;
    if (!t) return NULL;
    n = strlen(t);
    if (n >= 2 && t[0] == t[n - 1] && (t[0] == '"' || t[0] == '\'')) {
        char *out = (char *)malloc(n - 1);
        if (!out) { free(t); return NULL; }
        memcpy(out, t + 1, n - 2);
        out[n - 2] = '\0';
        free(t);
        return out;
    }
    return t;
}

/* --- _parse_dotenv_map ---------------------------------------------------- */

/* ^([A-Za-z_][A-Za-z0-9_]*)=(.*)$ -- anchored, `.` never crosses a newline. */
static int cmd79_split_env_line(const char *line, char **key_out, const char **value_out) {
    size_t i = 0;
    char *key;
    if (!line) return 0;
    if (!((line[0] >= 'A' && line[0] <= 'Z') || (line[0] >= 'a' && line[0] <= 'z') || line[0] == '_')) {
        return 0;
    }
    i = 1;
    while ((line[i] >= 'A' && line[i] <= 'Z') || (line[i] >= 'a' && line[i] <= 'z') ||
           (line[i] >= '0' && line[i] <= '9') || line[i] == '_') {
        i++;
    }
    if (line[i] != '=') return 0;
    key = (char *)malloc(i + 1);
    if (!key) return 0;
    memcpy(key, line, i);
    key[i] = '\0';
    *key_out = key;
    *value_out = line + i + 1;
    return 1;
}

json_t *w20_command_79acbf_parse_dotenv_map(const char *stdout_text) {
    json_t *out = json_object();
    char *norm;
    size_t i, len, start = 0;
    if (!stdout_text) return out;

    /* stdout.replace("\r\n", "\n") */
    len = strlen(stdout_text);
    norm = (char *)malloc(len + 1);
    if (!norm) return out;
    {
        size_t w = 0;
        for (i = 0; i < len; i++) {
            if (stdout_text[i] == '\r' && i + 1 < len && stdout_text[i + 1] == '\n') continue;
            norm[w++] = stdout_text[i];
        }
        norm[w] = '\0';
        len = w;
    }

    for (i = 0; i <= len; i++) {
        if (i == len || norm[i] == '\n') {
            char *raw = (char *)malloc(i - start + 1);
            if (raw) {
                char *line;
                memcpy(raw, norm + start, i - start);
                raw[i - start] = '\0';
                line = cmd79_strip(raw);
                free(raw);
                if (line) {
                    if (line[0] && line[0] != '#') {
                        char *key = NULL;
                        const char *value = NULL;
                        if (cmd79_split_env_line(line, &key, &value) && key) {
                            char *unquoted = w20_command_79acbf_unquote_dotenv_value(value);
                            json_set(out, key, json_string(unquoted ? unquoted : ""));
                            free(unquoted);
                            free(key);
                        }
                    }
                    free(line);
                }
            }
            start = i + 1;
        }
    }
    free(norm);
    return out;
}

/* --- coerce_float --------------------------------------------------------- */

/* float(value) with `default` for malformed config values (base.coerce_float). */
static double cmd79_coerce_float(const json_t *v, double def) {
    if (!v || v->type == JSON_NULL) return def;          /* TypeError */
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s;
        char *end = NULL;
        double d;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
        if (*p == '\0') return def;                       /* float("") -> ValueError */
        {
            const char *q = (*p == '+' || *p == '-') ? p + 1 : p;
            if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X')) return def;  /* float() rejects hex */
        }
        if (!strcmp(p, "inf") || !strcmp(p, "+inf") || !strcmp(p, "Infinity")) return INFINITY;
        if (!strcmp(p, "-inf") || !strcmp(p, "-Infinity")) return -INFINITY;
        if (!strcmp(p, "nan") || !strcmp(p, "+nan") || !strcmp(p, "-nan")) return NAN;
        d = strtod(p, &end);
        if (end == p) return def;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') end++;
        if (*end != '\0') return def;                     /* trailing junk -> ValueError */
        return d;
    }
    return def;                                           /* TypeError */
}

/* --- CommandSource.fetch -------------------------------------------------- */

static json_t *cmd79_result(const char *error, const char *kind) {
    json_t *out = json_object();
    json_set(out, "error", error ? json_string(error) : json_null());
    json_set(out, "error_kind", kind ? json_string(kind) : json_null());
    json_set(out, "warnings", json_array());
    json_set(out, "secrets", json_object());
    return out;
}

json_t *w20_command_79acbf_fetch_plan(const json_t *cfg, int is_windows,
                                      const json_t *helper_stdout) {
    static const char *const NOT_CONFIGURED_MSG =
        "secrets.command.enabled is true but secrets.command.command is empty.  "
        "Set the helper command in config.yaml.";
    static const char *const WINDOWS_MSG =
        "the 'command' secret source is POSIX-only (needs /bin/sh); skipping on Windows";
    static const char *const FAILED_MSG =
        "helper command failed (see structured fields above); no secrets applied";
    static const char *const EMPTY_MSG =
        "helper output was not a KEY=VALUE map; nothing to apply";
    json_t *out, *warnings, *secrets;
    json_t *raw_command;
    char *command;
    double timeout;

    if (!cfg || cfg->type != JSON_OBJECT) cfg = NULL;   /* cfg if isinstance(cfg, dict) else {} */

    raw_command = cfg ? json_obj_get(cfg, "command") : NULL;
    command = cmd79_str_scalar(cmd79_truthy(raw_command) ? raw_command : NULL);
    {
        char *stripped = cmd79_strip(command ? command : "");
        free(command);
        command = stripped;
    }

    out = cmd79_result(NULL, NULL);
    json_set(out, "timeout",
             json_number(cmd79_coerce_float(cfg ? json_obj_get(cfg, "helper_timeout_seconds") : NULL, 3.0)));

    if (!command || !command[0]) {
        json_t *r = cmd79_result(NOT_CONFIGURED_MSG, "not_configured");
        json_set(r, "timeout", json_number(cmd79_coerce_float(
                     cfg ? json_obj_get(cfg, "helper_timeout_seconds") : NULL, 3.0)));
        free(command);
        json_free(out);
        return r;
    }
    free(command);

    if (is_windows) {
        json_t *r = cmd79_result(WINDOWS_MSG, "not_configured");
        json_set(r, "timeout", json_number(cmd79_coerce_float(
                     cfg ? json_obj_get(cfg, "helper_timeout_seconds") : NULL, 3.0)));
        json_free(out);
        return r;
    }

    timeout = json_get_num(out, "timeout", 3.0);
    (void)timeout;                                       /* already carried in "out" */

    if (!helper_stdout || helper_stdout->type != JSON_STRING) {
        json_t *r = cmd79_result(FAILED_MSG, "internal");
        json_set(r, "timeout", json_number(cmd79_coerce_float(
                     cfg ? json_obj_get(cfg, "helper_timeout_seconds") : NULL, 3.0)));
        json_free(out);
        return r;
    }

    secrets = w20_command_79acbf_parse_dotenv_map(helper_stdout->str_val);
    if (json_object_size(secrets) == 0) {
        warnings = json_array();
        json_append(warnings, json_string(EMPTY_MSG));
        json_set(out, "warnings", warnings);
        json_set(out, "secrets", secrets);
        return out;
    }
    json_set(out, "secrets", secrets);
    return out;
}

/* --- config_schema -------------------------------------------------------- */

json_t *w20_command_79acbf_config_schema(void) {
    json_t *out = json_object();
    json_t *enabled = json_object();
    json_t *command = json_object();
    json_t *timeout = json_object();
    json_t *override = json_object();
    json_set(enabled, "description", json_string("Master switch"));
    json_set(enabled, "default", json_bool(0));
    json_set(command, "description",
             json_string("Helper run via /bin/sh -c; must print a KEY=VALUE blob on stdout"));
    json_set(command, "default", json_string(""));
    json_set(timeout, "description", json_string("Hard timeout for one helper run"));
    json_set(timeout, "default", json_number(3.0));
    json_set(override, "description", json_string("Helper values overwrite .env/shell values"));
    json_set(override, "default", json_bool(0));
    json_set(out, "enabled", enabled);
    json_set(out, "command", command);
    json_set(out, "helper_timeout_seconds", timeout);
    json_set(out, "override_existing", override);
    return out;
}
