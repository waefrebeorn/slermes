/*
 * web_server_lifecycle.c - C11 port of upstream
 * hermes_cli/web_server_lifecycle.py (gate w34_webserverl_f96a1a).
 *
 * PoP: w34_webserverl_f96a1a_constants @ hermes_cli/web_server_lifecycle.py:<module literals>
 * PoP: w34_webserverl_f96a1a_valid_parent_start_marker @ hermes_cli/web_server_lifecycle.py:_valid_parent_start_marker
 * PoP: w34_webserverl_f96a1a_parent_start_markers_match @ hermes_cli/web_server_lifecycle.py:_parent_start_markers_match
 * PoP: w34_webserverl_f96a1a_parent_start_marker_mismatch_is_conclusive @ hermes_cli/web_server_lifecycle.py:_parent_start_marker_mismatch_is_conclusive
 * PoP: w34_webserverl_f96a1a_serve_orphaned_decision @ hermes_cli/web_server_lifecycle.py:_is_serve_orphaned
 * PoP: w34_webserverl_f96a1a_watchdog_plan @ hermes_cli/web_server_lifecycle.py:_start_parent_death_watchdog
 * PoP: w34_webserverl_f96a1a_addr_in_use @ hermes_cli/web_server_lifecycle.py:_is_addr_in_use_error
 * PoP: w34_webserverl_f96a1a_port_preflight @ hermes_cli/web_server_lifecycle.py:_port_bind_conflict
 * PoP: w34_webserverl_f96a1a_port_in_use_sentinel @ hermes_cli/web_server_lifecycle.py:_PORT_IN_USE_SENTINEL
 * PoP: w34_webserverl_f96a1a_port_in_use_hint @ hermes_cli/web_server_lifecycle.py:_report_port_in_use
 * PoP: w34_webserverl_f96a1a_forwarded_allow_ips @ hermes_cli/web_server_lifecycle.py:_dashboard_forwarded_allow_ips
 * PoP: w34_webserverl_f96a1a_browser_open_url @ hermes_cli/web_server_lifecycle.py:_maybe_open_browser
 *
 * Seams that stay in the caller: the PID/start-marker process probes
 * (/proc, OpenProcess, ps), socket bind probes, ready-file writes, fd-1
 * sentinel writes, signal delivery, imports/uvicorn logging and the
 * serve/browser threads. What is ported is the part that is a pure function
 * of values: which parent markers are usable and when a mismatch proves PID
 * reuse, whether the watchdog may arm, whether a bind failure is a conflict,
 * which proxy addresses may be trusted, and the URL the opener would use.
 *
 * IPv4/IPv6 parsing and rendering are hand-rolled (no winsock link dependency
 * on this host) and follow Python's ``ipaddress`` textual rules: strict dotted
 * quads, RFC 5952 compression of the longest zero run of length >= 2,
 * ``::ffff:a.b.c.d`` for IPv4-mapped addresses, lowercase hex.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "noc_w34_webserverl_f96a1a.h"

/* ---------------------------------------------------------------- helpers */

static const char *w34_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int w34_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python's numeric view of a JSON value. */
static int w34_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

static int w34_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() over ASCII whitespace; malloc'd copy (caller frees). */
static char *w34_strip_dup(const char *s) {
    size_t b = 0, e, n;
    char *out;
    if (!s) return NULL;
    e = strlen(s);
    while (s[b] && w34_is_ws(s[b])) b++;
    while (e > b && w34_is_ws(s[e - 1])) e--;
    n = e - b;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s + b, n);
    out[n] = '\0';
    return out;
}

/* True when s is a non-empty ASCII digit run (Python str.isdigit, ASCII
 * subset: a marker that carries non-ASCII digits never comes off a process
 * table this backend wrote). */
