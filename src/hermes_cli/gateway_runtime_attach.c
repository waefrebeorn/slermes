/*
 * gateway_runtime_attach.c -- real C11 port of one client attach
 *   hermes_cli/gateway_runtime_attach.py.
 *
 * PoP: w20_gatewayrun_a66bb8_client_launch_env @ hermes_cli/gateway_runtime_attach.py:CLIENT_LAUNCH_ENV
 * PoP: w20_gatewayrun_a66bb8_discover_timeout @ hermes_cli/gateway_runtime_attach.py:DISCOVER_TIMEOUT
 * PoP: w20_gatewayrun_a66bb8_service_session_warning @ hermes_cli/gateway_runtime_attach.py:SERVICE_SESSION_START_WARNING
 * PoP: w20_gatewayrun_a66bb8_windows_service_session @ hermes_cli/gateway_runtime_attach.py:windows_service_session
 * PoP: w20_gatewayrun_a66bb8_failure_message @ hermes_cli/gateway_runtime_attach.py:failure_message
 * PoP: w20_gatewayrun_a66bb8_bounded_detail @ hermes_cli/gateway_runtime_discovery.py:DiscoveryError.bounded_detail
 * PoP: w20_gatewayrun_a66bb8_ticket_unavailable @ hermes_cli/gateway_runtime_attach.py:TicketUnavailable
 * PoP: w20_gatewayrun_a66bb8_attach @ hermes_cli/gateway_runtime_attach.py:attach_gateway
 *
 * The runtime controls (discover/ensure), the owner's control socket and the
 * ticket RPC are not pure: they are injected here as a scripted receipt and a
 * scripted mint result, exactly like the reference gate injects its service.
 * Launch policy, verdict text and the attach decision path are the ported code.
 */
#include "hermes_json.h"
#include "noc_w20_gatewayrun_a66bb8.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GWATT_DISCOVER_TIMEOUT 5.0
#define GWATT_DETAIL_BOUND 500

/* CLIENT_LAUNCH_ENV, source order: launch policy travels in session.create,
 * never into daemon-wide defaults. */
static const char *const GWATT_CLIENT_LAUNCH_ENV[] = {
    "HERMES_MODEL", "HERMES_INFERENCE_MODEL", "HERMES_TUI_PROVIDER",
    "HERMES_INFERENCE_PROVIDER", "HERMES_TUI_TOOLSETS", "HERMES_TUI_SKILLS",
    "HERMES_CWD", "TERMINAL_CWD", "HERMES_YOLO", "HERMES_ACCEPT_HOOKS",
    "HERMES_TUI_CHECKPOINTS", "HERMES_TUI_PASS_SESSION_ID",
    "HERMES_KANBAN_BOARD", "HERMES_TUI_CLIENT_ENV",
    NULL
};

static const char GWATT_SERVICE_SESSION_WARNING[] =
    "warning: this is an elevated Windows session-0 login (e.g. an administrator OpenSSH login); unless a "
    "registered Hermes Scheduled Task starts it, a gateway started here refuses the user's normal Hermes windows "
    "(gateway_other_session). Start Hermes from a normal (non-administrator) window on this machine's desktop, "
    "then attach without --start";

json_t *w20_gatewayrun_a66bb8_client_launch_env(void) {
    json_t *arr = json_array();
    int i;
    for (i = 0; GWATT_CLIENT_LAUNCH_ENV[i]; i++) {
        json_append(arr, json_string(GWATT_CLIENT_LAUNCH_ENV[i]));
    }
    return arr;
}

double w20_gatewayrun_a66bb8_discover_timeout(void) {
    return GWATT_DISCOVER_TIMEOUT;
}

const char *w20_gatewayrun_a66bb8_service_session_warning(void) {
    return GWATT_SERVICE_SESSION_WARNING;
}

/* ---- small JSON readers -------------------------------------------------- */

static const char *gwa_str(const json_t *obj, const char *key) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static const char *gwa_str_or(const json_t *obj, const char *key, const char *def) {
    const char *s = gwa_str(obj, key);
    return s ? s : def;
}

static int gwa_bool_or(const json_t *obj, const char *key, int def) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return def;
    v = json_obj_get(obj, key);
    if (!v || v->type != JSON_BOOL) return def;
    return v->bool_val ? 1 : 0;
}

