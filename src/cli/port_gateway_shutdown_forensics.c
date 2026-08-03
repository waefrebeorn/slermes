/*
 * port_gateway_shutdown_forensics.c — C port of gateway/shutdown_forensics.py
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* PoP: cli_gateway_shutdown_forensics__signal_name @ gateway/shutdown_forensics.py:_signal_name */
const char* cli_gateway_shutdown_forensics__signal_name(int sig) {
    switch (sig) {
        case SIGTERM: return "SIGTERM";
        case SIGINT:  return "SIGINT";
        case SIGHUP:  return "SIGHUP";
#ifdef SIGQUIT
        case SIGQUIT: return "SIGQUIT";
#endif
#ifdef SIGUSR1
        case SIGUSR1: return "SIGUSR1";
#endif
#ifdef SIGUSR2
        case SIGUSR2: return "SIGUSR2";
#endif
        default: {
            static char buf[32];
            snprintf(buf, sizeof(buf), "signal#%d", sig);
            return buf;
        }
    }
}

/* PoP: cli_gateway_shutdown_forensics__read_proc_field @ gateway/shutdown_forensics.py:_read_proc_field */
char* cli_gateway_shutdown_forensics__read_proc_field(int pid, const char *key) {
    if (pid <= 0 || !key) return NULL;
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[256];
    char *result = NULL;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == ':') {
            const char *val = line + strlen(key) + 1;
            while (*val == ' ' || *val == '\t') val++;
            result = strdup(val);
            if (result) {
                size_t len = strlen(result);
                while (len > 0 && (result[len-1] == '\n' || result[len-1] == '\r')) result[--len] = '\0';
            }
            break;
        }
    }
    fclose(f);
    return result;
}

/* PoP: cli_gateway_shutdown_forensics__read_proc_cmdline @ gateway/shutdown_forensics.py:_read_proc_cmdline */
char* cli_gateway_shutdown_forensics__read_proc_cmdline(int pid) {
    if (pid <= 0) return NULL;
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char raw[4096];
    size_t n = fread(raw, 1, sizeof(raw) - 1, f);
    fclose(f);
    if (n == 0) return NULL;
    raw[n] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (raw[i] == '\0') raw[i] = ' ';
    }
    char *result = strdup(raw);
    if (result) {
        size_t len = strlen(result);
        while (len > 0 && result[len-1] == ' ') result[--len] = '\0';
    }
    return result;
}

/* PoP: cli_gateway_shutdown_forensics__proc_summary @ gateway/shutdown_forensics.py:_proc_summary */
json_node_t* cli_gateway_shutdown_forensics__proc_summary(int pid) {
    json_node_t *summary = json_new_object();
    if (!summary) return NULL;
    json_object_set(summary, "pid", json_new_number(pid));
    if (pid <= 0) return summary;
    char *name = cli_gateway_shutdown_forensics__read_proc_field(pid, "Name");
    if (name) { json_object_set(summary, "name", json_new_string(name)); free(name); }
    char *state = cli_gateway_shutdown_forensics__read_proc_field(pid, "State");
    if (state) { json_object_set(summary, "state", json_new_string(state)); free(state); }
    char *ppid_str = cli_gateway_shutdown_forensics__read_proc_field(pid, "PPid");
    if (ppid_str) { json_object_set(summary, "ppid", json_new_number(atoi(ppid_str))); free(ppid_str); }
    char *cmdline = cli_gateway_shutdown_forensics__read_proc_cmdline(pid);
    if (cmdline) {
        size_t len = strlen(cmdline);
        if (len > 300) cmdline[300] = '\0';
        json_object_set(summary, "cmdline", json_new_string(cmdline));
        free(cmdline);
    }
    return summary;
}

