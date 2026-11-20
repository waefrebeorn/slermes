/*
 * discord_adapter_flow2.c -- FLOW remainder of
 * plugins/platforms/discord/adapter.py (hermes-agent): the per-adapter
 * authorization-gate cluster (_extra_or_env_flag and its flag wrappers,
 * _snapshot_gate_env / _gate_env / _gate_raw / _gate_csv_set and the
 * channel / allowlist / allow-bots readers, resolved_allowlist_user_ids),
 * the bot-tag debounce cluster (_bot_tag_debounce_key / _window_seconds /
 * _record_bot_tag_debounce / _is_bot_tag_debounce_continuation), the
 * mention / channel-key cluster (_raw_mentioned_user_ids,
 * _self_is_explicitly_mentioned / _self_is_raw_mentioned,
 * _discord_bots_require_inline_mention, _discord_channel_keys[_from_channel],
 * _discord_free_response_channels, _is_discord_voice_message_attachment,
 * _discord_max_attachment_bytes, _discord_history_backfill[_limit],
 * _discord_thread_require_mention) and the command-sync payload
 * canonicalization cluster (_get_discord_command_sync_policy,
 * _canonicalize_app_command_payload / _option, _normalize_permissions,
 * _existing_command_to_payload, _patchable_app_command_payload).
 *
 * The 33 flow1 siblings stay CLAIMED there; here every ported sibling is
 * SEAMED when it is not the case target (the oracle binds the real def
 * only for the target and recorders for the rest), so composition runs
 * through the same universal seam as foreign calls.
 *
 * Decision sequences only: every live I/O / injected callable / sibling
 * method enters through ONE universal seam: da2_call(ctx, name, args).
 * The harness wires it to a recording stub fed the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name", "args", "ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and da2_-prefixed.
 *
 * Seam name table (name -> live call):
 *   _scoped_gate_env              gateway.platforms._shared.platform_gate_env(name, default="")
 *   _extra_or_secret              gateway.platforms._shared.extra_or_secret(extra, key, env, default)
 *   _decode_json_list_literal     gateway.platforms._shared.decode_json_list_literal(raw)
 *   is_automated_message          adapter_authorization.is_automated_message(message)
 *   time.monotonic                time.monotonic()
 *   logger.warning / logger.debug logger.<level>(fmt, *args)
 *   to_dict                       command.to_dict() (_existing_command_to_payload)
 *   is_voice_message              att.is_voice_message() (callable marker "seam")
 *   _extra_or_env_flag / _gate_env / _gate_raw / _gate_csv_set
 *   _bot_tag_debounce_key / _bot_tag_window_seconds
 *   _self_is_explicitly_mentioned / _raw_mentioned_user_ids
 *   _discord_channel_keys_from_channel
 *   _normalize_permissions / _canonicalize_app_command_payload
 *   _canonicalize_app_command_option
 *                                 sibling method calls (scripted when not target)
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object
 *   (OSError adds errno, message "[Errno N] msg"). BaseException kinds
 *   (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) pass
 *   through every except-Exception site.
 * - Attribute doubles (message/channel/author/parent/att/command/config
 *   param): JSON objects materialized as SimpleNamespace; an OBJECT double
 *   is ALWAYS truthy (even empty); scalars keep Python value truthiness.
 *   config.extra and the config PARAM's extra stay RAW dicts (isinstance
 *   dict gates). client.user / mentions entries are UserDoubles compared
 *   by id equality (discord.py User semantics); ids are str or int.
 * - Set returns project to SORTED deduped string lists on both sides;
 *   set-returning seams are scripted as lists (membership-only consumers).
 * - state fields: name (log name, default "Discord"), config {extra},
 *   _gate_env_snapshot (dict/null), _username_resolved_ids (list of str),
 *   _text_batch_delay_seconds / _text_batch_split_delay_seconds (numbers,
 *   absent -> 0.0), _bot_tag_debounce_until (dict of numbers / null),
 *   _client {user: {id}}. Absent config -> None carrier.
 * - State mutation: _snapshot_gate_env writes _gate_env_snapshot;
 *   _record_bot_tag_debounce / _is_bot_tag_debounce_continuation write/pop
 *   _bot_tag_debounce_until (created on first write when absent). out.state
 *   echoes the mutated copy; the oracle echoes the same two fields only
 *   when present in input OR written non-empty.
 * - is_voice_message marker: null/absent -> fallback duration+waveform
 *   probe; "seam" -> callable recording stub; any other value -> raw
 *   non-callable attribute (bool(marker)).
 * - Seam args keep explicit nulls; logger records {"fmt", "args"} verbatim
 *   (no %-formatting); the voice-failure logger arg projects str(exc).
 *
 * Fixture corpus domains (documented restrictions):
 * - str() positions carry strings / integral numbers / bools / None only
 *   (no non-integral float repr, no container repr).
 * - int() string parsing folds ASCII digits + Unicode White_Space strip;
 *   invalid tokens are printable ASCII without quotes/backslashes.
 * - str.lower()/strip() fold ASCII / full Unicode White_Space; mention
 *   regex \d folds ASCII digits.
 * - _gate_env_snapshot / _bot_tag_debounce_until are dicts or null;
 *   debounce values are numbers; _username_resolved_ids elements are str.
 * - to_dict / _canonicalize_* seam results are dicts; channel_types is a
 *   list / falsy scalar / str (list(str) -> codepoint chars); contexts /
 *   integration_types / options / choices / mentions are lists or falsy.
 * - client.user and mention entries carry id; command payload is a dict.
 * - No ported function reads a missing attribute on a scalar carrier
 *   (those AttributeError messages are oracle-double-class dependent).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct da2_ctx {
    json_t *state;      /* owned; input state copy, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da2_ctx_t;

/* -- basic helpers -- */
static bool da2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *da2_raise_kind(const json_t *v) {
    return da2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException kinds pass through every except-Exception site. */
static bool da2_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0
                 || strcmp(kind, "KeyboardInterrupt") == 0
                 || strcmp(kind, "GeneratorExit") == 0
                 || strcmp(kind, "CancelledError") == 0);
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool da2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* Attribute-double truthiness: an OBJECT carrier is always truthy. */
static bool da2_attr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_OBJECT) return true;
    return da2_truthy(v);
}

static bool da2_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static const char *da2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool da2_str_eq(const json_t *v, const char *s) {
    const char *t = da2_str(v);
    return t && s && strcmp(t, s) == 0;
}

static json_t *da2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static bool da2_is_oserror_type(const char *t) {
    static const char *fam[] = {
        "OSError", "IOError", "TimeoutError", "PermissionError",
        "FileNotFoundError", "FileExistsError", "BlockingIOError",
        "ConnectionError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "BrokenPipeError", "InterruptedError",
        "NotADirectoryError", "IsADirectoryError", "ChildProcessError",
        "ProcessLookupError", "DirectoryNotEmptyError",
    };
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(t, fam[i]) == 0) return true;
    return false;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *da2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && da2_is_oserror_type(type)) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = da2_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return da2_err(type, msg);
}

/* str(exc) projection of a raise spec (the voice-failure logger arg).
 * The spec message is ALREADY str(exc): da2_make_exc embeds the OSError
 * "[Errno N] " prefix, so return it verbatim (do not re-format errno). */
static char *da2_exc_str(const json_t *spec) {
    return strdup(json_get_str(spec, "message", ""));
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). A
 * missing/exhausted script RETURNS a fail-closed RuntimeError spec (the
 * event is recorded before the caller propagates it). */