static double gwa_num_or(const json_t *obj, const char *key, double def) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return def;
    v = json_obj_get(obj, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

/* Python truthiness of a decoded value; absent reads as None. */
static int gwa_truthy(const json_t *v) {
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

/* str(x) for the scalar shapes a receipt detail can carry. */
static void gwa_scalar_text(const json_t *v, char *buf, size_t cap) {
    if (!v) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
    case JSON_STRING: snprintf(buf, cap, "%s", v->str_val); break;
    case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); break;
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            fabs(v->num_val) < 9.007199254740992e15) {
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, cap, "%.17g", v->num_val);
        }
        break;
    case JSON_NULL:   snprintf(buf, cap, "None"); break;
    default:          snprintf(buf, cap, "%s", ""); break;
    }
}

/* str.splitlines(): \n, \r\n, \r, \v and \f all end a line. */
static int gwa_is_line_break(char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static void gwa_append_fmt(char **buf, size_t *len, size_t *cap, const char *fmt, ...) {
    va_list ap;
    int need;
    va_start(ap, fmt);
    need = vsnprintf(*buf + *len, *cap - *len, fmt, ap);
    va_end(ap);
    if (need < 0) return;
    if ((size_t)need >= *cap - *len) {
        size_t want = *cap * 2 + (size_t)need + 64;
        char *grown = (char *)realloc(*buf, want);
        if (!grown) return;
        *buf = grown;
        *cap = want;
        va_start(ap, fmt);
        vsnprintf(*buf + *len, *cap - *len, fmt, ap);
        va_end(ap);
    }
    *len += strlen(*buf + *len);
}

json_t *w20_gatewayrun_a66bb8_failure_message(const json_t *receipt) {
    const char *state = gwa_str_or(receipt, "state", "");
    const char *reason_code = gwa_str(receipt, "reason_code");
    const char *rc = (reason_code && reason_code[0]) ? reason_code : "not ready";
    json_t *detail = (receipt && receipt->type == JSON_OBJECT) ? json_obj_get(receipt, "detail") : NULL;
    char head[1024];
    char detail_text[8192];
    char *out;
    size_t len = 0, cap = 4096;
    size_t i;

    snprintf(head, sizeof(head), "gateway %s: %s", state, rc);
    if (!gwa_truthy(detail)) return json_string(head);

    gwa_scalar_text(detail, detail_text, sizeof(detail_text));
    if (receipt && gwa_str(receipt, "reason_code") &&
        !strcmp(gwa_str(receipt, "reason_code"), "runtime_exited")) {
        char *joined = (char *)malloc(strlen(head) + strlen(detail_text) + 2);
        if (!joined) return json_string(head);
        sprintf(joined, "%s\n%s", head, detail_text);
        {
            json_t *j = json_string(joined);
            free(joined);
            return j;
        }
    }

    out = (char *)malloc(cap);
    if (!out) return json_string(head);
    out[0] = '\0';
    gwa_append_fmt(&out, &len, &cap, "%s", head);
    i = 0;
    while (i <= strlen(detail_text)) {
        size_t start = i, end;
        char line[2048];
        size_t n, k;
        while (detail_text[i] && !gwa_is_line_break(detail_text[i])) i++;
        end = i;
        n = end - start;
        if (n >= sizeof(line)) n = sizeof(line) - 1;
        memcpy(line, detail_text + start, n);
        line[n] = '\0';
        /* str.strip() also trims spaces and tabs. */
        k = 0;
        while (line[k] == ' ' || line[k] == '\t') k++;
        memmove(line, line + k, strlen(line + k) + 1);
        n = strlen(line);
        while (n && (line[n - 1] == ' ' || line[n - 1] == '\t')) line[--n] = '\0';
        if (n) gwa_append_fmt(&out, &len, &cap, "\ngateway %s: %s", state, line);
        if (!detail_text[i]) break;
        /* skip the break itself (and the LF of a CRLF pair) */
        if (detail_text[i] == '\r' && detail_text[i + 1] == '\n') i += 2;
        else i += 1;
    }
    {
        json_t *j = json_string(out);
        free(out);
        return j;
    }
}

/* ---- bounded_detail ------------------------------------------------------ */

static const char GWATT_TRUNC_MARKER[] = "\xe2\x80\xa6 [truncated]";   /* U+2026 + " [truncated]" */
#define GWATT_TRUNC_MARKER_CP 13
static const char GWATT_FIX_NOTICE[] = "; repair command omitted (path too long to print)";

json_t *w20_gatewayrun_a66bb8_bounded_detail(const json_t *req) {
    json_t *detail_j = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "detail") : NULL;
    const char *fix = gwa_str(req, "fix");
    const char *cause;
    char *full;
    double limit_d = gwa_num_or(req, "limit", GWATT_DETAIL_BOUND);
    size_t limit = limit_d > 0 ? (size_t)limit_d : 0;
    const char *notice = fix ? GWATT_FIX_NOTICE : "";
    size_t notice_len = strlen(notice);
    size_t room, cause_len;

    if (!gwa_truthy(detail_j)) return json_null();
    if (detail_j->type != JSON_STRING) {
        /* detail is built from strings upstream; a non-string degrades to str() */
        char buf[64];
        gwa_scalar_text(detail_j, buf, sizeof(buf));
        return json_string(buf);
    }
    cause = detail_j->str_val;
    /* DiscoveryError.__init__ joins the repair command onto the detail; the
     * bound applies to that joined text while the cause stays the bare detail. */
    if (fix) {
        full = (char *)malloc(strlen(cause) + strlen(fix) + 3);
        if (!full) return json_string(cause);
        sprintf(full, "%s; %s", cause, fix);
    } else {
        full = (char *)malloc(strlen(cause) + 1);
        if (!full) return json_string(cause);
        strcpy(full, cause);
    }
    if (strlen(full) <= limit) {
        json_t *j = json_string(full);
        free(full);
        return j;
    }
    free(full);

    room = limit > notice_len ? limit - notice_len : 0;
    cause_len = strlen(cause);
    if (cause_len <= room) {
        char *joined = (char *)malloc(cause_len + notice_len + 1);
        json_t *j;
        if (!joined) return json_null();
        sprintf(joined, "%s%s", cause, notice);
        j = json_string(joined);
        free(joined);
        return j;
    }
    {
        size_t cut = room > GWATT_TRUNC_MARKER_CP ? room - GWATT_TRUNC_MARKER_CP : 0;
        size_t total = cut + strlen(GWATT_TRUNC_MARKER) + notice_len + 1;
        char *joined = (char *)malloc(total);
        json_t *j;
        if (!joined) return json_null();
        memcpy(joined, cause, cut);
        joined[cut] = '\0';
        strcat(joined, GWATT_TRUNC_MARKER);
        strcat(joined, notice);
        j = json_string(joined);
        free(joined);
        return j;
    }
}