/* PoP: cli_gateway_shutdown_forensics_snapshot_shutdown_context @ gateway/shutdown_forensics.py:snapshot_shutdown_context */
json_node_t* cli_gateway_shutdown_forensics_snapshot_shutdown_context(int received_signal) {
    json_node_t *ctx = json_new_object();
    if (!ctx) return NULL;
    time_t now = time(NULL);
    pid_t pid = getpid();
    pid_t ppid = getppid();
    const char *sig_name = cli_gateway_shutdown_forensics__signal_name(received_signal);
    json_object_set(ctx, "ts", json_new_number(now));
    json_object_set(ctx, "signal", json_new_string(sig_name));
    json_object_set(ctx, "signal_num", json_new_number(received_signal));
    json_object_set(ctx, "pid", json_new_number(pid));
    json_object_set(ctx, "ppid", json_new_number(ppid));
    json_node_t *parent = cli_gateway_shutdown_forensics__proc_summary(ppid);
    if (parent) json_object_set(ctx, "parent", parent);
    json_node_t *self = cli_gateway_shutdown_forensics__proc_summary(pid);
    if (self) json_object_set(ctx, "self", self);
    const char *invocation_id = getenv("INVOCATION_ID");
    if (invocation_id) json_object_set(ctx, "systemd_invocation_id", json_new_string(invocation_id));
    json_object_set(ctx, "under_systemd", json_new_bool(invocation_id != NULL || ppid == 1));
    return ctx;
}

/* PoP: cli_gateway_shutdown_forensics_spawn_async_diagnostic @ gateway/shutdown_forensics.py:spawn_async_diagnostic */
int cli_gateway_shutdown_forensics_spawn_async_diagnostic(const char *log_path, const char *signal_name) {
    if (!log_path || !signal_name) return -1;
    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;
    pid_t child = fork();
    if (child == 0) {
        setsid();
        close(0);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
        char script[2048];
        snprintf(script, sizeof(script),
            "echo '=== shutdown diagnostic @ %s ==='\n"
            "date -u +%%Y-%%m-%%dT%%H:%%M:%%SZ\n"
            "ps auxf --sort=-pcpu 2>/dev/null | head -60\n"
            "echo '=== end ==='\n", signal_name);
        execlp("bash", "bash", "-c", script, NULL);
        _exit(1);
    }
    close(fd);
    return child > 0 ? child : -1;
}

/* PoP: cli_gateway_shutdown_forensics_format_context_for_log @ gateway/shutdown_forensics.py:format_context_for_log */
char* cli_gateway_shutdown_forensics_format_context_for_log(json_node_t *ctx) {
    if (!ctx) return strdup("(null)");
    const char *sig = json_object_get_string(ctx, "signal", "?");
    json_node_t *parent = json_object_get(ctx, "parent");
    const char *parent_cmd = parent ? json_object_get_string(parent, "cmdline", "(unknown)") : "(unknown)";
    const char *parent_name = parent ? json_object_get_string(parent, "name", "?") : "?";
    /* parent_pid: number; "?" when missing (Python: parent.get("pid") or "?") */
    char parent_pid[32];
    json_node_t *pid_node = parent ? json_object_get(parent, "pid") : NULL;
    if (pid_node && !json_is_null(pid_node)) {
        snprintf(parent_pid, sizeof(parent_pid), "%g", json_get_num(parent, "pid", 0));
    } else {
        snprintf(parent_pid, sizeof(parent_pid), "?");
    }
    int under_systemd = json_get_bool(ctx, "under_systemd", false);
    /* loadavg_1m: "?" when missing/null (Python: isinstance(load,(int,float)) else "?") */
    json_node_t *load_node = json_object_get(ctx, "loadavg_1m");
    char load_str[32];
    if (load_node && !json_is_null(load_node)) {
        snprintf(load_str, sizeof(load_str), "%.2f", json_get_num(ctx, "loadavg_1m", 0.0));
    } else {
        snprintf(load_str, sizeof(load_str), "?");
    }
    /* extras */
    char extras[512];
    extras[0] = '\0';
    json_node_t *takeover = json_object_get(ctx, "takeover_marker");
    if (takeover && !json_is_null(takeover)) {
        int for_self = json_get_bool(ctx, "takeover_marker_for_self", false);
        snprintf(extras, sizeof(extras), " takeover_marker_present=%s", for_self ? "self" : "other");
    }
    json_node_t *planned = json_object_get(ctx, "planned_stop_marker");
    if (planned && !json_is_null(planned)) {
        strncat(extras, " planned_stop_marker_present=yes", sizeof(extras) - strlen(extras) - 1);
    }
    json_node_t *tracer = json_object_get(ctx, "tracer_pid");
    if (tracer && !json_is_null(tracer)) {
        char tbuf[64];
        snprintf(tbuf, sizeof(tbuf), " tracer_pid=%g", json_get_num(ctx, "tracer_pid", 0));
        strncat(extras, tbuf, sizeof(extras) - strlen(extras) - 1);
    }
    /* parent_cmdline is emitted repr-style (single-quoted) like Python !r */
    size_t need = strlen(sig) + strlen(parent_pid) + strlen(parent_name)
                + strlen(load_str) + strlen(parent_cmd) + strlen(extras) + 128;
    char *buf = malloc(need);
    if (!buf) return strdup("(oom)");
    snprintf(buf, need,
        "signal=%s under_systemd=%s parent_pid=%s parent_name=%s loadavg_1m=%s%s parent_cmdline='%s'",
        sig, under_systemd ? "yes" : "no", parent_pid, parent_name, load_str, extras, parent_cmd);
    return buf;
}

