#ifndef NOC_W12_ANONCHALLE_ED340F_H
#define NOC_W12_ANONCHALLE_ED340F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_anonchalle_ed340f_server_message @ hermes_cli/anon_challenge.py:server_message */
struct json_t *w12_anonchalle_ed340f_server_message(const struct json_t *value);

/* PoP: w12_anonchalle_ed340f_same_origin @ hermes_cli/anon_challenge.py:_same_origin */
int w12_anonchalle_ed340f_same_origin(const char *url, const char *portal_base_url);

/* PoP: w12_anonchalle_ed340f_number @ hermes_cli/anon_challenge.py:_number */
struct json_t *w12_anonchalle_ed340f_number(const struct json_t *value, double default_value);

/* PoP: w12_anonchalle_ed340f_parse @ hermes_cli/anon_challenge.py:parse_browser_challenge
 * Returns BrowserChallenge.as_payload() dict or JSON null. */
struct json_t *w12_anonchalle_ed340f_parse(const struct json_t *payload,
                                           const char *portal_base_url);

/* PoP: w12_anonchalle_ed340f_constants @ hermes_cli/anon_challenge.py:<module constants> */
struct json_t *w12_anonchalle_ed340f_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_ANONCHALLE_ED340F_H */
