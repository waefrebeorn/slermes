/*
 * feishu_adapter_flow3.c — Feishu/Lark adapter INBOUND-ADMISSION FLOW cores of
 * plugins/platforms/feishu/adapter.py (hermes-agent). REMAINDER port:
 * the sibling feishu_adapter_flow.c owns the message-normalization tree (44
 * funcs) and feishu_adapter_flow2.c owns the inbound-event / card-action
 * decision cluster (18 funcs). This file ports the UNCLAIMED synchronous
 * inbound-admission / mention-gating cluster (L3329-3393, L3426-3443,
 * L3370-3385):
 *   _admit, _require_mention_for, _allow_group_message, _mentions_self,
 *   _message_mentions_bot, _post_mentions_bot, _bot_identity,
 *   _warn_once_empty_allowlist_deny.
 *
 * Universal seam: every SEAMED sibling / foreign call enters through ONE
 * callback fa3_call(ctx, name, args). The harness wires ctx->io to a recording
 * stub fed the case's scripted results; the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. args is ADOPTED into the trace event; the return is a NEW owned
 * json_t (NULL == Python None; {"__raise__":cls,"message":str(exc)} == a raised
 * exception). A missing/exhausted script is a fail-closed RuntimeError on both
 * sides.
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: fa3_is_base_exc() names the BaseException
 * family that bypasses `except Exception` (BaseException, SystemExit,
 * KeyboardInterrupt, GeneratorExit, CancelledError) while fa3_call() round-trips
 * every scripted raise descriptor verbatim; none of the ported bodies carries a
 * try/except, so EVERY scripted raise (incl. BaseException kinds) propagates
 * straight out. The oracle's EXC_CLASSES table carries the full Exception side
 * (ValueError/TypeError/RuntimeError/KeyError/AttributeError/ImportError/
 * OSError/StopIteration/IndexError/ZeroDivisionError/AssertionError/... plus
 * Exception itself).
 *
 * Mode table: every ported function runs its REAL body when it is the case
 * target; all of its sibling / foreign calls are SEAMED (fa3_call, scripted).
 * A ported sibling called BY the target is scripted everywhere except when it
 * is itself the target. Seamed-only collaborators (not ported here):
 *   _sender_identity, _is_bot_sender (claimed top-level),
 *   _FeishuBotIdentity.matches (claimed), _normalize (unclaimed method),
 *   empty_allowlist_drop_warning (plugins.platforms.feishu.
 *   feishu_admission_diagnostics), and logger_warning.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow3.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - `self` travels as args.self (a JSON object whose keys are the live
 *   attributes the body reads): _bot_open_id/_bot_user_id/_bot_name (strings),
 *   _allow_bots (str), _allow_all_dm (bool), _allowed_group_users/_admins
 *   (string arrays modelled as sets), _group_rules (chat_id -> rule object
 *   {require_mention: bool|null, policy: str, allowlist: [], blacklist: []}),
 *   _require_mention (bool), _default_group_policy/_group_policy (str|null),
 *   _warned_empty_allowlist_deny (bool).
 * - Attribute doubles (sender, message, sender_id, mention) are JSON objects
 *   whose keys are the live attributes; a missing key models a missing
 *   attribute (getattr default).
 * - _bot_identity returns {open_id, user_id, name}; matches seam takes the
 *   three stripped strings. _normalize seam returns {mentions:[{is_self:b}]}.
 * - string slots are restricted to str/null/bool/int (no containers, no
 *   non-integral floats); .strip()/.lower() corpus is ASCII.
 * - State emission (out.state): _warn_once_empty_allowlist_deny emits
 *   {_warned_empty_allowlist_deny}; every other func emits {}.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa3_-prefixed.
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
typedef struct fa3_ctx {
    json_t *state;      /* owned; emitted as out.state (mutating funcs) */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa3_ctx_t;

