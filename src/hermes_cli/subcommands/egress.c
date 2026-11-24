/*
 * egress.c -- real C11 port of hermes_cli/subcommands/egress.py.
 *
 * PoP: w22_egress_c95264_spec @ hermes_cli/subcommands/egress.py:build_egress_parser
 * PoP: w22_egress_c95264_dispatch @ hermes_cli/subcommands/egress.py:_dispatch_egress
 *
 * argparse and the proxy_cli registration are the CLI seam. What the module
 * itself owns is the parser identity (name/help/description, which the docs
 * quote) and the dispatcher rule: a named subcommand with a foreign handler
 * runs it, otherwise the egress help is printed and 0 returned. The
 * dispatcher closure is reproduced as a pure predicate.
 */
#include "hermes_json.h"
#include "noc_w22_egress_c95264.h"

#include <stdlib.h>
#include <string.h>

json_t *w22_egress_c95264_spec(void) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "name", json_string("egress"));
    json_set(o, "help",
             json_string("Manage the iron-proxy egress credential-injection firewall"));
    json_set(o, "description",
             json_string("Manage iron-proxy, the optional TLS-intercepting egress firewall "
                         "that swaps proxy tokens for real API credentials before outbound "
                         "requests leave a sandbox.  Disabled by default.  See: "
                         "https://hermes-agent.nousresearch.com/docs/user-guide/egress/iron-proxy"));
    return o;
}

json_t *w22_egress_c95264_dispatch(int has_sub, int has_func, int func_is_self) {
    json_t *o = json_object();
    int call = has_sub && has_func && !func_is_self;
    if (!o) return NULL;
    json_set(o, "action", json_string(call ? "call" : "help"));
    json_set(o, "help", json_bool(call ? 0 : 1));
    return o;
}
