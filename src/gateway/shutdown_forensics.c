/*
 * shutdown_forensics.c — Capture context when the gateway receives SIGTERM/SIGINT.
 *
 * Port of Python gateway/shutdown_forensics.py.
 *
 * The shutdown_signal_handler runs synchronously inside the event loop.
 * This module provides a fast (<10ms) non-blocking probe that returns a
 * structured dict, plus a fire-and-forget spawn_async_diagnostic that runs
 * as a detached subprocess so it can't block teardown even if /proc is wedged.
 */

#include "hermes_core_types.h"
#include "hermes_gateway_forensics.h"
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

/* ================================================================
 *  Internal: signal name lookup
 *  Port of Python _signal_name()
 * ================================================================ */

/* PoP: forensics_signal_name @ gateway/shutdown_forensics.py:_signal_name */

static const char *forensics_signal_name(int sig, char *buf, size_t buf_size) {
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
        default:
            snprintf(buf, buf_size, "signal#%d", sig);
            return buf;
    }
}

/* ================================================================
 *  Internal: read a single field from /proc/<pid>/status
 *  Port of Python _read_proc_field()
 * ================================================================ */

/* PoP: forensics_read_proc_field @ gateway/shutdown_forensics.py:_read_proc_field */

static char *forensics_read_proc_field(pid_t pid, const char *key) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    char line[256];
    char *result = NULL;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == ':') {
            const char *val = line + strlen(key) + 1;
            while (*val == ' ' || *val == '\t') val++;
            size_t len = strlen(val);
            while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r')) len--;
            result = malloc(len + 1);
            if (result) {
                memcpy(result, val, len);
                result[len] = '\0';
            }
            break;
        }
    }
    fclose(f);
    return result;
}

/* ================================================================
 *  Internal: read /proc/<pid>/cmdline as a printable string
 *  Port of Python _read_proc_cmdline()
 * ================================================================ */

/* PoP: forensics_read_proc_cmdline @ gateway/shutdown_forensics.py:_read_proc_cmdline */

static char *forensics_read_proc_cmdline(pid_t pid) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* Read up to 4KB */
    char raw[4096];
    size_t n = fread(raw, 1, sizeof(raw) - 1, f);
    fclose(f);
    if (n == 0) return NULL;

    raw[n] = '\0';
    /* Replace NUL separators with spaces */
    for (size_t i = 0; i < n; i++) {
        if (raw[i] == '\0') raw[i] = ' ';
    }

    char *result = strdup(raw);
    /* Trim trailing spaces */
    if (result) {
        size_t len = strlen(result);
        while (len > 0 && result[len-1] == ' ') result[--len] = '\0';
    }
    return result;
}

/* ================================================================
 *  Internal: compact /proc/<pid> snapshot
 *  Port of Python _proc_summary()
 * ================================================================ */

/* PoP: forensics_proc_summary @ gateway/shutdown_forensics.py:_proc_summary */

static json_node_t *forensics_proc_summary(pid_t pid) {
    json_node_t *summary = json_new_object();
    if (!summary) return NULL;

    json_object_set(summary, "pid", json_new_number(pid));

    if (pid <= 0) return summary;

    char *name = forensics_read_proc_field(pid, "Name");
    if (name) {
        json_object_set(summary, "name", json_new_string(name));
        free(name);
    }

    char *state = forensics_read_proc_field(pid, "State");
    if (state) {
        json_object_set(summary, "state", json_new_string(state));
        free(state);
    }

    char *ppid_str = forensics_read_proc_field(pid, "PPid");
    if (ppid_str) {
        json_object_set(summary, "ppid", json_new_number(atol(ppid_str)));
        free(ppid_str);
    }

    char *uid = forensics_read_proc_field(pid, "Uid");
    if (uid) {
        /* "real effective saved fs" — take first token */
        char *space = strchr(uid, ' ');
        if (space) *space = '\0';
        json_object_set(summary, "uid", json_new_string(uid));
        free(uid);
    }

    char *cmdline = forensics_read_proc_cmdline(pid);
    if (cmdline) {
        /* Truncate to 300 chars */
        size_t len = strlen(cmdline);
        if (len > 300) cmdline[300] = '\0';
        json_object_set(summary, "cmdline", json_new_string(cmdline));
        free(cmdline);
    }

    return summary;
}

/* ================================================================
 *  Fast (<10ms) snapshot of who/what is asking us to shut down
 *  Port of Python snapshot_shutdown_context()
 * ================================================================ */

/* PoP: forensics_snapshot_context @ gateway/shutdown_forensics.py:snapshot_shutdown_context */

