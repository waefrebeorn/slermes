/*
 * journey.c -- real C11 port of the declarative half of upstream Hermes
 *   hermes_cli/subcommands/journey.py.
 *
 * PoP: w22_journey_b8f31d_parser_spec @ hermes_cli/subcommands/journey.py:build_journey_parser
 * PoP: w22_journey_b8f31d_wiring_outcome @ hermes_cli/subcommands/journey.py:build_journey_parser
 *
 * add_parser() needs an argparse tree and register_cli() mutates that tree, so
 * both stay upstream; the spec the subcommand is declared with (including its
 * learning / memory-graph aliases) and the except-Exception guard are ported.
 */
#include "hermes_json.h"
#include "noc_w22_journey_b8f31d.h"

#include <stdlib.h>
#include <string.h>

/* Growable byte buffer: always NUL-terminated, even with nothing appended. */
typedef struct { char *buf; size_t len, cap; } jo8f31d_buf;

static int jo8f31d_buf_init(jo8f31d_buf *b) {
    b->cap = 128;
    b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (!b->buf) { b->cap = 0; return 0; }
    b->buf[0] = '\0';
    return 1;
}

static int jo8f31d_buf_add(jo8f31d_buf *b, const char *s) {
    size_t n = s ? strlen(s) : 0;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *grown;
        while (b->len + n + 1 > cap) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) return 0;
        b->buf = grown;
        b->cap = cap;
    }
    if (n) memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return 1;
}

static const char *const JO8F31D_HELP = "Timeline of learned skills + memories over time";

static const char *const JO8F31D_DESC[] = {
    "A terminal rendition of the desktop Star Map / Memory Graph: a ",
    "timeline bar chart of learned skills and memories over time ",
    "(oldest at top, newest at bottom) plus a playable constellation ",
    "scrubber. Mirrors the TUI `/journey` overlay and the desktop panel.",
    NULL
};

json_t *w22_journey_b8f31d_parser_spec(void) {
    jo8f31d_buf b;
    json_t *out, *aliases;
    int i;

    if (!jo8f31d_buf_init(&b)) return json_null();
    for (i = 0; JO8F31D_DESC[i]; i++) {
        if (!jo8f31d_buf_add(&b, JO8F31D_DESC[i])) { free(b.buf); return json_null(); }
    }
    aliases = json_array();
    json_append(aliases, json_string("learning"));
    json_append(aliases, json_string("memory-graph"));

    out = json_object();
    json_set(out, "name", json_string("journey"));
    json_set(out, "aliases", aliases);
    json_set(out, "help", json_string(JO8F31D_HELP));
    json_set(out, "description", json_string(b.buf));
    free(b.buf);
    return out;
}

/* Class names that are BaseException but not Exception: `except Exception` misses them. */
static int jo8f31d_escapes(const char *type) {
    static const char *const NAMES[] = {
        "BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit", NULL
    };
    int i;
    if (!type) return 0;
    for (i = 0; NAMES[i]; i++) {
        if (!strcmp(type, NAMES[i])) return 1;
    }
    return 0;
}

json_t *w22_journey_b8f31d_wiring_outcome(const json_t *exc) {
    const char *type, *message;
    json_t *out;

    /* No exception object: register_cli returned. */
    if (!exc || exc->type != JSON_OBJECT) {
        out = json_object();
        json_set(out, "wired", json_bool(1));
        json_set(out, "log", json_null());
        return out;
    }

    type = json_get_str(exc, "type", "Exception");
    message = json_get_str(exc, "message", "");

    if (jo8f31d_escapes(type)) {
        out = json_object();
        json_set(out, "wired", json_bool(0));
        json_set(out, "propagated", json_string(type));
        json_set(out, "log", json_null());
        return out;
    }

    out = json_object();
    json_set(out, "wired", json_bool(0));
    {
        jo8f31d_buf b;
        json_t *log = json_object();
        if (jo8f31d_buf_init(&b)) {
            jo8f31d_buf_add(&b, "journey CLI wiring failed: ");
            jo8f31d_buf_add(&b, message);
            json_set(log, "message", json_string(b.buf));
            free(b.buf);
        } else {
            json_set(log, "message", json_string("journey CLI wiring failed: "));
        }
        json_set(log, "logger", json_string("hermes_cli.main"));
        json_set(log, "level", json_string("DEBUG"));
        json_set(out, "log", log);
    }
    return out;
}
