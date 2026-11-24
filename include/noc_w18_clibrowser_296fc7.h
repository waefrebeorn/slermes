#ifndef NOC_W18_CLIBROWSER_296FC7_H
#define NOC_W18_CLIBROWSER_296FC7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_clibrowser_296fc7_gateway_is_remote @ hermes_cli/cli_browser_gateway.py:_gateway_is_remote
 * Returns 1 when the (already stripped) gateway URL's host is a non-loopback
 * host.  `env_value` is the HERMES_TUI_GATEWAY_URL value injected by the caller
 * (the C side never mutates the process environment). */
int w18_clibrowser_296fc7_gateway_is_remote(const char *env_value);

/* PoP: w18_clibrowser_296fc7_normalize_cdp_url @ hermes_cli/cli_browser_gateway.py:_normalize_cdp_url
 * Validate a /browser connect URL.  Returns a NEW json_t* array [url, port]
 * on success (url string, port number) or JSON null when Python would return
 * None (unsupported scheme / bad port / missing host).  Caller frees. */
struct json_t *w18_clibrowser_296fc7_normalize_cdp_url(const char *cdp_url);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_CLIBROWSER_296FC7_H */