static json_t *da2_call(da2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = da2_make_exc(raise);
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

/* -- string helpers -- */
static char *da2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* decode the codepoint at byte p (valid UTF-8 fixture corpus). */
static unsigned int da2_decode_cp(const unsigned char *p) {
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xe0) == 0xc0)
        return ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0)
        return ((unsigned)(p[0] & 0x0f) << 12) | ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3f) << 12)
        | ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}

static size_t da2_cp_len(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xe0) == 0xc0) return 2;
    if ((p[0] & 0xf0) == 0xe0) return 3;
    return 4;
}

/* Python str.strip() White_Space set (full Unicode, not just ASCII). */
static bool da2_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f))
        return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000)
        return true;
    return false;
}

/* str.strip(): full Unicode White_Space both ends. Caller frees. */
static char *da2_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *p = (const unsigned char *)s;
    while (*p && da2_is_ws_cp(da2_decode_cp(p))) p += da2_cp_len(p);
    const unsigned char *end = p + strlen((const char *)p);
    while (end > p) {
        const unsigned char *q = end - 1;
        while (q > p && (*q & 0xc0) == 0x80) q--;
        if (!da2_is_ws_cp(da2_decode_cp(q))) break;
        end = q;
    }
    size_t n = (size_t)(end - p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}

/* str.lower(): ASCII fold (fixture corpus folds ASCII only). */
static char *da2_lower(const char *s) {
    char *o = da2_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

static bool da2_contains(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}

/* JSON type name for Python error messages (fixture scalar/container set). */
static const char *da2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* -- Python numeric coercion on the fixture corpus -- */

/* int(str): ASCII digits, underscores BETWEEN digits, Unicode strip, sign. */
static bool da2_int_str(const char *s, long long *out) {
    char *t = da2_strip(s);
    if (!t) return false;
    const char *p = t;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    char clean[512];
    size_t k = 0;
    if (neg) clean[k++] = '-';
    size_t ndigits = 0;
    bool prev_digit = false;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c >= '0' && c <= '9') {
            if (k + 1 >= sizeof clean) { free(t); return false; }
            clean[k++] = (char)c;
            ndigits++;
            prev_digit = true;
            p++;
        } else if (c == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { free(t); return false; }
            prev_digit = false;
            p++;
        } else {
            free(t);
            return false;
        }
    }
    if (ndigits == 0) { free(t); return false; }
    clean[k] = '\0';
    *out = strtoll(clean, NULL, 10);
    free(t);
    return true;
}

/* repr of an ASCII-printable string for int() ValueError messages. */
static char *da2_repr_ascii(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n * 2 + 3);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = '\'';
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\' || s[i] == '\'') o[k++] = '\\';
        o[k++] = s[i];
    }
    o[k++] = '\'';
    o[k] = '\0';
    return o;
}

/* int() of a JSON value. kind: 0 ok, 1 TypeError, 2 ValueError. */
static int da2_int_of_json(const json_t *v, long long *out, json_t **err) {
    *err = NULL;
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        *out = (long long)(d < 0 ? ceil(d) : floor(d));
        return 0;
    }
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v && v->type == JSON_STRING) {
        if (da2_int_str(v->str_val, out)) return 0;
        char *r = da2_repr_ascii(v->str_val);
        char buf[600];
        snprintf(buf, sizeof buf, "invalid literal for int() with base 10: %s", r ? r : "''");
        free(r);
        *err = da2_err("ValueError", buf);
        return 2;
    }
    char buf[192];
    snprintf(buf, sizeof buf,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             da2_type_name(v));
    *err = da2_err("TypeError", buf);
    return 1;
}

/* str() projection: strings verbatim, integral numbers as ints, bools,
 * None -> "None". Non-integral floats / containers are outside the corpus. */
static char *da2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da2_strdup("None");
    if (v->type == JSON_STRING) return da2_strdup(v->str_val);
    if (v->type == JSON_BOOL) return da2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof buf, "%.0f", v->num_val);
        return da2_strdup(buf);
    }
    return da2_strdup("");
}

/* Python max(a, b) on doubles: NaN comparisons are false. */
static double da2_max2(double a, double b) { return b > a ? b : a; }

/* id equality for UserDouble membership: str==str, number==number
 * (integral floats == ints); absent==absent; mixed types are unequal. */
static bool da2_json_eq(const json_t *a, const json_t *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val, b->str_val) == 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
        return a->num_val == b->num_val;
    if (a->type == JSON_NULL && b->type == JSON_NULL) return true;
    return false;
}

/* -- sorted string-set builder (set[str] -> sorted deduped list) -- */
typedef struct da2_set {
    char **items;
    size_t n, cap;
} da2_set_t;

static void da2_set_init(da2_set_t *s) { s->items = NULL; s->n = s->cap = 0; }

static void da2_set_free(da2_set_t *s) {
    for (size_t i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL; s->n = s->cap = 0;
}

/* add a copy of str (dedup); empty strings are the caller's filter. */
static void da2_set_add(da2_set_t *s, const char *str) {
    if (!str) return;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->items[i], str) == 0) return;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        char **ni = realloc(s->items, nc * sizeof(char *));
        if (!ni) return;
        s->items = ni; s->cap = nc;
    }
    s->items[s->n++] = da2_strdup(str);
}

static int da2_set_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *da2_set_array(da2_set_t *s) {
    if (s->n > 1) qsort(s->items, s->n, sizeof(char *), da2_set_cmp);
    json_t *a = json_array();
    for (size_t i = 0; i < s->n; i++) json_append(a, json_string(s->items[i]));
    return a;
}

/* -- adapter state readers -- */

/* getattr(self.config, "extra", None): config absent/null -> NULL;
 * extra stays the RAW node (dict gate downstream). */
static const json_t *da2_config_extra(da2_ctx_t *ctx) {
    const json_t *config = json_obj_get(ctx->state, "config");
    if (!config || config->type != JSON_OBJECT) return NULL;
    return json_obj_get(config, "extra");
}

/* self.config.extra.get(key): two AttributeError failure points.
 * Returns the BORROWED value (NULL absent, JSON_NULL node present-null);
 * *err carries an owned raise spec on a non-carrier config/extra. */
static const json_t *da2_extra_get(da2_ctx_t *ctx, const char *key, json_t **err) {
    *err = NULL;
    const json_t *config = json_obj_get(ctx->state, "config");
    if (!config || config->type == JSON_NULL) {
        *err = da2_err("AttributeError", "'NoneType' object has no attribute 'extra'");
        return NULL;
    }
    if (config->type != JSON_OBJECT) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'extra'", da2_type_name(config));
        *err = da2_err("AttributeError", buf);
        return NULL;
    }
    const json_t *extra = json_obj_get(config, "extra");
    if (!extra || extra->type != JSON_OBJECT) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'", da2_type_name(extra));
        *err = da2_err("AttributeError", buf);
        return NULL;
    }
    return json_obj_get(extra, key);
}

/* state numeric field (absent/non-number -> def; corpus carries numbers). */
static double da2_state_num(da2_ctx_t *ctx, const char *key, double def) {
    const json_t *v = json_obj_get(ctx->state, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

/* _bot_tag_debounce_until[key] = val (create the dict on first write). */
static json_t *da2_deb_set(da2_ctx_t *ctx, const char *key, double val) {
    json_t *deb = json_obj_get(ctx->state, "_bot_tag_debounce_until");
    if (!deb) {
        deb = json_object();
        json_set(ctx->state, "_bot_tag_debounce_until", deb);
    } else if (deb->type != JSON_OBJECT) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object does not support item assignment",
                 da2_type_name(deb));
        return da2_err("TypeError", buf);
    }
    json_set(deb, key, json_number(val));
    return NULL;
}