json_t *w20_gatewayrun_a66bb8_ticket_unavailable(const json_t *req) {
    const char *reason = gwa_str_or(req, "reason", "");
    json_t *detail = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "detail") : NULL;
    json_t *out = json_object();
    char message[1024];
    snprintf(message, sizeof(message), "ticket_unavailable: %s", reason);
    json_set(out, "message", json_string(message));
    json_set(out, "reason", json_string(reason));
    json_set(out, "detail", detail ? json_copy(detail) : json_null());
    return out;
}

/* ---- windows_service_session -------------------------------------------- */

int w20_gatewayrun_a66bb8_windows_service_session(const json_t *probe) {
    json_t *sid;
    const char *err;
    if (!probe || probe->type != JSON_OBJECT) return 0;
    err = gwa_str(probe, "error");
    if (err && err[0]) return 0;              /* the probe raised: False, never a crash */
    sid = json_obj_get(probe, "session_id");
    /* `windows_session_id() == 0`: True is not 0 in Python for a stray bool,
     * and a missing fact is None, which is not 0 either. */
    if (!sid || sid->type != JSON_NUMBER) return 0;
    return sid->num_val == 0.0 && gwa_bool_or(probe, "elevated", 0);
}

/* ---- attach_gateway ------------------------------------------------------ */

typedef struct {
    const json_t *receipt;
    int attachable;
    double timeout;
} gwa_receipt;

static gwa_receipt gwa_read_receipt(const json_t *receipts, const char *which) {
    gwa_receipt r;
    r.receipt = (receipts && receipts->type == JSON_OBJECT) ? json_obj_get(receipts, which) : NULL;
    if (r.receipt && r.receipt->type != JSON_OBJECT) r.receipt = NULL;
    r.attachable = gwa_bool_or(r.receipt, "attachable", 0);
    r.timeout = 0.0;
    return r;
}

/* TicketUnavailable(exc.reason, exc.bounded_detail(500)) for a DiscoveryError,
 * or TicketUnavailable(type(exc).__name__) for a raw OSError/ValueError. */