json_node_t *forensics_snapshot_context(int received_signal) {
    json_node_t *ctx = json_new_object();
    if (!ctx) return NULL;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double mono = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;

    pid_t pid = getpid();
    pid_t ppid = getppid();
    char sig_buf[32];

    json_object_set(ctx, "ts", json_new_number(now));
    json_object_set(ctx, "ts_monotonic", json_new_number(mono));
    json_object_set(ctx, "signal", json_new_string(forensics_signal_name(received_signal, sig_buf, sizeof(sig_buf))));
    json_object_set(ctx, "signal_num", json_new_number(received_signal));
    json_object_set(ctx, "pid", json_new_number((int)pid));
    json_object_set(ctx, "ppid", json_new_number((int)ppid));

    /* Parent and self proc summaries */
    json_node_t *parent = forensics_proc_summary(ppid);
    if (parent) json_object_set(ctx, "parent", parent);

    json_node_t *self = forensics_proc_summary(pid);
    if (self) json_object_set(ctx, "self", self);

    /* systemd context */
    const char *invocation_id = getenv("INVOCATION_ID");
    if (invocation_id) {
        json_object_set(ctx, "systemd_invocation_id", json_new_string(invocation_id));
    }
    const char *journal_stream = getenv("JOURNAL_STREAM");
    if (journal_stream) {
        json_object_set(ctx, "systemd_journal_stream", json_new_string(journal_stream));
    }
    json_object_set(ctx, "under_systemd",
                    json_new_bool(invocation_id != NULL || ppid == 1));

    /* Load average */
    double loadavg[3];
    if (getloadavg(loadavg, 3) == 3) {
        char loadbuf[32];
        snprintf(loadbuf, sizeof(loadbuf), "%.2f", loadavg[0]);
        json_object_set(ctx, "loadavg_1m", json_new_number(loadavg[0]));
    }

    /* TracerPid check */
    char *tracer = forensics_read_proc_field(pid, "TracerPid");
    if (tracer) {
        int tracer_pid = atoi(tracer);
        if (tracer_pid > 0) {
            json_object_set(ctx, "tracer_pid", json_new_number(tracer_pid));
            json_node_t *tracer_summary = forensics_proc_summary(tracer_pid);
            if (tracer_summary) {
                json_object_set(ctx, "tracer", tracer_summary);
            }
        }
        free(tracer);
    }

    /* Takeover/planned-stop markers on disk */
    const char *hermes_home = getenv("HERMES_HOME");
    if (hermes_home) {
        char takeover_path[1024];
        snprintf(takeover_path, sizeof(takeover_path),
                 "%s/.gateway-takeover.json", hermes_home);
        FILE *tf = fopen(takeover_path, "r");
        if (tf) {
            char raw[512] = {0};
            size_t n = fread(raw, 1, sizeof(raw) - 1, tf);
            fclose(tf);
            raw[n] = '\0';
            json_object_set(ctx, "takeover_marker", json_new_string(raw));
            /* Check if marker mentions our PID */
            char needle[64];
            snprintf(needle, sizeof(needle), "target_pid\": %d", (int)pid);
            json_object_set(ctx, "takeover_marker_for_self",
                            json_new_bool(strstr(raw, needle) != NULL));
        }
        char planned_stop_path[1024];
        snprintf(planned_stop_path, sizeof(planned_stop_path),
                 "%s/.gateway-planned-stop.json", hermes_home);
        FILE *psf = fopen(planned_stop_path, "r");
        if (psf) {
            char raw[512] = {0};
            size_t n = fread(raw, 1, sizeof(raw) - 1, psf);
            fclose(psf);
            raw[n] = '\0';
            json_object_set(ctx, "planned_stop_marker", json_new_string(raw));
        }
    }

    return ctx;
}

/* ================================================================
 *  Fire-and-forget ps-style snapshot written to log_path
 *  Port of Python spawn_async_diagnostic()
 * ================================================================ */

/* PoP: forensics_spawn_diagnostic @ gateway/shutdown_forensics.py:spawn_async_diagnostic */

