/*
 * fallback.c -- real C11 port of the `hermes fallback` subcommand parser of
 * upstream Hermes hermes_cli/subcommands/fallback.py.
 *
 * PoP: w21_fallback_689de3_parser_spec @ hermes_cli/subcommands/fallback.py:build_fallback_parser
 * PoP: w21_fallback_689de3_subcommand @ hermes_cli/subcommands/fallback.py:build_fallback_parser
 * PoP: w21_fallback_689de3_default_func @ hermes_cli/subcommands/fallback.py:build_fallback_parser
 *
 * argparse is the CLI seam: `add_parser` / `add_subparsers` / `add_argument` /
 * `set_defaults` only record the tree here, and the recorded tree is exactly
 * what the gate compares. Every help string, alias and flag order is the
 * literal the source passes.
 */
#include "hermes_json.h"
#include "noc_w21_fallback_689de3.h"

#include <stdlib.h>
#include <string.h>

#define FB_HELP_TOP "Manage fallback providers (tried when the primary model fails)"
#define FB_DESC_TOP "Manage the fallback provider chain.  Fallback providers are tried " \
                    "in order when the primary model fails with rate-limit, overload, or " \
                    "connection errors.  See: " \
                    "https://hermes-agent.nousresearch.com/docs/user-guide/features/fallback-providers"

typedef struct {
    const char *name;
    const char *alias;              /* NULL when the source passes no aliases */
    const char *help;
    const char *flag;               /* NULL: no options on this choice */
    const char *flag_help;
    const char *extra_flag;         /* the --json flag add_json_flag attaches */
    const char *extra_flag_help;
} fb_choice_row;

static const fb_choice_row FB_CHOICES[] = {
    {"list", "ls",
     "Show the current fallback chain (default when no subcommand)",
     NULL, NULL, NULL, NULL},
    {"add", NULL,
     "Pick a provider + model (same picker as `hermes model`) and append to the chain",
     NULL, NULL, NULL, NULL},
    {"remove", "rm",
     "Pick an entry to delete from the chain",
     NULL, NULL, NULL, NULL},
    {"clear", NULL,
     "Remove all fallback entries",
     NULL, NULL, NULL, NULL},
    {"check", NULL,
     "Check each fallback entry is usable, without starting an agent session (exit 1 if not)",
     "--live", "Also send one minimal inference request per entry (on that entry's own provider/model)",
     "--json", "Print the results as JSON"},
    {NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

static json_t *fb_arg(const char *flag, const char *help) {
    json_t *arg = json_object();
    json_t *flags = json_array();
    json_append(flags, json_string(flag));
    json_set(arg, "flags", flags);
    json_set(arg, "action", json_string("store_true"));
    json_set(arg, "help", json_string(help));
    return arg;
}

static json_t *fb_choice(const fb_choice_row *row) {
    json_t *choice = json_object();
    json_t *aliases = json_array();
    json_t *args = json_array();
    json_set(choice, "name", json_string(row->name));
    if (row->alias) json_append(aliases, json_string(row->alias));
    json_set(choice, "aliases", aliases);
    json_set(choice, "help", json_string(row->help));
    if (row->flag) json_append(args, fb_arg(row->flag, row->flag_help));
    if (row->extra_flag) json_append(args, fb_arg(row->extra_flag, row->extra_flag_help));
    json_set(choice, "arguments", args);
    return choice;
}

json_t *w21_fallback_689de3_parser_spec(void) {
    json_t *spec = json_object();
    json_t *subs = json_object();
    json_t *choices = json_array();
    json_t *defaults = json_object();
    int i;

    json_set(spec, "prog", json_string("fallback"));
    json_set(spec, "help", json_string(FB_HELP_TOP));
    json_set(spec, "description", json_string(FB_DESC_TOP));
    json_set(subs, "dest", json_string("fallback_command"));
    for (i = 0; FB_CHOICES[i].name; i++) json_append(choices, fb_choice(&FB_CHOICES[i]));
    json_set(subs, "choices", choices);
    json_set(spec, "subparsers", subs);
    json_set(defaults, "func", json_string("cmd_fallback"));
    json_set(spec, "defaults", defaults);
    return spec;
}

json_t *w21_fallback_689de3_subcommand(const char *name) {
    int i;
    if (!name) return json_null();
    for (i = 0; FB_CHOICES[i].name; i++) {
        const fb_choice_row *row = &FB_CHOICES[i];
        if (!strcmp(name, row->name)) return fb_choice(row);
        if (row->alias && !strcmp(name, row->alias)) return fb_choice(row);
    }
    return json_null();
}

const char *w21_fallback_689de3_default_func(void) {
    return "cmd_fallback";
}
