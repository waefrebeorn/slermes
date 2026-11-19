/*
 * gateway_run_flow2.c — gateway/run.py FLOW cores + leaves, second wave
 * (universal-seam convention, twin of run_adapters_flow / gateway_agent_cli_flow).
 *
 * Ports 28 UNCLAIMED gateway/run.py functions: the config->env bridge family
 * (section / max_turns / terminal / auxiliary / full config / load), the
 * error-reply leaves, the best-effort + housekeeping chore wrappers, the
 * sessions-dir path resolution pair, the corrupt-config guard, the pid-exit
 * poll, the handoff-watch scope resolver, the standalone-profile boot notice
 * and the host-attach decision flow. Every live I/O (logging, i18n, config
 * loaders, env expanduser, plugin discovery, media policy, status writes,
 * process probes, asyncio sleeps, host decisions) enters through
 * gr2_seams_t.call(ctx, name, args); args is owned by gr2_call (freed after
 * the borrowed call); results are NEW json_t with NULL == Python None and
 * {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit) pass through
 * every `except Exception` site. Fail-closed: the harness delivers a
 * RuntimeError raise spec for an unscripted seam.
 *
 * Modeling notes (differential oracle tests/oracle_gateway_run_flow2.py):
 *  - os.environ is MUTABLE STATE (in.env -> out.state.env), not a seam:
 *    reads/writes/deletes are silent; the state diff carries them. Values
 *    are strings. os.path.expanduser IS a seam.
 *  - bridge_config_scalars / apply_media_policy_env are seamed collaborators
 *    (inert scripted results); fixtures do not rely on their env writes.
 *  - Claimed siblings are seams: _is_ssh_remote_tilde_cwd,
 *    _event_media_type_at, _handoff_watch_scopes,
 *    _start_gateway_replace_existing_instance. Ported siblings compose
 *    inline (config_to_env -> terminal/auxiliary/max_turns/section).
 *  - str() positions accept str/int/bool/null ONLY (no floats/containers:
 *    integral-float vs int identity and Python container repr are outside
 *    the fixture domain). json.dumps positions (terminal list/dict values)
 *    accept ASCII str/int/bool/null containers. float(str) accepts ASCII
 *    whitespace-padded decimal literals with underscores between digits;
 *    inf/nan/hex/astral results are outside the domain (fail closed).
 *  - Paths are POSIX strings normalized like PurePosixPath (collapse //,
 *    drop "." and trailing /, keep "..", "//" prefix preserved); the oracle
 *    seeds Path=PurePosixPath so str() keeps POSIX separators on Windows.
 *  - MessageType enums travel as their value strings; the harness/oracle
 *    validate membership values before dispatch (ValueError otherwise).
 *  - _aux_bridged_keys is a set in live: iteration order is arbitrary but
 *    env keys are distinct per task (fixture plugin keys are lowercase and
 *    collision-free), so the final env is order-independent; partial plugin
 *    key additions before a swallowed raise ARE kept (live set mutation).
 *  - logger seams record {fmt, args?[, exc_info]}; exception args normalize
 *    to raise specs. print seams record {text[, file:"stderr"]}.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include "libjson/json.h"

typedef struct gr2_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gr2_seams_t;

/* ── universal-seam helpers ── */
static json_t *gr2_call(const gr2_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool gr2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *gr2_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static const char *gr2_msg(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "msg") : NULL;
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static json_t *gr2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool gr2_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static bool gr2_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static const json_t *gr2_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *gr2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool gr2_is_null_node(const json_t *v) { return !v || v->type == JSON_NULL; }
static json_t *gr2_args(void) { return json_object(); }
static void gr2_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gr2_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void gr2_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void gr2_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python type name for a JSON node (integral numbers read as int: the
 * fixture domain keeps integral literals integral). */
static const char *gr2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
/* ── logging seams (RecLogger twin): {fmt, args?[, exc_info]} ── */
static json_t *gr2_log(const gr2_seams_t *s, const char *level,
                       const char *fmt, json_t *args_arr) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    json_t *a = gr2_args();
    gr2_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    return gr2_call(s, name, a);
}
static json_t *gr2_log_exc_info(const gr2_seams_t *s, const char *level, const char *fmt) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    json_t *a = gr2_args();
    gr2_s(a, "fmt", fmt);
    gr2_b(a, "exc_info", true);
    return gr2_call(s, name, a);
}
static json_t *gr2_args1(const json_t *x) {
    json_t *a = json_array();
    json_append(a, x ? json_copy(x) : json_null());
    return a;
}
static json_t *gr2_args2s(const char *a, const char *b) {
    json_t *x = json_array();
    json_append(x, json_string(a ? a : ""));
    json_append(x, json_string(b ? b : ""));
    return x;
}
/* ── Python value helpers ── */
/* str(v) for the scalar domain (str/int/bool/null). Containers fall back to
 * JSON serialization — OUT OF DOMAIN, fixtures never reach it. malloc'd. */
static char *gr2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%.17g", d);
            return strdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}
/* ASCII whitespace strip (str.strip domain restricted to ASCII ws). */
static char *gr2_strip(const char *s) {
    if (!s) s = "";
    while (*s && (*s == ' ' || (*s >= '\t' && *s <= '\r'))) s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || (s[n-1] >= '\t' && s[n-1] <= '\r'))) n--;
    char *out = malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *gr2_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (out) for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}
static char *gr2_upper(const char *s) {
    char *out = strdup(s ? s : "");
    if (out) for (char *p = out; *p; p++) *p = (char)toupper((unsigned char)*p);
    return out;
}
static bool gr2_startswith(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
/* Python numeric-literal underscores: every '_' must sit BETWEEN two digits. */
static bool gr2_underscores_ok(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (i == 0 || i + 1 >= n) return false;
            if (!(s[i-1] >= '0' && s[i-1] <= '9')) return false;
            if (!(s[i+1] >= '0' && s[i+1] <= '9')) return false;
        }
    }
    return true;
}
/* float(str) with Python acceptance for the fixture domain: ASCII-ws padded
 * decimal literals, underscores between digits, optional sign/exponent.
 * inf/nan/hex are rejected (outside the JSON-serializable result domain).
 * *ok=false on ValueError. Caller strips whitespace first. */
static double gr2_float_str(const char *raw, bool *ok) {
    *ok = false;
    if (!raw || !raw[0]) return 0.0;
    size_t n = strlen(raw);
    if (!gr2_underscores_ok(raw, n)) return 0.0;
    char *buf = malloc(n + 1);
    if (!buf) return 0.0;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) if (raw[i] != '_') buf[k++] = raw[i];
    buf[k] = '\0';
    if (k == 0) { free(buf); return 0.0; }
    const char *p = buf;
    if (*p == '+' || *p == '-') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X' || p[1] == 'b' || p[1] == 'B'
                        || p[1] == 'o' || p[1] == 'O')) { free(buf); return 0.0; }
    /* reject inf/nan words (out of domain: non-finite results) */
    char *low = gr2_lower(p);
    bool special = low && (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0
                           || strcmp(low, "nan") == 0);
    free(low);
    if (special) { free(buf); return 0.0; }
    /* grammar: digits [. digits] [eE [+-] digits] — strtod alone would accept
     * trailing junk like "1.5abc" via end check, but also "0x1p3" (rejected
     * above) and bare "." / "1." / ".e1" forms Python rejects. */
    const char *q = p;
    bool any_int = false, any_frac = false;
    while (*q >= '0' && *q <= '9') { any_int = true; q++; }
    if (*q == '.') {
        q++;
        while (*q >= '0' && *q <= '9') { any_frac = true; q++; }
    }
    if (!any_int && !any_frac) { free(buf); return 0.0; }
    if (*q == 'e' || *q == 'E') {
        q++;
        if (*q == '+' || *q == '-') q++;
        bool any_exp = false;
        while (*q >= '0' && *q <= '9') { any_exp = true; q++; }
        if (!any_exp) { free(buf); return 0.0; }
    }
    if (*q != '\0') { free(buf); return 0.0; }
    char *end = NULL;
    errno = 0;
    double d = strtod(buf, &end);
    free(buf);
    if (!end || *end != '\0') return 0.0;
    if (!isfinite(d)) return 0.0;  /* overflow to inf: out of domain */
    *ok = true;
    return d;
}
/* float(v) for a JSON node. Returns the value; on failure *kind names the
 * exception and *vmsg (malloc'd, may be NULL) carries the message. */
