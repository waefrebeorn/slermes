/*
 * verify.c -- real C11 port of the `hermes verify` subcommand parser, upstream
 * hermes_cli/subcommands/verify.py.
 *
 * PoP: w21_verify_824376_spec @ hermes_cli/subcommands/verify.py:build_verify_parser
 * PoP: w21_verify_824376_parse @ hermes_cli/subcommands/verify.py:build_verify_parser
 *
 * The live module's only artifact is the argparse surface it attaches, so this
 * port carries that surface as data (order, nargs, default, type, action,
 * choices and help strings, with the computed "(default: 600)" text) and a
 * parser that honours it. `add_json_flag` is ported inline (it only adds
 * `--json`), and the two timeout defaults are the module's own constants.
 *
 * ASCII source: the module carries no literal text.
 */
#include "hermes_json.h"
#include "noc_w21_verify_824376.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VR_DEFAULT_PHASE_TIMEOUT 600.0
#define VR_DEFAULT_READY_TIMEOUT 60.0

enum { VR_STORE_TRUE, VR_APPEND, VR_INT, VR_FLOAT };

typedef struct {
    const char *dest;      /* argparse dest */
    const char *flag;      /* option string, NULL for the positional */
    int kind;              /* store_true / append / int / float */
    const char *help;
    const char *choices[5];  /* NULL-terminated when non-empty */
} vr_arg;

static const vr_arg VR_ARGS[] = {
    {"path", NULL, VR_FLOAT, "Project root to verify (default: current directory)", {NULL}},
    {"detect_only", "--detect-only", VR_STORE_TRUE,
     "Only detect and print the recipe as JSON; run nothing", {NULL}},
    {"save", "--save", VR_STORE_TRUE,
     "Save the recipe as .hermes/environment.json in the project", {NULL}},
    {"skip_start", "--skip-start", VR_STORE_TRUE,
     "Run command phases but skip starting the app / readiness poll", {NULL}},
    {"phase", "--phase", VR_APPEND, "Run only the given phase(s); repeatable",
     {"bootstrap", "build", "test", "start", NULL}},
    {"port", "--port", VR_INT, "Override the port used for the readiness poll", {NULL}},
    {"timeout", "--timeout", VR_FLOAT, "Per-phase timeout in seconds (default: 600)", {NULL}},
    {"ready_timeout", "--ready-timeout", VR_FLOAT,
     "Readiness poll timeout in seconds (default: 60)", {NULL}},
    {"json", "--json", VR_STORE_TRUE, "Emit a machine-readable JSON result", {NULL}},
};

#define VR_NARGS ((int)(sizeof(VR_ARGS) / sizeof(VR_ARGS[0])))

static const char *VR_HELP = "Detect a project's run recipe and smoke-test it";
static const char *VR_DESCRIPTION =
    "Detect how the current project is built, tested, and started "
    "(or load the saved manifest at .hermes/environment.json), then "
    "run a verification pass: bootstrap -> build -> test -> start in "
    "background -> poll readiness -> teardown.";

static json_t *vr_str_or_null(const char *s) {
    return s ? json_string(s) : json_null();
}

json_t *w21_verify_824376_spec(void) {
    json_t *spec = json_object(), *args = json_array(), *defaults = json_object();
    int i;

    json_set(spec, "name", json_string("verify"));
    json_set(spec, "help", json_string(VR_HELP));
    json_set(spec, "description", json_string(VR_DESCRIPTION));
    for (i = 0; i < VR_NARGS; i++) {
        const vr_arg *a = &VR_ARGS[i];
        json_t *entry = json_object();
        json_t *flags = json_array();
        json_t *choices = json_null();
        json_append(flags, json_string(a->flag ? a->flag : a->dest));
        json_set(entry, "name", json_string(a->dest));
        json_set(entry, "flags", flags);
        if (!a->flag) json_set(entry, "nargs", json_string("?"));
        else if (a->kind == VR_STORE_TRUE) json_set(entry, "nargs", json_number(0));
        else json_set(entry, "nargs", json_null());
        if (!a->flag) {
            json_set(entry, "default", json_null());
        } else if (a->kind == VR_STORE_TRUE) {
            json_set(entry, "default", json_bool(0));   /* argparse: store_true -> False */
        } else if (!strcmp(a->dest, "timeout")) {
            json_set(entry, "default", json_number(VR_DEFAULT_PHASE_TIMEOUT));
        } else if (!strcmp(a->dest, "ready_timeout")) {
            json_set(entry, "default", json_number(VR_DEFAULT_READY_TIMEOUT));
        } else {
            json_set(entry, "default", json_null());
        }
        if (a->kind == VR_INT) json_set(entry, "type", json_string("int"));
        else if (a->kind == VR_FLOAT && a->flag) json_set(entry, "type", json_string("float"));
        else json_set(entry, "type", json_null());
        if (a->kind == VR_STORE_TRUE) json_set(entry, "action", json_string("store_true"));
        else if (a->kind == VR_APPEND) json_set(entry, "action", json_string("append"));
        else json_set(entry, "action", json_string("store"));
        if (a->choices[0]) {
            int c;
            choices = json_array();
            for (c = 0; a->choices[c]; c++) json_append(choices, json_string(a->choices[c]));
        }
        json_set(entry, "choices", choices);
        json_set(entry, "help", json_string(a->help));
        json_append(args, entry);
    }
    json_set(spec, "arguments", args);
    json_set(defaults, "func", json_string("cmd_verify"));
    json_set(spec, "defaults", defaults);
    return spec;
}

/* --- the parser --------------------------------------------------------- */

