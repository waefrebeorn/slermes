/*
 * port_secrets_cli.h -- pure helpers ported from hermes_cli/secrets_cli.py and
 * hermes_cli/_secrets_common.py.
 *
 * The bws subprocess, rich rendering, prompting and .env persistence stay in
 * Python.
 */
#ifndef PORT_SECRETS_CLI_H
#define PORT_SECRETS_CLI_H

#include <stddef.h>
#include "hermes_json.h"

/* _secrets_common.py:section_cfg -- cfg["secrets"][section] or {}.
 * Returns the section (borrowed) or a NEW empty object; *created says which. */
const json_t *secrets_section_cfg(const json_t *cfg, const char *section, json_t **created);

/* secrets_cli.py:_bw_cfg -- the "bitwarden" secrets section. */
const json_t *secrets_bw_cfg(const json_t *cfg, json_t **created);

/* secrets_cli.py:_fetch_status -- rich-marked status line for one key.
 * env_has stands in for os.environ.get. Caller frees. */
char *secrets_fetch_status(const char *key, const char *token_env,
                           int (*env_has)(const char *name));

/* secrets_cli.py:_missing_noninteractive_flags -- the flags a no-TTY run must
 * supply, given the three values and the BWS_SERVER_URL env fallback.
 * Returns a newly allocated array of static flag strings; free the ARRAY only.
 * *out_count receives the number of entries. */
const char **secrets_missing_flags(const char *access_token,
                                   const char *server_url,
                                   const char *project_id,
                                   const char *env_server_url,
                                   int *out_count);

#endif /* PORT_SECRETS_CLI_H */