static double gr2_float_node(const json_t *v, const char **kind, char **vmsg) {
    *kind = NULL; *vmsg = NULL;
    if (!v || v->type == JSON_NULL) {
        *kind = "TypeError";
        *vmsg = strdup("float() argument must be a string or a real number, not 'NoneType'");
        return 0.0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        char *stripped = gr2_strip(v->str_val ? v->str_val : "");
        bool ok = false;
        double d = gr2_float_str(stripped, &ok);
        free(stripped);
        if (ok) return d;
        *kind = "ValueError";
        const char *tok = v->str_val ? v->str_val : "";
        size_t n = strlen(tok);
        *vmsg = malloc(n + 64);
        if (*vmsg) snprintf(*vmsg, n + 64, "could not convert string to float: '%s'", tok);
        return 0.0;
    }
    *kind = "TypeError";
    const char *tn = gr2_type_name(v);
    size_t n = strlen(tn);
    *vmsg = malloc(n + 80);
    if (*vmsg) snprintf(*vmsg, n + 80,
                        "float() argument must be a string or a real number, not '%s'", tn);
    return 0.0;
}
/* ── json.dumps twin (default separators ", " / ": ", ensure_ascii) ──
 * Domain: ASCII str/int/bool/null containers (no floats: integral-float
 * identity is outside the fixture domain). Growable buffer. */
typedef struct { char *p; size_t n, cap; } gr2_buf;
static void gr2_buf_init(gr2_buf *b) { b->p = malloc(64); b->n = 0; b->cap = 64; if (b->p) b->p[0] = '\0'; }
static void gr2_buf_put(gr2_buf *b, const char *s, size_t n) {
    if (!b->p) return;
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap * 2;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) { free(b->p); b->p = NULL; return; }
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = '\0';
}
static void gr2_buf_puts(gr2_buf *b, const char *s) { gr2_buf_put(b, s, strlen(s)); }
static void gr2_dumps_str(gr2_buf *b, const char *s) {
    gr2_buf_puts(b, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  gr2_buf_puts(b, "\\\""); break;
            case '\\': gr2_buf_puts(b, "\\\\"); break;
            case '\n': gr2_buf_puts(b, "\\n"); break;
            case '\r': gr2_buf_puts(b, "\\r"); break;
            case '\t': gr2_buf_puts(b, "\\t"); break;
            case '\b': gr2_buf_puts(b, "\\b"); break;
            case '\f': gr2_buf_puts(b, "\\f"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    gr2_buf_puts(b, esc);
                } else if (c < 0x80) {
                    gr2_buf_put(b, (const char *)&c, 1);
                } else {
                    /* non-ASCII: decode UTF-8 codepoint, emit \uXXXX (BMP
                     * domain; astral chars are outside the fixture domain) */
                    unsigned cp = 0; int extra = 0;
                    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
                    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
                    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
                    for (int i = 0; i < extra && p[1]; i++) { cp = (cp << 6) | (p[1] & 0x3F); p++; }
                    char esc[16];
                    if (cp > 0xFFFF) {
                        unsigned v = cp - 0x10000;
                        snprintf(esc, sizeof(esc), "\\u%04x\\u%04x",
                                 0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
                    } else {
                        snprintf(esc, sizeof(esc), "\\u%04x", cp);
                    }
                    gr2_buf_puts(b, esc);
                }
        }
    }
    gr2_buf_puts(b, "\"");
}
static void gr2_dumps_val(gr2_buf *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { gr2_buf_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: gr2_buf_puts(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%.17g", d);
            gr2_buf_puts(b, buf);
            break;
        }
        case JSON_STRING: gr2_dumps_str(b, v->str_val ? v->str_val : ""); break;
        case JSON_ARRAY:
            gr2_buf_puts(b, "[");
            for (size_t i = 0; i < json_len(v); i++) {
                if (i) gr2_buf_puts(b, ", ");
                gr2_dumps_val(b, json_get(v, i));
            }
            gr2_buf_puts(b, "]");
            break;
        case JSON_OBJECT:
            gr2_buf_puts(b, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) gr2_buf_puts(b, ", ");
                gr2_dumps_str(b, v->c.keys[i]);
                gr2_buf_puts(b, ": ");
                gr2_dumps_val(b, v->c.items[i]);
            }
            gr2_buf_puts(b, "}");
            break;
    }
}
static char *gr2_json_dumps(const json_t *v) {
    gr2_buf b; gr2_buf_init(&b);
    gr2_dumps_val(&b, v);
    return b.p ? b.p : strdup("");
}
/* ── PurePosixPath normalization ──
 * Collapse repeated '/', drop '.' components and trailing '/', keep '..'.
 * Exactly two leading slashes are preserved (POSIX //); '' -> '.'. */
static char *gr2_posix_norm(const char *path) {
    if (!path) path = "";
    size_t lead = 0;
    while (path[lead] == '/') lead++;
    const char *prefix = (lead == 2) ? "//" : (lead > 0 ? "/" : "");
    gr2_buf b; gr2_buf_init(&b);
    gr2_buf_puts(&b, prefix);
    const char *p = path + lead;
    bool first = true;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t n = slash ? (size_t)(slash - p) : strlen(p);
        if (n > 0 && !(n == 1 && p[0] == '.')) {
            if (!first) gr2_buf_puts(&b, "/");
            gr2_buf_put(&b, p, n);
            first = false;
        }
        if (!slash) break;
        p = slash + 1;
    }
    if (first && !prefix[0]) gr2_buf_puts(&b, ".");
    return b.p ? b.p : strdup(".");
}
/* home / "sessions" (PurePosixPath join appends a COMPONENT, not a string
 * concat: home "/" must yield "/sessions", not "//sessions"). */
static char *gr2_path_join_sessions(const char *home) {
    size_t n = strlen(home);
    char *tmp = malloc(n + 16);
    if (!tmp) return strdup("sessions");
    bool slash = n > 0 && home[n-1] == '/';
    snprintf(tmp, n + 16, "%s%ssessions", home, slash ? "" : "/");
    char *out = gr2_posix_norm(tmp);
    free(tmp);
    return out;
}
/* ── static env bridge maps (live dict order, gateway/run.py:1920-1936,
 * 1964-1997) ── */
static const char *gr2_agent_env_bridge[][2] = {
    {"gateway_timeout", "HERMES_AGENT_TIMEOUT"},
    {"gateway_turn_lease_timeout", "HERMES_TURN_LEASE_TIMEOUT"},
    {"gateway_timeout_warning", "HERMES_AGENT_TIMEOUT_WARNING"},
    {"gateway_notify_interval", "HERMES_AGENT_NOTIFY_INTERVAL"},
    {"session_stall_timeout", "HERMES_SESSION_STALL_TIMEOUT"},
    {"restart_drain_timeout", "HERMES_RESTART_DRAIN_TIMEOUT"},
    {"cron_drain_timeout", "HERMES_CRON_DRAIN_TIMEOUT"},
    {"gateway_auto_continue_freshness", "HERMES_AUTO_CONTINUE_FRESHNESS"},
    {"gateway_startup_restore_drain_timeout", "HERMES_STARTUP_RESTORE_DRAIN_TIMEOUT"},
    {"gateway_startup_warmup_timeout", "HERMES_STARTUP_WARMUP_TIMEOUT"}};
