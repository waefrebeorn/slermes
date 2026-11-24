/*
 * connectors.c -- real C11 port of upstream Hermes
 *   hermes_cli/subcommands/connectors.py.
 *
 * PoP: w21_connectors_0e0582_spec @ hermes_cli/subcommands/connectors.py:build_connectors_parser
 * PoP: w21_connectors_0e0582_lookup @ hermes_cli/subcommands/connectors.py:build_connectors_parser
 * PoP: w21_connectors_0e0582_env_probe @ hermes_cli/subcommands/connectors.py:build_connectors_parser
 *
 * The module IS the declaration: `hermes connectors` and its four subcommands.
 * Every field argparse would carry after build_connectors_parser() runs -- the
 * help strings, the `ls` alias, nargs "?" on `accounts app`, the required
 * positionals of rename/disconnect, `--to` being required, `--yes/-y` sharing a
 * dest, every store_true default -- is reproduced here exactly, together with
 * the token lookup argparse performs. Only cmd_connectors (the command body) is
 * upstream.
 */
#include "hermes_json.h"
#include "noc_w21_connectors_0e0582.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CN_DEF_NONE = 0, CN_DEF_FALSE = 1, CN_DEF_TRUE = 2 };

typedef struct {
    const char *dest;
    const char *options[3];               /* option strings, NULL-terminated */
    const char *nargs;                    /* "?", "0", or NULL */
    int required;
    const char *action;                   /* argparse action class name */
    int def;                              /* CN_DEF_* */
    const char *help;
} cn_arg;

typedef struct {
    const char *name;
    const char *aliases[2];
    const char *help;
    const cn_arg *args;
    int nargs;
} cn_sub;

static const cn_arg CN_LIST_ARGS[] = { {NULL, {NULL}, NULL, 0, NULL, 0, NULL} };

static const cn_arg CN_ACCOUNTS_ARGS[] = {
    {"app", {NULL}, "?", 0, "_StoreAction", CN_DEF_NONE,
     "Only show accounts of this app"},
    {"all", {"--all", NULL}, "0", 0, "_StoreTrueAction", CN_DEF_FALSE,
     "Also show retired accounts"},
    {NULL, {NULL}, NULL, 0, NULL, 0, NULL}
};

static const cn_arg CN_CONNECT_ARGS[] = {
    {"app", {NULL}, NULL, 1, "_StoreAction", CN_DEF_NONE,
     "App to connect, for example gmail"},
    {"alias", {"--alias", NULL}, NULL, 0, "_StoreAction", CN_DEF_NONE,
     "Name for a new account (for example work); with --reconnect, the name or label to repair"},
    {"reconnect", {"--reconnect", NULL}, "0", 0, "_StoreTrueAction", CN_DEF_FALSE,
     "Repair an account that stopped working"},
    {"force", {"--force", NULL}, "0", 0, "_StoreTrueAction", CN_DEF_FALSE,
     "With --reconnect, sign in again even if the account is connected"},
    {NULL, {NULL}, NULL, 0, NULL, 0, NULL}
};

static const cn_arg CN_RENAME_ARGS[] = {
    {"app", {NULL}, NULL, 1, "_StoreAction", CN_DEF_NONE, "App the account belongs to"},
    {"name", {NULL}, NULL, 1, "_StoreAction", CN_DEF_NONE,
     "Current name or label of the account"},
    {"to", {"--to", NULL}, NULL, 1, "_StoreAction", CN_DEF_NONE,
     "New name for the account"},
    {NULL, {NULL}, NULL, 0, NULL, 0, NULL}
};

static const cn_arg CN_DISCONNECT_ARGS[] = {
    {"app", {NULL}, NULL, 1, "_StoreAction", CN_DEF_NONE, "App the account belongs to"},
    {"alias", {"--alias", NULL}, NULL, 0, "_StoreAction", CN_DEF_NONE,
     "Name or label of the account to remove"},
    {"yes", {"--yes", "-y", NULL}, "0", 0, "_StoreTrueAction", CN_DEF_FALSE,
     "Skip the confirmation prompt"},
    {NULL, {NULL}, NULL, 0, NULL, 0, NULL}
};

static const cn_sub CN_SUBS[] = {
    {"list", {"ls", NULL}, "Show every connector and whether it is connected",
     CN_LIST_ARGS, 0},
    {"accounts", {NULL}, "Show connected accounts", CN_ACCOUNTS_ARGS, 2},
    {"connect", {NULL}, "Connect an app account in your browser", CN_CONNECT_ARGS, 4},
    {"rename", {NULL}, "Rename an account", CN_RENAME_ARGS, 3},
    {"disconnect", {NULL}, "Remove an account", CN_DISCONNECT_ARGS, 3},
    {NULL, {NULL}, NULL, NULL, 0}
};

