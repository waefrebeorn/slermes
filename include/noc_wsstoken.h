#ifndef NOC_WSSTOKEN_H
#define NOC_WSSTOKEN_H

/* C port of hermes_cli/web_server_session_token.py. All exports wsstoken_-prefixed.
 *
 * Environment and random-token generation are seam-injected from the fixture:
 *   env_value  = value of HERMES_DASHBOARD_SESSION_TOKEN ("" == unset/empty)
 *   minted     = the fresh token a no-env call would mint
 *   file_error = non-NULL => Path.read_text raised OSError(this message)
 *   file_content = what Path.read_text(encoding="utf-8-sig") returned
 * (See PORT-KIT: Windows CRT setenv("",1) unsets, so never mutate process env.)
 */

typedef struct {
    int   is_exit;   /* 1 => SystemExit message in value, 0 => token in value */
    char *value;     /* malloc'd, always non-NULL */
} wsstoken_result_t;

wsstoken_result_t wsstoken_resolve(const char *env_value,
                                   const char *file_content,
                                   const char *minted,
                                   const char *file_error);
void wsstoken_result_free(wsstoken_result_t *r);

#endif