static const char *gr2_sessions_env_bridge[][2] = {
    {"search_slow_ms", "HERMES_SEARCH_SLOW_MS"}};
static const char *gr2_display_env_bridge[][2] = {
    {"busy_input_mode", "HERMES_GATEWAY_BUSY_INPUT_MODE"},
    {"busy_text_mode", "HERMES_GATEWAY_BUSY_TEXT_MODE"},
    {"busy_ack_enabled", "HERMES_GATEWAY_BUSY_ACK_ENABLED"}};
static const char *gr2_terminal_env_map[][2] = {
    {"backend", "TERMINAL_ENV"},
    {"degraded_mode", "TERMINAL_DEGRADED_MODE"},
    {"cwd", "TERMINAL_CWD"},
    {"timeout", "TERMINAL_TIMEOUT"},
    {"home_mode", "TERMINAL_HOME_MODE"},
    {"lifetime_seconds", "TERMINAL_LIFETIME_SECONDS"},
    {"docker_image", "TERMINAL_DOCKER_IMAGE"},
    {"docker_forward_env", "TERMINAL_DOCKER_FORWARD_ENV"},
    {"modal_image", "TERMINAL_MODAL_IMAGE"},
    {"daytona_image", "TERMINAL_DAYTONA_IMAGE"},
    {"vercel_runtime", "TERMINAL_VERCEL_RUNTIME"},
    {"vercel_image", "TERMINAL_VERCEL_IMAGE"},
    {"ssh_host", "TERMINAL_SSH_HOST"},
    {"ssh_user", "TERMINAL_SSH_USER"},
    {"ssh_port", "TERMINAL_SSH_PORT"},
    {"ssh_key", "TERMINAL_SSH_KEY"},
    {"container_cpu", "TERMINAL_CONTAINER_CPU"},
    {"container_memory", "TERMINAL_CONTAINER_MEMORY"},
    {"container_disk", "TERMINAL_CONTAINER_DISK"},
    {"container_persistent", "TERMINAL_CONTAINER_PERSISTENT"},
    {"docker_volumes", "TERMINAL_DOCKER_VOLUMES"},
    {"docker_env", "TERMINAL_DOCKER_ENV"},
    {"docker_extra_args", "TERMINAL_DOCKER_EXTRA_ARGS"},
    {"docker_shm_size", "TERMINAL_DOCKER_SHM_SIZE"},
    {"docker_mount_cwd_to_workspace", "TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE"},
    {"docker_network", "TERMINAL_DOCKER_NETWORK"},
    {"docker_run_as_host_user", "TERMINAL_DOCKER_RUN_AS_HOST_USER"},
    {"docker_snap_compat", "TERMINAL_DOCKER_SNAP_COMPAT"},
    {"docker_persist_across_processes", "TERMINAL_DOCKER_PERSIST_ACROSS_PROCESSES"},
    {"docker_shared_container_key", "TERMINAL_DOCKER_SHARED_CONTAINER_KEY"},
    {"docker_orphan_reaper", "TERMINAL_DOCKER_ORPHAN_REAPER"},
    {"sandbox_dir", "TERMINAL_SANDBOX_DIR"},
    {"persistent_shell", "TERMINAL_PERSISTENT_SHELL"}};
/* ── ported flows ──
 * Return convention: owned json_t value, NULL == Python None, or a
 * {"__raise__":K,"msg":M} spec == a raised exception (propagate or swallow
 * per the live except clauses). Void flows return NULL on success. */

/* PoP: gr2_context_overflow_reply @ gateway/run.py:_context_overflow_reply */
static json_t *gr2_context_overflow_reply(const gr2_seams_t *s) {
    json_t *a = gr2_args();
    gr2_s(a, "key", "gateway.errors.context_overflow");
    json_set(a, "kwargs", json_object());
    return gr2_call(s, "t", a);
}

/* PoP: gr2_rate_limit_reply @ gateway/run.py:_rate_limit_reply */
static json_t *gr2_rate_limit_reply(const char *text, const gr2_seams_t *s) {
    json_t *a = gr2_args();
    gr2_s(a, "text", text ? text : "");
    json_t *r = gr2_call(s, "reset_delay_from_message", a);
    if (gr2_raised(r)) return r;
    /* seconds = reset_delay_from_message(text) or 0 */
    double secs = 0.0;
    if (gr2_truth(r)) {
        if (r->type == JSON_NUMBER) secs = r->num_val;
        else if (r->type == JSON_BOOL) secs = r->bool_val ? 1.0 : 0.0;
        else {
            char m[128];
            snprintf(m, sizeof(m), "'<' not supported between instances of '%s' and 'int'",
                     gr2_type_name(r));
            json_free(r);
            return gr2_raise("TypeError", m);
        }
    }
    json_free(r);
    if (secs < 120) {
        a = gr2_args();
        gr2_s(a, "key", "gateway.errors.rate_limited");
        json_set(a, "kwargs", json_object());
        return gr2_call(s, "t", a);
    }
    a = gr2_args();
    gr2_n(a, "seconds", secs);
    json_t *w = gr2_call(s, "format_reset_window", a);
    if (gr2_raised(w)) return w;
    a = gr2_args();
    gr2_s(a, "key", "gateway.errors.usage_limit_resets");
    json_t *kw = json_object();
    gr2_j(kw, "window", w);
    json_set(a, "kwargs", kw);
    json_free(w);
    return gr2_call(s, "t", a);
}

/* PoP: gr2_bridge_section_to_env @ gateway/run.py:_bridge_section_to_env */
static json_t *gr2_bridge_section_to_env(const json_t *section, const json_t *mapping,
                                         json_t *env) {
    if (!section || section->type != JSON_OBJECT) return NULL;
    if (!mapping || mapping->type != JSON_OBJECT) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'items'", gr2_type_name(mapping));
        return gr2_raise("AttributeError", m);
    }
    for (size_t i = 0; i < mapping->c.count; i++) {
        const char *cfg_key = mapping->c.keys[i];
        const char *env_var = gr2_sval(mapping->c.items[i]);
        if (!env_var) continue;
        if (json_has(section, cfg_key)) {
            char *sv = gr2_pystr(json_obj_get(section, cfg_key));
            json_set(env, env_var, json_string(sv));
            free(sv);
        }
    }
    return NULL;
}

/* PoP: gr2_bridge_max_turns_to_env @ gateway/run.py:_bridge_max_turns_to_env */
static json_t *gr2_bridge_max_turns_to_env(const json_t *agent_cfg, json_t *env) {
    if (!agent_cfg || agent_cfg->type != JSON_OBJECT || !json_has(agent_cfg, "max_turns"))
        return NULL;
    const json_t *raw = json_obj_get(agent_cfg, "max_turns");
    if (!gr2_is_null_node(raw)) {
        char *sv = gr2_pystr(raw);
        json_set(env, "HERMES_MAX_ITERATIONS", json_string(sv));
        free(sv);
    } else if (json_has(env, "HERMES_MAX_ITERATIONS")) {
        json_obj_del(env, "HERMES_MAX_ITERATIONS");
    }
    return NULL;
}

