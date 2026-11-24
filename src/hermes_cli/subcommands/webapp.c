/*
 * webapp.c -- real C11 port of hermes_cli/subcommands/webapp.py.
 *
 * PoP: w22_webapp_0bcaf2_spec @ hermes_cli/subcommands/webapp.py:build_webapp_parser
 * PoP: w22_webapp_0bcaf2_parse @ hermes_cli/subcommands/webapp.py:build_webapp_parser
 *
 * The Python function only registers an argparse subcommand; the work it does
 * is the parser contract. That contract (options, dests, help text, the
 * mutually exclusive trio, set_defaults(func=...)) is returned as data by
 * _spec, and _parse applies the argparse rules the contract implies: prefix
 * abbreviations, repeated flags, unknown positionals, ambiguous shortenings,
 * `--flag=value`, and the mutually-exclusive conflict message. The handler
 * itself (cmd_webapp) is injected by the caller both upstream and here.
 */
#include "hermes_json.h"
#include "noc_w22_webapp_0bcaf2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WA_WEBAPP_DESCRIPTION \
    "Open the Hermes Desktop workspace in a browser. The gateway serves it when " \
    "webapp.enabled is true in config.yaml; this command builds the bundle when needed, " \
    "starts the gateway and opens a one-use sign-in link."

typedef struct {
    const char *flag;
    const char *dest;
    const char *help;
    int exclusive;
} wa_option;

/* Registration order == argparse action order == conflict message order. */
static const wa_option WA_OPTIONS[] = {
    {"--no-open", "no_open",
     "Print the one-use link instead of opening a browser", 0},
    {"--force-build", "force_build",
     "Rebuild the Webapp bundle even when its build receipt is current", 0},
    {"--skip-build", "skip_build",
     "Use the existing Webapp bundle as is", 0},
    {"--build-only", "build_only",
     "Build the Webapp bundle and stop", 1},
    {"--status", "status",
     "Show whether the gateway is serving the Webapp", 1},
    {"--sign-out", "sign_out",
     "Sign out every Webapp tab (each needs a new link from hermes webapp)", 1},
};

#define WA_OPTION_COUNT ((int)(sizeof(WA_OPTIONS) / sizeof(WA_OPTIONS[0])))

json_t *w22_webapp_0bcaf2_spec(void) {
    json_t *spec = json_object();
    json_t *options = json_array();
    json_t *exclusive = json_array();
    json_t *group = json_array();
    int i;

    json_set(spec, "prog_suffix", json_string("webapp"));
    json_set(spec, "description", json_string(WA_WEBAPP_DESCRIPTION));
    for (i = 0; i < WA_OPTION_COUNT; i++) {
        json_t *opt = json_object();
        json_t *flags = json_array();
        json_append(flags, json_string(WA_OPTIONS[i].flag));
        json_set(opt, "flags", flags);
        json_set(opt, "dest", json_string(WA_OPTIONS[i].dest));
        json_set(opt, "action", json_string("store_true"));
        json_set(opt, "default", json_bool(0));
        json_set(opt, "help", json_string(WA_OPTIONS[i].help));
        json_append(options, opt);
        if (WA_OPTIONS[i].exclusive) json_append(group, json_string(WA_OPTIONS[i].flag));
    }
    json_append(exclusive, group);
    json_set(spec, "options", options);
    json_set(spec, "exclusive", exclusive);
    json_set(spec, "func", json_string("cmd_webapp"));
    return spec;
}

static json_t *wa_error(const char *text, int exit_code) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(text ? text : ""));
    json_set(out, "exit", json_number((double)exit_code));
    return out;
}

/* Number of registered flags the token abbreviates (`--s`). */
static int wa_prefix_matches(const char *token, int *index) {
    size_t n = strlen(token);
    int i, hits = 0;
    for (i = 0; i < WA_OPTION_COUNT; i++) {
        if (!strncmp(WA_OPTIONS[i].flag, token, n)) {
            hits++;
            *index = i;
        }
    }
    return hits;
}