int forensics_spawn_diagnostic(const char *log_path,
                                const char *signal_name,
                                int timeout_seconds) {
    if (!log_path || !signal_name) return -1;

    /* Ensure parent directory exists */
    char dir[1024];
    size_t plen = strlen(log_path);
    while (plen > 0 && log_path[plen-1] != '/') plen--;
    if (plen > 0) {
        memcpy(dir, log_path, plen);
        dir[plen] = '\0';
        struct stat st;
        if (stat(dir, &st) != 0) {
            /* mkdir -p */
            char tmp[1024];
            snprintf(tmp, sizeof(tmp), "mkdir -p '%s' 2>/dev/null", dir);
            system(tmp);
        }
    }

    /* Open log file in append mode */
    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;

    /* Format timeout string for execlp */
    char timeout_str[16];
    snprintf(timeout_str, sizeof(timeout_str), "%d",
             timeout_seconds > 0 ? timeout_seconds : 30);

    pid_t child = fork();
    if (child == 0) {
        /* Child: detach from process group, redirect stdout to fd, exec bash */
        setsid();
        close(0); /* close stdin */
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
        /* Build the diagnostic script */
        char script[4096];
        snprintf(script, sizeof(script),
            "echo '=== shutdown diagnostic @ %s ==='\n"
            "echo '--- date ---'\ndate -u +%%Y-%%m-%%dT%%H:%%M:%%SZ\n"
            "echo '--- ps auxf (top 60 by cpu) ---'\n"
            "ps auxf --sort=-pcpu 2>/dev/null | head -60\n"
            "echo '--- pstree of self ---'\n"
            "pstree -plau %d 2>/dev/null | head -40 || true\n"
            "echo '--- /proc/loadavg ---'\n"
            "cat /proc/loadavg 2>/dev/null || true\n"
            "echo '--- recent dmesg/syslog ---'\n"
            "dmesg -T 2>/dev/null | tail -20 || journalctl --user -n 20 --no-pager 2>/dev/null | tail -20 || true\n"
            "echo '=== end ==='\n",
            signal_name, (int)getpid());

        execlp("timeout", "timeout",
               timeout_str,
               "bash", "-c", script, NULL);
        execlp("bash", "bash", "-c", script, NULL);
        _exit(1);
    }

    close(fd);
    return child > 0 ? (int)child : -1;
}

/* ================================================================
 *  Render shutdown context dict as scannable log line
 *  Port of Python format_context_for_log()
 * ================================================================ */

/* PoP: forensics_format_context @ gateway/shutdown_forensics.py:format_context_for_log */

char *forensics_format_context(json_node_t *ctx) {
    if (!ctx) return strdup("(null context)");

    const char *sig = json_object_get_string(ctx, "signal", "?");
    json_node_t *parent = json_object_get(ctx, "parent");
    const char *parent_cmd = parent ? json_object_get_string(parent, "cmdline", "(unknown)") : "(unknown)";
    const char *parent_name = parent ? json_object_get_string(parent, "name", "?") : "?";
    json_node_t *parent_pid_node = parent ? json_object_get(parent, "pid") : NULL;
    bool under_systemd = json_get_bool(ctx, "under_systemd", false);
    double loadavg = json_object_get_number(ctx, "loadavg_1m", -1.0);
    bool takeover = json_object_get_bool(ctx, "takeover_marker_for_self", false);
    bool planned_stop = json_has(ctx, "planned_stop_marker");

    /* Determine parent pid */
    int ppid_val = 0;
    if (parent_pid_node && parent_pid_node->type == JSON_NUMBER) {
        double dv = parent_pid_node->num_val;
        ppid_val = (int)dv;
    }

    char buf[2048];
    int n = snprintf(buf, sizeof(buf),
        "signal=%s under_systemd=%s parent_pid=%d parent_name=%s loadavg_1m=%.2f",
        sig ? sig : "?",
        under_systemd ? "yes" : "no",
        ppid_val,
        parent_name ? parent_name : "?",
        loadavg);

    if (takeover) {
        n += snprintf(buf + n, sizeof(buf) - (size_t)n,
                      " takeover_marker_present=self");
    }
    if (planned_stop) {
        n += snprintf(buf + n, sizeof(buf) - (size_t)n,
                      " planned_stop_marker_present=yes");
    }

    int tracer_pid = (int)json_object_get_number(ctx, "tracer_pid", 0);
    if (tracer_pid > 0) {
        n += snprintf(buf + n, sizeof(buf) - (size_t)n,
                      " tracer_pid=%d", tracer_pid);
    }

    snprintf(buf + n, sizeof(buf) - (size_t)n,
             " parent_cmdline=%s",
             parent_cmd ? parent_cmd : "");

    return strdup(buf);
}

/* ================================================================
 *  JSON-serialise context dict for structured ingestion
 *  Port of Python context_as_json()
 * ================================================================ */

/* PoP: forensics_context_to_json @ gateway/shutdown_forensics.py:context_as_json */

char *forensics_context_to_json(json_node_t *ctx) {
    if (!ctx) return strdup("{}");

    char *json_str = json_serialize(ctx);
    if (!json_str) return strdup("{}");
    return json_str;
}

/* ================================================================
 *  Parse systemd duration string to microseconds
 *  Port of Python _parse_systemd_duration_to_us()
 * ================================================================ */