/* PoP: gr2_bridge_terminal_config_to_env @ gateway/run.py:_bridge_terminal_config_to_env */
static json_t *gr2_bridge_terminal_config_to_env(const json_t *cfg, json_t *env,
                                                 const gr2_seams_t *s) {
    if (!cfg || cfg->type != JSON_OBJECT) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'get'", gr2_type_name(cfg));
        return gr2_raise("AttributeError", m);
    }
    /* backend = str(cfg.get("backend") or env.get("TERMINAL_ENV") or "").strip().lower() */
    const json_t *b = gr2_oget(cfg, "backend");
    const json_t *pick = gr2_truth(b) ? b : gr2_oget(env, "TERMINAL_ENV");
    char *raw = gr2_truth(pick) ? gr2_pystr(pick) : strdup("");
    char *stripped = gr2_strip(raw);
    char *backend = gr2_lower(stripped);
    free(raw); free(stripped);
    for (size_t i = 0; i < sizeof(gr2_terminal_env_map) / sizeof(gr2_terminal_env_map[0]); i++) {
        const char *cfg_key = gr2_terminal_env_map[i][0];
        const char *env_var = gr2_terminal_env_map[i][1];
        if (!json_has(cfg, cfg_key)) continue;
        const json_t *val = json_obj_get(cfg, cfg_key);
        json_t *owned_val = NULL;
        if (strcmp(cfg_key, "cwd") == 0) {
            char *sv = gr2_pystr(val);
            bool placeholder = strcmp(sv, ".") == 0 || strcmp(sv, "auto") == 0
                            || strcmp(sv, "cwd") == 0;
            free(sv);
            if (placeholder) continue;
            if (val && val->type == JSON_STRING) {
                char *vs = gr2_strip(val->str_val ? val->str_val : "");
                json_t *a = gr2_args();
                gr2_s(a, "backend", backend);
                gr2_s(a, "cwd", vs);
                free(vs);
                json_t *ssh = gr2_call(s, "_is_ssh_remote_tilde_cwd", a);
                if (gr2_raised(ssh)) { free(backend); return ssh; }
                bool keep_tilde = gr2_truth(ssh);
                json_free(ssh);
                if (!keep_tilde) {
                    a = gr2_args();
                    gr2_s(a, "path", val->str_val ? val->str_val : "");
                    json_t *ex = gr2_call(s, "os.path.expanduser", a);
                    if (gr2_raised(ex)) { free(backend); return ex; }
                    owned_val = ex ? ex : json_null();
                    val = owned_val;
                }
            }
        }
        char *out;
        if (val && (val->type == JSON_ARRAY || val->type == JSON_OBJECT))
            out = gr2_json_dumps(val);
        else
            out = gr2_pystr(val);
        json_set(env, env_var, json_string(out));
        free(out);
        json_free(owned_val);
    }
    free(backend);
    return NULL;
}

/* PoP: gr2_bridge_auxiliary_config_to_env @ gateway/run.py:_bridge_auxiliary_config_to_env */
static json_t *gr2_bridge_auxiliary_config_to_env(const json_t *cfg, json_t *env,
                                                  const gr2_seams_t *s) {
    /* _aux_bridged_keys = {"vision", "approval"} + plugin keys; a swallowed
     * plugin-discovery raise keeps the keys added before it (live set). */
    json_t *keys = json_array();
    json_append(keys, json_string("vision"));
    json_append(keys, json_string("approval"));
    json_t *r = gr2_call(s, "get_plugin_auxiliary_tasks", gr2_args());
    if (gr2_raised(r)) {
        if (gr2_is_base_exc(gr2_kind(r))) { json_free(keys); return r; }
        json_free(r);  /* except Exception: pass */
    } else if (!r || r->type != JSON_ARRAY) {
        /* for _entry in None / non-iterable -> TypeError, swallowed */
        json_free(r);
    } else {
        for (size_t i = 0; i < json_len(r); i++) {
            const json_t *entry = json_get(r, i);
            if (!entry || entry->type != JSON_OBJECT) break;  /* TypeError swallowed */
            const json_t *key = gr2_oget(entry, "key");
            const char *ks = gr2_sval(key);
            if (!ks) break;  /* KeyError('key') / unhashable: swallowed */
            bool dup = false;
            for (size_t j = 0; j < json_len(keys); j++) {
                const char *have = gr2_sval(json_get(keys, j));
                if (have && strcmp(have, ks) == 0) { dup = true; break; }
            }
            if (!dup) json_append(keys, json_string(ks));
        }
        json_free(r);
    }
    if (!cfg || cfg->type != JSON_OBJECT) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'get'", gr2_type_name(cfg));
        json_free(keys);
        return gr2_raise("AttributeError", m);
    }
    static const char *fields[][2] = {{"model", "MODEL"}, {"base_url", "BASE_URL"},
                                      {"api_key", "API_KEY"}};
    for (size_t i = 0; i < json_len(keys); i++) {
        const char *task_key = gr2_sval(json_get(keys, i));
        if (!task_key) continue;
        const json_t *task_cfg = gr2_oget(cfg, task_key);
        if (task_cfg && task_cfg->type != JSON_OBJECT) continue;
        char *upper = gr2_upper(task_key);
        /* provider: str(task_cfg.get("provider", "")).strip() */
        const json_t *pv = task_cfg ? gr2_oget(task_cfg, "provider") : NULL;
        char *praw = pv ? gr2_pystr(pv) : strdup("");
        char *prov = gr2_strip(praw);
        free(praw);
        if (prov[0] && strcmp(prov, "auto") != 0) {
            char name[160];
            snprintf(name, sizeof(name), "AUXILIARY_%s_PROVIDER", upper);
            json_set(env, name, json_string(prov));
        }
        free(prov);
        for (size_t f = 0; f < 3; f++) {
            const json_t *fv = task_cfg ? gr2_oget(task_cfg, fields[f][0]) : NULL;
            char *fraw = fv ? gr2_pystr(fv) : strdup("");
            char *value = gr2_strip(fraw);
            free(fraw);
            if (value[0]) {
                char name[160];
                snprintf(name, sizeof(name), "AUXILIARY_%s_%s", upper, fields[f][1]);
                json_set(env, name, json_string(value));
            }
            free(value);
        }
        free(upper);
    }
    json_free(keys);
    return NULL;
}
/* PoP: gr2_bridge_config_to_env @ gateway/run.py:_bridge_config_to_env */
static json_t *gr2_bridge_config_to_env(const json_t *cfg, json_t *env,
                                        const gr2_seams_t *s) {
    /* bridge_config_scalars fires FIRST, before any .get on cfg (live order). */
    json_t *a = gr2_args();
    gr2_j(a, "cfg", cfg);
    gr2_s(a, "target", "env");
    json_t *r = gr2_call(s, "bridge_config_scalars", a);
    if (gr2_raised(r)) return r;
    json_free(r);
    if (!cfg || cfg->type != JSON_OBJECT) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'get'", gr2_type_name(cfg));
        return gr2_raise("AttributeError", m);
    }
    const json_t *terminal = gr2_oget(cfg, "terminal");
    if (gr2_truth(terminal) && terminal->type == JSON_OBJECT) {
        r = gr2_bridge_terminal_config_to_env(terminal, env, s);
        if (r) return r;
    }
    const json_t *auxiliary = gr2_oget(cfg, "auxiliary");
    if (gr2_truth(auxiliary) && auxiliary->type == JSON_OBJECT) {
        r = gr2_bridge_auxiliary_config_to_env(auxiliary, env, s);
        if (r) return r;
    }
    const json_t *agent = gr2_oget(cfg, "agent");
    r = gr2_bridge_max_turns_to_env(agent, env);
    if (r) return r;
    json_t *amap = json_object();
    for (size_t i = 0; i < sizeof(gr2_agent_env_bridge) / sizeof(gr2_agent_env_bridge[0]); i++)
        json_set(amap, gr2_agent_env_bridge[i][0], json_string(gr2_agent_env_bridge[i][1]));
    r = gr2_bridge_section_to_env(agent, amap, env);
    json_free(amap);
    if (r) return r;
    json_t *smap = json_object();
    for (size_t i = 0; i < sizeof(gr2_sessions_env_bridge) / sizeof(gr2_sessions_env_bridge[0]); i++)
        json_set(smap, gr2_sessions_env_bridge[i][0], json_string(gr2_sessions_env_bridge[i][1]));
    r = gr2_bridge_section_to_env(gr2_oget(cfg, "sessions"), smap, env);
    json_free(smap);
    if (r) return r;
    const json_t *display = gr2_oget(cfg, "display");
    json_t *dmap = json_object();
    for (size_t i = 0; i < sizeof(gr2_display_env_bridge) / sizeof(gr2_display_env_bridge[0]); i++)
        json_set(dmap, gr2_display_env_bridge[i][0], json_string(gr2_display_env_bridge[i][1]));
    r = gr2_bridge_section_to_env(display, dmap, env);
    json_free(dmap);
    if (r) return r;
    /* service-manager override: env wins when already set */
    if (display && display->type == JSON_OBJECT && json_has(display, "busy_steer_ack_enabled")
            && !json_has(env, "HERMES_GATEWAY_BUSY_STEER_ACK_ENABLED")) {
        char *sv = gr2_pystr(json_obj_get(display, "busy_steer_ack_enabled"));
        json_set(env, "HERMES_GATEWAY_BUSY_STEER_ACK_ENABLED", json_string(sv));
        free(sv);
    }
    const json_t *tz = gr2_oget(cfg, "timezone");
    if (gr2_truth(tz) && tz->type == JSON_STRING) {
        char *st = gr2_strip(tz->str_val ? tz->str_val : "");
        json_set(env, "HERMES_TIMEZONE", json_string(st));
        free(st);
    }
    const json_t *security = gr2_oget(cfg, "security");
    if (security && security->type == JSON_OBJECT) {
        const json_t *rs = gr2_oget(security, "redact_secrets");
        if (!gr2_is_null_node(rs)) {
            char *sv = gr2_pystr(rs);
            char *low = gr2_lower(sv);
            json_set(env, "HERMES_REDACT_SECRETS", json_string(low));
            free(sv); free(low);
        }
    }
    const json_t *gateway = gr2_oget(cfg, "gateway");
    json_t *gw_empty = NULL;
    if (!gateway) { gw_empty = json_object(); gateway = gw_empty; }  /* .get("gateway", {}) */
    if (gateway->type == JSON_OBJECT) {
        a = gr2_args();
        gr2_j(a, "cfg", cfg);
        r = gr2_call(s, "apply_media_policy_env", a);
        if (gr2_raised(r)) { json_free(gw_empty); return r; }
        json_free(r);
        const json_t *tr = gr2_oget(gateway, "trust_recent_files_seconds");
        if (!gr2_is_null_node(tr)) {
            char *sv = gr2_pystr(tr);
            json_set(env, "HERMES_MEDIA_TRUST_RECENT_SECONDS", json_string(sv));
            free(sv);
        }
        /* escape hatch: env WINS if already set (non-blank) */
        if (json_has(gateway, "platform_connect_timeout")) {
            const json_t *cur = gr2_oget(env, "HERMES_GATEWAY_PLATFORM_CONNECT_TIMEOUT");
            const char *cs = gr2_sval(cur);
            char *stripped = gr2_strip(cs ? cs : "");
            bool blank = stripped[0] == '\0';
            free(stripped);
            if (blank) {
                char *sv = gr2_pystr(json_obj_get(gateway, "platform_connect_timeout"));
                json_set(env, "HERMES_GATEWAY_PLATFORM_CONNECT_TIMEOUT", json_string(sv));
                free(sv);
            }
        }
    }
    json_free(gw_empty);
    return NULL;
}