/* PoP: cli_gateway_shutdown_forensics_context_as_json @ gateway/shutdown_forensics.py:context_as_json */
char* cli_gateway_shutdown_forensics_context_as_json(json_node_t *ctx) {
    if (!ctx) return strdup("{}");
    char *json_str = json_serialize(ctx);
    return json_str ? json_str : strdup("{}");
}

/* PoP: cli_gateway_shutdown_forensics_check_systemd_timing_alignment @ gateway/shutdown_forensics.py:check_systemd_timing_alignment */
json_node_t* cli_gateway_shutdown_forensics_check_systemd_timing_alignment(double drain_timeout) {
    const char *invocation_id = getenv("INVOCATION_ID");
    if (!invocation_id) return NULL;
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    json_object_set(result, "drain_timeout", json_new_number(drain_timeout));
    json_object_set(result, "under_systemd", json_new_bool(true));
    hermes_log(LOG_DEBUG, "shutdown_forensics", "check_systemd_timing: drain_timeout=%.1f", drain_timeout);
    return result;
}

/* PoP: cli_gateway_shutdown_forensics__parse_systemd_duration_to_us @ gateway/shutdown_forensics.py:_parse_systemd_duration_to_us */
/* Faithful port of the Python tokenizer: digits/'.' accumulate a number,
 * alpha accumulates a unit token; on whitespace the pending (digits, unit)
 * pair is applied. Units: us/ms/s/sec/min/h/hr. A bare number (no unit)
 * means SECONDS. Unknown unit, empty input, or total==0 => None, which we
 * signal as -1 (the Python return type is Optional[int]). */
long cli_gateway_shutdown_forensics__parse_systemd_duration_to_us(const char *raw) {
    if (!raw || !*raw) return -1;
    static const struct { const char *u; long mult; } UNITS[] = {
        { "us", 1 }, { "ms", 1000 }, { "s", 1000000 }, { "sec", 1000000 },
        { "min", 60000000 }, { "h", 3600000000L }, { "hr", 3600000000L },
    };
    double total_us = 0;
    char digits[128] = "";   /* pending number text (digits + '.') */
    size_t dn = 0;
    char token[32] = "";     /* pending unit text (alpha) */
    size_t tn = 0;

    for (const char *p = raw; ; p++) {
        char ch = *p;
        int is_digit = (ch >= '0' && ch <= '9');
        int is_alpha = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        if (is_digit || ch == '.') {
            if (tn > 0) {
                /* End previous unit, start new number. */
                if (dn == 0) return -1;
                long mult = -1;
                for (size_t i = 0; i < sizeof(UNITS)/sizeof(UNITS[0]); i++) {
                    if (strcmp(token, UNITS[i].u) == 0) { mult = UNITS[i].mult; break; }
                }
                if (mult < 0) return -1;  /* unknown unit */
                double num = atof(digits);
                total_us += num * (double)mult;
                dn = 0; tn = 0;
            }
            if (dn < sizeof(digits) - 1) digits[dn++] = ch;
            digits[dn] = '\0';
        } else if (is_alpha) {
            if (tn < sizeof(token) - 1) token[tn++] = (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
            token[tn] = '\0';
        } else if (dn > 0 && tn > 0) {
            /* whitespace / other: apply pending (digits, unit) */
            long mult = -1;
            for (size_t i = 0; i < sizeof(UNITS)/sizeof(UNITS[0]); i++) {
                if (strcmp(token, UNITS[i].u) == 0) { mult = UNITS[i].mult; break; }
            }
            if (mult < 0) return -1;
            double num = atof(digits);
            total_us += num * (double)mult;
            dn = 0; tn = 0;
        } else if (dn > 0 && tn == 0) {
            /* Bare number = seconds */
            double num = atof(digits);
            total_us += num * 1000000.0;
            dn = 0;
        }
        if (ch == '\0') break;
    }
    return total_us > 0 ? (long)total_us : -1;
}
