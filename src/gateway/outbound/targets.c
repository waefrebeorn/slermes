/*
 * targets.c -- real C11 port of upstream Hermes gateway/outbound/targets.py.
 *
 * PoP: w21_targets_e73d84_parse_ref @ gateway/outbound/targets.py:_parse_target_ref
 * PoP: w21_targets_e73d84_resolve @ gateway/outbound/targets.py:resolve_send_target
 * PoP: w21_targets_e73d84_unknown_platform_error @ gateway/outbound/targets.py:unknown_platform_error
 * PoP: w21_targets_e73d84_env_probe @ gateway/outbound/targets.py:resolve_send_target
 *
 * Both functions are pure policy over the plugin registry: the parser, the
 * validator, Platform membership and the channel-directory lookup are the seams
 * and arrive as `world`. The ordering that makes this module matter is here --
 * a plugin parser outranks the generic id rules, NOT_EXPLICIT sends the target
 * straight to the directory, a plugin with a parser stays strict even under
 * pass_unresolved_references, and the error text names exactly which of the
 * three hints applies.
 */
#include "hermes_json.h"
#include "noc_w21_targets_e73d84.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TG_BUF 2048

static const char *const TG_PHONE_PLATFORMS[] = {"photon", "signal", "sms", "whatsapp", NULL};

static int tg_streq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static int tg_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int tg_isdigit(char c) { return c >= '0' && c <= '9'; }

/* re.fullmatch(r"^\s*\+(\d{7,15})\s*$", target_ref) */
static int tg_e164(const char *s) {
    const char *p = s;
    int digits = 0;
    if (!p) return 0;
    while (tg_ws(*p)) p++;
    if (*p != '+') return 0;
    p++;
    while (tg_isdigit(*p)) { digits++; p++; }
    if (digits < 7 || digits > 15) return 0;
    while (tg_ws(*p)) p++;
    return *p == '\0';
}

/* target_ref.lstrip("-").isdigit(): at least one digit, all digits. */
static int tg_bare_numeric(const char *s) {
    int any = 0;
    if (!s) return 0;
    while (*s == '-') s++;
    for (; *s; s++) {
        if (!tg_isdigit(*s)) return 0;
        any = 1;
    }
    return any;
}

/* str.strip() for the E.164 result. */
static void tg_strip(const char *s, char *out, size_t cap) {
    const char *end;
    size_t n;
    if (!s) s = "";
    while (tg_ws(*s)) s++;
    end = s + strlen(s);
    while (end > s && tg_ws(end[-1])) end--;
    n = (size_t)(end - s);
    if (n >= cap) n = cap - 1;
    memcpy(out, s, n);
    out[n] = '\0';                       /* NUL-terminate after shortening */
}

static json_t *tg_triple(json_t *chat, json_t *thread, json_t *third) {
    json_t *arr = json_array();
    json_append(arr, chat ? chat : json_null());
    json_append(arr, thread ? thread : json_null());
    json_append(arr, third ? third : json_null());
    return arr;
}

/* --- _target_syntax_fn's result, as the fixture encodes it ------------------ */

enum { TG_NO_PARSER = 0, TG_NOT_EXPLICIT, TG_PARSED_NONE, TG_PARSED_TUPLE };

static int tg_parser_kind(const json_t *parser, const json_t **value) {
    const char *kind;
    if (value) *value = parser;
    if (!parser || parser->type == JSON_NULL) return TG_NO_PARSER;
    if (parser->type != JSON_OBJECT) return TG_NO_PARSER;
    kind = json_get_str(parser, "kind", "");
    if (!strcmp(kind, "not_explicit")) return TG_NOT_EXPLICIT;
    if (!strcmp(kind, "tuple")) return TG_PARSED_TUPLE;
    return TG_PARSED_NONE;
}