/* ── basic helpers ── */
static bool fa3_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
/* The BaseException family that bypasses `except Exception`. */
static bool fa3_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "TruncatedBytes") == 0);
}
static bool fa3_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa3_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *fa3_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa3_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    /* BaseException kinds are flagged here (no try/except in these bodies, so
       they still propagate identically); kept as an explicit hierarchy check. */
    (void)fa3_is_base_exc(type);
    return fa3_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *fa3_call(fa3_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa3_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa3_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa3_make_exc(raise);
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
/* Convenience args builders. */
static json_t *fa3_args(void) { return json_object(); }
static void fa3_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa3_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void fa3_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void fa3_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* logger.<level>(fmt) — fmt is the raw message, positional args empty, kwargs {} */
static json_t *fa3_log(fa3_ctx_t *ctx, const char *name, const char *fmt) {
    json_t *a = fa3_args();
    fa3_set_s(a, "fmt", fmt);
    json_set(a, "args", json_array());
    json_set(a, "kwargs", json_object());
    return fa3_call(ctx, name, a);
}

/* ── string / set helpers ── */
static char *fa3_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
/* Python str.strip() for the ASCII fixture domain. */
static char *fa3_strip(const char *s) {
    if (!s) return fa3_strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (p) { memcpy(p, s + a, b - a); p[b - a] = '\0'; }
    return p;
}
static const char *fa3_str_or_empty(const json_t *v) {
    return fa3_str(v) ? fa3_str(v) : "";
}
static bool fa3_contains(const char *s, const char *sub) {
    return s && sub && strstr(s, sub) != NULL;
}
static bool fa3_arr_has(const json_t *arr, const char *s) {
    if (!s || !arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = fa3_str(json_get(arr, i));
        if (v && strcmp(v, s) == 0) return true;
    }
    return false;
}
static bool fa3_nonempty(const json_t *a) {
    return a && a->type == JSON_ARRAY && json_len(a) > 0;
}
static bool fa3_set_inter(const json_t *a, const json_t *b) {
    if (!a || !b) return false;
    for (size_t i = 0; i < json_len(a); i++) {
        const char *v = fa3_str(json_get(a, i));
        if (v && fa3_arr_has(b, v)) return true;
    }
    return false;
}
/* {getattr(sender_id,"open_id",None), getattr(sender_id,"user_id",None)} - {None} */
static json_t *fa3_sender_ids(const json_t *s) {
    json_t *a = json_array();
    const char *o = (s && s->type == JSON_OBJECT) ? fa3_str(json_obj_get(s, "open_id")) : NULL;
    const char *u = (s && s->type == JSON_OBJECT) ? fa3_str(json_obj_get(s, "user_id")) : NULL;
    if (o) json_append(a, json_string(o));
    if (u && (!o || strcmp(o, u) != 0)) json_append(a, json_string(u));
    return a;
}

/* ── ported functions ── */

/* PoP: fa3__bot_identity @ plugins/platforms/feishu/adapter.py:_bot_identity */
static json_t *fa3__bot_identity(fa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *self = json_obj_get(args, "self");
    json_t *r = json_object();
    fa3_set_copy(r, "open_id", self ? json_obj_get(self, "_bot_open_id") : NULL);
    fa3_set_copy(r, "user_id", self ? json_obj_get(self, "_bot_user_id") : NULL);
    fa3_set_copy(r, "name", self ? json_obj_get(self, "_bot_name") : NULL);
    return r;
}

/* PoP: fa3__post_mentions_bot @ plugins/platforms/feishu/adapter.py:_post_mentions_bot */
static json_t *fa3__post_mentions_bot(fa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *mentions = json_obj_get(args, "mentions");
    if (mentions && mentions->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(mentions); i++) {
            const json_t *m = json_get(mentions, i);
            if (m && m->type == JSON_OBJECT && fa3_truthy(json_obj_get(m, "is_self")))
                return json_bool(true);
        }
    }
    return json_bool(false);
}

