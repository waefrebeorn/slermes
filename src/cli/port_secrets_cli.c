/*
 * port_secrets_cli.c
 *
 * Pure helpers ported from hermes_cli/secrets_cli.py (Bitwarden Secrets
 * Manager bootstrap) and its _secrets_common helper.
 *
 * The module's real work is subprocess (bws CLI), rich rendering, prompting
 * and .env persistence. What is portable is the config lookup and the flag /
 * status decisions:
 *
 *   secrets_section_cfg    <- _secrets_common.py:section_cfg
 *   secrets_bw_cfg         <- secrets_cli.py:_bw_cfg
 *   secrets_fetch_status   <- secrets_cli.py:_fetch_status
 *   secrets_missing_flags  <- secrets_cli.py:_missing_noninteractive_flags
 *
 * The env lookups are passed in as a callback (Python reads os.environ
 * directly; a C body should not own that).
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ===========================================================================
 *  section_cfg  (_secrets_common.py:section_cfg)
 *
 *      (cfg.get("secrets") or {}).get(section) or {}
 *
 *  The two `or` fallbacks are Python TRUTHINESS: a missing "secrets" key, a
 *  JSON null, or an empty object all fall through. Returns the section object
 *  (borrowed), or a NEW empty object when absent. *created tells the caller
 *  which, so it knows what to free.
 * =========================================================================== */

/* PoP: section_cfg @ hermes_cli/_secrets_common.py:section_cfg */
const json_t *secrets_section_cfg(const json_t *cfg, const char *section, json_t **created)
{
    if (created) *created = NULL;
    json_t *secs = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "secrets") : NULL;
    if (!secs || secs->type != JSON_OBJECT) {
        json_t *empty = json_object();
        if (created) *created = empty;
        return empty;
    }
    json_t *v = json_obj_get(secs, section);
    if (!v || v->type != JSON_OBJECT) {
        json_t *empty = json_object();
        if (created) *created = empty;
        return empty;
    }
    return v;
}

/* ===========================================================================
 *  _bw_cfg  (secrets_cli.py:_bw_cfg)
 *
 *      section_cfg(cfg, "bitwarden")
 * =========================================================================== */

/* PoP: _bw_cfg @ hermes_cli/secrets_cli.py:_bw_cfg */
const json_t *secrets_bw_cfg(const json_t *cfg, json_t **created)
{
    return secrets_section_cfg(cfg, "bitwarden", created);
}

/* ===========================================================================
 *  _fetch_status  (secrets_cli.py:_fetch_status)
 *
 *      if key == token_env: return "[dim]bootstrap token — never overrides itself[/dim]"
 *      if os.environ.get(key): return "[yellow]already set in env (will be overwritten)[/yellow]"
 *      return "[green]new[/green]"
 *
 *  `env_has` is a caller-supplied predicate standing in for os.environ.get.
 *  Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _fetch_status @ hermes_cli/secrets_cli.py:_fetch_status */
char *secrets_fetch_status(const char *key, const char *token_env,
                           int (*env_has)(const char *name))
{
    const char *msg;
    if (token_env && key && strcmp(key, token_env) == 0)
        msg = "[dim]bootstrap token \xe2\x80\x94 never overrides itself[/dim]";
    else if (env_has && env_has(key))
        msg = "[yellow]already set in env (will be overwritten)[/yellow]";
    else
        msg = "[green]new[/green]";

    char *out = (char *)malloc(strlen(msg) + 1);
    if (!out) return NULL;
    strcpy(out, msg);
    return out;
}

/* ===========================================================================
 *  _missing_noninteractive_flags  (secrets_cli.py:_missing_noninteractive_flags)
 *
 *      provided = {
 *          "--access-token": args.access_token,
 *          "--server-url": (args.server_url or "").strip() or os.environ.get("BWS_SERVER_URL", ""),
 *          "--project-id": args.project_id}
 *      return [flag for flag, value in provided.items() if not (value and value.strip())]
 *
 *  A flag is "provided" when its value is non-NULL and has a non-space
 *  character. `--server-url` falls back to $BWS_SERVER_URL. The result is a
 *  newly allocated NULL-terminated array of static flag strings; the caller
 *  frees the ARRAY only (the strings are literals).
 * =========================================================================== */

/* PoP: _missing_noninteractive_flags @ hermes_cli/secrets_cli.py:_missing_noninteractive_flags */
const char **secrets_missing_flags(const char *access_token,
                                   const char *server_url,
                                   const char *project_id,
                                   const char *env_server_url,
                                   int *out_count)
{
    static const char *FLAGS[] = { "--access-token", "--server-url", "--project-id" };
    const char **missing = (const char **)calloc(3, sizeof(char *));
    if (!missing) { if (out_count) *out_count = 0; return NULL; }

    /* --server-url falls back to the environment */
    const char *su = (server_url && *server_url) ? server_url : (env_server_url ? env_server_url : "");

    const char *values[3];
    values[0] = access_token;
    values[1] = su;
    values[2] = project_id;

    int n = 0;
    for (int i = 0; i < 3; i++) {
        const char *v = values[i];
        int provided = 0;
        if (v) {
            for (const char *p = v; *p; p++) {
                if (!isspace((unsigned char)*p)) { provided = 1; break; }
            }
        }
        if (!provided) missing[n++] = FLAGS[i];
    }
    if (out_count) *out_count = n;
    return missing;
}