json_t *w22_webapp_0bcaf2_parse(const json_t *flags) {
    int seen[WA_OPTION_COUNT];
    int first_exclusive = -1;
    json_t *unrec = json_array();
    json_t *values, *out;
    size_t count, f;
    char buf[512];

    /* An absent (or non-list) argv is the empty argv. */
    if (flags && flags->type != JSON_ARRAY) flags = NULL;
    for (f = 0; f < (size_t)WA_OPTION_COUNT; f++) seen[f] = 0;

    count = flags ? json_len(flags) : 0;
    for (f = 0; f < count; f++) {
        json_t *item = json_get(flags, f);
        const char *token;
        const char *eq;
        int idx = -1, hits;

        if (!item || item->type != JSON_STRING) {
            char *dump = item ? json_dumps(item, 0) : NULL;
            json_append(unrec, json_string(dump ? dump : ""));
            free(dump);
            continue;
        }
        token = item->str_val;
        /* -h / --help are the implicit help action: argparse prints usage and
         * exits 0 before any of the rest is looked at. */
        if (!strcmp(token, "-h") || !strcmp(token, "--help")) {
            json_free(unrec);
            return wa_error("help", 0);
        }
        /* `--flag=value` on a store_true action: "ignored explicit argument". */
        eq = strchr(token, '=');
        if (eq && eq != token) {
            char name[128];
            size_t len = (size_t)(eq - token);
            int j;
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, token, len);
            name[len] = '\0';
            for (j = 0; j < WA_OPTION_COUNT; j++) {
                if (!strcmp(name, WA_OPTIONS[j].flag)) {
                    json_free(unrec);
                    snprintf(buf, sizeof(buf),
                             "argument %s: ignored explicit argument '%s'",
                             WA_OPTIONS[j].flag, eq + 1);
                    return wa_error(buf, 2);
                }
            }
        }
        for (idx = 0; idx < WA_OPTION_COUNT; idx++) {
            if (!strcmp(token, WA_OPTIONS[idx].flag)) break;
        }
        if (idx == WA_OPTION_COUNT) {
            /* allow_abbrev: a unique prefix of a long option is that option. */
            idx = -1;
            hits = token[0] == '-' ? wa_prefix_matches(token, &idx) : 0;
            if (hits == 0) {
                json_append(unrec, json_string(token));
                continue;
            }
            if (hits > 1) {
                size_t used = 0;
                int j, first = 1;
                json_free(unrec);
                buf[0] = '\0';
                used = (size_t)snprintf(buf, sizeof(buf), "ambiguous option: %s could match ", token);
                for (j = 0; j < WA_OPTION_COUNT; j++) {
                    if (strncmp(WA_OPTIONS[j].flag, token, strlen(token))) continue;
                    if (used >= sizeof(buf) - 1) break;
                    used += (size_t)snprintf(buf + used, sizeof(buf) - used, "%s%s",
                                             first ? "" : ", ", WA_OPTIONS[j].flag);
                    first = 0;
                }
                return wa_error(buf, 2);
            }
        }
        if (WA_OPTIONS[idx].exclusive) {
            if (first_exclusive >= 0 && first_exclusive != idx) {
                json_free(unrec);
                snprintf(buf, sizeof(buf), "argument %s: not allowed with argument %s",
                         WA_OPTIONS[idx].flag, WA_OPTIONS[first_exclusive].flag);
                return wa_error(buf, 2);
            }
            if (first_exclusive < 0) first_exclusive = idx;
        }
        seen[idx] = 1;
    }
    if (json_len(unrec) > 0) {
        size_t used = 0;
        size_t i;
        buf[0] = '\0';
        used = (size_t)snprintf(buf, sizeof(buf), "unrecognized arguments:");
        for (i = 0; i < json_len(unrec); i++) {
            json_t *tok = json_get(unrec, i);
            if (used >= sizeof(buf) - 1) break;
            used += (size_t)snprintf(buf + used, sizeof(buf) - used, " %s",
                                     (tok && tok->type == JSON_STRING && tok->str_val)
                                         ? tok->str_val : "");
        }
        json_free(unrec);
        return wa_error(buf, 2);
    }
    json_free(unrec);

    values = json_object();
    {
        int j;
        for (j = 0; j < WA_OPTION_COUNT; j++) {
            json_set(values, WA_OPTIONS[j].dest, json_bool(seen[j] ? 1 : 0));
        }
    }
    out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "values", values);
    json_set(out, "func", json_bool(1));
    return out;
}