static int w34_all_digits(const char *s) {
    size_t i;
    if (!s || !*s) return 0;
    for (i = 0; s[i]; i++)
        if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* int(text): optional surrounding whitespace and sign, ASCII digits only
 * (Python also allows '_' separators; not mirrored). */
static int w34_py_int(const char *s, long long *out) {
    const char *p = s;
    int neg = 0, digits = 0;
    unsigned long long acc = 0;
    if (!s) return 0;
    while (*p && w34_is_ws(*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') {
        if (acc > (unsigned long long)LLONG_MAX / 10ULL) return 0;
        acc = acc * 10ULL + (unsigned long long)(*p - '0');
        if (acc > (unsigned long long)LLONG_MAX) return 0;
        p++;
        digits++;
    }
    if (!digits) return 0;
    while (*p && w34_is_ws(*p)) p++;
    if (*p) return 0;
    *out = neg ? -(long long)acc : (long long)acc;
    return 1;
}

/* float(text) for the shapes a config knob holds; strtod + full consumption. */
static int w34_py_float(const char *s, double *out) {
    char *end = NULL;
    double v;
    if (!s) return 0;
    v = strtod(s, &end);
    if (end == s) return 0;
    while (*end && w34_is_ws(*end)) end++;
    if (*end) return 0;
    *out = v;
    return 1;
}

static int w34_startswith(const char *s, const char *prefix) {
    size_t n;
    if (!s || !prefix) return 0;
    n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

/* --------------------------------------------------- parent marker predicates */

json_t *w34_webserverl_f96a1a_constants(void) {
    json_t *o = json_object();
    json_t *defaults = json_array();
    json_append(defaults, json_string("127.0.0.1"));
    json_append(defaults, json_string("::1"));
    json_set(o, "port_in_use_exit_code", json_number(75.0));
    json_set(o, "orphan_exit_ceiling_s", json_number(10.0));
    json_set(o, "sentinel", json_string("BACKEND_PORT_IN_USE port={port}"));
    json_set(o, "default_forwarded_allow_ips", defaults);
    return o;
}

json_t *w34_webserverl_f96a1a_valid_parent_start_marker(const json_t *req) {
    const char *marker = w34_str(json_obj_get(req, "marker"));
    const char *sep, *value, *p;
    char *prefix = NULL;
    int ok = 0;

    if (!marker) return json_bool(0);
    sep = strchr(marker, ':');
    if (!sep || !sep[1]) return json_bool(0);
    value = sep + 1;

    /* value != value.strip() -> a token that lost its tail is never usable. */
    {
        char *stripped = w34_strip_dup(value);
        if (!stripped) return json_bool(0);
        if (strcmp(stripped, value) != 0) { free(stripped); return json_bool(0); }
        free(stripped);
    }

    prefix = (char *)malloc((size_t)(sep - marker) + 1);
    if (!prefix) return json_bool(0);
    memcpy(prefix, marker, (size_t)(sep - marker));
    prefix[sep - marker] = '\0';

    if (!strcmp(prefix, "linux") || !strcmp(prefix, "win") || !strcmp(prefix, "winms")) {
        ok = w34_all_digits(value);
    } else if (!strcmp(prefix, "ps")) {
        /* "Sat Aug 29 15:04:31 2026": >= 4 tokens, a 4-digit year, a HH:MM:SS. */
        size_t ntokens = 0;
        int has_year = 0, has_time = 0;
        p = value;
        while (*p) {
            const char *start;
            size_t len;
            while (*p && w34_is_ws(*p)) p++;
            if (!*p) break;
            start = p;
            while (*p && !w34_is_ws(*p)) p++;
            len = (size_t)(p - start);
            ntokens++;
            if (len == 4 && start[0] >= '0' && start[0] <= '9' && start[1] >= '0'
                && start[1] <= '9' && start[2] >= '0' && start[2] <= '9'
                && start[3] >= '0' && start[3] <= '9')
                has_year = 1;
            if (memchr(start, ':', len)) has_time = 1;
        }
        ok = (ntokens >= 4) && has_year && has_time;
    }
    free(prefix);
    return json_bool(ok ? 1 : 0);
}

json_t *w34_webserverl_f96a1a_parent_start_markers_match(const json_t *req) {
    const char *actual = w34_str(json_obj_get(req, "actual"));
    const char *expected = w34_str(json_obj_get(req, "expected"));
    long long ticks, unix_ms;

    if (!actual || !expected) return json_bool(0);
    if (!strcmp(actual, expected)) return json_bool(1);
    if (!w34_startswith(actual, "win:") || !w34_startswith(expected, "winms:")) return json_bool(0);

    if (!w34_py_int(actual + 4, &ticks)) return json_bool(0);
    if (!w34_py_int(expected + 6, &unix_ms)) return json_bool(0);

    /* (dotnet_ticks - 621355968000000000) // 10_000, floored like Python. */
    {
        long long delta = ticks - 621355968000000000LL;
        long long q = delta / 10000LL;
        if (delta < 0 && delta % 10000LL != 0) q -= 1;
        return json_bool(q == unix_ms ? 1 : 0);
    }
}

json_t *w34_webserverl_f96a1a_parent_start_marker_mismatch_is_conclusive(const json_t *req) {
    const char *actual = w34_str(json_obj_get(req, "actual"));
    const char *expected = w34_str(json_obj_get(req, "expected"));
    if (!actual || !expected) return json_bool(0);
    return json_bool((w34_startswith(actual, "ps:") || w34_startswith(expected, "ps:")) ? 0 : 1);
}

json_t *w34_webserverl_f96a1a_serve_orphaned_decision(const json_t *req) {
    const json_t *exp = json_obj_get(req, "expected");
    const json_t *probe = json_obj_get(req, "probe");
    const json_t *pidrep = json_obj_get(req, "pid");
    const char *expected = NULL;
    const char *actual = NULL;

    if (exp && exp->type == JSON_OBJECT) {
        const char *kind = w34_str(json_obj_get(exp, "kind"));
        if (kind && !strcmp(kind, "marker")) expected = w34_str(json_obj_get(exp, "value"));
    }

    if (expected) {
        const char *probe_kind =
            (probe && probe->type == JSON_OBJECT) ? w34_str(json_obj_get(probe, "kind")) : NULL;
        if (probe_kind && !strcmp(probe_kind, "marker")) {
            actual = w34_str(json_obj_get(probe, "value"));
        } else if (probe_kind && !strcmp(probe_kind, "lookup")) {
            return json_bool(1); /* ProcessLookupError -> the parent is gone */
        } else {
            actual = NULL; /* any other probe failure is inconclusive */
        }

        if (actual) {
            json_t *mreq = json_object();
            int matched, conclusive;
            json_set(mreq, "actual", json_string(actual));
            json_set(mreq, "expected", json_string(expected));
            matched = w34_truthy(w34_webserverl_f96a1a_parent_start_markers_match(mreq));
            conclusive =
                w34_truthy(w34_webserverl_f96a1a_parent_start_marker_mismatch_is_conclusive(mreq));
            json_free(mreq);
            if (matched) return json_bool(0);
            if (conclusive) return json_bool(1);
            /* Inconclusive marker: degrade to PID liveness instead of exiting. */
        }
    }

    if (!pidrep || pidrep->type != JSON_OBJECT) return json_bool(0);
    {
        const char *kind = w34_str(json_obj_get(pidrep, "kind"));
        if (kind && !strcmp(kind, "bool"))
            return json_bool(w34_truthy(json_obj_get(pidrep, "value")) ? 0 : 1);
    }
    return json_bool(0); /* pid probe raised: fail safe, keep serving */
}

json_t *w34_webserverl_f96a1a_watchdog_plan(const json_t *req) {
    const char *raw_pid = w34_str(json_obj_get(req, "parent_pid"));
    const char *start_marker = w34_str(json_obj_get(req, "start_marker"));
    const char *nonce = w34_str(json_obj_get(req, "nonce"));
    const char *poll_raw = w34_str(json_obj_get(req, "poll_env"));
    json_t *out = json_object();
    long long desktop_pid = 0;
    double poll = 2.0;
    int has_marker, has_nonce;

    /* Empty inherited values mean "absent", not "present but blank". */
    if (start_marker && !start_marker[0]) start_marker = NULL;
    if (nonce && !nonce[0]) nonce = NULL;

    json_set(out, "armed", json_bool(0));
    json_set(out, "desktop_pid", json_null());
    json_set(out, "poll", json_null());

    if (!w34_py_int(raw_pid ? raw_pid : "", &desktop_pid)) return out;
    if (desktop_pid <= 0) return out;

    has_marker = start_marker != NULL;
    has_nonce = nonce != NULL;
    if (has_marker != has_nonce) return out;
    if (has_marker) {
        json_t *mreq = json_object();
        int valid;
        char *stripped;
        json_set(mreq, "marker", json_string(start_marker));
        valid = w34_truthy(w34_webserverl_f96a1a_valid_parent_start_marker(mreq));
        json_free(mreq);
        stripped = w34_strip_dup(nonce);
        if (!valid || !stripped || strcmp(stripped, nonce) != 0) {
            free(stripped);
            return out;
        }
        free(stripped);
    }

    /* poll = max(0.5, float(env.get("HERMES_SERVE_WATCHDOG_POLL_S", "2.0"))) */
    if (poll_raw && !w34_py_float(poll_raw, &poll)) poll = 2.0;
    if (!(poll > 0.5)) poll = 0.5;

    json_set(out, "armed", json_bool(1));
    json_set(out, "desktop_pid", json_number((double)desktop_pid));
    json_set(out, "poll", json_number(poll));
    return out;
}

/* ------------------------------------------------- port-conflict derivation */

json_t *w34_webserverl_f96a1a_addr_in_use(const json_t *req) {
    const json_t *ev = json_obj_get(req, "errno");
    const json_t *wv = json_obj_get(req, "winerror");
    double e = 0.0, w = 0.0;
    int in_set = 0;

    if (w34_number(ev, &e)) {
        /* errno.EADDRINUSE resolves to 98 on POSIX, 48 on macOS and 10048 on
         * Windows CPython (WSAEADDRINUSE); the upstream set spells all three
         * out next to 10048, so the literals are the whole set. */
        if (e == 98.0 || e == 48.0 || e == 10048.0) in_set = 1;
    }
    if (w34_number(wv, &w) && w == 10048.0) in_set = 1;
    return json_bool(in_set ? 1 : 0);
}

json_t *w34_webserverl_f96a1a_port_preflight(const json_t *req) {
    const char *host = w34_str(json_obj_get(req, "host"));
    const json_t *pv = json_obj_get(req, "port");
    json_t *out = json_object();
    double pd = 0.0;

    json_set(out, "conflict", json_bool(0));
    json_set(out, "family", json_null());
    if (!w34_number(pv, &pd)) return out;
    if (pd == 0.0) return out; /* ephemeral: never conflicts, never probed */

    json_set(out, "family", json_string((host && strchr(host, ':')) ? "AF_INET6" : "AF_INET"));
    json_set(out, "conflict", json_copy(w34_webserverl_f96a1a_addr_in_use(req)));
    return out;
}

json_t *w34_webserverl_f96a1a_port_in_use_sentinel(const json_t *req) {
    const json_t *pv = json_obj_get(req, "port");
    double pd = 0.0;
    char buf[64];
    if (!w34_number(pv, &pd)) pd = 0.0;
    snprintf(buf, sizeof buf, "BACKEND_PORT_IN_USE port=%lld", (long long)pd);
    return json_string(buf);
}

json_t *w34_webserverl_f96a1a_port_in_use_hint(const json_t *req) {
    const char *host = w34_str(json_obj_get(req, "host"));
    const json_t *pv = json_obj_get(req, "port");
    double pd = 0.0;
    size_t need;
    char *buf;
    json_t *out;

    if (!w34_number(pv, &pd)) pd = 0.0;
    if (!host) host = "";
    /* "  Port {port} on {host} is already in use \u2014 likely another "
     * "'hermes serve' / 'hermes dashboard' backend or the Hermes gateway. "
     * "Stop the other process, or pass --port <other> "
     * "(--port 0 picks a free ephemeral port)." */
    need = strlen(host) + 320;
    buf = (char *)malloc(need);
    if (!buf) return json_null();
    snprintf(buf, need,
             "  Port %lld on %s is already in use \xE2\x80\x94 likely another "
             "'hermes serve' / 'hermes dashboard' backend or the Hermes gateway. "
             "Stop the other process, or pass --port <other> "
             "(--port 0 picks a free ephemeral port).",
             (long long)pd, host);
    out = json_string(buf);
    free(buf);
    return out;
}

/* --------------------------------------------------- trusted proxy allow-list */

static int w34_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Strict dotted quad: four decimal fields 0..255, no leading zeros. */
static int w34_parse_ipv4(const char *s, unsigned long *out) {
    unsigned long v = 0;
    int i;
    if (!s) return 0;
    for (i = 0; i < 4; i++) {
        unsigned field = 0;
        int digits = 0;
        if (i) {
            if (*s != '.') return 0;
            s++;
        }
        if (!(*s >= '0' && *s <= '9')) return 0;
        if (s[0] == '0' && s[1] >= '0' && s[1] <= '9') return 0;
        while (*s >= '0' && *s <= '9') {
            field = field * 10u + (unsigned)(*s - '0');
            if (field > 255u) return 0;
            s++;
            digits++;
            if (digits > 3) return 0;
        }
        v = (v << 8) | field;
    }
    if (*s) return 0;
    *out = v;
    return 1;
}

/* IPv6 text -> 8 groups; "::" hole expansion and a trailing dotted quad. */
static int w34_parse_ipv6(const char *s, unsigned short g[8]) {
    unsigned short tmp[8];
    int count = 0, hole = -1;
    const char *p = s;

    if (!s || !*s) return 0;
    if (*p == ':') {
        if (p[1] != ':') return 0;
        hole = 0;
        p += 2;
        if (!*p) {
            memset(g, 0, 8 * sizeof(unsigned short));
            return 1;
        }
    }
    while (*p) {
        unsigned val = 0;
        int len = 0;
        const char *start = p;
        int hv;
        if (*p == ':') return 0; /* empty group */
        while ((hv = w34_hexval(*p)) >= 0) {
            val = (val << 4) | (unsigned)hv;
            p++;
            len++;
            if (len > 4) return 0;
        }
        if (*p == '.') {
            unsigned long v4;
            if (!w34_parse_ipv4(start, &v4)) return 0;
            if (count + 2 > 8) return 0;
            tmp[count++] = (unsigned short)(v4 >> 16);
            tmp[count++] = (unsigned short)(v4 & 0xffffUL);
            p = start + strlen(start);
            break;
        }
        if (len == 0) return 0;
        if (count >= 8) return 0;
        tmp[count++] = (unsigned short)val;
        if (!*p) break;
        if (*p != ':') return 0;
        p++;
        if (*p == ':') {
            if (hole >= 0) return 0;
            hole = count;
            p++;
            if (!*p) break;
            continue;
        }
        if (!*p) return 0; /* trailing single colon */
    }

    if (hole < 0) {
        if (count != 8) return 0;
        memcpy(g, tmp, 8 * sizeof(unsigned short));
        return 1;
    }
    if (count >= 8) return 0;
    {
        int zeros = 8 - count, i;
        for (i = 0; i < hole; i++) g[i] = tmp[i];
        for (i = 0; i < zeros; i++) g[hole + i] = 0;
        for (i = hole; i < count; i++) g[zeros + i] = tmp[i];
    }
    return 1;
}

/* Python ipaddress rendering: longest zero run of length >= 2, leftmost on a
 * tie; "::ffff:a.b.c.d" for IPv4-mapped; lowercase hex, no padding. */
static void w34_format_ipv6(const unsigned short g[8], char *out, size_t cap) {
    int i, best_start = -1, best_len = 0, run_start = -1, run_len = 0;
    size_t o = 0;
    int all_zero = 1;

    if (cap == 0) return;
    out[0] = '\0';
    for (i = 0; i < 8; i++)
        if (g[i] != 0) all_zero = 0;
    if (all_zero) { snprintf(out, cap, "::"); return; }
    if (!(g[0] | g[1] | g[2] | g[3] | g[4]) && g[5] == 0xffff) {
        snprintf(out, cap, "::ffff:%u.%u.%u.%u",
                 (unsigned)(g[6] >> 8), (unsigned)(g[6] & 0xff),
                 (unsigned)(g[7] >> 8), (unsigned)(g[7] & 0xff));
        return;
    }
    for (i = 0; i < 8; i++) {
        if (g[i] == 0) {
            if (run_start < 0) run_start = i;
            run_len++;
            if (run_len > best_len) { best_len = run_len; best_start = run_start; }
        } else {
            run_start = -1;
            run_len = 0;
        }
    }
    if (best_len < 2) best_start = -1;

    for (i = 0; i < 8; i++) {
        if (i == best_start) {
            if (o + 3 <= cap) { out[o++] = ':'; out[o++] = ':'; out[o] = '\0'; }
            i += best_len - 1;
            continue;
        }
        if (o > 0 && out[o - 1] != ':' && o + 1 < cap) { out[o++] = ':'; out[o] = '\0'; }
        if (o < cap) {
            int n = snprintf(out + o, cap - o, "%x", (unsigned)g[i]);
            if (n > 0 && (size_t)n < cap - o) o += (size_t)n;
        }
    }
}

/* Prefix: a bit length, or a contiguous netmask in the same family. */
static int w34_prefix_len(const char *pre, int v6, int *out) {
    if (!pre || !*pre) return 0;
    if (!v6 && strchr(pre, '.')) {
        unsigned long m = 0;
        int ones = 0, seen_zero = 0, i;
        if (!w34_parse_ipv4(pre, &m)) return 0;
        for (i = 31; i >= 0; i--) {
            if ((m >> i) & 1UL) {
                if (seen_zero) return 0; /* non-contiguous mask */
                ones++;
            } else {
                seen_zero = 1;
            }
        }
        *out = ones;
        return 1;
    }
    if (v6 && strchr(pre, ':')) return 0; /* ip_network takes no IPv6 netmask */
    {
        long long v = 0;
        if (!w34_all_digits(pre)) return 0;
        if (!w34_py_int(pre, &v)) return 0;
        if (v > (v6 ? 128 : 32)) return 0;
        *out = (int)v;
    }
    return 1;
}

/* ip_address()/ip_network(strict=False) -> canonical text; "" on ValueError. */
static void w34_normalize_ip(const char *entry, char *out, size_t cap) {
    const char *slash;
    char addr[128], pre[128];
    size_t alen;
    int v6, prefix = -1;

    out[0] = '\0';
    if (!entry || !*entry) return;
    if (strlen(entry) >= sizeof addr) return;
    slash = strchr(entry, '/');
    if (slash) {
        alen = (size_t)(slash - entry);
        if (alen == 0 || alen >= sizeof addr) return;
        if (strchr(slash + 1, '/')) return;
        if (strlen(slash + 1) >= sizeof pre) return;
        memcpy(addr, entry, alen);
        addr[alen] = '\0';
        strcpy(pre, slash + 1);
    } else {
        strcpy(addr, entry);
        pre[0] = '\0';
    }
    v6 = strchr(addr, ':') != NULL;

    if (!v6) {
        unsigned long v4;
        if (!w34_parse_ipv4(addr, &v4)) return;
        if (slash) {
            unsigned long mask;
            if (!w34_prefix_len(pre, 0, &prefix)) return;
            if (prefix == 0) return; /* unbounded network fails closed */
            mask = prefix >= 32 ? 0xffffffffUL : (0xffffffffUL << (32 - prefix)) & 0xffffffffUL;
            v4 &= mask;
            snprintf(out, cap, "%lu.%lu.%lu.%lu/%d",
                     (v4 >> 24) & 0xffUL, (v4 >> 16) & 0xffUL, (v4 >> 8) & 0xffUL, v4 & 0xffUL,
                     prefix);
            return;
        }
        snprintf(out, cap, "%lu.%lu.%lu.%lu",
                 (v4 >> 24) & 0xffUL, (v4 >> 16) & 0xffUL, (v4 >> 8) & 0xffUL, v4 & 0xffUL);
        return;
    }

    {
        unsigned short g[8];
        if (!w34_parse_ipv6(addr, g)) return;
        if (slash) {
            int i;
            char text[128];
            if (!w34_prefix_len(pre, 1, &prefix)) return;
            if (prefix == 0) return;
            for (i = 0; i < 8; i++) {
                int keep = prefix - i * 16;
                if (keep >= 16) continue;
                if (keep <= 0) { g[i] = 0; continue; }
                g[i] = (unsigned short)(g[i] & (unsigned short)(0xffffu << (16 - keep)));
            }
            w34_format_ipv6(g, text, sizeof text);
            snprintf(out, cap, "%s/%d", text, prefix);
            return;
        }
        w34_format_ipv6(g, out, cap);
    }
}

/* The "unset" arm of ``configured in (None, "")``. */
static int w34_config_unset(const json_t *v) {
    if (!v) return 1;
    if (v->type == JSON_NULL) return 1;
    if (v->type == JSON_STRING && (!v->str_val || !v->str_val[0])) return 1;
    return 0;
}

static int w34_array_has(const json_t *arr, const char *s) {
    size_t i, n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *it = json_get(arr, i);
        if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, s)) return 1;
    }
    return 0;
}