/* PoP: fa3__message_mentions_bot @ plugins/platforms/feishu/adapter.py:_message_mentions_bot */
static json_t *fa3__message_mentions_bot(fa3_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const json_t *mentions = json_obj_get(args, "mentions");
    /* bot = self._bot_identity() — SEAMED sibling (executed BEFORE the loop). */
    json_t *ba = fa3_args();
    json_t *bot = fa3_call(ctx, "_bot_identity", ba);
    if (fa3_is_raise(bot)) return bot;
    if (!bot || bot->type != JSON_OBJECT) {
        json_free(bot);
        return fa3_err("AttributeError", "'NoneType' object has no attribute 'matches'");
    }
    if (mentions && mentions->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(mentions); i++) {
            const json_t *m = json_get(mentions, i);
            const json_t *mid = (m && m->type == JSON_OBJECT) ? json_obj_get(m, "id") : NULL;
            char *o = fa3_strip((mid && mid->type == JSON_OBJECT) ? fa3_str(json_obj_get(mid, "open_id")) : NULL);
            char *u = fa3_strip((mid && mid->type == JSON_OBJECT) ? fa3_str(json_obj_get(mid, "user_id")) : NULL);
            char *n = fa3_strip((m && m->type == JSON_OBJECT) ? fa3_str(json_obj_get(m, "name")) : NULL);
            json_t *ma = fa3_args();
            fa3_set_s(ma, "open_id", o);
            fa3_set_s(ma, "user_id", u);
            fa3_set_s(ma, "name", n);
            json_t *verdict = fa3_call(ctx, "matches", ma);
            free(o); free(u); free(n);
            if (fa3_is_raise(verdict)) { json_free(bot); return verdict; }
            bool hit = fa3_truthy(verdict);
            json_free(verdict);
            if (hit) { json_free(bot); return json_bool(true); }
        }
    }
    json_free(bot);
    return json_bool(false);
}

/* PoP: fa3__require_mention_for @ plugins/platforms/feishu/adapter.py:_require_mention_for */
static json_t *fa3__require_mention_for(fa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *self = json_obj_get(args, "self");
    const char *chat_id = fa3_str_or_empty(json_obj_get(args, "chat_id"));
    const json_t *rules = (self && self->type == JSON_OBJECT) ? json_obj_get(self, "_group_rules") : NULL;
    const json_t *rule = (chat_id[0] && rules && rules->type == JSON_OBJECT)
                         ? json_obj_get(rules, chat_id) : NULL;
    if (rule && rule->type == JSON_OBJECT) {
        const json_t *rm = json_obj_get(rule, "require_mention");
        if (rm && rm->type != JSON_NULL) return json_copy(rm);
    }
    const json_t *dflt = self ? json_obj_get(self, "_require_mention") : NULL;
    return dflt ? json_copy(dflt) : json_bool(false);
}

/* PoP: fa3__allow_group_message @ plugins/platforms/feishu/adapter.py:_allow_group_message */
static json_t *fa3__allow_group_message(fa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *self = json_obj_get(args, "self");
    const char *chat_id = fa3_str_or_empty(json_obj_get(args, "chat_id"));
    bool is_bot = json_get_bool(args, "is_bot", false);
    json_t *sender_ids = fa3_sender_ids(json_obj_get(args, "sender_id"));
    const json_t *admins = self ? json_obj_get(self, "_admins") : NULL;

    if (fa3_nonempty(sender_ids) && fa3_nonempty(admins) && fa3_set_inter(sender_ids, admins)) {
        json_free(sender_ids);
        return json_bool(true);
    }
    const json_t *rules = (self && self->type == JSON_OBJECT) ? json_obj_get(self, "_group_rules") : NULL;
    const json_t *rule = (chat_id[0] && rules && rules->type == JSON_OBJECT)
                         ? json_obj_get(rules, chat_id) : NULL;
    const char *policy;
    const json_t *allowlist, *blacklist;
    if (rule && rule->type == JSON_OBJECT) {
        policy = fa3_str_or_empty(json_obj_get(rule, "policy"));
        allowlist = json_obj_get(rule, "allowlist");
        blacklist = json_obj_get(rule, "blacklist");
    } else {
        const json_t *dgp = self ? json_obj_get(self, "_default_group_policy") : NULL;
        const char *dgp_s = fa3_str(dgp);
        policy = (dgp_s && dgp_s[0]) ? dgp_s : fa3_str_or_empty(self ? json_obj_get(self, "_group_policy") : NULL);
        allowlist = self ? json_obj_get(self, "_allowed_group_users") : NULL;
        blacklist = NULL;
    }
    bool result;
    if (strcmp(policy, "disabled") == 0 || strcmp(policy, "admin_only") == 0) {
        result = false;
    } else if (strcmp(policy, "open") == 0 || is_bot) {
        result = true;
    } else if (strcmp(policy, "allowlist") == 0) {
        result = fa3_nonempty(sender_ids) && fa3_set_inter(sender_ids, allowlist);
    } else if (strcmp(policy, "blacklist") == 0) {
        result = fa3_nonempty(sender_ids) && !fa3_set_inter(sender_ids, blacklist);
    } else {
        result = fa3_nonempty(sender_ids) && fa3_set_inter(sender_ids,
                     self ? json_obj_get(self, "_allowed_group_users") : NULL);
    }
    json_free(sender_ids);
    return json_bool(result);
}

