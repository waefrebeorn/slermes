/*
 * noc_w21_connectors_0e0582.h -- public C API for the port of upstream Hermes
 *   hermes_cli/subcommands/connectors.py (gate w21_connectors_0e0582).
 *
 * Ported: the whole declared CLI surface of `hermes connectors` -- every
 * subcommand, its aliases, its help text and every argument (dest, option
 * strings, nargs, required, action class, default) -- plus the name/alias
 * lookup that argparse performs on the token. cmd_connectors itself is the
 * subcommand's body (a network/DB command) and stays upstream.
 */
#ifndef NOC_W21_CONNECTORS_0E0582_H
#define NOC_W21_CONNECTORS_0E0582_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The parser tree build_connectors_parser() attaches:
 * {"name","help","description","action_dest","func","subcommands":[
 *   {"name","aliases":[...],"help","arguments":[
 *     {"dest","options":[...],"nargs","required","action","default","help"}]}]} */
json_t *w21_connectors_0e0582_spec(void);

/* One subcommand as argparse resolves it (name or alias), or null for an
 * unknown token: {"name","aliases":[...],"required":[...],"flags":[...],
 * "positionals":[...]}. */
json_t *w21_connectors_0e0582_lookup(const char *token);

/* Probe of the harness environment: {"sler":bool,"home":bool,"same":bool}. */
json_t *w21_connectors_0e0582_env_probe(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_CONNECTORS_0E0582_H */