json_t *w34_webserverl_f96a1a_forwarded_allow_ips(const json_t *req) {
    const json_t *cfg = json_obj_get(req, "config");
    const json_t *conf = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "trusted_proxies")
                                                           : NULL;
    json_t *out = json_array();
    size_t i, n;
    char norm[256];

    json_append(out, json_string("127.0.0.1"));
    json_append(out, json_string("::1"));

    if (w34_config_unset(conf)) return out;
    if (conf->type == JSON_STRING) {
        char *e = w34_strip_dup(conf->str_val);
        if (e && e[0]) {
            w34_normalize_ip(e, norm, sizeof norm);
            if (norm[0] && !w34_array_has(out, norm)) json_append(out, json_string(norm));
        }
        free(e);
        return out;
    }
    if (conf->type != JSON_ARRAY) return out; /* not a list/tuple: ignored */

    n = json_len(conf);
    for (i = 0; i < n; i++) {
        const json_t *raw = json_get(conf, i);
        char *e;
        if (!raw || raw->type != JSON_STRING || !raw->str_val) continue;
        e = w34_strip_dup(raw->str_val);
        if (!e || !e[0]) { free(e); continue; }
        w34_normalize_ip(e, norm, sizeof norm);
        if (norm[0] && !w34_array_has(out, norm)) json_append(out, json_string(norm));
        free(e);
    }
    return out;
}