/* PoP: forensics_parse_systemd_duration_us @ gateway/shutdown_forensics.py:_parse_systemd_duration_to_us */
long forensics_parse_systemd_duration_us(const char *raw) {
    if (!raw || !*raw) return 0;

    struct {
        const char *suffix;
        long multiplier;
    } units[] = {
        {"us",  1},
        {"ms",  1000},
        {"s",   1000000},
        {"sec", 1000000},
        {"min", 60000000},
        {"h",   3600000000L},
        {"hr",  3600000000L},
        {NULL,  0}
    };

    long total_us = 0;
    const char *p = raw;
    char digits[64];
    char token[32];

    while (*p) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        /* Read digits */
        int di = 0;
        int has_dot = 0;
        while ((*p >= '0' && *p <= '9') || (*p == '.' && !has_dot)) {
            if (*p == '.') has_dot = 1;
            if (di < (int)sizeof(digits) - 1) digits[di++] = *p;
            p++;
        }
        digits[di] = '\0';

        if (di == 0) {
            /* No digits, skip the character */
            p++;
            continue;
        }

        /* Read alpha token */
        int ti = 0;
        while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
            if (ti < (int)sizeof(token) - 1) token[ti++] = *p;
            p++;
        }
        token[ti] = '\0';

        /* Look up multiplier */
        long mult = 1000000; /* Default: seconds */
        for (int u = 0; units[u].suffix; u++) {
            if (strcasecmp(token, units[u].suffix) == 0) {
                mult = units[u].multiplier;
                break;
            }
        }

        /* If no token read, bare number = seconds */
        if (ti == 0) mult = 1000000;

        double val = atof(digits);
        total_us += (long)(val * mult);
    }

    return total_us > 0 ? total_us : 0;
}

/* ================================================================
 *  Check systemd TimeoutStopSec >= drain_timeout
 *  Port of Python check_systemd_timing_alignment()
 * ================================================================ */

/* Forward declaration */
long forensics_parse_systemd_duration_us(const char *raw);

/* PoP: forensics_check_systemd_timing @ gateway/shutdown_forensics.py:check_systemd_timing_alignment */
json_node_t *forensics_check_systemd_timing(double drain_timeout) {
    const char *invocation_id = getenv("INVOCATION_ID");
    if (!invocation_id) return NULL;

    /* Try to identify unit name from /proc/self/cgroup */
    char unit_name[256] = {0};
    FILE *cg = fopen("/proc/self/cgroup", "r");
    if (cg) {
        char line[512];
        while (fgets(line, sizeof(line), cg)) {
            if (strstr(line, ".service")) {
                char *last_slash = strrchr(line, '/');
                if (last_slash) {
                    char *dot = strstr(last_slash, ".service");
                    if (dot) {
                        size_t len = (size_t)(dot - last_slash + 8);
                        if (len < sizeof(unit_name)) {
                            memcpy(unit_name, last_slash + 1, len - 1);
                            unit_name[len - 1] = '\0';
                        }
                    }
                }
            }
        }
        fclose(cg);
    }

    if (!unit_name[0]) return NULL;

    /* Query systemctl TimeoutStopUSec */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "systemctl --user show '%s' --property=TimeoutStopUSec 2>/dev/null || "
             "systemctl show '%s' --property=TimeoutStopUSec 2>/dev/null",
             unit_name, unit_name);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    long timeout_us = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "TimeoutStopUSec=", 16) == 0) {
            const char *value = line + 16;
            /* Trim newline */
            size_t vlen = strlen(value);
            while (vlen > 0 && (value[vlen-1] == '\n' || value[vlen-1] == '\r')) vlen--;

            if (vlen > 0) {
                char valcopy[128];
                memcpy(valcopy, value, vlen);
                valcopy[vlen] = '\0';

                /* Try numeric microseconds first */
                char *end = NULL;
                long num = strtol(valcopy, &end, 10);
                if (end && *end == '\0' && num > 0) {
                    timeout_us = num;
                } else {
                    timeout_us = forensics_parse_systemd_duration_us(valcopy);
                }
            }
            break;
        }
    }
    pclose(fp);

    if (timeout_us <= 0) return NULL;

    double timeout_stop_sec = (double)timeout_us / 1000000.0;
    double headroom = 30.0;
    double expected = drain_timeout + headroom;

    json_node_t *result = json_new_object();
    if (!result) return NULL;

    json_object_set(result, "unit", json_new_string(unit_name));
    json_object_set(result, "timeout_stop_sec", json_new_number(timeout_stop_sec));
    json_object_set(result, "drain_timeout", json_new_number(drain_timeout));
    json_object_set(result, "expected_min", json_new_number(expected));
    json_object_set(result, "mismatch", json_new_bool(timeout_stop_sec < expected));

    return result;
}
