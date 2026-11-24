/*
 * secrets.c -- C11 port of upstream Hermes
 *   hermes_cli/subcommands/secrets.py
 *
 * PoP: w21_secrets_95fd41_parser @ hermes_cli/subcommands/secrets.py:build_secrets_parser
 * PoP: w21_secrets_95fd41_dispatch @ hermes_cli/subcommands/secrets.py:_dispatch_secrets
 *
 * The `hermes secrets` subcommand wiring: the command/alias/help table plus the
 * dispatch rule (no sub-command prints the parser help and exits 0, otherwise
 * the registered handler's return code is the exit code). The per-backend flags
 * and handlers come from hermes_cli.secrets_cli / onepassword_secrets_cli --
 * they stay upstream (they pull cryptography's native extension) and register
 * into the two sub-parsers, so only this module's own table is ported here.
 */
#include "hermes_json.h"
#include "noc_w21_secrets_95fd41.h"

#include <stdio.h>
#include <string.h>

#define W21SECRETS_HELP "Manage external secret sources (Bitwarden, 1Password)"
#define W21SECRETS_DESCRIPTION \
    "Pull API keys from an external secret manager at process startup " \
    "instead of storing them in ~/.hermes/.env.  Supports Bitwarden " \
    "Secrets Manager and 1Password.  See: " \
    "https://hermes-agent.nousresearch.com/docs/user-guide/secrets/"
#define W21SECRETS_BW_HELP "Bitwarden Secrets Manager integration"
#define W21SECRETS_OP_HELP "1Password (op:// references) integration"

json_t *w21_secrets_95fd41_parser(void) {
    json_t *out = json_object();
    json_t *choices = json_array();
    json_t *helps = json_object();
    json_t *aliases = json_object();
    json_t *descriptions = json_object();

    json_set(out, "command", json_string("secrets"));
    json_set(out, "help", json_string(W21SECRETS_HELP));
    json_set(out, "description", json_string(W21SECRETS_DESCRIPTION));
    json_set(out, "dest", json_string("secrets_command"));

    json_append(choices, json_string("bitwarden"));
    json_append(choices, json_string("onepassword"));
    json_set(out, "choices", choices);

    json_set(helps, "bitwarden", json_string(W21SECRETS_BW_HELP));
    json_set(helps, "onepassword", json_string(W21SECRETS_OP_HELP));
    json_set(out, "helps", helps);

    /* add_parser(aliases=[...]) -- aliases sort after their canonical name. */
    json_set(aliases, "bw", json_string("bitwarden"));
    json_set(aliases, "op", json_string("onepassword"));
    json_set(aliases, "1password", json_string("onepassword"));
    json_set(out, "aliases", aliases);

    /* Neither sub-parser is given a description, only help. */
    json_set(descriptions, "bitwarden", json_null());
    json_set(descriptions, "onepassword", json_null());
    json_set(out, "descriptions", descriptions);

    json_set(out, "func", json_string("_dispatch_secrets"));
    return out;
}

json_t *w21_secrets_95fd41_dispatch(const json_t *state) {
    json_t *sub = json_obj_get(state, "sub");
    json_t *out = json_object();
    int printed_help = 0;
    int rc = 0;

    if (!sub || sub->type == JSON_NULL) {
        /* secrets_parser.print_help(); return 0 */
        printed_help = 1;
        rc = 0;
    } else {
        rc = (int)json_get_num(state, "handler_rc", 0.0);
    }
    json_set(out, "rc", json_number((double)rc));
    json_set(out, "printed_help", json_bool(printed_help));
    return out;
}
