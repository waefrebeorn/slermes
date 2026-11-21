/*
 * feishu_adapter_flow14.c — Feishu/Lark adapter "remainders" FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * FOURTEENTH remainder port beyond feishu_adapter_flow.c .. flow13. Ports 12
 * genuinely unclaimed defs (none is a TARGET of flows 1-13):
 *   _run_blocking, _run_official_feishu_ws_client, _send_exec_approval_prompt,
 *   _send_interactive_card, _send_raw_message, _send_uploaded_file_message,
 *   _send_uploaded_key, _shutdown_sdk_executor, _standalone_send,
 *   _stop_webhook_server, _submit_on_loop, _supervise_websocket_thread.
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa14_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical {"name","args","ret"} event from the SAME scripts. args is ADOPTED
 * into the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":...} == a raised exception). A missing/exhausted
 * script is a fail-closed RuntimeError on both sides.
 *
 * FULL BASEEXCEPTION PASSTHROUGH: fa14_is_base_exc + fa14_raise_is_base name the
 * family that bypasses `except Exception`; those raises propagate verbatim.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow14.py):
 * - NULL json_t* == Python None; JSON_NULL node is a real null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - SendResult is projected to {success,message_id,error}; an ExecApprovalPrompt
 *   is projected to the input args object.
 * - Fixture domain: ASCII scalars, JSON dict/list values. State is {}.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa14_-prefixed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct fa14_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors (also state cursors) */
    json_t *trace;      /* owned; ordered seam events */
} fa14_ctx_t;