/* --------------------------------------------------------- browser open URL */

/* urllib.parse.quote(s) with its default safe='/' . */
static char *w34_url_quote(const char *s) {
    static const char hex[] = "0123456789ABCDEF";
    size_t i, n, o = 0;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n * 3 + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '_' || c == '.' || c == '-' || c == '~' || c == '/') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0xf];
        }
    }
    out[o] = '\0';
    return out;
}

/* hermes_cli/url_utils.py:format_url_host. */
static char *w34_format_url_host(const char *host) {
    char *s = w34_strip_dup(host ? host : "");
    size_t n, i, o = 0;
    char *out;
    int bracketed;
    if (!s) return NULL;
    n = strlen(s);
    bracketed = (n >= 2 && s[0] == '[' && s[n - 1] == ']');
    if (bracketed) {
        if (!memchr(s + 1, ':', n - 2)) return s; /* already an authority */
        out = (char *)malloc(n * 3 + 3);
        if (!out) { free(s); return NULL; }
        out[o++] = '[';
        for (i = 1; i + 1 < n; i++) {
            if (s[i] == '%') { memcpy(out + o, "%25", 3); o += 3; }
            else out[o++] = s[i];
        }
        out[o++] = ']';
        out[o] = '\0';
        free(s);
        return out;
    }
    if (!memchr(s, ':', n)) return s;
    out = (char *)malloc(n * 3 + 3);
    if (!out) { free(s); return NULL; }
    out[o++] = '[';
    for (i = 0; i < n; i++) {
        if (s[i] == '%') { memcpy(out + o, "%25", 3); o += 3; }
        else out[o++] = s[i];
    }
    out[o++] = ']';
    out[o] = '\0';
    free(s);
    return out;
}