/* _bot_tag_debounce_until.pop(key, None): absent dict is a no-op. */
static void da2_deb_pop(da2_ctx_t *ctx, const char *key) {
    json_t *deb = json_obj_get(ctx->state, "_bot_tag_debounce_until");
    if (deb && deb->type == JSON_OBJECT) json_obj_del(deb, key);
}

/* -- per-adapter authorization gates -- */

/* PoP: da2_extra_or_env_flag @ plugins/platforms/discord/adapter.py:_extra_or_env_flag */
static json_t *da2_extra_or_env_flag(da2_ctx_t *ctx, const json_t *args) {
    const json_t *extra = da2_config_extra(ctx);
    json_t *a = json_object();
    json_set(a, "extra", (extra && extra->type == JSON_OBJECT) ? json_copy(extra) : json_null());
    json_set(a, "key", json_copy(json_obj_get(args, "key")));
    json_set(a, "env", json_copy(json_obj_get(args, "env_key")));
    json_set(a, "default", json_null());
    json_t *configured = da2_call(ctx, "_extra_or_secret", a);
    if (da2_is_raise(configured)) return configured;
    if (da2_is_null(configured)) {
        json_free(configured);
        const json_t *ed = json_obj_get(args, "env_default");
        configured = ed ? json_copy(ed) : json_null();
    }
    if (configured && configured->type == JSON_BOOL) return configured;
    char *s = da2_py_str(configured);
    char *t = da2_strip(s);
    free(s);
    char *l = da2_lower(t);
    free(t);
    bool truthy = json_get_bool(args, "truthy", false);
    bool hit;
    if (truthy)
        hit = strcmp(l, "true") == 0 || strcmp(l, "1") == 0
           || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0;
    else
        hit = !(strcmp(l, "false") == 0 || strcmp(l, "0") == 0
             || strcmp(l, "no") == 0 || strcmp(l, "off") == 0);
    free(l);
    json_free(configured);
    return json_bool(hit);
}

/* flag wrappers: one scripted _extra_or_env_flag sibling call each. */
static json_t *da2_flag_wrapper(da2_ctx_t *ctx, const char *key,
                                const char *env_key, const char *env_default,
                                bool truthy) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set_hdr(a, "env_key", env_key);
    json_set_hdr(a, "env_default", env_default);
    json_set(a, "truthy", json_bool(truthy));
    return da2_call(ctx, "_extra_or_env_flag", a);
}

/* PoP: da2_discord_require_mention @ plugins/platforms/discord/adapter.py:_discord_require_mention */
static json_t *da2_discord_require_mention(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da2_flag_wrapper(ctx, "require_mention", "DISCORD_REQUIRE_MENTION", "true", false);
}

/* PoP: da2_discord_free_response_auto_thread @ plugins/platforms/discord/adapter.py:_discord_free_response_auto_thread */
static json_t *da2_discord_free_response_auto_thread(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da2_flag_wrapper(ctx, "free_response_auto_thread",
                            "DISCORD_FREE_RESPONSE_AUTO_THREAD", "false", true);
}

/* PoP: da2_discord_thread_require_mention @ plugins/platforms/discord/adapter.py:_discord_thread_require_mention */
static json_t *da2_discord_thread_require_mention(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da2_flag_wrapper(ctx, "thread_require_mention",
                            "DISCORD_THREAD_REQUIRE_MENTION", "false", true);
}

/* PoP: da2_discord_history_backfill @ plugins/platforms/discord/adapter.py:_discord_history_backfill */
static json_t *da2_discord_history_backfill(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da2_flag_wrapper(ctx, "history_backfill", "DISCORD_HISTORY_BACKFILL", "true", true);
}

/* PoP: da2_discord_max_attachment_bytes @ plugins/platforms/discord/adapter.py:_discord_max_attachment_bytes */
static json_t *da2_discord_max_attachment_bytes(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *configured = da2_extra_get(ctx, "max_attachment_bytes", &err);
    if (err) return err;
    json_t *owned = NULL;
    if (da2_is_null(configured)) {
        json_t *a = json_object();
        json_set_hdr(a, "name", "DISCORD_MAX_ATTACHMENT_BYTES");
        json_set_hdr(a, "default", "");
        json_t *r = da2_call(ctx, "_scoped_gate_env", a);
        if (da2_is_raise(r)) return r;
        if (da2_truthy(r)) { configured = r; owned = r; } /* ... or None */
        else json_free(r);
    }
    if (da2_is_null(configured) || da2_str_eq(configured, "")) {
        if (owned) json_free(owned);
        return json_number(32.0 * 1024.0 * 1024.0);
    }
    long long value = 0;
    json_t *ierr = NULL;
    if (da2_int_of_json(configured, &value, &ierr) != 0) {
        json_free(ierr); /* except (TypeError, ValueError) */
        json_t *a = json_object();
        json_set_hdr(a, "fmt",
            "[Discord] Invalid max_attachment_bytes value %r, falling back to 32 MiB");
        json_t *aa = json_array();
        json_append(aa, json_copy(configured));
        json_set(a, "args", aa);
        json_t *lr = da2_call(ctx, "logger.warning", a);
        if (da2_is_raise(lr)) { if (owned) json_free(owned); return lr; }
        json_free(lr);
        if (owned) json_free(owned);
        return json_number(32.0 * 1024.0 * 1024.0);
    }
    if (owned) json_free(owned);
    return json_number((double)(value < 0 ? 0 : value)); /* max(0, value) */
}

/* PoP: da2_is_discord_voice_message_attachment @ plugins/platforms/discord/adapter.py:_is_discord_voice_message_attachment */
static json_t *da2_is_discord_voice_message_attachment(da2_ctx_t *ctx, const json_t *args) {
    const json_t *att = json_obj_get(args, "att");
    const json_t *marker = json_obj_get(att, "is_voice_message");
    if (!da2_is_null(marker)) {
        if (da2_str_eq(marker, "seam")) {
            json_t *r = da2_call(ctx, "is_voice_message", json_object());
            if (da2_is_raise(r)) {
                if (da2_is_base_exc(da2_raise_kind(r))) return r;
                /* except Exception: log str(exc) and degrade to False. */
                char *es = da2_exc_str(r);
                json_t *a = json_object();
                json_set_hdr(a, "fmt",
                    "[Discord] is_voice_message() failed for attachment: %s");
                json_t *aa = json_array();
                json_append(aa, json_string(es ? es : ""));
                free(es);
                json_set(a, "args", aa);
                json_t *lr = da2_call(ctx, "logger.debug", a);
                if (da2_is_raise(lr)) { json_free(r); return lr; }
                json_free(lr);
                json_free(r);
                return json_bool(false);
            }
            bool hit = da2_truthy(r);
            json_free(r);
            return json_bool(hit);
        }
        return json_bool(da2_truthy(marker));
    }
    const json_t *duration = json_obj_get(att, "duration");
    const json_t *waveform = json_obj_get(att, "waveform");
    return json_bool(!da2_is_null(duration) && !da2_is_null(waveform));
}

/* PoP: da2_snapshot_gate_env @ plugins/platforms/discord/adapter.py:_snapshot_gate_env */
static json_t *da2_snapshot_gate_env(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    /* _GATE_ENV_KEYS tuple order (module constant). */
    static const char *keys[] = {
        "DISCORD_ALLOWED_USERS", "DISCORD_ALLOWED_ROLES", "DISCORD_ALLOWED_CHANNELS",
        "DISCORD_IGNORED_CHANNELS", "DISCORD_NO_THREAD_CHANNELS", "DISCORD_FREE_RESPONSE_CHANNELS",
        "DISCORD_MISSED_MESSAGE_BACKFILL_CHANNELS", "DISCORD_ALLOW_ALL_USERS", "DISCORD_ALLOW_BOTS",
        "GATEWAY_ALLOW_ALL_USERS", "GATEWAY_ALLOWED_USERS",
    };
    json_t *snap = json_object();
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        json_t *a = json_object();
        json_set_hdr(a, "name", keys[i]);
        json_set_hdr(a, "default", "");
        json_t *r = da2_call(ctx, "_scoped_gate_env", a);
        if (da2_is_raise(r)) { json_free(snap); return r; }
        json_set(snap, keys[i], r);
    }
    json_set(ctx->state, "_gate_env_snapshot", snap);
    return NULL; /* live returns None */
}

