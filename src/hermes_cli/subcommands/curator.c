/*
 * curator.c -- real C11 port of the declarative half of upstream Hermes
 *   hermes_cli/subcommands/curator.py.
 *
 * PoP: w22_curator_233719_parser_spec @ hermes_cli/subcommands/curator.py:build_curator_parser
 * PoP: w22_curator_233719_wiring_outcome @ hermes_cli/subcommands/curator.py:build_curator_parser
 *
 * add_parser() itself needs an argparse tree and register_cli() mutates that
 * tree, so both stay upstream; the spec the subcommand is declared with and the
 * except-Exception guard around the delegation are ported exactly.
 */
#include "hermes_json.h"
#include "noc_w22_curator_233719.h"

#include <stdlib.h>
#include <string.h>

/* Growable byte buffer: always NUL-terminated, even with nothing appended. */
typedef struct { char *buf; size_t len, cap; } cu233719_buf;

static int cu233719_buf_init(cu233719_buf *b) {
    b->cap = 128;
    b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (!b->buf) { b->cap = 0; return 0; }
    b->buf[0] = '\0';
    return 1;
}

static int cu233719_buf_add(cu233719_buf *b, const char *s) {
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

/* U+2014 EM DASH, spelled in UTF-8 escapes so this source stays ASCII. */
static const char *const CU233719_HELP =
    "Background skill maintenance (curator) \xE2\x80\x94"
    " status, run, pause, pin";

static const char *const CU233719_DESC[] = {
    "The curator is an auxiliary-model background task that ",
    "periodically reviews agent-created skills, prunes stale ones, ",
    "consolidates overlaps, and archives obsolete skills. ",
    "Bundled and hub-installed skills are never touched. ",
    "Archives are recoverable; auto-deletion never happens.",
    NULL
};

json_t *w22_curator_233719_parser_spec(void) {
    cu233719_buf b;
    json_t *out;
    int i;

    if (!cu233719_buf_init(&b)) return json_null();
    for (i = 0; CU233719_DESC[i]; i++) {
        if (!cu233719_buf_add(&b, CU233719_DESC[i])) { free(b.buf); return json_null(); }
    }
    out = json_object();
    json_set(out, "name", json_string("curator"));
    json_set(out, "aliases", json_array());
    json_set(out, "help", json_string(CU233719_HELP));
    json_set(out, "description", json_string(b.buf));
    free(b.buf);
    return out;
}

/* Class names that are BaseException but not Exception: `except Exception` misses them. */
static int cu233719_escapes(const char *type) {
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

json_t *w22_curator_233719_wiring_outcome(const json_t *exc) {
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

    if (cu233719_escapes(type)) {
        out = json_object();
        json_set(out, "wired", json_bool(0));
        json_set(out, "propagated", json_string(type));
        json_set(out, "log", json_null());
        return out;
    }

    out = json_object();
    json_set(out, "wired", json_bool(0));
    {
        cu233719_buf b;
        json_t *log = json_object();
        if (cu233719_buf_init(&b)) {
            cu233719_buf_add(&b, "curator CLI wiring failed: ");
            cu233719_buf_add(&b, message);
            json_set(log, "message", json_string(b.buf));
            free(b.buf);
        } else {
            json_set(log, "message", json_string("curator CLI wiring failed: "));
        }
        json_set(log, "logger", json_string("hermes_cli.main"));
        json_set(log, "level", json_string("DEBUG"));
        json_set(out, "log", log);
    }
    return out;
}
