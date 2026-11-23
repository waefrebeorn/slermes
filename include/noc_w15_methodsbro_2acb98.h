#ifndef NOC_W15_METHODSBRO_2ACB98_H
#define NOC_W15_METHODSBRO_2ACB98_H
/*
 * Public C API for the port of tui_gateway/methods_browser.py
 * (gate w15_methodsbro_2acb98). All exported symbols are gate-prefixed.
 *
 * PoP: w15_methodsbro_2acb98_is_default_local_cdp @ tui_gateway/methods_browser.py:_is_default_local_cdp
 * PoP: w15_methodsbro_2acb98_cdp_schemes          @ tui_gateway/methods_browser.py:_CDP_SCHEMES
 *
 * The connect/disconnect handlers do network I/O and drive the browser
 * process; only the deterministic CDP URL classification (no I/O) and the
 * scheme literal are ported. The caller passes the raw URL string.
 */
#include "hermes_json.h"

/* URL string -> the exact result of _is_default_local_cdp(urlparse(url)).
 * Invalid ports (ValueError in .port) yield 0. */
int w15_methodsbro_2acb98_is_default_local_cdp(const char *url);

/* sorted(_CDP_SCHEMES) -> JSON array of strings. */
json_t *w15_methodsbro_2acb98_cdp_schemes(void);

#endif /* NOC_W15_METHODSBRO_2ACB98_H */