/* PoP: fa3__mentions_self @ plugins/platforms/feishu/adapter.py:_mentions_self */
static json_t *fa3__mentions_self(fa3_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const json_t *message = json_obj_get(args, "message");
    const char *raw = fa3_str_or_empty(message ? json_obj_get(message, "content") : NULL);
    if (fa3_contains(raw, "@_all")) return json_bool(true);
    const json_t *mentions = (message && message->type == JSON_OBJECT)
                             ? json_obj_get(message, "mentions") : NULL;
    if (fa3_nonempty(mentions)) {
        json_t *ma = fa3_args();
        fa3_set_copy(ma, "mentions", mentions);
        json_t *hit = fa3_call(ctx, "_message_mentions_bot", ma);
        if (fa3_is_raise(hit)) return hit;
        bool yes = fa3_truthy(hit);
        json_free(hit);
        if (yes) return json_bool(true);
    }
    /* normalized = self._normalize(msg_type or "", raw, mentions) */
    json_t *na = fa3_args();
    fa3_set_s(na, "msg_type", fa3_str_or_empty(message ? json_obj_get(message, "message_type") : NULL));
    fa3_set_s(na, "raw_content", raw);
    fa3_set_copy(na, "mentions", mentions);
    json_t *norm = fa3_call(ctx, "_normalize", na);
    if (fa3_is_raise(norm)) return norm;
    json_t *pa = fa3_args();
    fa3_set_copy(pa, "mentions", (norm && norm->type == JSON_OBJECT)
                 ? json_obj_get(norm, "mentions") : NULL);
    json_free(norm);
    return fa3_call(ctx, "_post_mentions_bot", pa);
}

