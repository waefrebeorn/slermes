/*
 * vault.c — faithful C11 port of the observable CLI surface of upstream
 * Hermes hermes_cli/vault.py (gate w6_vault_9ab785).
 *
 * PoP: w6_vault_actions  @ hermes_cli/vault.py:register_cli
 * PoP: w6_vault_handler  @ hermes_cli/vault.py:vault_command
 * PoP: w6_vault_arg_meta @ hermes_cli/vault.py:register_cli
 *
 * The module's side-effect-free contract is the argparse tree registered by
 * register_cli() and the handler dispatch performed by vault_command().  The
 * interactive command bodies are effectful (getpass / console / store I/O)
 * and are not part of this deterministic surface.
 */
#include "noc_w6_vault_9ab785.h"
#include "hermes_json.h"
#include <string.h>

static const char *const ACTIONS[] = { "add", "list", "rm", "sources", "key" };
static const size_t NACTIONS = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

const char *const *w6_vault_actions(size_t *count) {
    if (count) *count = NACTIONS;
    return ACTIONS;
}

const char *w6_vault_handler(const char *action, const char *subaction) {
    (void)subaction; /* key and key/migrate both dispatch to _cmd_key */
    if (!action || !*action) return NULL; /* vault_command: no handler -> _cmd_list */
    if (!strcmp(action, "add"))     return "_cmd_add";
    if (!strcmp(action, "list"))    return "_cmd_list";
    if (!strcmp(action, "rm"))      return "_cmd_rm";
    if (!strcmp(action, "sources")) return "_cmd_sources";
    if (!strcmp(action, "key"))     return "_cmd_key";
    return NULL;
}

/* Build a descriptor json object for an argparse argument.
 * choices may be a string array or null; metavar a string or null. */
static json_t *meta(const char *const *choices, size_t nchoices,
                    int required, const char *metavar) {
    json_t *o = json_object();
    if (choices) {
        json_t *a = json_array();
        for (size_t i = 0; i < nchoices; i++) json_append(a, json_string(choices[i]));
        json_set(o, "choices", a);
    } else {
        json_set(o, "choices", json_null());
    }
    json_set(o, "default", json_null());
    json_set(o, "required", json_bool(required != 0));
    json_set(o, "metavar", metavar ? json_string(metavar) : json_null());
    return o;
}

json_t *w6_vault_arg_meta(const char *action, const char *opt) {
    if (!action || !opt) return json_null();
    if (!strcmp(action, "add") && !strcmp(opt, "--kind")) {
        static const char *const C[] = { "login", "payment", "address" };
        return meta(C, 3, 0, NULL);
    }
    if (!strcmp(action, "rm") && !strcmp(opt, "handle"))
        return meta(NULL, 0, 1, NULL);
    if (!strcmp(action, "sources") && !strcmp(opt, "--disable"))
        return meta(NULL, 0, 0, "NAME");
    if (!strcmp(action, "sources") && !strcmp(opt, "--enable"))
        return meta(NULL, 0, 0, "NAME");
    if (!strcmp(action, "key/migrate") && !strcmp(opt, "--to")) {
        static const char *const C[] = { "keychain", "file" };
        return meta(C, 2, 1, NULL);
    }
    return json_null();
}
