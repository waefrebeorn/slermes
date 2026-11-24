/*
 * vault.c -- real C11 port of the `hermes vault` subcommand spec and its mount
 * order from upstream Hermes hermes_cli/subcommands/vault.py.
 *
 * PoP: w22_vault_d2e854_spec @ hermes_cli/subcommands/vault.py:build_vault_parser
 * PoP: w22_vault_d2e854_mount @ hermes_cli/subcommands/vault.py:build_vault_parser
 *
 * Host seam: `hermes_cli.vault.register_cli` / `vault_command`. The builder
 * imports them, mounts the add/list/rm flags on the parser and binds the
 * dispatch func; that module owns a live vault backend, so only the call is
 * counted here. Everything else -- the add_parser name, help, description, the
 * prog argparse derives and the duplicate-name failure -- is ported.
 */
#include "hermes_json.h"
#include "noc_w22_vault_d2e854.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const VAULT_NAME = "vault";
static const char *const VAULT_HELP =
    "Manage the local encrypted autofill vault (add/list/rm credentials)";
static const char *const VAULT_DESCRIPTION =
    "Store login credentials in a locally encrypted vault. The agent "
    "sees handles and login identifiers (metadata); passwords are "
    "injected server-side by browser_vault on the exact origin "
    "they were saved for and never enter the conversation.";
static const char *const VAULT_DEFAULTS_FUNC = "vault_command";

/* argparse derives the subparser prog from the parent's usage line, which is
 * stripped: surrounding whitespace goes, interior spacing stays. */
static char *vault_child_prog(const char *parent_prog) {
    const char *parent = parent_prog ? parent_prog : "";
    const char *start = parent;
    const char *end;
    size_t prefix_len;
    size_t n;
    char *out;

    while (*start == ' ' || (*start >= '\t' && *start <= '\r')) start++;
    end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || (end[-1] >= '\t' && end[-1] <= '\r'))) end--;
    prefix_len = (size_t)(end - start);
    n = prefix_len + 1 + strlen(VAULT_NAME) + 1;
    out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "%.*s %s", (int)prefix_len, start, VAULT_NAME);
    return out;
}

json_t *w22_vault_d2e854_spec(void) {
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "name", json_string(VAULT_NAME));
    json_set(out, "help", json_string(VAULT_HELP));
    json_set(out, "description", json_string(VAULT_DESCRIPTION));
    json_set(out, "defaults_func", json_string(VAULT_DEFAULTS_FUNC));
    json_set(out, "register_calls", json_number(1));
    return out;
}

static int vault_taken_has(const json_t *taken) {
    size_t i;
    if (!taken || taken->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(taken); i++) {
        json_t *item = json_get(taken, i);
        if (item && item->type == JSON_STRING && !strcmp(item->str_val, VAULT_NAME)) return 1;
    }
    return 0;
}

json_t *w22_vault_d2e854_mount(const char *parent_prog, const json_t *taken) {
    json_t *spec = w22_vault_d2e854_spec();
    json_t *out;
    char *prog;

    if (!spec) return NULL;
    if (vault_taken_has(taken)) {
        /* argparse: raise ValueError("conflicting subparser: %s" % name) */
        out = json_object();
        if (!out) {
            json_free(spec);
            return NULL;
        }
        json_set(out, "ok", json_bool(0));
        json_set(out, "error_type", json_string("ValueError"));
        json_set(out, "error", json_string("conflicting subparser: vault"));
        json_free(spec);
        return out;
    }
    prog = vault_child_prog(parent_prog);
    out = json_object();
    if (!out || !prog) {
        json_free(spec);
        json_free(out);
        free(prog);
        return NULL;
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "prog", json_string(prog));
    json_set(out, "name", json_copy(json_obj_get(spec, "name")));
    json_set(out, "help", json_copy(json_obj_get(spec, "help")));
    json_set(out, "description", json_copy(json_obj_get(spec, "description")));
    json_set(out, "defaults_func", json_copy(json_obj_get(spec, "defaults_func")));
    json_set(out, "register_calls", json_number(1));
    free(prog);
    json_free(spec);
    return out;
}