/* PoP: da2_gate_env @ plugins/platforms/discord/adapter.py:_gate_env */
static json_t *da2_gate_env(da2_ctx_t *ctx, const json_t *args) {
    const json_t *name = json_obj_get(args, "name");
    const json_t *def = json_obj_get(args, "default");
    const json_t *snap = json_obj_get(ctx->state, "_gate_env_snapshot");
    if (snap && snap->type != JSON_NULL && da2_str(name)) {
        const json_t *v = json_obj_get(snap, name->str_val);
        if (v) /* name in snap */
            return da2_truthy(v) ? json_copy(v)
                                 : (def ? json_copy(def) : json_string(""));
    }
    json_t *a = json_object();
    json_set(a, "name", name ? json_copy(name) : json_null());
    json_set(a, "default", def ? json_copy(def) : json_string(""));
    return da2_call(ctx, "_scoped_gate_env", a);
}

/* PoP: da2_gate_raw @ plugins/platforms/discord/adapter.py:_gate_raw */
static json_t *da2_gate_raw(da2_ctx_t *ctx, const json_t *args) {
    const json_t *cfg = json_obj_get(args, "config");
    json_t *a = json_object();
    json_set(a, "name", json_copy(json_obj_get(args, "env_key")));
    json_set_hdr(a, "default", "");
    json_t *val = da2_is_null(cfg) ? da2_call(ctx, "_gate_env", a)
                                   : da2_call(ctx, "_scoped_gate_env", a);
    if (da2_is_raise(val)) return val;
    if (da2_truthy(val)) return val;
    json_free(val);
    /* getattr(self.config if config is None else config, "extra", None) */
    const json_t *src = da2_is_null(cfg) ? json_obj_get(ctx->state, "config") : cfg;
    const json_t *extra = json_obj_get(src, "extra");
    if (extra && extra->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(extra, json_get_str(args, "extra_key", ""));
        return v ? json_copy(v) : json_null();
    }
    return json_null();
}

/* PoP: da2_gate_csv_set @ plugins/platforms/discord/adapter.py:_gate_csv_set */
static json_t *da2_gate_csv_set(da2_ctx_t *ctx, const json_t *args) {
    const json_t *raw = json_obj_get(args, "raw");
    if (da2_is_null(raw)) return json_array(); /* set() */
    json_t *a = json_object();
    json_set(a, "raw", json_copy(raw));
    json_t *decoded = da2_call(ctx, "_decode_json_list_literal", a);
    if (da2_is_raise(decoded)) return decoded;
    da2_set_t set;
    da2_set_init(&set);
    if (decoded && decoded->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(decoded); i++) {
            char *s = da2_py_str(json_get(decoded, i));
            char *t = da2_strip(s);
            free(s);
            if (t && t[0]) da2_set_add(&set, t);
            free(t);
        }
    } else {
        char *s = da2_py_str(decoded);
        /* str(raw).split(",") then strip/filter each part. */
        char *p = s;
        while (p) {
            char *comma = strchr(p, ',');
            if (comma) *comma = '\0';
            char *t = da2_strip(p);
            if (t && t[0]) da2_set_add(&set, t);
            free(t);
            p = comma ? comma + 1 : NULL;
        }
        free(s);
    }
    json_free(decoded);
    json_t *r = da2_set_array(&set);
    da2_set_free(&set);
    return r;
}

/* channel-gate readers: _gate_raw then _gate_csv_set, in order. */
static json_t *da2_channel_gate(da2_ctx_t *ctx, const char *extra_key, const char *env_key) {
    json_t *a = json_object();
    json_set_hdr(a, "extra_key", extra_key);
    json_set_hdr(a, "env_key", env_key);
    json_set(a, "config", json_null());
    json_t *raw = da2_call(ctx, "_gate_raw", a);
    if (da2_is_raise(raw)) return raw;
    json_t *a2 = json_object();
    json_set(a2, "raw", raw); /* adopted */
    return da2_call(ctx, "_gate_csv_set", a2);
}

/* PoP: da2_get_allowed_channels @ plugins/platforms/discord/adapter.py:_get_allowed_channels */
static json_t *da2_get_allowed_channels(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da2_channel_gate(ctx, "allowed_channels", "DISCORD_ALLOWED_CHANNELS");
}

/* PoP: da2_get_ignored_channels @ plugins/platforms/discord/adapter.py:_get_ignored_channels */
static json_t *da2_get_ignored_channels(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da2_channel_gate(ctx, "ignored_channels", "DISCORD_IGNORED_CHANNELS");
}

/* PoP: da2_get_no_thread_channels @ plugins/platforms/discord/adapter.py:_get_no_thread_channels */
static json_t *da2_get_no_thread_channels(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da2_channel_gate(ctx, "no_thread_channels", "DISCORD_NO_THREAD_CHANNELS");
}

/* PoP: da2_resolved_allowlist_user_ids @ plugins/platforms/discord/adapter.py:resolved_allowlist_user_ids */
static json_t *da2_resolved_allowlist_user_ids(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *ids = json_obj_get(ctx->state, "_username_resolved_ids");
    if (!ids || ids->type != JSON_ARRAY) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object is not iterable", da2_type_name(ids));
        return da2_err("TypeError", buf);
    }
    da2_set_t set;
    da2_set_init(&set);
    for (size_t i = 0; i < json_len(ids); i++) {
        const json_t *e = json_get(ids, i);
        if (e && e->type == JSON_STRING) da2_set_add(&set, e->str_val);
    }
    json_t *r = da2_set_array(&set);
    da2_set_free(&set);
    return r;
}

/* PoP: da2_discord_allow_all_users @ plugins/platforms/discord/adapter.py:_discord_allow_all_users */
static json_t *da2_discord_allow_all_users(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "extra_key", "allow_all_users");
    json_set_hdr(a, "env_key", "DISCORD_ALLOW_ALL_USERS");
    json_set(a, "config", json_null());
    json_t *raw = da2_call(ctx, "_gate_raw", a);
    if (da2_is_raise(raw)) return raw;
    char *s = da2_truthy(raw) ? da2_py_str(raw) : da2_strdup("");
    json_free(raw);
    char *t = da2_strip(s);
    free(s);
    char *l = da2_lower(t);
    free(t);
    bool hit = strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0;
    free(l);
    return json_bool(hit);
}

/* PoP: da2_gateway_allow_all_users @ plugins/platforms/discord/adapter.py:_gateway_allow_all_users */
static json_t *da2_gateway_allow_all_users(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "name", "GATEWAY_ALLOW_ALL_USERS");
    json_set_hdr(a, "default", "");
    json_t *r = da2_call(ctx, "_gate_env", a);
    if (da2_is_raise(r)) return r;
    if (!r || r->type != JSON_STRING) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'strip'", da2_type_name(r));
        json_free(r);
        return da2_err("AttributeError", buf);
    }
    char *t = da2_strip(r->str_val);
    char *l = da2_lower(t);
    free(t);
    json_free(r);
    bool hit = strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0;
    free(l);
    return json_bool(hit);
}

