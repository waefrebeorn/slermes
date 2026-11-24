/*
 * noc_w21_secrets_95fd41.h -- public C API for the C11 port of upstream Hermes
 *   hermes_cli/subcommands/secrets.py
 *
 * `hermes secrets` subcommand wiring. Every exported symbol carries the
 * w21_secrets_95fd41_ prefix.
 */
#ifndef NOC_W21_SECRETS_95FD41_H
#define NOC_W21_SECRETS_95FD41_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * build_secrets_parser() table:
 *   {"command":"secrets","help":...,"description":...,"dest":"secrets_command",
 *    "choices":["bitwarden","onepassword"],
 *    "aliases":{"bw":"bitwarden","op":"onepassword","1password":"onepassword"},
 *    "helps":{name:help},"descriptions":{name:null},"func":"_dispatch_secrets"}
 */
json_t *w21_secrets_95fd41_parser(void);

/*
 * _dispatch_secrets(). state: {"sub":str|null, "handler_rc":number}
 * -> {"rc":int,"printed_help":bool}; no sub-command prints the parser help and
 * returns 0, otherwise the registered handler's return code is the rc.
 */
json_t *w21_secrets_95fd41_dispatch(const json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_SECRETS_95FD41_H */