/* PoP: gr2_load_bridge_config @ gateway/run.py:_load_bridge_config */
static json_t *gr2_load_bridge_config(const char *config_path, const gr2_seams_t *s) {
    json_t *snap = gr2_call(s, "worker_config_snapshot", gr2_args());
    if (gr2_raised(snap)) return snap;
    if (snap) return snap;  /* `is not None`: an empty dict IS returned */
    json_t *a = gr2_args();
    char *norm = gr2_posix_norm(config_path ? config_path : "");
    gr2_s(a, "config_path", norm);
    free(norm);
    return gr2_call(s, "load_user_config_effective", a);
}

/* PoP: gr2_best_effort @ gateway/run.py:_best_effort */
static json_t *gr2_best_effort(const char *debug_msg, const gr2_seams_t *s) {
    json_t *r = gr2_call(s, "fn", gr2_args());
    if (!gr2_raised(r)) return r;
    if (gr2_is_base_exc(gr2_kind(r))) return r;
    if (debug_msg && debug_msg[0]) {
        json_t *lr = gr2_log(s, "debug", debug_msg, gr2_args1(r));
        if (gr2_raised(lr)) { json_free(r); return lr; }
        json_free(lr);
    }
    json_free(r);
    return NULL;
}

/* PoP: gr2_runtime_agent_kwargs @ gateway/run.py:_runtime_agent_kwargs */
static json_t *gr2_runtime_agent_kwargs(const json_t *runtime) {
    if (!runtime || runtime->type != JSON_OBJECT) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'get'", gr2_type_name(runtime));
        return gr2_raise("AttributeError", m);
    }
    json_t *out = json_object();
    static const char *pass_keys[] = {"api_key", "base_url", "provider", "requested_provider",
                                      "api_mode", "command"};
    for (size_t i = 0; i < 6; i++)
        gr2_j(out, pass_keys[i], gr2_oget(runtime, pass_keys[i]));
    const json_t *args = gr2_oget(runtime, "args");
    json_t *args_out;
    if (gr2_truth(args)) {
        if (args->type == JSON_ARRAY) args_out = json_copy(args);
        else {
            char m[96];
            snprintf(m, sizeof(m), "'%s' object is not iterable", gr2_type_name(args));
            json_free(out);
            return gr2_raise("TypeError", m);
        }
    } else {
        args_out = json_array();
    }
    json_set(out, "args", args_out);
    gr2_j(out, "credential_pool", gr2_oget(runtime, "credential_pool"));
    gr2_j(out, "request_overrides", gr2_oget(runtime, "request_overrides"));
    return out;
}

/* PoP: gr2_event_media_kind_is @ gateway/run.py:_event_media_kind_is */
static json_t *gr2_event_media_kind_is(const json_t *event, double index,
                                       const char *mime_prefix, const json_t *fallback_types,
                                       const gr2_seams_t *s) {
    json_t *a = gr2_args();
    gr2_j(a, "event", event);
    gr2_n(a, "index", index);
    json_t *r = gr2_call(s, "_event_media_type_at", a);
    if (gr2_raised(r)) return r;
    const char *mtype = gr2_sval(r);
    if (gr2_truth(r) && !mtype) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'startswith'", gr2_type_name(r));
        json_free(r);
        return gr2_raise("AttributeError", m);
    }
    if (mtype && mtype[0]) {
        bool hit = gr2_startswith(mtype, mime_prefix ? mime_prefix : "");
        json_free(r);
        return json_bool(hit);
    }
    json_free(r);
    const json_t *mt = gr2_oget(event, "message_type");
    const char *mts = gr2_sval(mt);
    bool hit = false;
    if (mts && fallback_types && fallback_types->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(fallback_types); i++) {
            const char *f = gr2_sval(json_get(fallback_types, i));
            if (f && strcmp(f, mts) == 0) { hit = true; break; }
        }
    }
    return json_bool(hit);
}