json_t *w34_webserverl_f96a1a_browser_open_url(const json_t *req) {
    const char *host = w34_str(json_obj_get(req, "host"));
    const json_t *pv = json_obj_get(req, "port");
    const char *profile = w34_str(json_obj_get(req, "profile"));
    const char *platform = w34_str(json_obj_get(req, "platform"));
    const char *display = w34_str(json_obj_get(req, "display"));
    const char *wayland = w34_str(json_obj_get(req, "wayland"));
    double pd = 0.0;
    const char *display_host;
    char *fhost, *quoted = NULL;
    size_t need;
    char *url;
    json_t *out;
    int has_display;

    if (!w34_truthy(json_obj_get(req, "open_browser"))) return json_null();
    has_display = (platform && strcmp(platform, "linux") != 0)
                  || (display && display[0]) || (wayland && wayland[0]);
    if (!has_display) return json_null();

    display_host = host ? host : "";
    if (!strcmp(display_host, "0.0.0.0") || !strcmp(display_host, "::")) display_host = "127.0.0.1";
    fhost = w34_format_url_host(display_host);
    if (!fhost) return json_null();
    if (!w34_number(pv, &pd)) pd = 0.0;
    if (profile && profile[0]) {
        quoted = w34_url_quote(profile);
        if (!quoted) { free(fhost); return json_null(); }
    }
    need = strlen(fhost) + (quoted ? strlen(quoted) : 0) + 64;
    url = (char *)malloc(need);
    if (!url) { free(fhost); free(quoted); return json_null(); }
    if (quoted)
        snprintf(url, need, "http://%s:%lld/?profile=%s", fhost, (long long)pd, quoted);
    else
        snprintf(url, need, "http://%s:%lld", fhost, (long long)pd);
    out = json_string(url);
    free(fhost);
    free(quoted);
    free(url);
    return out;
}
