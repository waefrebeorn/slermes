/*
 * gateway_launchd.c — C port of upstream Hermes hermes_cli/gateway_launchd.py.
 *
 * PoP: w13_gatewaylau_3aa886_constants @ hermes_cli/gateway_launchd.py (module constants)
 * PoP: w13_gatewaylau_3aa886_get_label @ hermes_cli/gateway_launchd.py:get_launchd_label
 * PoP: w13_gatewaylau_3aa886_error_indicates_unloaded @ hermes_cli/gateway_launchd.py:_launchd_error_indicates_unloaded
 * PoP: w13_gatewaylau_3aa886_domain_unsupported @ hermes_cli/gateway_launchd.py:_launchctl_domain_unsupported
 * PoP: w13_gatewaylau_3aa886_program_arguments @ hermes_cli/gateway_launchd.py:launchd_program_arguments
 *
 * The profile suffix (a facade seam) is injected by the fixture. Command wrapping
 * mirrors shlex.join/quote + json.dumps for ASCII paths.
 */
#include "noc_w13_gatewaylau_3aa886.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_t *w13_gatewaylau_3aa886_constants(void) {
    json_t *o = json_object();
    json_t *a = json_array();
    json_append(a, json_number(3)); json_append(a, json_number(113)); json_append(a, json_number(125));
    json_set(o, "JOB_UNLOADED", a);
    json_t *b = json_array();
    json_append(b, json_number(5)); json_append(b, json_number(125));
    json_set(o, "DOMAIN_UNSUPPORTED", b);
    json_set(o, "BOOTSTRAP_EIO", json_number(5));
    return o;
}

struct json_t *w13_gatewaylau_3aa886_get_label(const char *suffix) {
    char buf[512];
    if (suffix && *suffix) snprintf(buf, sizeof(buf), "ai.hermes.gateway-%s", suffix);
    else snprintf(buf, sizeof(buf), "ai.hermes.gateway");
    return json_string(buf);
}

struct json_t *w13_gatewaylau_3aa886_error_indicates_unloaded(int returncode) {
    return json_bool((returncode == 3 || returncode == 113 || returncode == 125) ? true : false);
}

struct json_t *w13_gatewaylau_3aa886_domain_unsupported(int returncode) {
    return json_bool((returncode == 5 || returncode == 125) ? true : false);
}

/* Python shlex.quote (POSIX) for ASCII input. */
static int shell_safe(unsigned char c) {
    return isalnum(c) || strchr("_@%+=:,./-", c) != NULL;
}

static char *quote_dup(const char *s) {
    if (!s || !*s) {
        char *r = (char *)malloc(3); r[0] = r[1] = '\''; r[2] = '\0'; return r;
    }
    int safe = 1;
    for (const char *p = s; *p; p++) if (!shell_safe((unsigned char)*p)) { safe = 0; break; }
    if (safe) return strdup(s);
    size_t cap = strlen(s) * 5 + 3, n = 0;
    char *r = (char *)malloc(cap);
    r[n++] = '\'';
    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            r[n++] = '\''; r[n++] = '"'; r[n++] = '\''; r[n++] = '"'; r[n++] = '\'';
        } else {
            r[n++] = *p;
        }
    }
    r[n++] = '\''; r[n] = '\0';
    return r;
}

static char *join_command(const struct json_t *arr) {
    size_t total = 1;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        total += (v && v->type == JSON_STRING && v->str_val ? strlen(v->str_val) : 0) + 6 + 1;
    }
    char *out = (char *)calloc(1, total + 16);
    size_t n = 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        char *q = quote_dup(v && v->type == JSON_STRING ? v->str_val : "");
        if (i) out[n++] = ' ';
        size_t ql = strlen(q);
        memcpy(out + n, q, ql); n += ql;
        free(q);
    }
    out[n] = '\0';
    return out;
}

struct json_t *w13_gatewaylau_3aa886_program_arguments(const struct json_t *command,
                                                       const char *stdout_log, const char *stderr_log) {
    char *cmd = join_command(command);
    char *so = quote_dup(stdout_log), *se = quote_dup(stderr_log);
    char shell[16384];
    snprintf(shell, sizeof(shell), "exec %s >> %s 2>> %s", cmd, so, se);
    char *js = json_escape_string(shell); /* NULL-free JSON literal, includes surrounding quotes */
    char script[20480];
    snprintf(script, sizeof(script),
             "ObjC.import(\"stdlib\"); const status=$.system(%s); const signal=status & 127; "
             "$.exit(status === -1 ? 1 : signal === 0 ? (status >> 8) & 255 : 128 + signal);",
             js ? js : "\"\"");
    json_t *a = json_array();
    json_append(a, json_string("/usr/bin/osascript"));
    json_append(a, json_string("-l"));
    json_append(a, json_string("JavaScript"));
    json_append(a, json_string("-e"));
    json_append(a, json_string(script));
    free(cmd); free(so); free(se); free(js);
    return a;
}
