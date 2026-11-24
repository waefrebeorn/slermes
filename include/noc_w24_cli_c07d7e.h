/*
 * noc_w24_cli_c07d7e.h - C surface for upstream
 * plugins/google_meet/node/cli.py (gate w24_cli_c07d7e).
 *
 * The `hermes meet node ...` subcommand tree. The command handlers reach the
 * registry file, the node server and the network; every one of those is a seam.
 * What is ported here is what the CLI prints and returns around them: the
 * `_COMMANDS` dispatch table and its unknown-command refusal, the listing /
 * approval / removal lines, the `run` banner and the `ping` failure payloads.
 */
#ifndef W24_CLI_C07D7E_H
#define W24_CLI_C07D7E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {cmd} -> {handler, rc, error}: the handler node_command() would call
 * (rc null: its own exit code is its seam), or rc 2 plus the stderr text. */
json_t *w24_cli_c07d7e_dispatch(const json_t *req);

/* req: {nodes: [{name, url, token}...]} -> the exact stdout of _cmd_list:
 * "no nodes registered\n", or one "<name>\t<url>\ttoken=<first6>…" per line. */
json_t *w24_cli_c07d7e_format_list(const json_t *req);

/* req: {name, url} -> the stdout of _cmd_approve. */
json_t *w24_cli_c07d7e_approve_message(const json_t *req);

/* req: {name, ok} -> {message, rc}: the stdout of _cmd_remove and its code. */
json_t *w24_cli_c07d7e_remove_result(const json_t *req);

/* req: {host, port, display_name, token} -> the banner _cmd_run prints before
 * it serves. */
json_t *w24_cli_c07d7e_run_banner(const json_t *req);

/* req: {name, error} -> {stdout, rc}: a registered node whose ping raised. */
json_t *w24_cli_c07d7e_ping_fail(const json_t *req);

/* req: {name} -> {stderr, rc}: the node is not registered at all. */
json_t *w24_cli_c07d7e_ping_missing(const json_t *req);

/* The `node` parser defaults and the command table. */
json_t *w24_cli_c07d7e_constants(void);

#ifdef __cplusplus
}
#endif
#endif