/* (chat_id, thread_id, explicit) from _parse_target_ref. */
static void tg_parse_core(const char *platform_name, const char *target_ref,
                          const json_t *parser, const char **chat, const char **thread,
                          int *explicit_out)
{
    const json_t *pv = NULL;
    int kind = tg_parser_kind(parser, &pv);
    const char *p = target_ref ? target_ref : "";

    *chat = NULL;
    *thread = NULL;
    *explicit_out = 0;

    if (kind == TG_NOT_EXPLICIT) return;            /* NOT_EXPLICIT: not explicit */
    if (kind == TG_PARSED_TUPLE) {
        *chat = json_get_str(pv, "chat", NULL);
        *thread = json_get_str(pv, "thread", NULL);
        *explicit_out = 1;
        return;
    }
    if (kind == TG_PARSED_NONE) {
        /* parser returned None: fall through to the generic id rules */
    }

    if (platform_name) {
        int i;
        for (i = 0; TG_PHONE_PLATFORMS[i]; i++) {
            if (tg_streq(platform_name, TG_PHONE_PLATFORMS[i]) && tg_e164(p)) {
                static char stripped[TG_BUF];
                tg_strip(p, stripped, sizeof(stripped));
                *chat = stripped;
                *explicit_out = 1;
                return;
            }
        }
    }
    if (tg_bare_numeric(p)
        || (tg_streq(platform_name, "matrix") && (p[0] == '!' || p[0] == '@'))
        || (tg_streq(platform_name, "xmpp") && strchr(p, '@') != NULL)) {
        *chat = p;
        *explicit_out = 1;
        return;
    }
}

json_t *w21_targets_e73d84_parse_ref(const char *platform_name, const char *target_ref,
                                     const json_t *parser) {
    const char *chat, *thread;
    int explicit_flag;

    tg_parse_core(platform_name, target_ref, parser, &chat, &thread, &explicit_flag);
    return tg_triple(chat ? json_string(chat) : NULL,
                     thread ? json_string(thread) : NULL,
                     json_bool(explicit_flag ? 1 : 0));
}

json_t *w21_targets_e73d84_unknown_platform_error(const char *platform_name,
                                                  const char *install_hint) {
    char buf[TG_BUF];
    const char *p = platform_name ? platform_name : "";
    const char *hint = install_hint ? install_hint : "";
    snprintf(buf, sizeof(buf), "Unknown or unregistered plugin platform: %s%s", p, hint);
    return json_string(buf);
}

/* _validated(chat_id, thread_id): the plugin validator has the final say. */
static json_t *tg_validated(const char *platform_name, const char *target_ref,
                            const json_t *entry, const char *chat, const char *thread)
{
    const char *verdict;
    char msg[TG_BUF];

    if (!entry || entry->type != JSON_OBJECT || !json_has(entry, "validate")) {
        return tg_triple(chat ? json_string(chat) : NULL,
                         thread ? json_string(thread) : NULL, NULL);
    }
    verdict = json_get_str(entry, "validate", "none");
    if (!strcmp(verdict, "none")) {
        return tg_triple(chat ? json_string(chat) : NULL,
                         thread ? json_string(thread) : NULL, NULL);
    }
    if (!strcmp(verdict, "raises")) {
        snprintf(msg, sizeof(msg), "Target validator failed for platform '%s'",
                 platform_name ? platform_name : "");
        return tg_triple(NULL, NULL, json_string(msg));
    }
    if (!strcmp(verdict, "true")) {
        return tg_triple(chat ? json_string(chat) : NULL,
                         thread ? json_string(thread) : NULL, NULL);
    }
    if (!strncmp(verdict, "str:", 4) && verdict[4]) {
        snprintf(msg, sizeof(msg), "Invalid target '%s' on %s: %s",
                 target_ref ? target_ref : "", platform_name ? platform_name : "", verdict + 4);
    } else {
        /* a False (or any other non-True) verdict: no detail */
        snprintf(msg, sizeof(msg), "Invalid target '%s' on %s",
                 target_ref ? target_ref : "", platform_name ? platform_name : "");
    }
    return tg_triple(NULL, NULL, json_string(msg));
}

