#ifndef NOC_W11_MCPTOOLTR_2238D8_H
#define NOC_W11_MCPTOOLTR_2238D8_H

#include "hermes_json.h"

/* PoP port of tools/mcp_tool_transport.py (gate w11_mcptooltr_2238d8). */

/* _content_type_base: resp.headers.get("content-type","").split(";")[0].strip().lower().
 * Caller frees the returned malloc'd string. */
char *w11_mcptooltr_2238d8_content_type_base(const char *content_type);

/* _is_2xx: 200 <= resp.status_code < 300. */
int w11_mcptooltr_2238d8_is_2xx(int status_code);

/* _present: dict(kwargs) minus the None values. Caller frees the result. */
json_t *w11_mcptooltr_2238d8_present(json_t *kwargs);

#endif /* NOC_W11_MCPTOOLTR_2238D8_H */