/* PoP: fa3__admit @ plugins/platforms/feishu/adapter.py:_admit */
static json_t *fa3__admit(fa3_ctx_t *ctx, const json_t *args) {
    const json_t *self = json_obj_get(args, "self");
    const json_t *sender = json_obj_get(args, "sender");
    const json_t *message = json_obj_get(args, "message");

    /* sender_ids = _sender_identity(sender) */
    json_t *sa = fa3_args();
    fa3_set_copy(sa, "sender", sender);
    json_t *sender_ids = fa3_call(ctx, "_sender_identity", sa);
    if (fa3_is_raise(sender_ids)) return sender_ids;
    /* self_ids = frozenset(v for v in (self._bot_open_id, self._bot_user_id) if v) */
    json_t *self_ids = json_array();
    const char *bo = (self && self->type == JSON_OBJECT) ? fa3_str(json_obj_get(self, "_bot_open_id")) : NULL;
    const char *bu = (self && self->type == JSON_OBJECT) ? fa3_str(json_obj_get(self, "_bot_user_id")) : NULL;
    if (bo && bo[0]) json_append(self_ids, json_string(bo));
    if (bu && bu[0] && (!bo || strcmp(bo, bu) != 0)) json_append(self_ids, json_string(bu));
    /* is_bot = _is_bot_sender(sender) */
    json_t *ia = fa3_args();
    fa3_set_copy(ia, "sender", sender);
    json_t *is_bot = fa3_call(ctx, "_is_bot_sender", ia);
    if (fa3_is_raise(is_bot)) { json_free(self_ids); json_free(sender_ids); return is_bot; }
    bool is_group = strcmp(fa3_str_or_empty(message ? json_obj_get(message, "chat_type") : NULL), "p2p") != 0;
    const char *chat_id = fa3_str_or_empty(message ? json_obj_get(message, "chat_id") : NULL);

    json_t *req = json_obj_get(args, "require_mention");
    if (!req || req->type == JSON_NULL) {
        json_t *ra = fa3_args();
        fa3_set_s(ra, "chat_id", chat_id);
        req = fa3_call(ctx, "_require_mention_for", ra);
        if (fa3_is_raise(req)) { json_free(self_ids); json_free(sender_ids); json_free(is_bot); return req; }
    } else {
        req = json_copy(req);
    }
    bool require_mention = is_group && fa3_truthy(req);

    json_t *ret = NULL;
    if (fa3_nonempty(self_ids) && fa3_set_inter(sender_ids, self_ids)) {
        ret = json_string("self_echo");
        goto done;
    }
    if (fa3_truthy(is_bot)) {
        const char *mode = fa3_str_or_empty(self ? json_obj_get(self, "_allow_bots") : NULL);
        if (strcmp(mode, "mentions") != 0 && strcmp(mode, "all") != 0) {
            ret = json_string("bots_disabled"); goto done;
        }
        if (!fa3_nonempty(self_ids) || !fa3_nonempty(sender_ids)) {
            ret = json_string("self_ids_unknown"); goto done;
        }
        if (strcmp(mode, "mentions") == 0 && !require_mention) {
            json_t *ma = fa3_args();
            fa3_set_copy(ma, "message", message);
            json_t *mret = fa3_call(ctx, "_mentions_self", ma);
            if (fa3_is_raise(mret)) { ret = mret; goto done; }
            bool yes = fa3_truthy(mret);
            json_free(mret);
            if (!yes) { ret = json_string("bot_not_mentioned"); goto done; }
        }
    }
    if (!is_group) {
        bool allow_all = self ? json_get_bool(self, "_allow_all_dm", false) : false;
        const json_t *allowed = self ? json_obj_get(self, "_allowed_group_users") : NULL;
        if (allow_all || !fa3_nonempty(allowed)) {
            ret = NULL; goto done;
        }
        ret = fa3_set_inter(sender_ids, allowed) ? NULL : json_string("dm_policy_rejected");
        goto done;
    }
    {
        json_t *ga = fa3_args();
        fa3_set_copy(ga, "sender_id", (sender && sender->type == JSON_OBJECT)
                     ? json_obj_get(sender, "sender_id") : NULL);
        fa3_set_s(ga, "chat_id", chat_id);
        json_set(ga, "is_bot", json_copy(is_bot));
        json_t *verdict = fa3_call(ctx, "_allow_group_message", ga);
        if (fa3_is_raise(verdict)) { ret = verdict; goto done; }
        bool ok = fa3_truthy(verdict);
        json_free(verdict);
        if (!ok) { ret = json_string("group_policy_rejected"); goto done; }
    }
    if (require_mention) {
        json_t *ma = fa3_args();
        fa3_set_copy(ma, "message", message);
        json_t *mret = fa3_call(ctx, "_mentions_self", ma);
        if (fa3_is_raise(mret)) { ret = mret; goto done; }
        bool yes = fa3_truthy(mret);
        json_free(mret);
        if (!yes) { ret = json_string("group_policy_rejected"); goto done; }
    }
    ret = NULL;
done:
    json_free(req);
    json_free(self_ids);
    json_free(sender_ids);
    json_free(is_bot);
    return ret;
}

/* PoP: fa3__warn_once_empty_allowlist_deny @ plugins/platforms/feishu/adapter.py:_warn_once_empty_allowlist_deny */
static json_t *fa3__warn_once_empty_allowlist_deny(fa3_ctx_t *ctx, const json_t *args) {
    json_t *self = (json_t *)json_obj_get(args, "self");
    if (self && self->type == JSON_OBJECT && fa3_truthy(json_obj_get(self, "_warned_empty_allowlist_deny")))
        return NULL;
    json_t *wa = fa3_args();
    fa3_set_s(wa, "chat_id", fa3_str_or_empty(json_obj_get(args, "chat_id")));
    fa3_set_copy(wa, "group_rules", self ? json_obj_get(self, "_group_rules") : NULL);
    fa3_set_copy(wa, "default_group_policy", self ? json_obj_get(self, "_default_group_policy") : NULL);
    fa3_set_copy(wa, "allowed_group_users", self ? json_obj_get(self, "_allowed_group_users") : NULL);
    json_t *text = fa3_call(ctx, "empty_allowlist_drop_warning", wa);
    if (fa3_is_raise(text)) return text;
    bool has = fa3_truthy(text);
    if (has) {
        if (self && self->type == JSON_OBJECT)
            json_set(self, "_warned_empty_allowlist_deny", json_bool(true));
        json_t *lr = fa3_log(ctx, "logger_warning", fa3_str_or_empty(text));
        json_free(text);
        if (lr) return lr;
        return NULL;
    }
    json_free(text);
    return NULL;
}
