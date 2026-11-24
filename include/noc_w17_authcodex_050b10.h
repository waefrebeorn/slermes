#ifndef NOC_W17_AUTHCODEX_050B10_H
#define NOC_W17_AUTHCODEX_050B10_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/auth_codex_browser.py (gate w17_authcodex_050b10).
 * The config dependency is injected as `config`; only deterministic helpers
 * (authorize-URL construction and login-flow selection) are ported here. */
json_t *w17_authcodex_050b10_authorize_url(const char *redirect_uri, const char *state,
                                           const char *code_challenge);
json_t *w17_authcodex_050b10_login_flow(int browser, const json_t *config);

#endif /* NOC_W17_AUTHCODEX_050B10_H */