static int vr_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* int(text) for base 10 with Python's underscore rule. */
static int vr_parse_int(const char *text, long long *out) {
    const char *p = text;
    int neg = 0, digits = 0, after_digit = 0;
    long long acc = 0;
    if (!p) return 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) return 0;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            int d = *p - '0';
            if (acc > (9223372036854775807LL - d) / 10) return 0;
            acc = acc * 10 + d;
            digits++;
            after_digit = 1;
        } else if (*p == '_') {
            if (!after_digit || !(p[1] >= '0' && p[1] <= '9')) return 0;
            after_digit = 0;
        } else {
            return 0;
        }
        p++;
    }
    if (!digits) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

/* float(text): Python's grammar, no hex, whole string consumed. */
static int vr_parse_float(const char *text, double *out) {
    const char *p = text;
    char *endp = NULL;
    double value;
    if (!p) return 0;
    while (vr_is_space(*p)) p++;
    if (!*p) return 0;
    if (strchr(p, 'x') || strchr(p, 'X')) return 0;   /* Python rejects hex floats */
    value = strtod(p, &endp);
    if (!endp || endp == p) return 0;
    while (vr_is_space(*endp)) endp++;
    if (*endp) return 0;
    *out = value;
    return 1;
}

static int vr_choice_ok(const vr_arg *a, const char *value) {
    int i;
    if (!a->choices[0]) return 1;
    for (i = 0; a->choices[i]; i++) {
        if (!strcmp(a->choices[i], value)) return 1;
    }
    return 0;
}

static json_t *vr_error(void) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "code", json_number(2));
    return out;
}

json_t *w21_verify_824376_parse(const json_t *argv) {
    json_t *values = json_object();
    int have_path = 0, i, terminated = 0;
    size_t n = (argv && argv->type == JSON_ARRAY) ? json_len(argv) : 0;
    size_t idx;

    for (i = 0; i < VR_NARGS; i++) {
        const vr_arg *a = &VR_ARGS[i];
        if (!strcmp(a->dest, "timeout")) json_set(values, a->dest, json_number(VR_DEFAULT_PHASE_TIMEOUT));
        else if (!strcmp(a->dest, "ready_timeout")) json_set(values, a->dest, json_number(VR_DEFAULT_READY_TIMEOUT));
        else if (!strcmp(a->dest, "port") || !strcmp(a->dest, "path") || !strcmp(a->dest, "phase"))
            json_set(values, a->dest, json_null());
        else json_set(values, a->dest, json_bool(0));
    }

    for (idx = 0; idx < n; idx++) {
        const json_t *item = json_get(argv, idx);
        const char *token = (item && item->type == JSON_STRING) ? item->str_val : NULL;
        char name[128];
        const char *inline_value = NULL;
        const vr_arg *match = NULL;
        const char *eq;
        size_t tlen;

        if (!token) return vr_error();     /* argparse wants strings only */
        if (terminated) {
            if (have_path) return vr_error();
            have_path = 1;
            json_set(values, "path", json_string(token));
            continue;
        }
        if (!strcmp(token, "--")) { terminated = 1; continue; }
        /* argparse treats a negative-number-looking token as a positional (no
         * option of this parser looks like a negative number). */
        if (token[0] == '-' && (isdigit((unsigned char)token[1]) ||
                                (token[1] == '.' && isdigit((unsigned char)token[2])))) {
            if (have_path) return vr_error();
            have_path = 1;
            json_set(values, "path", json_string(token));
            continue;
        }
        if (token[0] != '-' || !token[1]) {
            if (have_path) return vr_error();   /* only one positional slot */
            have_path = 1;
            json_set(values, "path", json_string(token));
            continue;
        }
        tlen = strlen(token);
        eq = strchr(token, '=');
        if (eq) {
            size_t nlen = (size_t)(eq - token);
            if (nlen >= sizeof(name)) return vr_error();
            memcpy(name, token, nlen);
            name[nlen] = '\0';
            inline_value = eq + 1;
        } else {
            if (tlen >= sizeof(name)) return vr_error();
            memcpy(name, token, tlen + 1);
        }
        for (i = 0; i < VR_NARGS; i++) {
            if (VR_ARGS[i].flag && !strcmp(VR_ARGS[i].flag, name)) { match = &VR_ARGS[i]; break; }
        }
        if (!match) return vr_error();          /* unrecognised option */
        if (match->kind == VR_STORE_TRUE) {
            if (inline_value) return vr_error();   /* ignored explicit argument */
            json_set(values, match->dest, json_bool(1));
            continue;
        }
        {
            const char *value = inline_value;
            if (!value) {
                idx++;
                if (idx >= n) return vr_error();   /* expected one argument */
                {
                    const json_t *next = json_get(argv, idx);
                    if (!next || next->type != JSON_STRING) return vr_error();
                    value = next->str_val;
                }
            }
            if (match->kind == VR_APPEND) {
                json_t *list = json_obj_get(values, "phase");
                if (!vr_choice_ok(match, value)) return vr_error();
                if (!list || list->type != JSON_ARRAY) {
                    json_set(values, "phase", json_array());
                    list = json_obj_get(values, "phase");
                }
                json_append(list, json_string(value));
            } else if (match->kind == VR_INT) {
                long long parsed;
                if (!vr_parse_int(value, &parsed)) return vr_error();
                json_set(values, match->dest, json_number((double)parsed));
            } else {
                double parsed;
                if (!vr_parse_float(value, &parsed)) return vr_error();
                json_set(values, match->dest, json_number(parsed));
            }
        }
    }
    {
        json_t *out = json_object();
        json_set(out, "status", json_string("ok"));
        json_set(out, "value", values);
        return out;
    }
}