/* PoP: gr2_format_gateway_process_notification @ gateway/run.py:_format_gateway_process_notification */
static json_t *gr2_format_gateway_process_notification(const json_t *evt,
                                                       const gr2_seams_t *s) {
    if (!evt || evt->type != JSON_OBJECT) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'get'", gr2_type_name(evt));
        return gr2_raise("AttributeError", m);
    }
    const json_t *tn = gr2_oget(evt, "type");
    const char *evt_type = gr2_sval(tn);
    if (!evt_type) evt_type = tn ? NULL : "completion";  /* present non-str: no branch matches */
    /* f-string positions: get(k, default) then str(); present-null -> "None" */
    #define GR2_EVT_STR(key, def) ({ \
        const json_t *_n = gr2_oget(evt, key); \
        _n ? gr2_pystr(_n) : strdup(def); })
    if (evt_type && (strcmp(evt_type, "watch_disabled") == 0
                     || strcmp(evt_type, "watch_overflow_tripped") == 0
                     || strcmp(evt_type, "watch_overflow_released") == 0)) {
        char *msg = GR2_EVT_STR("message", "");
        size_t n = strlen(msg);
        char *out = malloc(n + 32);
        if (out) snprintf(out, n + 32, "[IMPORTANT: %s]", msg);
        free(msg);
        json_t *node = out ? json_string(out) : json_null();
        free(out);
        return node;
    }
    if (evt_type && strcmp(evt_type, "watch_match") == 0) {
        char *sid = GR2_EVT_STR("session_id", "unknown");
        char *cmd = GR2_EVT_STR("command", "unknown");
        char *pat = GR2_EVT_STR("pattern", "?");
        char *outv = GR2_EVT_STR("output", "");
        const json_t *sup = gr2_oget(evt, "suppressed");
        gr2_buf b; gr2_buf_init(&b);
        gr2_buf_puts(&b, "[IMPORTANT: Background process ");
        gr2_buf_puts(&b, sid);
        gr2_buf_puts(&b, " matched watch pattern \"");
        gr2_buf_puts(&b, pat);
        gr2_buf_puts(&b, "\".\nCommand: ");
        gr2_buf_puts(&b, cmd);
        gr2_buf_puts(&b, "\nMatched output:\n");
        gr2_buf_puts(&b, outv);
        if (gr2_truth(sup)) {
            char *ss = gr2_pystr(sup);
            gr2_buf_puts(&b, "\n(");
            gr2_buf_puts(&b, ss);
            gr2_buf_puts(&b, " earlier matches were suppressed by rate limit)");
            free(ss);
        }
        gr2_buf_puts(&b, "]");
        free(sid); free(cmd); free(pat); free(outv);
        json_t *out = b.p ? json_string(b.p) : json_null();
        free(b.p);
        return out;
    }
    #undef GR2_EVT_STR
    if (evt_type && (strcmp(evt_type, "async_delegation") == 0
                     || strcmp(evt_type, "heartbeat") == 0)) {
        json_t *a = gr2_args();
        gr2_j(a, "evt", evt);
        return gr2_call(s, "format_process_notification", a);
    }
    return NULL;
}
/* PoP: gr2_reconnect_attention_after_secs @ gateway/run.py:_reconnect_attention_after_secs */
static json_t *gr2_reconnect_attention_after_secs(const gr2_seams_t *s) {
    json_t *cfg = gr2_call(s, "load_config_readonly", gr2_args());
    if (gr2_raised(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'get'", gr2_type_name(cfg));
        json_free(cfg);
        return gr2_raise("AttributeError", m);
    }
    const json_t *agent_cfg = gr2_oget(cfg, "agent");
    const json_t *raw = (agent_cfg && agent_cfg->type == JSON_OBJECT)
                      ? gr2_oget(agent_cfg, "reconnect_attention_after") : NULL;
    const char *kind = NULL;
    char *vmsg = NULL;
    double d = gr2_float_node(raw, &kind, &vmsg);
    json_free(cfg);
    if (kind) {
        /* except (TypeError, ValueError): fall back to the default config.
         * DEFAULT_CONFIG["agent"]["reconnect_attention_after"] == 7200
         * (hermes_cli/config_defaults.py:239). */
        free(vmsg);
        return json_number(7200.0);
    }
    return json_number(d);
}

/* PoP: gr2_write_runtime_status_quiet @ gateway/run.py:_write_runtime_status_quiet */
static json_t *gr2_write_runtime_status_quiet(const json_t *fields, const gr2_seams_t *s) {
    json_t *a = fields ? json_copy(fields) : json_object();
    json_t *r = gr2_call(s, "publish_runtime_status", a);
    if (gr2_raised(r)) {
        if (gr2_is_base_exc(gr2_kind(r))) return r;
        json_free(r);
        return NULL;
    }
    json_free(r);
    return NULL;
}

/* PoP: gr2_clear_takeover_marker_quiet @ gateway/run.py:_clear_takeover_marker_quiet */
static json_t *gr2_clear_takeover_marker_quiet(const gr2_seams_t *s) {
    json_t *r = gr2_call(s, "clear_takeover_marker", gr2_args());
    if (gr2_raised(r)) {
        if (gr2_is_base_exc(gr2_kind(r))) return r;
        json_free(r);
        return NULL;
    }
    json_free(r);
    return NULL;
}

/* PoP: gr2_housekeeping_chore @ gateway/run.py:_housekeeping_chore */
static json_t *gr2_housekeeping_chore(const char *label, const json_t *args,
                                      const json_t *kwargs, const gr2_seams_t *s) {
    json_t *a = gr2_args();
    json_set(a, "args", args ? json_copy(args) : json_array());
    json_set(a, "kwargs", kwargs ? json_copy(kwargs) : json_object());
    json_t *r = gr2_call(s, "fn", a);
    if (gr2_raised(r)) {
        if (gr2_is_base_exc(gr2_kind(r))) return r;
        json_t *la = json_array();
        json_append(la, json_string(label ? label : ""));
        json_append(la, json_copy(r));
        json_t *lr = gr2_log(s, "debug", "%s error: %s", la);
        if (gr2_raised(lr)) { json_free(r); return lr; }
        json_free(lr);
        json_free(r);
        return NULL;
    }
    json_free(r);  /* statement-position call: result discarded */
    return NULL;
}

/* PoP: gr2_housekeeping_paste_sweep @ gateway/run.py:_housekeeping_paste_sweep */
static json_t *gr2_housekeeping_paste_sweep(const gr2_seams_t *s) {
    json_t *r = gr2_call(s, "_sweep_expired_pastes", gr2_args());
    if (gr2_raised(r)) return r;
    /* deleted, remaining = _sweep_expired_pastes() */
    if (!r || r->type != JSON_ARRAY) {
        char m[96];
        snprintf(m, sizeof(m), "cannot unpack non-iterable %s object", gr2_type_name(r));
        json_free(r);
        return gr2_raise("TypeError", m);
    }
    size_t n = json_len(r);
    if (n != 2) {
        char m[96];
        snprintf(m, sizeof(m), "%s values to unpack (expected 2, got %zu)",
                 n < 2 ? "not enough" : "too many", n);
        json_free(r);
        return gr2_raise("ValueError", m);
    }
    const json_t *deleted = json_get(r, 0);
    const json_t *remaining = json_get(r, 1);
    json_t *lr = NULL;
    if (gr2_truth(deleted)) {
        json_t *la = json_array();
        json_append(la, deleted ? json_copy(deleted) : json_null());
        json_append(la, remaining ? json_copy(remaining) : json_null());
        lr = gr2_log(s, "info", "Paste sweep: deleted %d expired paste(s), %d pending", la);
    }
    json_free(r);
    if (lr && gr2_raised(lr)) return lr;
    json_free(lr);
    return NULL;
}

/* PoP: gr2_housekeeping_misfire_catch_up @ gateway/run.py:_housekeeping_misfire_catch_up */
static json_t *gr2_housekeeping_misfire_catch_up(const json_t *cron_provider,
                                                 const json_t *adapters,
                                                 const json_t *loop,
                                                 const gr2_seams_t *s) {
    json_t *a = gr2_args();
    gr2_j(a, "cron_provider", cron_provider);
    gr2_j(a, "adapters", adapters);
    gr2_j(a, "loop", loop);
    json_t *r = gr2_call(s, "fire_overdue_jobs", a);
    if (gr2_raised(r)) return r;
    json_t *lr = NULL;
    if (gr2_truth(r))
        lr = gr2_log(s, "info", "Misfire catch-up: fired %d overdue job(s)", gr2_args1(r));
    json_free(r);
    if (lr && gr2_raised(lr)) return lr;
    json_free(lr);
    return NULL;
}

/* PoP: gr2_housekeeping_memory_trim @ gateway/run.py:_housekeeping_memory_trim */
static json_t *gr2_housekeeping_memory_trim(const gr2_seams_t *s) {
    json_t *a = gr2_args();
    gr2_s(a, "reason", "messaging gateway housekeeping");
    json_t *r = gr2_call(s, "trim_memory_when_idle", a);
    if (gr2_raised(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: gr2_housekeeping_checkpoint_prune @ gateway/run.py:_housekeeping_checkpoint_prune */
static json_t *gr2_housekeeping_checkpoint_prune(const gr2_seams_t *s) {
    json_t *r = gr2_call(s, "auto_prune_from_config", gr2_args());
    if (gr2_raised(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: gr2_housekeeping_plugin_update_check @ gateway/run.py:_housekeeping_plugin_update_check */
static json_t *gr2_housekeeping_plugin_update_check(const gr2_seams_t *s) {
    json_t *a = gr2_args();
    gr2_s(a, "log", "logger");
    json_t *r = gr2_call(s, "maybe_run_gateway_check", a);
    if (gr2_raised(r)) return r;
    json_free(r);
    return NULL;
}
/* PoP: gr2_launch_sessions_dir @ gateway/run.py:_launch_sessions_dir */
static json_t *gr2_launch_sessions_dir(const json_t *config, const gr2_seams_t *s) {
    const json_t *sd = gr2_oget(config, "sessions_dir");  /* getattr default None */
    if (gr2_is_null_node(sd)) return NULL;
    const char *sds = gr2_sval(sd);
    if (!sds) {
        char m[160];
        snprintf(m, sizeof(m),
                 "argument should be a str or an os.PathLike object where __fspath__ "
                 "returns a str, not <class '%s'>", gr2_type_name(sd));
        return gr2_raise("TypeError", m);
    }
    json_t *hr = gr2_call(s, "get_hermes_home", gr2_args());
    if (gr2_raised(hr)) return hr;
    const char *hs = gr2_sval(hr);
    if (!hs) {
        char m[160];
        snprintf(m, sizeof(m),
                 "argument should be a str or an os.PathLike object where __fspath__ "
                 "returns a str, not <class '%s'>", gr2_type_name(hr));
        json_free(hr);
        return gr2_raise("TypeError", m);
    }
    char *home = gr2_posix_norm(hs);
    char *dir = gr2_posix_norm(sds);
    json_free(hr);
    json_t *out = json_array();
    json_append(out, json_string(home));
    json_append(out, json_string(dir));
    free(home); free(dir);
    return out;
}

/* PoP: gr2_profile_sessions_dir @ gateway/run.py:_profile_sessions_dir */
static json_t *gr2_profile_sessions_dir(const json_t *launch, const gr2_seams_t *s) {
    json_t *hr = gr2_call(s, "get_hermes_home", gr2_args());
    if (gr2_raised(hr)) return hr;
    const char *hs = gr2_sval(hr);
    if (!hs) {
        char m[160];
        snprintf(m, sizeof(m),
                 "argument should be a str or an os.PathLike object where __fspath__ "
                 "returns a str, not <class '%s'>", gr2_type_name(hr));
        json_free(hr);
        return gr2_raise("TypeError", m);
    }
    char *home = gr2_posix_norm(hs);
    json_free(hr);
    char *result = NULL;
    json_t *err = NULL;
    if (!launch || launch->type == JSON_NULL) {
        result = gr2_path_join_sessions(home);
    } else if (launch->type != JSON_ARRAY) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object is not subscriptable", gr2_type_name(launch));
        err = gr2_raise("TypeError", m);
    } else if (json_len(launch) < 1) {
        err = gr2_raise("IndexError", "tuple index out of range");
    } else {
        const json_t *h = json_get(launch, 0);
        const char *hstr = gr2_sval(h);
        if (!hstr) {
            char m[160];
            snprintf(m, sizeof(m),
                     "argument should be a str or an os.PathLike object where __fspath__ "
                     "returns a str, not <class '%s'>", gr2_type_name(h));
            err = gr2_raise("TypeError", m);
        } else {
            char *lh = gr2_posix_norm(hstr);
            bool same = strcmp(lh, home) == 0;
            free(lh);
            if (same) {
                if (json_len(launch) < 2) {
                    err = gr2_raise("IndexError", "tuple index out of range");
                } else {
                    const json_t *d = json_get(launch, 1);
                    const char *dstr = gr2_sval(d);
                    if (!dstr) {
                        char m[160];
                        snprintf(m, sizeof(m),
                                 "argument should be a str or an os.PathLike object where "
                                 "__fspath__ returns a str, not <class '%s'>", gr2_type_name(d));
                        err = gr2_raise("TypeError", m);
                    } else {
                        result = gr2_posix_norm(dstr);
                    }
                }
            } else {
                result = gr2_path_join_sessions(home);
            }
        }
    }
    free(home);
    if (err) { free(result); return err; }
    json_t *out = json_string(result ? result : "");
    free(result);
    return out;
}

/* PoP: gr2_guard_corrupt_user_config @ gateway/run.py:_guard_corrupt_user_config */
static json_t *gr2_guard_corrupt_user_config(const gr2_seams_t *s) {
    json_t *r = gr2_call(s, "require_parseable_user_config", gr2_args());
    if (!gr2_raised(r)) { json_free(r); return NULL; }
    if (strcmp(gr2_kind(r), "InvalidUserConfigError") != 0) return r;
    /* print(f"Error: {exc}", file=sys.stderr); raise SystemExit(2) from exc */
    const char *msg = gr2_msg(r);
    size_t n = strlen(msg);
    char *text = malloc(n + 16);
    if (text) snprintf(text, n + 16, "Error: %s", msg);
    json_t *a = gr2_args();
    gr2_s(a, "text", text ? text : "");
    gr2_s(a, "file", "stderr");
    free(text);
    json_t *pr = gr2_call(s, "print", a);
    if (gr2_raised(pr)) { json_free(r); return pr; }
    json_free(pr);
    json_free(r);
    return gr2_raise("SystemExit", "2");
}

/* PoP: gr2_wait_for_pid_exit @ gateway/run.py:_wait_for_pid_exit */
static json_t *gr2_wait_for_pid_exit(double pid, double attempts, double delay,
                                     const gr2_seams_t *s) {
    for (double i = 0; i < attempts; i++) {
        json_t *a = gr2_args();
        gr2_n(a, "pid", pid);
        json_t *r = gr2_call(s, "_pid_exists", a);
        if (gr2_raised(r)) return r;
        bool exists = gr2_truth(r);
        json_free(r);
        if (!exists) return json_bool(true);
        a = gr2_args();
        gr2_n(a, "delay", delay);
        r = gr2_call(s, "asyncio.sleep", a);
        if (gr2_raised(r)) return r;
        json_free(r);
    }
    return json_bool(false);
}

/* PoP: gr2_resolve_handoff_watch_scopes @ gateway/run.py:_resolve_handoff_watch_scopes */
static json_t *gr2_resolve_handoff_watch_scopes(const json_t *runner, const gr2_seams_t *s) {
    const json_t *config = gr2_oget(runner, "config");
    if (!gr2_is_null_node(config)
            && !gr2_truth(gr2_oget(config, "multiplex_profiles"))) {
        json_t *pair = json_array();
        json_append(pair, json_null());
        json_append(pair, json_null());
        json_t *out = json_array();
        json_append(out, pair);
        return out;
    }
    /* callable(offload): the fixture marker is a non-empty dict (a Python
     * dict is never callable; the oracle binds a stub only for that shape). */
    const json_t *offload = gr2_oget(runner, "_run_in_executor_with_context");
    bool callable = offload && offload->type == JSON_OBJECT && offload->c.count > 0;
    json_t *a = gr2_args();
    if (callable) {
        gr2_s(a, "fn", "_handoff_watch_scopes");
        gr2_j(a, "runner", runner);
        return gr2_call(s, "run_in_executor_with_context", a);
    }
    gr2_j(a, "runner", runner);
    return gr2_call(s, "_handoff_watch_scopes", a);
}
/* STANDALONE_DEPRECATION_NOTICE (hermes_cli/gateway_multiplex_mode.py:41) */
static const char *gr2_standalone_notice =
    "gateway.standalone is a temporary compatibility shim while multiplexing gaps are fixed; "
    "it will be removed once they are \xe2\x80\x94 plan to fold this profile with "
    "`hermes gateway migrate --multiplex`.";

/* PoP: gr2_log_standalone_profiles_at_boot @ gateway/run.py:_log_standalone_profiles_at_boot */
static json_t *gr2_log_standalone_profiles_at_boot(const json_t *runner, const gr2_seams_t *s) {
    json_t *served = NULL, *profiles = NULL, *r = NULL, *lr = NULL;
    json_t *ret = NULL;
    bool failed = false;
    if (!json_has(runner, "config")) { failed = true; goto except_path; }
    const json_t *config = gr2_oget(runner, "config");
    if (gr2_is_null_node(config)) return NULL;  /* getattr(None, ..., False) */
    if (!gr2_truth(gr2_oget(config, "multiplex_profiles"))) return NULL;
    served = gr2_call(s, "served_profile_names", gr2_args());
    if (gr2_raised(served)) {
        if (gr2_is_base_exc(gr2_kind(served))) { ret = served; served = NULL; goto cleanup; }
        failed = true; goto except_path;
    }
    if (!served || served->type != JSON_ARRAY) { failed = true; goto except_path; }
    {
        json_t *a = gr2_args();
        gr2_b(a, "multiplex", true);
        gr2_b(a, "include_standalone", true);
        gr2_b(a, "include_parked", true);
        profiles = gr2_call(s, "profiles_to_serve", a);
    }
    if (gr2_raised(profiles)) {
        if (gr2_is_base_exc(gr2_kind(profiles))) { ret = profiles; profiles = NULL; goto cleanup; }
        failed = true; goto except_path;
    }
    if (!profiles || profiles->type != JSON_ARRAY) { failed = true; goto except_path; }
    for (size_t i = 0; i < json_len(profiles); i++) {
        const json_t *pair = json_get(profiles, i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) != 2) {
            failed = true; goto except_path;  /* unpack ValueError swallowed */
        }
        const char *name = gr2_sval(json_get(pair, 0));
        if (!name) { failed = true; goto except_path; }
        if (strcmp(name, "default") == 0) continue;
        bool in_served = false;
        for (size_t j = 0; j < json_len(served); j++) {
            const char *sn = gr2_sval(json_get(served, j));
            if (sn && strcmp(sn, name) == 0) { in_served = true; break; }
        }
        if (in_served) continue;
        json_t *a = gr2_args();
        gr2_j(a, "home", json_get(pair, 1));
        r = gr2_call(s, "profile_is_standalone", a);
        if (gr2_raised(r)) {
            if (gr2_is_base_exc(gr2_kind(r))) { ret = r; r = NULL; goto cleanup; }
            failed = true; goto except_path;
        }
        bool standalone = gr2_truth(r);
        json_free(r); r = NULL;
        if (!standalone) continue;
        json_t *la = gr2_args2s(name, gr2_standalone_notice);
        lr = gr2_log(s, "warning",
                     "profile '%s' is standalone (gateway.standalone: true); not served by "
                     "this gateway. %s", la);
        if (gr2_raised(lr)) {
            if (gr2_is_base_exc(gr2_kind(lr))) { ret = lr; lr = NULL; goto cleanup; }
            failed = true; goto except_path;
        }
        json_free(lr); lr = NULL;
    }
    goto cleanup;
except_path:
    /* except Exception: logger.warning(..., exc_info=True) */
    (void)failed;
    lr = gr2_log_exc_info(s, "warning", "standalone-profile boot notice failed");
    if (gr2_raised(lr)) { ret = lr; lr = NULL; }
cleanup:
    json_free(served);
    json_free(profiles);
    json_free(r);
    json_free(lr);
    return ret;
}

/* PoP: gr2_host_attach_or_none @ gateway/run.py:_host_attach_or_none */
static json_t *gr2_host_attach_or_none(bool replace, bool force, const gr2_seams_t *s) {
    if (force) {
        json_t *r = gr2_log(s, "warning",
                            "--force: starting a gateway without asking the host owner.", NULL);
        if (gr2_raised(r)) return r;
        json_free(r);
        return NULL;
    }
    json_t *hr = gr2_call(s, "get_hermes_home", gr2_args());
    if (gr2_raised(hr)) return hr;
    const char *hs = gr2_sval(hr);
    char *home = gr2_posix_norm(hs ? hs : "");
    json_free(hr);
    json_t *a = gr2_args();
    gr2_s(a, "home", home);
    gr2_b(a, "replace", replace);
    free(home);
    json_t *decision = gr2_call(s, "decide", a);
    if (gr2_raised(decision)) return decision;
    if (!decision || decision->type != JSON_OBJECT) {
        char m[96];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'outcome'", gr2_type_name(decision));
        json_free(decision);
        return gr2_raise("AttributeError", m);
    }
    const char *outcome = gr2_sval(gr2_oget(decision, "outcome"));
    if (!outcome) {
        json_free(decision);
        return gr2_raise("AttributeError", "missing attribute 'outcome'");
    }
    const json_t *owner = gr2_oget(decision, "owner");
    bool owner_present = !gr2_is_null_node(owner);
    json_t *ret = NULL;
    if (strcmp(outcome, "attach") == 0 || strcmp(outcome, "refuse") == 0) {
        bool is_attach = strcmp(outcome, "attach") == 0;
        json_t *desc = NULL;
        if (owner_present) {
            a = gr2_args();
            gr2_j(a, "pid", gr2_oget(owner, "pid"));
            desc = gr2_call(s, "owner.describe", a);
            if (gr2_raised(desc)) { json_free(decision); return desc; }
        } else {
            desc = json_string("unknown");
        }
        json_t *lr = gr2_log(s, is_attach ? "info" : "error",
                             is_attach
                                 ? "Attaching to the host gateway instead of starting a second one: %s"
                                 : "Refusing to start a second gateway on this host: %s",
                             gr2_args1(desc));
        json_free(desc);
        if (gr2_raised(lr)) { json_free(decision); return lr; }
        json_free(lr);
        a = gr2_args();
        gr2_j(a, "text", gr2_oget(decision, "message"));
        json_t *pr = gr2_call(s, "print", a);
        if (gr2_raised(pr)) { json_free(decision); return pr; }
        json_free(pr);
        ret = json_bool(is_attach);
    } else if (strcmp(outcome, "replace-host") == 0 && owner_present) {
        a = gr2_args();
        gr2_j(a, "existing_pid", gr2_oget(owner, "pid"));
        gr2_b(a, "replace", true);
        json_t *r = gr2_call(s, "_start_gateway_replace_existing_instance", a);
        if (gr2_raised(r)) { json_free(decision); return r; }
        bool ok = gr2_truth(r);
        json_free(r);
        if (!ok) ret = json_bool(false);
    }
    json_free(decision);
    return ret;
}