/* PoP: da2_get_allow_bots @ plugins/platforms/discord/adapter.py:_get_allow_bots */
static json_t *da2_get_allow_bots(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "extra_key", "allow_bots");
    json_set_hdr(a, "env_key", "DISCORD_ALLOW_BOTS");
    json_set(a, "config", json_null());
    json_t *raw = da2_call(ctx, "_gate_raw", a);
    if (da2_is_raise(raw)) return raw;
    char *s = da2_truthy(raw) ? da2_py_str(raw) : da2_strdup("none");
    json_free(raw);
    char *l = da2_lower(s);
    free(s);
    char *t = da2_strip(l);
    free(l);
    json_t *r = json_string(t && t[0] ? t : "none");
    free(t);
    return r;
}

/* -- bot-tag debounce cluster -- */

/* PoP: da2_bot_tag_debounce_key @ plugins/platforms/discord/adapter.py:_bot_tag_debounce_key */
static json_t *da2_bot_tag_debounce_key(da2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *message = json_obj_get(args, "message");
    const json_t *channel = json_obj_get(message, "channel");
    const json_t *author = json_obj_get(message, "author");
    const json_t *cid = json_obj_get(channel, "id");
    const json_t *aid = json_obj_get(author, "id");
    char *cs = cid ? da2_py_str(cid) : da2_strdup("");
    char *as = aid ? da2_py_str(aid) : da2_strdup("");
    size_t n = strlen(cs) + strlen(as) + 2;
    char *buf = malloc(n);
    if (buf) snprintf(buf, n, "%s:%s", cs, as);
    json_t *r = json_string(buf ? buf : ":");
    free(cs); free(as); free(buf);
    return r;
}

/* PoP: da2_bot_tag_window_seconds @ plugins/platforms/discord/adapter.py:_bot_tag_window_seconds */
static json_t *da2_bot_tag_window_seconds(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    double a = da2_state_num(ctx, "_text_batch_delay_seconds", 0.0);
    double b = da2_state_num(ctx, "_text_batch_split_delay_seconds", 0.0);
    return json_number(da2_max2(a, b));
}

/* PoP: da2_record_bot_tag_debounce @ plugins/platforms/discord/adapter.py:_record_bot_tag_debounce */
static json_t *da2_record_bot_tag_debounce(da2_ctx_t *ctx, const json_t *args) {
    if (da2_state_num(ctx, "_text_batch_delay_seconds", 0.0) <= 0) return NULL;
    json_t *a = json_object();
    json_set(a, "message", json_copy(json_obj_get(args, "message")));
    json_t *r = da2_call(ctx, "is_automated_message", a);
    if (da2_is_raise(r)) return r;
    bool automated = da2_truthy(r);
    json_free(r);
    if (!automated) return NULL;
    a = json_object();
    json_set(a, "message", json_copy(json_obj_get(args, "message")));
    r = da2_call(ctx, "_self_is_explicitly_mentioned", a);
    if (da2_is_raise(r)) return r;
    bool mentioned = da2_truthy(r);
    json_free(r);
    if (!mentioned) return NULL;
    /* RHS first: time.monotonic() + self._bot_tag_window_seconds() ... */
    json_t *now = da2_call(ctx, "time.monotonic", json_object());
    if (da2_is_raise(now)) return now;
    json_t *win = da2_call(ctx, "_bot_tag_window_seconds", json_object());
    if (da2_is_raise(win)) { json_free(now); return win; }
    double sum = now->num_val + win->num_val;
    json_free(now); json_free(win);
    /* ... then the target subscript self._bot_tag_debounce_key(message). */
    a = json_object();
    json_set(a, "message", json_copy(json_obj_get(args, "message")));
    json_t *key = da2_call(ctx, "_bot_tag_debounce_key", a);
    if (da2_is_raise(key)) return key;
    json_t *err = da2_deb_set(ctx, json_get_str(key, "", ""), sum);
    json_free(key);
    if (err) return err;
    return NULL;
}

/* PoP: da2_is_bot_tag_debounce_continuation @ plugins/platforms/discord/adapter.py:_is_bot_tag_debounce_continuation */
static json_t *da2_is_bot_tag_debounce_continuation(da2_ctx_t *ctx, const json_t *args) {
    if (da2_state_num(ctx, "_text_batch_delay_seconds", 0.0) <= 0) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "message", json_copy(json_obj_get(args, "message")));
    json_t *r = da2_call(ctx, "is_automated_message", a);
    if (da2_is_raise(r)) return r;
    bool automated = da2_truthy(r);
    json_free(r);
    if (!automated) return json_bool(false);
    a = json_object();
    json_set(a, "message", json_copy(json_obj_get(args, "message")));
    json_t *key = da2_call(ctx, "_bot_tag_debounce_key", a);
    if (da2_is_raise(key)) return key;
    json_t *now = da2_call(ctx, "time.monotonic", json_object());
    if (da2_is_raise(now)) { json_free(key); return now; }
    /* self._bot_tag_debounce_until.get(key, 0.0) <= now */
    json_t *deb = json_obj_get(ctx->state, "_bot_tag_debounce_until");
    if (deb && deb->type != JSON_OBJECT) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'", da2_type_name(deb));
        json_free(key); json_free(now);
        return da2_err("AttributeError", buf);
    }
    const json_t *stored = deb ? json_obj_get(deb, json_get_str(key, "", "")) : NULL;
    double sval = 0.0;
    if (stored && stored->type != JSON_NUMBER) {
        char buf[192];
        snprintf(buf, sizeof buf,
                 "'<=' not supported between instances of '%s' and '%s'",
                 da2_type_name(stored), da2_type_name(now));
        json_free(key); json_free(now);
        return da2_err("TypeError", buf);
    }
    if (stored) sval = stored->num_val;
    if (sval <= now->num_val) {
        da2_deb_pop(ctx, json_get_str(key, "", ""));
        json_free(key); json_free(now);
        return json_bool(false);
    }
    json_t *win = da2_call(ctx, "_bot_tag_window_seconds", json_object());
    if (da2_is_raise(win)) { json_free(key); json_free(now); return win; }
    double sum = now->num_val + win->num_val;
    json_free(win);
    json_t *err = da2_deb_set(ctx, json_get_str(key, "", ""), sum);
    json_free(key); json_free(now);
    if (err) return err;
    return json_bool(true);
}

/* PoP: da2_discord_free_response_channels @ plugins/platforms/discord/adapter.py:_discord_free_response_channels */
static json_t *da2_discord_free_response_channels(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *raw = da2_extra_get(ctx, "free_response_channels", &err);
    if (err) return err;
    json_t *owned = NULL;
    if (da2_is_null(raw)) {
        json_t *a = json_object();
        json_set_hdr(a, "name", "DISCORD_FREE_RESPONSE_CHANNELS");
        json_set_hdr(a, "default", "");
        json_t *r = da2_call(ctx, "_gate_env", a);
        if (da2_is_raise(r)) return r;
        raw = r; owned = r;
    }
    json_t *a2 = json_object();
    json_set(a2, "raw", json_copy(raw));
    json_t *r2 = da2_call(ctx, "_gate_csv_set", a2);
    if (owned) json_free(owned);
    return r2;
}

/* -- mention / channel-key cluster -- */

