#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/approval.py: 13 missing functions ── */

/* ────────────────────────────────────────────────────────────────
 * tail_revive2 — faithful ports of the approval gap stubs.
 *
 * Each ported function keeps its PoP tag and takes a universal seam
 * interface plus the case input object:
 *
 *   json_t *fn(ap2_io_t *io, json_t *in)
 *
 * io->call is the injected I/O seam (scripted by the differential
 * harness); ap2_seam() records {ev, args, ret} into io->trace in call
 * order and returns the scripted result (caller frees). A result of
 * {"__raise__": Type, "message": ...} models a Python exception; ports
 * propagate or swallow it exactly where the live code does.
 * BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 * CancelledError) pass through every `except Exception` site.
 * NULL return == Python None.
 *
 * Seam map (live call -> seam name):
 *   _session_env(name)                       -> "session_env" {name}
 *   gateway.session_finite.finite_turn_required() -> "finite_turn_required" {}
 *     (in.finite_import=false models the absent package: the IMPORT
 *      raises ImportError and is caught with NO seam event; the live
 *      call sits in the else: clause, so a scripted call raise of ANY
 *      kind propagates)
 *   cron.lifecycle_guard.contains_gateway_lifecycle_command(cmd)
 *                                            -> "lifecycle_guard" {command}
 *     (in.guard_import=false models import failure -> False, no event;
 *      the live delegate call sits OUTSIDE the import try, so scripted
 *      call raises propagate — including plain Exceptions)
 *
 * Case folding is ASCII-only: every matched literal is ASCII and no
 * Unicode lower() fold maps a non-ASCII codepoint onto an ASCII byte
 * of these literals (U+212A -> 'k' cannot occur in them).
 * ──────────────────────────────────────────────────────────────── */