/* ── basic helpers ── */
static bool fa14_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool fa14_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool fa14_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "asyncio.CancelledError") == 0);
}
static bool fa14_raise_is_base(const json_t *r) {
    return fa14_is_raise(r) && fa14_is_base_exc(json_get_str(r, "__raise__", ""));
}
static double fa14_as_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool fa14_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa14_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *fa14_str_or(const json_t *v, const char *d) {
    const char *s = fa14_str(v);
    return s ? s : d;
}
static bool fa14_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static json_t *fa14_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa14_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return fa14_err(type, msg);
}
/* The one universal seam. args is ADOPTED; the return is a fresh owned node. */
static json_t *fa14_call(fa14_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa14_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa14_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa14_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}
static json_t *fa14_args(void) { return json_object(); }
static void fa14_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa14_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void fa14_set(json_t *a, const char *k, json_t *v) { json_set(a, k, v); }
static void fa14_set_n(json_t *a, const char *k, double n) { json_set(a, k, json_number(n)); }
static void fa14_set_b(json_t *a, const char *k, bool b) { json_set(a, k, json_bool(b)); }
static json_t *fa14_send_result(bool success, const char *mid, const char *err) {
    json_t *r = json_object();
    fa14_set_b(r, "success", success);
    fa14_set_copy(r, "message_id", mid ? json_string(mid) : json_null());
    fa14_set_copy(r, "error", err ? json_string(err) : json_null());
    return r;
}
/* Python str.strip() + optional lower, ASCII fixture domain. */
static const char *fa14_snorm(const char *s, char *out, size_t cap, bool lower) {
    size_t n = 0;
    const char *p = s ? s : "";
    while (*p && n + 1 < cap) {
        char c = *p++;
        out[n++] = (lower && c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[n] = '\0';
    size_t st = 0, en = n;
    while (st < en && (out[st]==' '||out[st]=='\t'||out[st]=='\n'||out[st]=='\r'||out[st]=='\v'||out[st]=='\f')) st++;
    while (en > st && (out[en-1]==' '||out[en-1]=='\t'||out[en-1]=='\n'||out[en-1]=='\r'||out[en-1]=='\v'||out[en-1]=='\f')) en--;
    out[en] = '\0';
    if (st) memmove(out, out + st, en - st + 1);
    return out;
}
static bool fa14_starts(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}
/* Python str.replace(old, "") — ASCII; returns owned buffer. */
static char *fa14_str_replace(const char *s, const char *old) {
    size_t cap = strlen(s ? s : "") * 2 + 64;
    char *out = (char *)malloc(cap);
    if (!out) return strdup(s ? s : "");
    if (!old || !old[0]) { snprintf(out, cap, "%s", s ? s : ""); return out; }
    size_t n = 0;
    const char *p = s ? s : "";
    size_t ol = strlen(old);
    while (*p) {
        if (strncmp(p, old, ol) == 0) { p += ol; continue; }
        if (n + 2 >= cap) { cap *= 2; char *n2 = (char *)realloc(out, cap); if (!n2) break; out = n2; }
        out[n++] = *p++;
    }
    out[n] = '\0';
    return out;
}
/* next state cursor (shared counter map) */
static size_t fa14_state_cursor(fa14_ctx_t *ctx, const char *name) {
    double cur = json_get_num(ctx->cursors, name, 0);
    json_set(ctx->cursors, name, json_number(cur + 1));
    return (size_t)cur;
}

/* ══ PoP functions ══ */

/* PoP: fa14__run_blocking @ plugins/platforms/feishu/adapter.py:FeishuAdapter._run_blocking */
static json_t *fa14__run_blocking(fa14_ctx_t *ctx, const json_t *args) {
    json_t *loop = fa14_call(ctx, "get_running_loop", fa14_args());
    if (fa14_is_raise(loop)) return loop;
    json_free(loop);
    json_t *executor = fa14_call(ctx, "get_sdk_executor", fa14_args());
    if (fa14_is_raise(executor)) return executor;
    json_t *a = fa14_args();
    fa14_set_copy(a, "executor", executor);
    fa14_set_copy(a, "func_args", json_obj_get(args, "func_args"));
    json_free(executor);
    return fa14_call(ctx, "run_in_executor", a);
}

/* PoP: fa14__run_official_feishu_ws_client @ plugins/platforms/feishu/adapter.py:_run_official_feishu_ws_client */
static json_t *fa14__run_official_feishu_ws_client(fa14_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *loop = fa14_call(ctx, "new_event_loop", fa14_args());
    if (fa14_is_raise(loop)) return loop;
    json_t *a = fa14_args(); fa14_set_copy(a, "loop", loop);
    json_t *r = fa14_call(ctx, "set_event_loop", a);
    if (fa14_is_raise(r)) { json_free(loop); return r; }
    json_free(r);
    r = fa14_call(ctx, "install_ws_isolation", fa14_args());
    if (fa14_is_raise(r)) { json_free(loop); return r; }
    json_free(r);
    /* _apply_runtime_ws_overrides(): a scripted raise is handled by its own
     * try/except Exception -> logger.debug; a BaseException propagates. */
    a = fa14_args();
    fa14_set_copy(a, "nonce", json_obj_get(args, "nonce"));
    fa14_set_copy(a, "reconnect_interval", json_obj_get(args, "reconnect_interval"));
    fa14_set_copy(a, "ping_interval", json_obj_get(args, "ping_interval"));
    r = fa14_call(ctx, "apply_overrides", a);
    if (fa14_is_raise(r)) {
        if (fa14_raise_is_base(r)) { json_free(loop); return r; }
        json_t *lg = fa14_args();
        fa14_set_s(lg, "message", "[Feishu] Failed to apply websocket runtime overrides");
        json_t *lr = fa14_call(ctx, "logger_debug", lg);
        if (fa14_is_raise(lr)) { json_free(r); json_free(loop); return lr; }
        json_free(lr);
    }
    json_free(r);
    /* try: ws_client.start() — except Exception: pass; BaseException pending. */
    json_t *pending = NULL;
    r = fa14_call(ctx, "start", fa14_args());
    if (fa14_is_raise(r)) {
        if (fa14_raise_is_base(r)) pending = r;
        else json_free(r);
    } else json_free(r);
    /* finally: clear state, cancel pending tasks, stop+close loop. */
    a = fa14_args(); fa14_set_copy(a, "loop", loop);
    r = fa14_call(ctx, "all_tasks", a);
    if (fa14_is_raise(r)) { if (pending) json_free(pending); json_free(loop); return r; }
    const json_t *tasks = (r && r->type == JSON_ARRAY) ? r : NULL;
    json_t *pending_tasks = json_array();
    /* live: pending = [t for t in all_tasks(loop) if not t.done()] — every
     * .done() is evaluated before the cancel loop starts. */
    if (tasks) {
        for (size_t i = 0; i < json_len(tasks); i++) {
            const json_t *t = json_get(tasks, i);
            json_t *da = fa14_args(); fa14_set_copy(da, "task", t);
            json_t *d = fa14_call(ctx, "task_done", da);
            if (fa14_is_raise(d)) { json_free(pending_tasks); json_free(r); if (pending) json_free(pending); json_free(loop); return d; }
            bool done = fa14_truthy(d); json_free(d);
            if (!done) json_append(pending_tasks, json_copy(t));
        }
        for (size_t i = 0; i < json_len(pending_tasks); i++) {
            const json_t *t = json_get(pending_tasks, i);
            json_t *ca = fa14_args(); fa14_set_copy(ca, "task", t);
            json_t *c = fa14_call(ctx, "task_cancel", ca);
            if (fa14_is_raise(c)) { json_free(pending_tasks); json_free(r); if (pending) json_free(pending); json_free(loop); return c; }
            json_free(c);
        }
    }
    json_free(r);
    if (json_len(pending_tasks) > 0) {
        json_t *ga = fa14_args(); json_set(ga, "tasks", pending_tasks); pending_tasks = NULL;
        json_t *g = fa14_call(ctx, "gather", ga);
        if (fa14_is_raise(g)) { if (pending) json_free(pending); json_free(loop); return g; }
        json_t *ua = fa14_args(); fa14_set_copy(ua, "coro", g); json_free(g);
        g = fa14_call(ctx, "run_until_complete", ua);
        if (fa14_is_raise(g)) { if (pending) json_free(pending); json_free(loop); return g; }
        json_free(g);
    }
    json_free(pending_tasks);
    /* for closer in (loop.stop, loop.close): try closer() except Exception: pass */
    {
        bool stop_done = false;
        while (!stop_done) {
            r = fa14_call(ctx, "loop_stop", fa14_args());
            stop_done = true;
            if (fa14_is_raise(r)) {
                if (fa14_raise_is_base(r)) { if (pending) json_free(pending); json_free(loop); return r; }
                json_free(r); /* except Exception: pass */
            } else json_free(r);
        }
    }
    {
        bool close_done = false;
        while (!close_done) {
            r = fa14_call(ctx, "loop_close", fa14_args());
            close_done = true;
            if (fa14_is_raise(r)) {
                if (fa14_raise_is_base(r)) { if (pending) json_free(pending); json_free(loop); return r; }
                json_free(r);
            } else json_free(r);
        }
    }
    json_free(loop);
    return pending ? pending : json_null();
}

/* helper: pop_validated_prompt_state args (approval + update) */
static json_t *fa14_popargs(const json_t *args, const char *label, const char *ident_key) {
    json_t *a = fa14_args();
    fa14_set_s(a, "ident", fa14_str_or(json_obj_get(args, ident_key), ""));
    fa14_set_s(a, "label", label);
    fa14_set_copy(a, "open_id", json_obj_get(args, "open_id"));
    fa14_set_copy(a, "chat_id", json_obj_get(args, "chat_id"));
    fa14_set_copy(a, "user_id", json_obj_get(args, "user_id"));
    fa14_set_copy(a, "union_id", json_obj_get(args, "union_id"));
    const char *op = fa14_str_or(json_obj_get(args, "open_id"), "<unknown>");
    fa14_set_s(a, "operator_repr", op);
    return a;
}
/* proxy error projection used by except paths */
static json_t *fa14_proxy_error(fa14_ctx_t *ctx, const char *err) {
    json_t *a = fa14_args(); fa14_set_s(a, "error", err);
    json_t *shown = fa14_call(ctx, "proxy_error", a);
    return shown;
}

/* PoP: fa14__send_exec_approval_prompt @ plugins/platforms/feishu/adapter.py:FeishuAdapter._send_exec_approval_prompt */
static json_t *fa14__send_exec_approval_prompt(fa14_ctx_t *ctx, const json_t *args) {
    if (!fa14_bool(json_obj_get(args, "has_client"), false))
        return fa14_send_result(false, NULL, "Not connected");
    const char *text = fa14_str_or(json_obj_get(args, "text"), "");
    json_t *exc = NULL;
    json_t *header = NULL;
    json_t *id = fa14_call(ctx, "next_approval", fa14_args());
    if (fa14_is_raise(id)) { exc = id; goto ea_except; }
    json_t *actions = json_array();
    const json_t *acts = json_obj_get(args, "actions");
    if (acts && acts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(acts); i++) {
            const json_t *row = json_get(acts, i);
            const char *label = fa14_str_or(json_get(row, 0), "");
            const char *ch = fa14_str_or(json_get(row, 1), "");
            const char *style = fa14_str_or(json_get(row, 2), "");
            if (!style[0]) style = "default";
            const json_t *cmap = json_obj_get(args, "card_actions");
            const json_t *action = cmap ? json_obj_get(cmap, ch) : NULL;
            if (!action) {
                char keymsg[512];
                snprintf(keymsg, sizeof keymsg, "'%s'", ch);
                exc = fa14_err("KeyError", keymsg);
                json_free(id); json_free(header); json_free(actions);
                goto ea_except;
            }
            json_t *ba = fa14_args();
            fa14_set_s(ba, "label", label);
            fa14_set_s(ba, "style", style);
            json_t *val = fa14_args();
            fa14_set_copy(val, "hermes_action", action);
            fa14_set_copy(val, "approval_id", id);
            fa14_set(ba, "value", val);
            json_t *btn = fa14_call(ctx, "card_button", ba);
            if (fa14_is_raise(btn)) {
                exc = btn; json_free(id); json_free(header); json_free(actions);
                goto ea_except;
            }
            json_append(actions, btn);
        }
    }
    /* live: card = _card(f"⚠️ {t(header_key)}", ...) — t() runs AFTER the
     * actions list comprehension (source-order evaluation). */
    json_t *ta = fa14_args(); fa14_set_s(ta, "key", "gateway.exec_approval.header");
    header = fa14_call(ctx, "t", ta);
    if (fa14_is_raise(header)) { exc = header; json_free(id); goto ea_except; }
    { char hbuf[8192];
      snprintf(hbuf, sizeof hbuf, "⚠️ %s", fa14_str_or(header, ""));
      json_free(header); header = json_string(hbuf); }
    json_t *ca = fa14_args();
    fa14_set_copy(ca, "header", header);
    fa14_set_s(ca, "template", "orange");
    fa14_set_s(ca, "markdown", text);
    json_set(ca, "actions", actions); actions = NULL;
    json_free(header);
    json_t *card = fa14_call(ctx, "card", ca);
    if (fa14_is_raise(card)) { exc = card; json_free(id); goto ea_except; }
    json_t *dl = fa14_call(ctx, "ea_deadline_line", fa14_args());
    if (fa14_is_raise(dl)) { exc = dl; json_free(card); json_free(id); goto ea_except; }
    char *body = fa14_str_replace(text, fa14_str_or(dl, ""));
    json_free(dl);
    json_t *extra = fa14_args();
    fa14_set_s(extra, "body", body);
    const char *rid = fa14_str_or(json_obj_get(args, "request_id"), "");
    fa14_set_s(extra, "request_id", rid);
    free(body);
    json_t *sa = fa14_args();
    fa14_set_copy(sa, "chat_id", json_obj_get(args, "chat_id"));
    fa14_set_copy(sa, "card", card);
    fa14_set_copy(sa, "metadata", json_obj_get(args, "metadata"));
    fa14_set_s(sa, "failure_message", "send_exec_approval failed");
    fa14_set_s(sa, "state_map_kind", "approval");
    fa14_set_copy(sa, "state_id", id);
    fa14_set_copy(sa, "session_key", json_obj_get(args, "session_key"));
    fa14_set(sa, "extra_state", extra);
    json_free(card); json_free(id);
    json_t *res = fa14_call(ctx, "send_interactive_card", sa);
    if (fa14_is_raise(res)) { exc = res; goto ea_except; }
    return res;
ea_except:
    {
        if (fa14_raise_is_base(exc)) return exc;  /* except Exception: pass-through */
        const char *emsg = json_get_str(exc, "message", "");
        json_t *shown = fa14_proxy_error(ctx, emsg);
        if (fa14_is_raise(shown)) { json_free(exc); return shown; }
        const char *sh = fa14_str_or(shown, emsg);
        char lgmsg[8192];
        snprintf(lgmsg, sizeof lgmsg, "[Feishu] send_exec_approval failed: %s", sh);
        json_t *lg = fa14_args(); fa14_set_s(lg, "message", lgmsg);
        json_t *lr = fa14_call(ctx, "log_warning", lg);
        if (fa14_is_raise(lr)) { json_free(shown); json_free(exc); return lr; }
        json_free(lr);
        json_t *out = fa14_send_result(false, NULL, sh);
        json_free(shown); json_free(exc);
        return out;
    }
}

/* PoP: fa14__send_interactive_card @ plugins/platforms/feishu/adapter.py:FeishuAdapter._send_interactive_card */
static json_t *fa14__send_interactive_card(fa14_ctx_t *ctx, const json_t *args) {
    json_t *da = fa14_args(); fa14_set_copy(da, "value", json_obj_get(args, "card"));
    json_t *payload = fa14_call(ctx, "json_dumps", da);
    if (fa14_is_raise(payload)) return payload;
    json_t *a = fa14_args();
    fa14_set_copy(a, "chat_id", json_obj_get(args, "chat_id"));
    fa14_set_s(a, "msg_type", "interactive");
    fa14_set_copy(a, "payload", payload);
    fa14_set(a, "reply_to", json_null());
    fa14_set_copy(a, "metadata", json_obj_get(args, "metadata"));
    json_free(payload);
    json_t *resp = fa14_call(ctx, "feishu_send_with_retry", a);
    if (fa14_is_raise(resp)) return resp;
    json_t *fa = fa14_args();
    fa14_set_copy(fa, "response", resp);
    fa14_set_copy(fa, "failure_message", json_obj_get(args, "failure_message"));
    json_free(resp);
    json_t *result = fa14_call(ctx, "finalize_send_result", fa);
    if (fa14_is_raise(result)) return result;
    if (fa14_bool(json_obj_get(result, "success"), false)) {
        json_t *sa = fa14_args();
        fa14_set_copy(sa, "state_id", json_obj_get(args, "state_id"));
        fa14_set_copy(sa, "session_key", json_obj_get(args, "session_key"));
        const char *mid = fa14_str_or(json_obj_get(result, "message_id"), "");
        fa14_set_s(sa, "message_id", mid);
        fa14_set_copy(sa, "chat_id", json_obj_get(args, "chat_id"));
        fa14_set_copy(sa, "extra_state", json_obj_get(args, "extra_state"));
        json_t *s = fa14_call(ctx, "state_set", sa);
        if (fa14_is_raise(s)) { json_free(result); return s; }
        json_free(s);
    }
    return result;
}

/* PoP: fa14__send_raw_message @ plugins/platforms/feishu/adapter.py:FeishuAdapter._send_raw_message */
static json_t *fa14__send_raw_message(fa14_ctx_t *ctx, const json_t *args) {
    const json_t *metadata = json_obj_get(args, "metadata");
    const json_t *thread_id = metadata ? json_obj_get(metadata, "thread_id") : NULL;
    bool has_thread = fa14_truthy(thread_id);
    const json_t *reply_mid = (has_thread && metadata) ? json_obj_get(metadata, "reply_to_message_id") : NULL;
    const json_t *reply_to = json_obj_get(args, "reply_to");
    bool has_reply = fa14_truthy(reply_to);
    const json_t *effective = has_reply ? reply_to : reply_mid;
    if (fa14_truthy(effective)) {
        json_t *u = fa14_call(ctx, "uuid4", fa14_args());
        if (fa14_is_raise(u)) return u;
        json_t *ba = fa14_args();
        fa14_set_copy(ba, "content", json_obj_get(args, "payload"));
        fa14_set_copy(ba, "msg_type", json_obj_get(args, "msg_type"));
        fa14_set_b(ba, "reply_in_thread", has_thread);
        fa14_set_copy(ba, "uuid_value", u);
        json_free(u);
        json_t *body = fa14_call(ctx, "build_reply_message_body", ba);
        if (fa14_is_raise(body)) return body;
        json_t *ra = fa14_args();
        fa14_set_copy(ra, "reply_to", effective);
        fa14_set_copy(ra, "body", body);
        json_free(body);
        json_t *req = fa14_call(ctx, "build_reply_message_request", ra);
        if (fa14_is_raise(req)) return req;
        json_t *rba = fa14_args();
        fa14_set_s(rba, "method", "reply");
        fa14_set_copy(rba, "request", req);
        json_free(req);
        return fa14_call(ctx, "run_blocking", rba);
    }
    const char *rid, *rtype;
    const char *chat = fa14_str_or(json_obj_get(args, "chat_id"), "");
    if (has_thread) { rid = fa14_str_or(thread_id, ""); rtype = "thread_id"; }
    else if (fa14_starts(chat, "feishu_user_id:")) { rid = chat + strlen("feishu_user_id:"); rtype = "user_id"; }
    else { rid = chat; rtype = fa14_starts(chat, "ou_") ? "open_id" : "chat_id"; }
    json_t *u = fa14_call(ctx, "uuid4", fa14_args());
    if (fa14_is_raise(u)) return u;
    json_t *ba = fa14_args();
    fa14_set_s(ba, "receive_id", rid);
    fa14_set_copy(ba, "msg_type", json_obj_get(args, "msg_type"));
    fa14_set_copy(ba, "content", json_obj_get(args, "payload"));
    fa14_set_copy(ba, "uuid_value", u);
    json_free(u);
    json_t *body = fa14_call(ctx, "build_create_message_body", ba);
    if (fa14_is_raise(body)) return body;
    json_t *ra = fa14_args();
    fa14_set_s(ra, "receive_id_type", rtype);
    fa14_set_copy(ra, "body", body);
    json_free(body);
    json_t *req = fa14_call(ctx, "build_create_message_request", ra);
    if (fa14_is_raise(req)) return req;
    json_t *rba = fa14_args();
    fa14_set_s(rba, "method", "create");
    fa14_set_copy(rba, "request", req);
    json_free(req);
    return fa14_call(ctx, "run_blocking", rba);
}

/* PoP: fa14__send_uploaded_key @ plugins/platforms/feishu/adapter.py:FeishuAdapter._send_uploaded_key */
static json_t *fa14__send_uploaded_key(fa14_ctx_t *ctx, const json_t *args) {
    const json_t *caption = json_obj_get(args, "caption");
    const char *mt;
    json_t *payload;
    if (fa14_truthy(caption)) {
        mt = "post";
        json_t *pa = fa14_args();
        fa14_set_copy(pa, "caption", caption);
        fa14_set_copy(pa, "media_tag", json_obj_get(args, "media_tag"));
        payload = fa14_call(ctx, "build_media_post_payload", pa);
    } else {
        mt = fa14_str_or(json_obj_get(args, "key_msg_type"), "");
        json_t *da = fa14_args(); fa14_set_copy(da, "value", json_obj_get(args, "key_payload"));
        payload = fa14_call(ctx, "json_dumps", da);
    }
    if (fa14_is_raise(payload)) return payload;
    json_t *a = fa14_args();
    fa14_set_copy(a, "chat_id", json_obj_get(args, "chat_id"));
    fa14_set_s(a, "msg_type", mt);
    fa14_set_copy(a, "payload", payload);
    fa14_set_copy(a, "reply_to", json_obj_get(args, "reply_to"));
    fa14_set_copy(a, "metadata", json_obj_get(args, "metadata"));
    json_free(payload);
    return fa14_call(ctx, "feishu_send_with_retry", a);
}

/* PoP: fa14__shutdown_sdk_executor @ plugins/platforms/feishu/adapter.py:FeishuAdapter._shutdown_sdk_executor */
static json_t *fa14__shutdown_sdk_executor(fa14_ctx_t *ctx, const json_t *args) {
    if (!fa14_bool(json_obj_get(args, "has_lock"), false)) return json_null();
    json_t *r = fa14_call(ctx, "lock_enter", fa14_args());
    if (fa14_is_raise(r)) return r;
    json_free(r);
    r = fa14_call(ctx, "take_executor", fa14_args());
    json_t *pending = NULL;
    if (fa14_is_raise(r)) { pending = r; }
    else json_free(r);
    json_t *lr = fa14_call(ctx, "lock_exit", fa14_args());
    if (fa14_is_raise(lr)) { if (pending) json_free(pending); return lr; }
    json_free(lr);
    if (pending) return pending;
    if (!fa14_bool(json_obj_get(args, "has_executor"), false)) return json_null();
    json_t *a = fa14_args();
    fa14_set_b(a, "wait", false); fa14_set_b(a, "cancel_futures", true);
    json_t *s = fa14_call(ctx, "executor_shutdown", a);
    if (fa14_is_raise(s)) {
        if (fa14_raise_is_base(s) || strcmp(json_get_str(s, "__raise__", ""), "TypeError") != 0) return s;
        json_free(s);
        json_t *a2 = fa14_args();
        fa14_set_b(a2, "wait", false); fa14_set_b(a2, "cancel_futures", false);
        return fa14_call(ctx, "executor_shutdown", a2);
    }
    json_free(s);
    return json_null();
}

/* PoP: fa14__stop_webhook_server @ plugins/platforms/feishu/adapter.py:FeishuAdapter._stop_webhook_server */
static json_t *fa14__stop_webhook_server(fa14_ctx_t *ctx, const json_t *args) {
    if (!fa14_bool(json_obj_get(args, "has_runner"), false)) return json_null();
    json_t *r = fa14_call(ctx, "webhook_cleanup", fa14_args());
    json_t *pending = NULL;
    if (fa14_is_raise(r)) pending = r; else json_free(r);
    json_t *c = fa14_call(ctx, "clear_webhook_state", fa14_args());
    if (fa14_is_raise(c)) { if (pending) json_free(pending); return c; }
    json_free(c);
    return pending ? pending : json_null();
}

/* PoP: fa14__submit_on_loop @ plugins/platforms/feishu/adapter.py:FeishuAdapter._submit_on_loop */
static json_t *fa14__submit_on_loop(fa14_ctx_t *ctx, const json_t *args) {
    json_t *a = fa14_args();
    fa14_set_copy(a, "coro", json_obj_get(args, "coro"));
    fa14_set_copy(a, "loop", json_obj_get(args, "loop"));
    fa14_set_s(a, "log_message", "[Feishu] Failed to schedule background callback work");
    fa14_set_s(a, "log_level", "WARNING");
    json_t *f = fa14_call(ctx, "safe_schedule_threadsafe", a);
    if (fa14_is_raise(f)) return f;
    if (fa14_is_null(f)) { json_free(f); return json_bool(false); }
    json_t *ca = fa14_args(); fa14_set_copy(ca, "future", f); json_free(f);
    json_t *r = fa14_call(ctx, "add_done_callback", ca);
    if (fa14_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* PoP: fa14__send_uploaded_file_message @ plugins/platforms/feishu/adapter.py:FeishuAdapter._send_uploaded_file_message */
static json_t *fa14__send_uploaded_file_message(fa14_ctx_t *ctx, const json_t *args) {
    if (!fa14_bool(json_obj_get(args, "has_client"), false))
        return fa14_send_result(false, NULL, "Not connected");
    const char *path = fa14_str_or(json_obj_get(args, "file_path"), "");
    json_t *a = fa14_args(); fa14_set_s(a, "path", path);
    json_t *ex = fa14_call(ctx, "path_exists", a);
    if (fa14_is_raise(ex)) return ex;
    bool exists = fa14_truthy(ex); json_free(ex);
    if (!exists) {
        char em[1024]; snprintf(em, sizeof em, "File not found: %s", path);
        return fa14_send_result(false, NULL, em);
    }
    const json_t *fname = json_obj_get(args, "file_name");
    const char *display;
    char dispbuf[2048];
    if (fa14_truthy(fname)) display = fa14_str_or(fname, "");
    else {
        json_t *ba = fa14_args(); fa14_set_s(ba, "path", path);
        json_t *bn = fa14_call(ctx, "path_basename", ba);
        if (fa14_is_raise(bn)) return bn;
        snprintf(dispbuf, sizeof dispbuf, "%s", fa14_str_or(bn, "")); json_free(bn);
        display = dispbuf;
    }
    json_t *ra = fa14_args();
    fa14_set_s(ra, "file_path", display);
    fa14_set_copy(ra, "requested_message_type", json_obj_get(args, "outbound_message_type"));
    json_t *route = fa14_call(ctx, "resolve_outbound_file_routing", ra);
    if (fa14_is_raise(route)) return route;
    const char *uftbuf, *rmtbuf;
    char uftstore[512], rmtstore[512];
    {
        const char *uft0 = fa14_str_or(json_get(route, 0), "file");
        const char *rmt0 = fa14_str_or(json_get(route, 1), "file");
        snprintf(uftstore, sizeof uftstore, "%s", uft0);
        snprintf(rmtstore, sizeof rmtstore, "%s", rmt0);
        uftbuf = uftstore; rmtbuf = rmtstore;
    }
    json_free(route);
    const char *uft = uftbuf;
    const char *rmt = rmtbuf;
    /* try: */
    json_t *pending = NULL;
    json_t *r = NULL;
    double duration_ms = 0.0;
    if (strcmp(uft, "opus") == 0) {
        json_t *da = fa14_args(); fa14_set_s(da, "file_path", path);
        r = fa14_call(ctx, "get_audio_duration_ms", da);
        if (fa14_is_raise(r)) { pending = r; goto except_block; }
        duration_ms = fa14_as_num(r, 0.0); json_free(r); r = NULL;
    }
    { json_t *oa = fa14_args(); fa14_set_s(oa, "path", path); fa14_set_s(oa, "mode", "rb");
      r = fa14_call(ctx, "open", oa);
      if (fa14_is_raise(r)) { pending = r; goto except_block; }
      json_free(r); r = NULL; }
    { json_t *ba = fa14_args(); fa14_set_s(ba, "file_type", uft); fa14_set_s(ba, "file_name", display);
      fa14_set_n(ba, "duration", duration_ms);
      r = fa14_call(ctx, "build_file_upload_body", ba);
      if (fa14_is_raise(r)) { pending = r; goto except_block; }
      json_t *qa = fa14_args(); fa14_set_copy(qa, "body", r); json_free(r);
      r = fa14_call(ctx, "build_file_upload_request", qa);
      if (fa14_is_raise(r)) { pending = r; goto except_block; }
      json_t *rb = fa14_args(); fa14_set_s(rb, "method", "file.create"); fa14_set_copy(rb, "request", r); json_free(r);
      r = fa14_call(ctx, "run_blocking", rb);
      if (fa14_is_raise(r)) { pending = r; goto except_block; }
    }
    { json_t *ea = fa14_args(); fa14_set_copy(ea, "response", r); fa14_set_s(ea, "field", "file_key");
      json_t *fk = fa14_call(ctx, "extract_response_field", ea);
      if (fa14_is_raise(fk)) { json_free(r); pending = fk; goto except_block; }
      if (!fa14_truthy(fk)) {
          json_t *er = fa14_args();
          fa14_set(er, "response", json_copy(r));
          fa14_set_s(er, "default_message", "file upload failed");
          fa14_set_s(er, "override_error", "Feishu file upload missing file_key");
          json_free(fk); json_free(r); r = NULL;
          return fa14_call(ctx, "response_error_result", er);
      }
      json_free(r); r = NULL;
      const char *key = fa14_str_or(fk, "");
      char keybuf[1024]; snprintf(keybuf, sizeof keybuf, "%s", key);
      json_free(fk);
      json_t *kp = fa14_args(); fa14_set_s(kp, "file_key", keybuf);
      json_t *mtg = fa14_args();
      fa14_set_s(mtg, "tag", "media"); fa14_set_s(mtg, "file_key", keybuf); fa14_set_s(mtg, "file_name", display);
      json_t *sa = fa14_args();
      fa14_set_copy(sa, "chat_id", json_obj_get(args, "chat_id"));
      fa14_set_copy(sa, "reply_to", json_obj_get(args, "reply_to"));
      fa14_set_copy(sa, "metadata", json_obj_get(args, "metadata"));
      fa14_set_copy(sa, "caption", json_obj_get(args, "caption"));
      fa14_set_s(sa, "key_msg_type", rmt);
      fa14_set(sa, "key_payload", kp);
      fa14_set(sa, "media_tag", mtg);
      r = fa14_call(ctx, "send_uploaded_key", sa);
      if (fa14_is_raise(r)) { pending = r; goto except_block; }
      /* audio retry condition — transcribe live `and` short-circuit order */
      const json_t *caption = json_obj_get(args, "caption");
      const json_t *meta = json_obj_get(args, "metadata");
      const json_t *tid = meta ? json_obj_get(meta, "thread_id") : NULL;
      bool enter_retry = false;
      if (!fa14_truthy(caption)) {
          json_t *sa1 = fa14_args(); fa14_set_copy(sa1, "response", r);
          json_t *succ = fa14_call(ctx, "response_succeeded", sa1);
          if (fa14_is_raise(succ)) { json_free(r); pending = succ; goto except_block; }
          bool ok1 = fa14_truthy(succ); json_free(succ);
          if (!ok1) {
              double code = fa14_as_num(json_obj_get(r, "code"), -1);
              if (code == 99992402.0 && strcmp(rmt, "audio") == 0 && fa14_truthy(tid))
                  enter_retry = true;
          }
      }
      if (enter_retry) {
          json_t *da = fa14_args(); fa14_set_copy(da, "value", kp);
          json_t *payload = fa14_call(ctx, "json_dumps", da);
          if (fa14_is_raise(payload)) { json_free(r); pending = payload; goto except_block; }
          const json_t *tmid = json_obj_get(meta, "reply_to_message_id");
          json_t *fetch = NULL;
          if (!fa14_truthy(tmid)) {
              json_t *fa = fa14_args(); fa14_set_copy(fa, "thread_id", tid);
              fetch = fa14_call(ctx, "fetch_last_message_in_thread", fa);
              if (fa14_is_raise(fetch)) { json_free(payload); json_free(r); pending = fetch; goto except_block; }
              tmid = fetch;
          }
          if (fa14_truthy(tmid)) {
              json_t *lg = fa14_args(); fa14_set_s(lg, "message", "[Feishu] Audio: retrying via reply API in thread");
              json_t *lr = fa14_call(ctx, "log_info", lg);
              if (fa14_is_raise(lr)) { json_free(fetch); json_free(payload); json_free(r); pending = lr; goto except_block; }
              json_free(lr);
              json_t *ra2 = fa14_args();
              fa14_set_copy(ra2, "chat_id", json_obj_get(args, "chat_id"));
              fa14_set_s(ra2, "msg_type", "audio");
              fa14_set_copy(ra2, "payload", payload);
              fa14_set_copy(ra2, "reply_to", tmid);
              fa14_set_copy(ra2, "metadata", meta);
              json_free(r);
              r = fa14_call(ctx, "feishu_send_with_retry", ra2);
              if (fa14_is_raise(r)) { json_free(fetch); json_free(payload); pending = r; goto except_block; }
          }
          { json_t *sa2 = fa14_args(); fa14_set_copy(sa2, "response", r);
            json_t *succ2 = fa14_call(ctx, "response_succeeded", sa2);
            if (fa14_is_raise(succ2)) { json_free(fetch); json_free(payload); json_free(r); pending = succ2; goto except_block; }
            bool ok2 = fa14_truthy(succ2); json_free(succ2);
            if (!ok2) {
                json_t *lg2 = fa14_args(); fa14_set_s(lg2, "message", "[Feishu] Audio send failed in thread, retrying with chat_id");
                json_t *lr2 = fa14_call(ctx, "log_warning", lg2);
                if (fa14_is_raise(lr2)) { json_free(fetch); json_free(payload); json_free(r); pending = lr2; goto except_block; }
                json_free(lr2);
                json_t *ra3 = fa14_args();
                fa14_set_copy(ra3, "chat_id", json_obj_get(args, "chat_id"));
                fa14_set_s(ra3, "msg_type", "audio");
                fa14_set_copy(ra3, "payload", payload);
                fa14_set(ra3, "reply_to", json_null());
                fa14_set(ra3, "metadata", json_null());
                json_free(r);
                r = fa14_call(ctx, "feishu_send_with_retry", ra3);
                if (fa14_is_raise(r)) { json_free(fetch); json_free(payload); pending = r; goto except_block; }
            }
          }
          json_free(fetch); json_free(payload);
      }
    }
    { json_t *fa = fa14_args(); fa14_set_copy(fa, "response", r); fa14_set_s(fa, "failure_message", "file send failed");
      json_free(r);
      return fa14_call(ctx, "finalize_send_result", fa); }
except_block:
    {
        json_t *exc = pending ? pending : fa14_err("RuntimeError", "");
        if (fa14_raise_is_base(exc)) return exc;
        const char *emsg = json_get_str(exc, "message", "");
        json_t *shown = fa14_proxy_error(ctx, emsg);
        if (fa14_is_raise(shown)) { json_free(exc); return shown; }
        const char *sh = fa14_str_or(shown, emsg);
        if (!sh[0]) sh = emsg;
        char lgmsg[8192];
        snprintf(lgmsg, sizeof lgmsg, "[Feishu] Failed to send file %s: %s", path, sh);
        json_t *lg = fa14_args(); fa14_set_s(lg, "message", lgmsg);
        json_t *lr = fa14_call(ctx, "log_error", lg);
        if (fa14_is_raise(lr)) { json_free(shown); json_free(exc); return lr; }
        json_free(lr);
        json_t *out = fa14_send_result(false, NULL, sh);
        json_free(shown); json_free(exc);
        return out;
    }
}

/* ── _standalone_send helper: extension classification ── */
static const char *fa14_ext_kind(const char *ext) {
    static const char *IMG[] = {".jpg",".jpeg",".png",".webp",".gif",NULL};
    static const char *VID[] = {".mp4",".mov",".avi",".mkv",".webm",".3gp",NULL};
    static const char *AUD[] = {".ogg",".opus",".mp3",".wav",".m4a",".flac",NULL};
    if (!ext) return "document";
    for (int i=0;IMG[i];i++) if (strcasecmp(ext,IMG[i])==0) return "image_file";
    for (int i=0;VID[i];i++) if (strcasecmp(ext,VID[i])==0) return "video";
    for (int i=0;AUD[i];i++) if (strcasecmp(ext,AUD[i])==0) return "voice";
    return "document";
}
static const char *fa14_ext_of(const char *path, char *out, size_t cap) {
    const char *slash = strrchr(path, '/'); const char *base = slash ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    if (!dot) { out[0]='\0'; return out; }
    size_t n = 0; const char *p = dot;
    while (*p && n + 1 < cap) { char c = *p++; out[n++] = (c>='A'&&c<='Z') ? (char)(c+32) : c; }
    out[n]='\0'; return out;
}

/* PoP: fa14__standalone_send @ plugins/platforms/feishu/adapter.py:_standalone_send */
static json_t *fa14__standalone_send(fa14_ctx_t *ctx, const json_t *args) {
    json_t *load = fa14_call(ctx, "load_lark_oapi", fa14_args());
    if (fa14_is_raise(load)) return load;
    bool avail = fa14_truthy(load); json_free(load);
    if (!avail) {
        json_t *a = fa14_args();
        fa14_set_s(a, "message", "Feishu dependencies not installed. Run `hermes setup` to install Feishu support.");
        return fa14_call(ctx, "send_error", a);
    }
    json_t *pending = NULL;
    { const json_t *pc = json_obj_get(args, "pconfig");
      json_t *a = fa14_args();
      fa14_set_s(a, "domain", fa14_str_or(pc ? json_obj_get(pc, "domain") : NULL, "feishu"));
      json_t *r = fa14_call(ctx, "build_lark_client", a);
      if (fa14_is_raise(r)) { pending = r; goto except_s; } json_free(r); }
    const json_t *tid = json_obj_get(args, "thread_id");
    bool has_tid = fa14_truthy(tid);
    json_t *metadata = fa14_args();
    if (has_tid) fa14_set_copy(metadata, "thread_id", tid);
    const char *msg = fa14_str_or(json_obj_get(args, "message"), "");
    char mcopy[8192]; fa14_snorm(msg, mcopy, sizeof mcopy, false);
    json_t *last = NULL;
    if (mcopy[0]) {
        json_t *a = fa14_args();
        fa14_set_copy(a, "chat_id", json_obj_get(args, "chat_id"));
        fa14_set_s(a, "message", msg);
        fa14_set(a, "metadata", has_tid ? json_copy(metadata) : json_null());
        json_t *r = fa14_call(ctx, "send", a);
        if (fa14_is_raise(r)) { pending = r; goto except_s; }
        bool ok = fa14_bool(json_obj_get(r, "success"), false);
        if (!ok) {
            const char *err = fa14_str_or(json_obj_get(r, "error"), "");
            char em[8192]; snprintf(em, sizeof em, "Feishu send failed: %s", err);
            json_free(r); json_free(metadata);
            json_t *a2 = fa14_args(); fa14_set_s(a2, "message", em);
            return fa14_call(ctx, "send_error", a2);
        }
        last = r;
    }
    const json_t *files = json_obj_get(args, "media_files");
    if (files && files->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(files); i++) {
            const json_t *row = json_get(files, i);
            const char *mp = fa14_str_or(json_get(row, 0), "");
            json_t *pa = fa14_args(); fa14_set_s(pa, "path", mp);
            json_t *ex = fa14_call(ctx, "path_exists", pa);
            if (fa14_is_raise(ex)) { if (last) json_free(last); pending = ex; goto except_s; }
            bool exists = fa14_truthy(ex); json_free(ex);
            if (!exists) {
                char em[8192]; snprintf(em, sizeof em, "Media file not found: %s", mp);
                if (last) json_free(last); json_free(metadata);
                json_t *a2 = fa14_args(); fa14_set_s(a2, "message", em);
                return fa14_call(ctx, "send_error", a2);
            }
            char extbuf[64]; fa14_ext_of(mp, extbuf, sizeof extbuf);
            const char *sender = fa14_ext_kind(extbuf);
            json_t *a = fa14_args();
            fa14_set_s(a, "sender", sender);
            fa14_set_copy(a, "chat_id", json_obj_get(args, "chat_id"));
            fa14_set_s(a, "path", mp);
            fa14_set(a, "metadata", has_tid ? json_copy(metadata) : json_null());
            json_t *r = fa14_call(ctx, "media_send", a);
            if (fa14_is_raise(r)) { if (last) json_free(last); pending = r; goto except_s; }
            bool ok = fa14_bool(json_obj_get(r, "success"), false);
            if (!ok) {
                const char *err = fa14_str_or(json_obj_get(r, "error"), "");
                char em[8192]; snprintf(em, sizeof em, "Feishu media send failed: %s", err);
                json_free(r); if (last) json_free(last); json_free(metadata);
                json_t *a2 = fa14_args(); fa14_set_s(a2, "message", em);
                return fa14_call(ctx, "send_error", a2);
            }
            if (last) json_free(last);
            last = r;
        }
    }
    json_free(metadata);
    if (!last) {
        json_t *a = fa14_args();
        fa14_set_s(a, "message", "No deliverable text or media remained after processing MEDIA tags");
        return fa14_call(ctx, "send_error", a);
    }
    json_t *out = json_object();
    fa14_set_b(out, "success", true);
    fa14_set_s(out, "platform", "feishu");
    fa14_set_copy(out, "chat_id", json_obj_get(args, "chat_id"));
    fa14_set_copy(out, "message_id", json_obj_get(last, "message_id"));
    json_free(last);
    return out;
except_s:
    {
        json_free(metadata);
        if (fa14_raise_is_base(pending)) return pending;
        const char *emsg = json_get_str(pending, "message", "");
        json_t *shown = fa14_proxy_error(ctx, emsg);
        if (fa14_is_raise(shown)) { json_free(pending); return shown; }
        const char *sh = fa14_str_or(shown, emsg); if (!sh[0]) sh = emsg;
        char em[8192]; snprintf(em, sizeof em, "Feishu send failed: %s", sh);
        json_free(shown); json_free(pending);
        json_t *a = fa14_args(); fa14_set_s(a, "message", em);
        return fa14_call(ctx, "send_error", a);
    }
}

/* PoP: fa14__supervise_websocket_thread @ plugins/platforms/feishu/adapter.py:FeishuAdapter._supervise_websocket_thread */
static json_t *fa14__supervise_websocket_thread(fa14_ctx_t *ctx, const json_t *args) {
    double initial = fa14_as_num(json_obj_get(args, "backoff"), 5.0);
    double backoff = initial;
    const json_t *runnings = json_obj_get(args, "runnings");
    const json_t *futures = json_obj_get(args, "futures");
    bool client_present = fa14_bool(json_obj_get(args, "ws_client_present"), true);
    const char *last_dead = NULL;
    char ldbuf[512];
    for (;;) {
        size_t ri = fa14_state_cursor(ctx, "@running");
        bool running = fa14_bool(json_get(runnings, ri), false);
        if (!running) return json_null();
        size_t fi = fa14_state_cursor(ctx, "@future");
        const json_t *wf = json_get(futures, fi);
        if (fa14_is_null(wf)) return json_null();
        const char *fid = fa14_str_or(wf, "");
        json_t *sa = fa14_args(); fa14_set_copy(sa, "future", wf);
        json_t *sh = fa14_call(ctx, "shield", sa);
        if (fa14_is_raise(sh)) {
            const char *kind = json_get_str(sh, "__raise__", "");
            if (fa14_is_base_exc(kind)) return sh;  /* CancelledError -> re-raise */
            json_free(sh); /* except Exception: pass */
        } else json_free(sh);
        size_t ri2 = fa14_state_cursor(ctx, "@running");
        bool running2 = fa14_bool(json_get(runnings, ri2), false);
        if (!running2 || !client_present) return json_null();
        if (!last_dead || strcmp(fid, last_dead) != 0) {
            json_t *lg = fa14_args();
            char msg[512]; snprintf(msg, sizeof msg, "[Feishu] WebSocket client thread exited unexpectedly; restarting in %.0fs", backoff);
            fa14_set_s(lg, "message", msg);
            json_t *lr = fa14_call(ctx, "logger_error", lg);
            if (fa14_is_raise(lr)) return lr;
            json_free(lr);
            snprintf(ldbuf, sizeof ldbuf, "%s", fid); last_dead = ldbuf;
            json_t *wa = fa14_args();
            fa14_set_s(wa, "status", "ws_link_lost");
            fa14_set_s(wa, "platform_state", "retrying");
            fa14_set(wa, "error_code", json_null());
            fa14_set_s(wa, "error_message", "Feishu websocket link lost; rebuilding");
            json_t *wr = fa14_call(ctx, "write_runtime_status", wa);
            if (fa14_is_raise(wr)) return wr;
            json_free(wr);
        }
        json_t *sl = fa14_args(); fa14_set_n(sl, "seconds", backoff);
        json_t *sr = fa14_call(ctx, "sleep", sl);
        if (fa14_is_raise(sr)) return sr;
        json_free(sr);
        size_t ri3 = fa14_state_cursor(ctx, "@running");
        bool running3 = fa14_bool(json_get(runnings, ri3), false);
        if (!running3) return json_null();
        json_t *c = fa14_call(ctx, "connect_websocket", fa14_args());
        if (fa14_is_raise(c)) {
            const char *emsg = json_get_str(c, "message", "");
            json_free(c);
            char wlmsg[2048];
            snprintf(wlmsg, sizeof wlmsg, "[Feishu] WebSocket restart failed (retrying): %s", emsg);
            json_t *lg = fa14_args(); fa14_set_s(lg, "message", wlmsg);
            json_t *lr = fa14_call(ctx, "logger_warning", lg);
            if (fa14_is_raise(lr)) return lr;
            json_free(lr);
            backoff = backoff * 2.0; if (backoff > 60.0) backoff = 60.0;
        } else {
            json_free(c);
            backoff = initial;
        }
    }
}
