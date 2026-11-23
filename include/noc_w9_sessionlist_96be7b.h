#ifndef NOC_W9_SESSIONLIST_96BE7B_H
#define NOC_W9_SESSIONLIST_96BE7B_H
#include "hermes_json.h"

/* PoP port of gateway/session_listing.py — cursor + page-param validation.
 * Every exported symbol is prefixed with the gate name. */
json_t *w9_sessionlist_96be7b_check_page_params(json_t *params);
char   *w9_sessionlist_96be7b_binding_digest(const char *workspace,
                                             const char *query, int include_hidden);
char   *w9_sessionlist_96be7b_encode_cursor(long long offset,
                                             const char *workspace,
                                             const char *query, int include_hidden);
json_t *w9_sessionlist_96be7b_decode_cursor(const char *cursor,
                                             const char *workspace,
                                             const char *query, int include_hidden);
#endif /* NOC_W9_SESSIONLIST_96BE7B_H */