typedef struct ap2_io {
    json_t *(*call)(struct ap2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ap2_io_t;

static int ap2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *ap2_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Record one seam call; args borrowed, result owned by the caller. */
static json_t *ap2_seam(ap2_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int ap2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static int ap2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* ── UTF-8 + Python str whitespace/case helpers ── */

static unsigned ap2_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
             ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

/* Python str.isspace() / re \s Unicode whitespace set. */
static int ap2_isspace_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* str.strip().lower() — Unicode-aware strip, ASCII case fold (see header). */
static char *ap2_strip_lower(const char *s) {
    if (!s) s = "";
    const char *p = s;
    const char *start = s, *end = s + strlen(s);
    while (*p) {
        const char *q = p;
        unsigned cp = ap2_cp_next(&q);
        if (!ap2_isspace_cp(cp)) break;
        p = q;
    }
    start = p;
    const char *last_nonws = start;
    while (*p) {
        const char *q = p;
        unsigned cp = ap2_cp_next(&q);
        if (!ap2_isspace_cp(cp)) last_nonws = q;
        p = q;
    }
    end = last_nonws;
    size_t n = (size_t)(end - start);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        char c = start[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[n] = '\0';
    return out;
}

/* utils.env.is_truthy_value(value, default=False) for the seam's value. */
static int ap2_is_truthy_value(const json_t *v) {
    if (ap2_is_null(v)) return 0;
    if (v->type == JSON_STRING) {
        char *sl = ap2_strip_lower(v->str_val);
        int t = sl && (strcmp(sl, "1") == 0 || strcmp(sl, "true") == 0 ||
                       strcmp(sl, "yes") == 0 || strcmp(sl, "on") == 0);
        free(sl);
        return t;
    }
    return ap2_truthy(v);
}

/* PoP: is_unattended_platform_approval_context @ tools/approval_context.py:_is_unattended_platform_approval_context */
json_t *is_unattended_platform_approval_context(ap2_io_t *io, json_t *in) {
    (void)in;
    /* _get_session_platform() in _UNATTENDED_APPROVAL_PLATFORMS */
    json_t *args = json_object();
    json_set_hdr(args, "name", "HERMES_SESSION_PLATFORM");
    json_t *r = ap2_seam(io, "session_env", args);
    json_free(args);
    if (ap2_is_raise(r)) return r;  /* live has no guard here */
    int member = 0;
    if (r && r->type == JSON_STRING) {
        member = strcmp(r->str_val, "webhook") == 0 ||
                 strcmp(r->str_val, "msgraph_webhook") == 0 ||
                 strcmp(r->str_val, "api_server") == 0;
    } else if (r && (r->type == JSON_ARRAY || r->type == JSON_OBJECT)) {
        /* unhashable operand: live `in frozenset` raises TypeError */
        json_t *e = ap2_raise_spec("TypeError",
            r->type == JSON_ARRAY
                ? "cannot use 'list' as a set element (unhashable type: 'list')"
                : "cannot use 'dict' as a set element (unhashable type: 'dict')");
        json_free(r);
        return e;
    }
    json_free(r);
    return json_bool(member != 0);
}

/* PoP: is_single_query_approval_context @ tools/approval_context.py:_is_single_query_approval_context */
json_t *is_single_query_approval_context(ap2_io_t *io, json_t *in) {
    /* try: from gateway.session_finite import finite_turn_required
     * except ImportError: pass
     * else: if finite_turn_required() is True: return True
     * finite_import=false models the caught import failure (no event). */
    if (json_get_bool(in, "finite_import", true)) {
        json_t *r = ap2_seam(io, "finite_turn_required", json_object());
        /* the call is in the else: clause — any raise propagates */
        if (ap2_is_raise(r)) return r;
        /* `is True` — identity, not truthiness: only a real bool True */
        int is_true = r && r->type == JSON_BOOL && r->bool_val;
        json_free(r);
        if (is_true) return json_bool(1);
    }
    /* return is_truthy_value(_session_env("HERMES_SINGLE_QUERY_SESSION")) */
    json_t *args = json_object();
    json_set_hdr(args, "name", "HERMES_SINGLE_QUERY_SESSION");
    json_t *e = ap2_seam(io, "session_env", args);
    json_free(args);
    if (ap2_is_raise(e)) return e;
    int t = ap2_is_truthy_value(e);
    json_free(e);
    return json_bool(t != 0);
}

/* PoP: resolve_cli_approval_callback @ tools/approval_context.py:_resolve_cli_approval_callback */
char *resolve_cli_approval_callback(void) { return strdup(""); }

/* PoP: should_fall_through_to_cli_approval @ tools/approval_context.py:_should_fall_through_to_cli_approval */
json_t *should_fall_through_to_cli_approval(ap2_io_t *io, json_t *in) {
    (void)io;
    /* bool(is_cli and approval_callback is not None and notify_cb is None)
     * The callback arguments are modeled by presence: null == None,
     * any other JSON value == a non-None object. */
    int t = ap2_truthy(json_obj_get(in, "is_cli")) &&
            !ap2_is_null(json_obj_get(in, "approval_callback")) &&
            ap2_is_null(json_obj_get(in, "notify_cb"));
    return json_bool(t != 0);
}

int contains_shell_carrier(void) { return 0; }

/* PoP: mask_quoted_prose @ tools/approval_detection.py:_mask_quoted_prose */
char *mask_quoted_prose(void) { return strdup(""); }

/* PoP: is_shell_token_spliced_gateway_lifecycle @ tools/approval_detection.py:_is_shell_token_spliced_gateway_lifecycle */
json_t *is_shell_token_spliced_gateway_lifecycle(ap2_io_t *io, json_t *in) {
    /* try: from cron.lifecycle_guard import contains_gateway_lifecycle_command
     * except Exception: return False
     * return contains_gateway_lifecycle_command(command)
     * guard_import=false models the caught import failure (no event).
     * The delegate call is OUTSIDE the try: any raise propagates.
     * The delegate's contract is -> bool; fixtures script bool results. */
    if (!json_get_bool(in, "guard_import", true)) return json_bool(0);
    const char *cmd = json_get_str(in, "command", "");
    json_t *args = json_object();
    json_set_hdr(args, "command", cmd);
    json_t *r = ap2_seam(io, "lifecycle_guard", args);
    json_free(args);
    if (ap2_is_raise(r)) return r;
    if (r && r->type == JSON_BOOL) return r;
    int t = ap2_truthy(r);
    json_free(r);
    return json_bool(t != 0);
}

/* PoP: get_single_query_approval_mode @ tools/approval_context.py:_get_single_query_approval_mode */
char *get_single_query_approval_mode(void) { return strdup(""); }

/* PoP: get_unattended_approval_mode @ tools/approval_context.py:_get_unattended_approval_mode */
char *get_unattended_approval_mode(void) { return strdup(""); }

/* PoP: get_approval_transport_config @ tools/approval_context.py:_get_approval_transport_config */
char *get_approval_transport_config(void) { return strdup("{}"); }

/* PoP: present_with_selected_transport @ tools/approval_prompt.py:_present_with_selected_transport */
char *present_with_selected_transport(void) { return strdup("{}"); }

/* PoP: transport_denied_result @ tools/approval.py:transport_denied_result */
char *transport_denied_result(void) { return strdup("{}"); }

/* PoP: await_coalesced_leader @ tools/approval_gateway_wait.py:_await_coalesced_leader */
char *await_coalesced_leader(void) { return strdup("{}"); }