/* PoP: da2_raw_mentioned_user_ids @ plugins/platforms/discord/adapter.py:_raw_mentioned_user_ids */
static json_t *da2_raw_mentioned_user_ids(da2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *message = json_obj_get(args, "message");
    const json_t *content = json_obj_get(message, "content");
    /* getattr(message, "content", "") or "" */
    const char *text = "";
    if (da2_truthy(content)) {
        if (content->type != JSON_STRING) {
            char buf[160];
            snprintf(buf, sizeof buf,
                     "expected string or bytes-like object, got '%s'", da2_type_name(content));
            return da2_err("TypeError", buf);
        }
        text = content->str_val;
    }
    /* re.finditer(r"<@!?(\d+)>", content): non-overlapping, ASCII digits. */
    da2_set_t set;
    da2_set_init(&set);
    const char *p = text;
    while ((p = strstr(p, "<@")) != NULL) {
        const char *q = p + 2;
        if (*q == '!') q++;
        const char *start = q;
        while (*q >= '0' && *q <= '9') q++;
        if (q > start && *q == '>') {
            size_t n = (size_t)(q - start);
            char *tmp = malloc(n + 1);
            if (tmp) { memcpy(tmp, start, n); tmp[n] = '\0'; da2_set_add(&set, tmp); free(tmp); }
            p = q + 1;
        } else {
            p = p + 1;
        }
    }
    json_t *r = da2_set_array(&set);
    da2_set_free(&set);
    return r;
}

/* client/user carrier guard shared by the two self-mention probes. */
static bool da2_client_user(da2_ctx_t *ctx, const json_t **user_out) {
    const json_t *client = json_obj_get(ctx->state, "_client");
    if (!da2_attr_truthy(client)) return false;
    const json_t *user = json_obj_get(client, "user");
    if (!da2_attr_truthy(user)) return false;
    *user_out = user;
    return true;
}

/* PoP: da2_self_is_explicitly_mentioned @ plugins/platforms/discord/adapter.py:_self_is_explicitly_mentioned */
static json_t *da2_self_is_explicitly_mentioned(da2_ctx_t *ctx, const json_t *args) {
    const json_t *user = NULL;
    if (!da2_client_user(ctx, &user)) return json_bool(false);
    const json_t *uid = json_obj_get(user, "id");
    /* self._client.user in getattr(message, "mentions", []): id equality. */
    const json_t *message = json_obj_get(args, "message");
    const json_t *mentions = json_obj_get(message, "mentions");
    if (mentions && mentions->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(mentions); i++) {
            const json_t *e = json_get(mentions, i);
            if (e && e->type == JSON_OBJECT) {
                if (da2_json_eq(uid, json_obj_get(e, "id"))) return json_bool(true);
            }
        }
    }
    json_t *a = json_object();
    json_set(a, "message", json_copy(message));
    json_t *r = da2_call(ctx, "_raw_mentioned_user_ids", a);
    if (da2_is_raise(r)) return r;
    char *us = da2_py_str(uid);
    bool hit = false;
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(r); i++)
            if (da2_str_eq(json_get(r, i), us)) { hit = true; break; }
    }
    free(us);
    json_free(r);
    return json_bool(hit);
}

/* PoP: da2_self_is_raw_mentioned @ plugins/platforms/discord/adapter.py:_self_is_raw_mentioned */
static json_t *da2_self_is_raw_mentioned(da2_ctx_t *ctx, const json_t *args) {
    const json_t *user = NULL;
    if (!da2_client_user(ctx, &user)) return json_bool(false);
    const json_t *uid = json_obj_get(user, "id");
    json_t *a = json_object();
    json_set(a, "message", json_copy(json_obj_get(args, "message")));
    json_t *r = da2_call(ctx, "_raw_mentioned_user_ids", a);
    if (da2_is_raise(r)) return r;
    char *us = da2_py_str(uid);
    bool hit = false;
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(r); i++)
            if (da2_str_eq(json_get(r, i), us)) { hit = true; break; }
    }
    free(us);
    json_free(r);
    return json_bool(hit);
}

/* PoP: da2_discord_bots_require_inline_mention @ plugins/platforms/discord/adapter.py:_discord_bots_require_inline_mention */
static json_t *da2_discord_bots_require_inline_mention(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *configured = da2_extra_get(ctx, "bots_require_inline_mention", &err);
    if (err) return err;
    if (configured && configured->type == JSON_STRING) {
        char *l = da2_lower(configured->str_val);
        bool hit = strcmp(l, "true") == 0 || strcmp(l, "1") == 0
                || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0;
        free(l);
        return json_bool(hit);
    }
    return da2_flag_wrapper(ctx, "bots_require_inline_mention",
                            "DISCORD_BOTS_REQUIRE_INLINE_MENTION", "true", true);
}

/* PoP: da2_discord_channel_keys @ plugins/platforms/discord/adapter.py:_discord_channel_keys */
static json_t *da2_discord_channel_keys(da2_ctx_t *ctx, const json_t *args) {
    const json_t *message = json_obj_get(args, "message");
    const json_t *channel = json_obj_get(message, "channel");
    json_t *a = json_object();
    json_set(a, "channel", channel ? json_copy(channel) : json_null());
    const json_t *parent = json_obj_get(args, "parent_channel_id");
    json_set(a, "parent_channel_id", parent ? json_copy(parent) : json_null());
    return da2_call(ctx, "_discord_channel_keys_from_channel", a);
}

/* PoP: da2_discord_channel_keys_from_channel @ plugins/platforms/discord/adapter.py:_discord_channel_keys_from_channel */
static json_t *da2_discord_channel_keys_from_channel(da2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *channel = json_obj_get(args, "channel");
    const json_t *parent = json_obj_get(args, "parent_channel_id");
    da2_set_t set;
    da2_set_init(&set);
    const json_t *cid = json_obj_get(channel, "id");
    if (!da2_is_null(cid)) {
        char *s = da2_py_str(cid);
        da2_set_add(&set, s);
        free(s);
    }
    const json_t *name = json_obj_get(channel, "name");
    char *s = name ? da2_py_str(name) : da2_strdup("");
    char *t = da2_strip(s);
    free(s);
    if (t && t[0]) {
        da2_set_add(&set, t);
        size_t n = strlen(t) + 2;
        char *h = malloc(n);
        if (h) { snprintf(h, n, "#%s", t); da2_set_add(&set, h); free(h); }
    }
    free(t);
    const json_t *pid = da2_truthy(parent) ? parent : json_obj_get(channel, "parent_id");
    if (da2_truthy(pid)) {
        s = da2_py_str(pid);
        da2_set_add(&set, s);
        free(s);
    }
    const json_t *pchannel = json_obj_get(channel, "parent");
    if (da2_attr_truthy(pchannel)) {
        const json_t *pn = json_obj_get(pchannel, "name");
        s = pn ? da2_py_str(pn) : da2_strdup("");
        t = da2_strip(s);
        free(s);
        if (t && t[0]) {
            da2_set_add(&set, t);
            size_t n = strlen(t) + 2;
            char *h = malloc(n);
            if (h) { snprintf(h, n, "#%s", t); da2_set_add(&set, h); free(h); }
        }
        free(t);
    }
    json_t *r = da2_set_array(&set);
    da2_set_free(&set);
    return r;
}

/* PoP: da2_discord_history_backfill_limit @ plugins/platforms/discord/adapter.py:_discord_history_backfill_limit */
static json_t *da2_discord_history_backfill_limit(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *configured = da2_extra_get(ctx, "history_backfill_limit", &err);
    if (err) return err;
    long long value = 0;
    json_t *ierr = NULL;
    if (!da2_is_null(configured)) {
        if (da2_int_of_json(configured, &value, &ierr) == 0)
            return json_number((double)value);
        json_free(ierr); /* except (ValueError, TypeError): pass */
    }
    json_t *a = json_object();
    json_set_hdr(a, "name", "DISCORD_HISTORY_BACKFILL_LIMIT");
    json_set_hdr(a, "default", "50");
    json_t *raw = da2_call(ctx, "_scoped_gate_env", a);
    if (da2_is_raise(raw)) return raw;
    if (da2_int_of_json(raw, &value, &ierr) == 0) {
        json_free(raw);
        return json_number((double)value);
    }
    json_free(ierr);
    json_free(raw);
    return json_number(50.0);
}