json_t *w21_targets_e73d84_resolve(const char *platform_name, const char *target_ref,
                                   int pass_unresolved, const json_t *world) {
    const json_t *entry = NULL;
    const char *platform = platform_name ? platform_name : "";
    const char *ref = target_ref ? target_ref : "";
    const char *install_hint;

    if (!world || world->type != JSON_OBJECT) world = NULL;
    if (world) {
        json_t *e = json_obj_get(world, "entry");
        if (e && e->type == JSON_OBJECT) entry = e;
    }
    install_hint = world ? json_get_str(world, "install_hint", "") : "";

    /* if entry is not None and entry.parse_target_ref_fn is not None: */
    if (entry) {
        const json_t *parse = json_obj_get(entry, "parse");
        const char *kind = (parse && parse->type == JSON_OBJECT)
                          ? json_get_str(parse, "kind", "") : "";
        char msg[TG_BUF];
        if (!strcmp(kind, "raises")) {
            snprintf(msg, sizeof(msg), "Target parser failed for platform '%s'", platform);
            return tg_triple(NULL, NULL, json_string(msg));
        }
        if (!strcmp(kind, "invalid") || !strcmp(kind, "not_explicit")) {
            /* a plugin parse_target_ref_fn returning the NOT_EXPLICIT sentinel
             * is not a (chat, thread) pair, so the live shape check rejects it. */
            snprintf(msg, sizeof(msg),
                     "Target parser for platform '%s' returned an invalid result", platform);
            return tg_triple(NULL, NULL, json_string(msg));
        }
        if (!strcmp(kind, "tuple")) {
            const char *chat = json_get_str(parse, "chat", NULL);
            const char *thread = json_get_str(parse, "thread", NULL);
            /* the tuple-shape check the live code performs */
            if (!chat || !chat[0]) {
                snprintf(msg, sizeof(msg),
                         "Target parser for platform '%s' returned an invalid result", platform);
                return tg_triple(NULL, NULL, json_string(msg));
            }
            return tg_validated(platform, ref, entry, chat, thread);
        }
        /* "none" and "not_explicit" fall through to _parse_target_ref's rules */
    }

    {
        const json_t *parse = entry ? json_obj_get(entry, "parse") : NULL;
        const char *chat, *thread;
        int explicit_flag;
        tg_parse_core(platform, ref, parse, &chat, &thread, &explicit_flag);
        if (explicit_flag && chat) {
            return tg_validated(platform, ref, entry, chat, thread);
        }
    }

    /* resolved = resolve_channel_name(platform_name, target_ref) */
    if (world && !json_get_bool(world, "resolve_raises", false)) {
        const char *resolved = json_get_str(world, "resolve", NULL);
        if (resolved && resolved[0]) {
            const json_t *parse = entry ? json_obj_get(entry, "parse") : NULL;
            const char *chat, *thread;
            int explicit_flag;
            tg_parse_core(platform, resolved, parse, &chat, &thread, &explicit_flag);
            if (chat && chat[0]) return tg_validated(platform, ref, entry, chat, thread);
            return tg_validated(platform, ref, entry, resolved, thread);
        }
    }

    {
        int is_builtin = world ? json_get_bool(world, "builtin", false) : 0;
        int is_plugin;
        char msg[TG_BUF];

        /* if entry is None and not is_builtin: return unknown_platform_error(...) */
        if (!entry && !is_builtin) {
            snprintf(msg, sizeof(msg), "Unknown or unregistered plugin platform: %s%s",
                     platform, install_hint);
            return tg_triple(NULL, NULL, json_string(msg));
        }
        is_plugin = entry && tg_streq(json_get_str(entry, "source", ""), "plugin") && !is_builtin;
        if (pass_unresolved) {
            int parse_kind = entry ? tg_parser_kind(json_obj_get(entry, "parse"), NULL)
                                   : TG_NO_PARSER;
            if (!is_plugin || parse_kind == TG_NO_PARSER) {
                /* hand the raw target to the adapter unchanged (it validates) */
                return tg_validated(platform, ref, entry, ref, NULL);
            }
        }
        if (is_plugin) {
            snprintf(msg, sizeof(msg),
                     "Could not resolve '%s' on %s. The plugin parser did not recognize it "
                     "and no channel-directory entry matched.", ref, platform);
        } else if (world && json_get_bool(world, "resolve_raises", false)) {
            snprintf(msg, sizeof(msg),
                     "Could not resolve '%s' on %s. Try using a numeric channel ID instead.",
                     ref, platform);
        } else {
            snprintf(msg, sizeof(msg),
                     "Could not resolve '%s' on %s. Run `hermes send --list` to see available "
                     "targets.", ref, platform);
        }
        return tg_triple(NULL, NULL, json_string(msg));
    }
}

json_t *w21_targets_e73d84_env_probe(void) {
    const char *sler = getenv("SLERMES_HOME");
    const char *home = getenv("HOME");
    json_t *out = json_object();
    json_set(out, "sler", json_bool(sler && sler[0]));
    json_set(out, "home", json_bool(home && home[0]));
    json_set(out, "same", json_bool(sler && home && !strcmp(sler, home)));
    return out;
}