static json_t *gwa_mint_failure(const json_t *mint) {
    const char *error = gwa_str(mint, "error");
    json_t *out = json_object();
    char message[1200];
    if (error && !strcmp(error, "DiscoveryError")) {
        json_t *bounded = w20_gatewayrun_a66bb8_bounded_detail(mint);
        const char *reason = gwa_str_or(mint, "reason", "");
        snprintf(message, sizeof(message), "ticket_unavailable: %s", reason);
        json_set(out, "status", json_string("raised"));
        json_set(out, "type", json_string("TicketUnavailable"));
        json_set(out, "message", json_string(message));
        json_set(out, "reason", json_string(reason));
        json_set(out, "detail", bounded ? bounded : json_null());
        return out;
    }
    if (error && (!strcmp(error, "OSError") || !strcmp(error, "ValueError"))) {
        snprintf(message, sizeof(message), "ticket_unavailable: %s", error);
        json_set(out, "status", json_string("raised"));
        json_set(out, "type", json_string("TicketUnavailable"));
        json_set(out, "message", json_string(message));
        json_set(out, "reason", json_string(error));
        json_set(out, "detail", json_null());
        return out;
    }
    if (error && error[0]) {
        /* anything else propagates untouched: only the class and str(exc) */
        const char *text = gwa_str_or(mint, "message", error);
        json_set(out, "status", json_string("raised"));
        json_set(out, "type", json_string(error));
        json_set(out, "message", json_string(text));
        json_set(out, "reason", json_null());
        json_set(out, "detail", json_null());
        return out;
    }
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string("TicketUnavailable"));
    json_set(out, "reason", json_string("unknown"));
    json_set(out, "detail", json_null());
    snprintf(message, sizeof(message), "ticket_unavailable: %s", "unknown");
    json_set(out, "message", json_string(message));
    return out;
}

json_t *w20_gatewayrun_a66bb8_attach(const json_t *req) {
    int start = gwa_bool_or(req, "start", 0);
    int recover = gwa_bool_or(req, "recover", 0);
    double timeout = gwa_num_or(req, "timeout", 0.0);
    const char *home = gwa_str_or(req, "home", "");
    const json_t *receipts = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "receipts") : NULL;
    const json_t *mint = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "mint") : NULL;
    gwa_receipt receipt;
    json_t *out;
    const char *path;
    int ensures = 0;
    double discover_timeout = 0.0;
    int has_discover_timeout = 0;

    receipt = gwa_read_receipt(receipts, start ? "ensure" : "discover");
    if (start) {
        ensures = 1;
        receipt.timeout = timeout;                 /* ensure_gateway_runtime(home, timeout=..) */
        path = "ensure";
    } else {
        discover_timeout = timeout < GWATT_DISCOVER_TIMEOUT ? timeout : GWATT_DISCOVER_TIMEOUT;
        has_discover_timeout = 1;
        path = "discover";
    }
    if (!start && recover && !receipt.attachable) {
        receipt = gwa_read_receipt(receipts, "ensure");
        ensures = 1;                                /* the re-ensure replaces the verdict */
        receipt.timeout = timeout;
        path = "ensure";
    }

    out = json_object();
    json_set(out, "path", json_string(path));
    json_set(out, "ensures", json_number((double)ensures));
    json_set(out, "discover_timeout",
             has_discover_timeout ? json_number(discover_timeout) : json_null());
    json_set(out, "ensure_timeout", ensures ? json_number(receipt.timeout) : json_null());
    json_set(out, "attached", json_bool(receipt.attachable ? 1 : 0));
    {
        const char *state = gwa_str(receipt.receipt, "state");
        json_set(out, "state", state ? json_string(state) : json_null());
    }
    if (!receipt.attachable) {
        json_set(out, "ticket", json_null());
        json_set(out, "mint", json_null());
        return out;
    }
    if (mint && gwa_str(mint, "error") && gwa_str(mint, "error")[0]) {
        json_t *failure = gwa_mint_failure(mint);
        json_free(out);
        return failure;
    }
    {
        json_t *call = json_object();
        json_set(call, "home", json_string(home));
        json_set(call, "endpoint", json_string(gwa_str_or(receipt.receipt, "endpoint", "")));
        json_set(call, "purpose", json_null());
        json_set(call, "scope", json_null());
        json_set(out, "ticket", json_string(gwa_str_or(mint, "ticket", "")));
        json_set(out, "mint", call);
    }
    return out;
}