/* -- command-sync payload canonicalization -- */

/* PoP: da2_get_discord_command_sync_policy @ plugins/platforms/discord/adapter.py:_get_discord_command_sync_policy */
static json_t *da2_get_discord_command_sync_policy(da2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "name", "DISCORD_COMMAND_SYNC_POLICY");
    json_set_hdr(a, "default", "safe");
    json_t *r = da2_call(ctx, "_scoped_gate_env", a);
    if (da2_is_raise(r)) return r;
    if (!r || r->type != JSON_STRING) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'lower'", da2_type_name(r));
        json_free(r);
        return da2_err("AttributeError", buf);
    }
    char *raw = da2_lower(r->str_val);
    json_free(r);
    /* _DISCORD_COMMAND_SYNC_POLICIES = {"safe", "bulk", "off"} */
    if (strcmp(raw, "safe") == 0 || strcmp(raw, "bulk") == 0 || strcmp(raw, "off") == 0) {
        json_t *ret = json_string(raw);
        free(raw);
        return ret;
    }
    if (raw[0]) {
        json_t *wa = json_object();
        json_set_hdr(wa, "fmt",
            "[%s] Invalid DISCORD_COMMAND_SYNC_POLICY=%r; falling back to 'safe'");
        json_t *aa = json_array();
        const json_t *name = json_obj_get(ctx->state, "name");
        json_append(aa, name ? json_copy(name) : json_string("Discord"));
        json_append(aa, json_string(raw));
        json_set(wa, "args", aa);
        json_t *wr = da2_call(ctx, "logger.warning", wa);
        if (da2_is_raise(wr)) { free(raw); return wr; }
        json_free(wr);
    }
    free(raw);
    return json_string("safe");
}

/* PoP: da2_normalize_permissions @ plugins/platforms/discord/adapter.py:_normalize_permissions */
static json_t *da2_normalize_permissions(da2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *value = json_obj_get(args, "value");
    if (da2_is_null(value)) return json_null();
    char *s = da2_py_str(value);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* int(payload.get(key, def) or def): raise spec on coercion failure. */
static json_t *da2_int_field(const json_t *payload, const char *key, double def, json_t **err) {
    const json_t *v = json_obj_get(payload, key);
    json_t *fallback = json_number(def);
    const json_t *eff = da2_truthy(v) ? v : fallback;
    long long out = 0;
    int k = da2_int_of_json(eff, &out, err);
    json_free(fallback);
    if (k != 0) return NULL;
    return json_number((double)out);
}

/* str(payload.get(key, "") or "") */
static json_t *da2_str_field(const json_t *payload, const char *key) {
    const json_t *v = json_obj_get(payload, key);
    char *s = da2_truthy(v) ? da2_py_str(v) : da2_strdup("");
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* bool(payload.get(key, def)) */
static bool da2_bool_field(const json_t *payload, const char *key, bool def) {
    const json_t *v = json_obj_get(payload, key);
    return v ? da2_truthy(v) : def;
}

/* sorted(int(c) for c in v) over a truthy carrier: array elements, str
 * codepoints, dict keys; other truthy scalars are not iterable. Atomic:
 * any int() failure discards the partial result. */
static json_t *da2_sorted_ints(const json_t *v, json_t **err) {
    *err = NULL;
    json_t *items = json_array();
    if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(v); i++) json_append(items, json_copy(json_get(v, i)));
    } else if (v->type == JSON_STRING) {
        const unsigned char *p = (const unsigned char *)v->str_val;
        while (*p) {
            size_t cl = da2_cp_len(p);
            char *tmp = malloc(cl + 1);
            if (tmp) { memcpy(tmp, p, cl); tmp[cl] = '\0'; json_append(items, json_string(tmp)); free(tmp); }
            p += cl;
        }
    } else if (v->type == JSON_OBJECT) {
        for (size_t i = 0; i < v->c.count; i++) json_append(items, json_string(v->c.keys[i]));
    } else {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object is not iterable", da2_type_name(v));
        *err = da2_err("TypeError", buf);
        json_free(items);
        return NULL;
    }
    size_t n = json_len(items);
    double *vals = n ? malloc(n * sizeof(double)) : NULL;
    for (size_t i = 0; i < n; i++) {
        long long out = 0;
        json_t *ie = NULL;
        if (da2_int_of_json(json_get(items, i), &out, &ie) != 0) {
            *err = ie;
            free(vals);
            json_free(items);
            return NULL;
        }
        vals[i] = (double)out;
    }
    /* insertion sort ascending (small n) */
    for (size_t i = 1; i < n; i++) {
        double x = vals[i];
        size_t j = i;
        while (j > 0 && vals[j - 1] > x) { vals[j] = vals[j - 1]; j--; }
        vals[j] = x;
    }
    json_t *r = json_array();
    for (size_t i = 0; i < n; i++) json_append(r, json_number(vals[i]));
    free(vals);
    json_free(items);
    return r;
}

/* list(payload.get(key, []) or []): array copy, str -> codepoint chars,
 * other truthy -> TypeError not iterable; falsy -> []. */
static json_t *da2_list_field(const json_t *payload, const char *key, json_t **err) {
    *err = NULL;
    const json_t *v = json_obj_get(payload, key);
    if (!da2_truthy(v)) return json_array();
    if (v->type == JSON_ARRAY) return json_copy(v);
    if (v->type == JSON_STRING) {
        json_t *r = json_array();
        const unsigned char *p = (const unsigned char *)v->str_val;
        while (*p) {
            size_t cl = da2_cp_len(p);
            char *tmp = malloc(cl + 1);
            if (tmp) { memcpy(tmp, p, cl); tmp[cl] = '\0'; json_append(r, json_string(tmp)); free(tmp); }
            p += cl;
        }
        return r;
    }
    char buf[160];
    snprintf(buf, sizeof buf, "'%s' object is not iterable", da2_type_name(v));
    *err = da2_err("TypeError", buf);
    return NULL;
}