static json_t *cn_default(int def) {
    if (def == CN_DEF_FALSE) return json_bool(0);
    if (def == CN_DEF_TRUE) return json_bool(1);
    return json_null();
}

static json_t *cn_arg_json(const cn_arg *arg) {
    json_t *o = json_object();
    json_t *opts = json_array();
    int i;
    json_set(o, "dest", json_string(arg->dest ? arg->dest : ""));
    for (i = 0; arg->options[i]; i++) json_append(opts, json_string(arg->options[i]));
    json_set(o, "options", opts);
    json_set(o, "nargs", arg->nargs ? json_string(arg->nargs) : json_null());
    json_set(o, "required", json_bool(arg->required ? 1 : 0));
    json_set(o, "action", json_string(arg->action ? arg->action : ""));
    json_set(o, "default", cn_default(arg->def));
    json_set(o, "help", arg->help ? json_string(arg->help) : json_null());
    return o;
}

static json_t *cn_sub_json(const cn_sub *sub) {
    json_t *o = json_object();
    json_t *aliases = json_array();
    json_t *args = json_array();
    int i;
    json_set(o, "name", json_string(sub->name));
    for (i = 0; sub->aliases[i]; i++) json_append(aliases, json_string(sub->aliases[i]));
    json_set(o, "aliases", aliases);
    json_set(o, "help", sub->help ? json_string(sub->help) : json_null());
    for (i = 0; i < sub->nargs; i++) json_append(args, cn_arg_json(&sub->args[i]));
    json_set(o, "arguments", args);
    return o;
}

json_t *w21_connectors_0e0582_spec(void) {
    json_t *o = json_object();
    json_t *subs = json_array();
    int i;
    json_set(o, "name", json_string("connectors"));
    json_set(o, "help", json_string("Manage hosted app connectors and their accounts"));
    json_set(o, "description", json_string(
        "List, connect, rename and remove the app accounts Hermes uses through Nous Portal."));
    json_set(o, "action_dest", json_string("connectors_action"));
    json_set(o, "func", json_string("cmd_connectors"));
    for (i = 0; CN_SUBS[i].name; i++) json_append(subs, cn_sub_json(&CN_SUBS[i]));
    json_set(o, "subcommands", subs);
    return o;
}

static const cn_sub *cn_find(const char *token) {
    int i, a;
    if (!token) return NULL;
    for (i = 0; CN_SUBS[i].name; i++) {
        if (!strcmp(token, CN_SUBS[i].name)) return &CN_SUBS[i];
        for (a = 0; CN_SUBS[i].aliases[a]; a++) {
            if (!strcmp(token, CN_SUBS[i].aliases[a])) return &CN_SUBS[i];
        }
    }
    return NULL;
}

json_t *w21_connectors_0e0582_lookup(const char *token) {
    const cn_sub *sub = cn_find(token);
    json_t *o, *aliases, *required, *flags, *positionals;
    int i, a;

    if (!sub) return json_null();
    o = json_object();
    aliases = json_array();
    required = json_array();
    flags = json_array();
    positionals = json_array();
    json_set(o, "name", json_string(sub->name));
    for (i = 0; sub->aliases[i]; i++) json_append(aliases, json_string(sub->aliases[i]));
    for (i = 0; i < sub->nargs; i++) {
        const cn_arg *arg = &sub->args[i];
        if (!arg->options[0]) {
            json_append(positionals, json_string(arg->dest));
            if (arg->required) json_append(required, json_string(arg->dest));
            continue;
        }
        for (a = 0; arg->options[a]; a++) json_append(flags, json_string(arg->options[a]));
        /* the first option string is what argparse names in "the following
         * arguments are required: --to" */
        if (arg->required) json_append(required, json_string(arg->options[0]));
    }
    json_set(o, "aliases", aliases);
    json_set(o, "required", required);
    json_set(o, "flags", flags);
    json_set(o, "positionals", positionals);
    return o;
}

json_t *w21_connectors_0e0582_env_probe(void) {
    const char *sler = getenv("SLERMES_HOME");
    const char *home = getenv("HOME");
    json_t *out = json_object();
    json_set(out, "sler", json_bool(sler && sler[0]));
    json_set(out, "home", json_bool(home && home[0]));
    json_set(out, "same", json_bool(sler && home && !strcmp(sler, home)));
    return out;
}
