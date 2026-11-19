#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/cli_output.py: 1 missing functions ── */

/* ── rest_revive1 seam infrastructure (cli_output_gaps3) ────────────────────
 * json_t *fn(co_io_t *io, json_t *in): io->call is the injected I/O seam.
 * co_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception. NULL ==
 * None.
 *
 * line_input seams: "stdin.isatty"/"stdout.isatty" (short-circuit and),
 * "input" (builtin input(prompt)), "import_prompt_toolkit_prompt" and
 * "import_ANSI" (the two from-imports; only ImportError degrades to input),
 * "prompt_toolkit_prompt" (ANSI() is an identity wrapper, documented).
 * KeyboardInterrupt/EOFError from the prompt re-raise; other Exception
 * degrades to input; the BaseException trio propagates everywhere.
 * Production default (no seam channel installed) is fail-closed. */
typedef struct co_io {
    json_t *(*call)(struct co_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} co_io_t;

static int co_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *co_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static int co_truthy(const json_t *v) {
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

static int co_is_import_error(const json_t *r) {
    if (!co_is_raise(r)) return 0;
    const char *k = json_get_str(r, "__raise__", "");
    return strcmp(k, "ImportError") == 0 || strcmp(k, "ModuleNotFoundError") == 0;
}

static json_t *co_seam(co_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return co_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static json_t *co_input(co_io_t *io, const char *prompt) {
    json_t *args = json_object();
    json_set_hdr(args, "prompt", prompt);
    return co_seam(io, "input", args);
}

/* PoP: line_input @ hermes_cli/cli_output.py:line_input */
json_t *line_input(co_io_t *io, json_t *in) {
    const char *prompt = json_get_str(in, "prompt_text", "");
    /* if not (sys.stdin.isatty() and sys.stdout.isatty()): return input(...) */
    json_t *tin = co_seam(io, "stdin.isatty", NULL);
    if (co_is_raise(tin)) return tin;
    int tty = co_truthy(tin);
    json_free(tin);
    if (tty) {
        json_t *tout = co_seam(io, "stdout.isatty", NULL);
        if (co_is_raise(tout)) return tout;
        tty = co_truthy(tout);
        json_free(tout);
    }
    if (!tty) return co_input(io, prompt);
    /* try: from prompt_toolkit import prompt; from ...formatted_text import ANSI
     * except ImportError: return input(...) */
    json_t *imp1 = co_seam(io, "import_prompt_toolkit_prompt", NULL);
    if (co_is_raise(imp1)) {
        if (!co_is_import_error(imp1)) return imp1;
        json_free(imp1);
        return co_input(io, prompt);
    }
    json_free(imp1);
    json_t *imp2 = co_seam(io, "import_ANSI", NULL);
    if (co_is_raise(imp2)) {
        if (!co_is_import_error(imp2)) return imp2;
        json_free(imp2);
        return co_input(io, prompt);
    }
    json_free(imp2);
    /* try: return prompt_toolkit_prompt(ANSI(prompt_text))
     * except (KeyboardInterrupt, EOFError): raise
     * except Exception: return input(...) */
    json_t *args = json_object();
    json_set_hdr(args, "prompt", prompt);
    json_t *r = co_seam(io, "prompt_toolkit_prompt", args);
    if (co_is_raise(r)) {
        const char *k = json_get_str(r, "__raise__", "");
        if (strcmp(k, "KeyboardInterrupt") == 0 || strcmp(k, "EOFError") == 0
            || strcmp(k, "SystemExit") == 0 || strcmp(k, "GeneratorExit") == 0)
            return r;
        json_free(r);
        return co_input(io, prompt);
    }
    return r ? r : json_null();
}