/* PoP: da2_canonicalize_app_command_option @ plugins/platforms/discord/adapter.py:_canonicalize_app_command_option */
static json_t *da2_canonicalize_app_command_option(da2_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    json_t *err = NULL;
    json_t *type = da2_int_field(payload, "type", 0.0, &err);
    if (err) return err;
    json_t *name = da2_str_field(payload, "name");
    json_t *description = da2_str_field(payload, "description");
    bool required = da2_bool_field(payload, "required", false);
    bool autocomplete = da2_bool_field(payload, "autocomplete", false);
    /* choices: dict items only (str/dict carriers iterate non-dicts -> []). */
    json_t *choices = json_array();
    const json_t *ch = json_obj_get(payload, "choices");
    if (da2_truthy(ch)) {
        if (ch->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(ch); i++) {
                const json_t *choice = json_get(ch, i);
                if (choice && choice->type == JSON_OBJECT) {
                    json_t *c = json_object();
                    json_set(c, "name", da2_str_field(choice, "name"));
                    const json_t *val = json_obj_get(choice, "value");
                    json_set(c, "value", val ? json_copy(val) : json_null());
                    json_append(choices, c);
                }
            }
        } else if (ch->type != JSON_STRING && ch->type != JSON_OBJECT) {
            char buf[160];
            snprintf(buf, sizeof buf, "'%s' object is not iterable", da2_type_name(ch));
            json_free(type); json_free(name); json_free(description); json_free(choices);
            return da2_err("TypeError", buf);
        }
    }
    json_t *channel_types = da2_list_field(payload, "channel_types", &err);
    if (err) {
        json_free(type); json_free(name); json_free(description); json_free(choices);
        return err;
    }
    /* options: recurse directly (target self-recursion is NOT a seam). */
    json_t *options = json_array();
    const json_t *opts = json_obj_get(payload, "options");
    if (da2_truthy(opts)) {
        if (opts->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(opts); i++) {
                const json_t *item = json_get(opts, i);
                if (item && item->type == JSON_OBJECT) {
                    json_t *a = json_object();
                    json_set(a, "payload", json_copy(item));
                    json_t *r = da2_canonicalize_app_command_option(ctx, a);
                    if (da2_is_raise(r)) {
                        json_free(type); json_free(name); json_free(description);
                        json_free(choices); json_free(channel_types); json_free(options);
                        return r;
                    }
                    json_append(options, r);
                }
            }
        } else if (opts->type != JSON_STRING && opts->type != JSON_OBJECT) {
            char buf[160];
            snprintf(buf, sizeof buf, "'%s' object is not iterable", da2_type_name(opts));
            json_free(type); json_free(name); json_free(description);
            json_free(choices); json_free(channel_types); json_free(options);
            return da2_err("TypeError", buf);
        }
    }
    json_t *out = json_object();
    json_set(out, "type", type);
    json_set(out, "name", name);
    json_set(out, "description", description);
    json_set(out, "required", json_bool(required));
    json_set(out, "autocomplete", json_bool(autocomplete));
    json_set(out, "choices", choices);
    json_set(out, "channel_types", channel_types);
    const json_t *mv = json_obj_get(payload, "min_value");
    json_set(out, "min_value", mv ? json_copy(mv) : json_null());
    const json_t *xv = json_obj_get(payload, "max_value");
    json_set(out, "max_value", xv ? json_copy(xv) : json_null());
    const json_t *ml = json_obj_get(payload, "min_length");
    json_set(out, "min_length", ml ? json_copy(ml) : json_null());
    const json_t *xl = json_obj_get(payload, "max_length");
    json_set(out, "max_length", xl ? json_copy(xl) : json_null());
    json_set(out, "options", options);
    return out;
}

/* PoP: da2_canonicalize_app_command_payload @ plugins/platforms/discord/adapter.py:_canonicalize_app_command_payload */
static json_t *da2_canonicalize_app_command_payload(da2_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    json_t *err = NULL;
    json_t *type = da2_int_field(payload, "type", 1.0, &err);
    if (err) return err;
    json_t *name = da2_str_field(payload, "name");
    json_t *description = da2_str_field(payload, "description");
    json_t *pa = json_object();
    const json_t *dmp = json_obj_get(payload, "default_member_permissions");
    json_set(pa, "value", dmp ? json_copy(dmp) : json_null());
    json_t *perms = da2_call(ctx, "_normalize_permissions", pa);
    if (da2_is_raise(perms)) {
        json_free(type); json_free(name); json_free(description);
        return perms;
    }
    bool dm_permission = da2_bool_field(payload, "dm_permission", true);
    bool nsfw = da2_bool_field(payload, "nsfw", false);
    const json_t *contexts = json_obj_get(payload, "contexts");
    json_t *ctxs = da2_truthy(contexts) ? da2_sorted_ints(contexts, &err) : json_null();
    if (err) {
        json_free(type); json_free(name); json_free(description); json_free(perms);
        return err;
    }
    const json_t *itypes = json_obj_get(payload, "integration_types");
    json_t *its = da2_truthy(itypes) ? da2_sorted_ints(itypes, &err) : json_null();
    if (err) {
        json_free(type); json_free(name); json_free(description); json_free(perms); json_free(ctxs);
        return err;
    }
    json_t *options = json_array();
    const json_t *opts = json_obj_get(payload, "options");
    if (da2_truthy(opts)) {
        if (opts->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(opts); i++) {
                const json_t *item = json_get(opts, i);
                if (item && item->type == JSON_OBJECT) {
                    json_t *a = json_object();
                    json_set(a, "payload", json_copy(item));
                    json_t *r = da2_call(ctx, "_canonicalize_app_command_option", a);
                    if (da2_is_raise(r)) {
                        json_free(type); json_free(name); json_free(description);
                        json_free(perms); json_free(ctxs); json_free(its); json_free(options);
                        return r;
                    }
                    json_append(options, r);
                }
            }
        } else if (opts->type != JSON_STRING && opts->type != JSON_OBJECT) {
            char buf[160];
            snprintf(buf, sizeof buf, "'%s' object is not iterable", da2_type_name(opts));
            json_free(type); json_free(name); json_free(description);
            json_free(perms); json_free(ctxs); json_free(its); json_free(options);
            return da2_err("TypeError", buf);
        }
    }
    json_t *out = json_object();
    json_set(out, "type", type);
    json_set(out, "name", name);
    json_set(out, "description", description);
    json_set(out, "default_member_permissions", perms);
    json_set(out, "dm_permission", json_bool(dm_permission));
    json_set(out, "nsfw", json_bool(nsfw));
    json_set(out, "contexts", ctxs);
    json_set(out, "integration_types", its);
    json_set(out, "options", options);
    return out;
}

/* PoP: da2_existing_command_to_payload @ plugins/platforms/discord/adapter.py:_existing_command_to_payload */
static json_t *da2_existing_command_to_payload(da2_ctx_t *ctx, const json_t *args) {
    const json_t *command = json_obj_get(args, "command");
    json_t *r = da2_call(ctx, "to_dict", json_object());
    if (da2_is_raise(r)) return r;
    if (!r || r->type != JSON_OBJECT) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object is not iterable", da2_type_name(r));
        json_free(r);
        return da2_err("TypeError", buf);
    }
    json_t *payload = r; /* dict(command.to_dict()) copy */
    const json_t *nsfw = json_obj_get(command, "nsfw");
    if (!da2_is_null(nsfw)) json_set(payload, "nsfw", json_bool(da2_truthy(nsfw)));
    const json_t *guild_only = json_obj_get(command, "guild_only");
    if (!da2_is_null(guild_only))
        json_set(payload, "dm_permission", json_bool(!da2_truthy(guild_only)));
    const json_t *dmp = json_obj_get(command, "default_member_permissions");
    if (!da2_is_null(dmp)) {
        if (dmp->type == JSON_OBJECT) {
            const json_t *val = json_obj_get(dmp, "value");
            json_set(payload, "default_member_permissions", val ? json_copy(val) : json_copy(dmp));
        } else {
            json_set(payload, "default_member_permissions", json_copy(dmp));
        }
    }
    return payload;
}

/* PoP: da2_patchable_app_command_payload @ plugins/platforms/discord/adapter.py:_patchable_app_command_payload */
static json_t *da2_patchable_app_command_payload(da2_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "payload", json_copy(json_obj_get(args, "payload")));
    json_t *canonical = da2_call(ctx, "_canonicalize_app_command_payload", a);
    if (da2_is_raise(canonical)) return canonical;
    if (!canonical || canonical->type != JSON_OBJECT) {
        char buf[160];
        snprintf(buf, sizeof buf, "'%s' object is not subscriptable", da2_type_name(canonical));
        json_free(canonical);
        return da2_err("TypeError", buf);
    }
    const json_t *name = json_obj_get(canonical, "name");
    if (!name) { json_free(canonical); return da2_err("KeyError", "'name'"); }
    const json_t *description = json_obj_get(canonical, "description");
    if (!description) { json_free(canonical); return da2_err("KeyError", "'description'"); }
    const json_t *options = json_obj_get(canonical, "options");
    if (!options) { json_free(canonical); return da2_err("KeyError", "'options'"); }
    json_t *out = json_object();
    json_set(out, "name", json_copy(name));
    json_set(out, "description", json_copy(description));
    json_set(out, "options", json_copy(options));
    json_free(canonical);
    return out;
}