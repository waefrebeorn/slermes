/*
 * pets.c -- real C11 port of upstream Hermes hermes_cli/subcommands/pets.py.
 *
 * PoP: w22_pets_b45cc9_spec @ hermes_cli/subcommands/pets.py:build_pets_parser
 * PoP: w22_pets_b45cc9_build @ hermes_cli/subcommands/pets.py:build_pets_parser
 *
 * The declaration is data (three verbatim strings); the guarded wiring is the
 * try/except that keeps a broken hermes_cli.pets from taking the CLI down.
 * argparse and the pets module are the host seam: the port answers what the
 * host is handed and what the one debug record says, which is the module's
 * whole observable behavior.
 */
#include "hermes_json.h"
#include "noc_w22_pets_b45cc9.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const PETS_NAME = "pets";
static const char *const PETS_HELP = "Browse, install, and select petdex animated pets";
static const char *const PETS_DESCRIPTION =
    "Petdex (https://github.com/crafter-station/petdex) is a public "
    "gallery of animated sprite pets for coding agents. Install one "
    "and Hermes shows it reacting to agent activity across the CLI, "
    "TUI, and desktop app.";
static const char *const PETS_LOG_PREFIX = "pets CLI wiring failed: ";

json_t *w22_pets_b45cc9_spec(void) {
    json_t *out = json_object();
    json_set(out, "name", json_string(PETS_NAME));
    json_set(out, "help", json_string(PETS_HELP));
    json_set(out, "description", json_string(PETS_DESCRIPTION));
    return out;
}

/* str(value) for the scalar exception messages the guard can carry. */
static char *pets_str(const json_t *v) {
    char buf[64];
    const char *src = "";
    size_t n;
    char *out;
    if (v) {
        switch (v->type) {
        case JSON_STRING: src = v->str_val ? v->str_val : ""; break;
        case JSON_BOOL:   src = v->bool_val ? "True" : "False"; break;
        case JSON_NUMBER:
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            src = buf;
            break;
        default: src = ""; break;
        }
    }
    n = strlen(src);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, src, n + 1);
    return out;
}

json_t *w22_pets_b45cc9_build(const json_t *req) {
    json_t *out = w22_pets_b45cc9_spec();
    const char *register_mode = "ok";
    json_t *message;
    int failed;
    char *text;

    if (req && req->type == JSON_OBJECT) {
        register_mode = json_get_str(req, "register", "ok");
        message = json_obj_get((json_t *)req, "message");
    } else {
        message = NULL;
    }
    failed = register_mode && !strcmp(register_mode, "error");
    json_set(out, "registered", json_bool(failed ? 0 : 1));

    if (!failed) {
        json_set(out, "log", json_null());
        return out;
    }
    text = pets_str(message);
    if (text) {
        size_t need = strlen(PETS_LOG_PREFIX) + strlen(text) + 1;
        char *line = (char *)malloc(need);
        if (line) {
            snprintf(line, need, "%s%s", PETS_LOG_PREFIX, text);
            json_set(out, "log", json_string(line));
            free(line);
        } else {
            json_set(out, "log", json_null());
        }
        free(text);
    } else {
        json_set(out, "log", json_null());
    }
    return out;
}
