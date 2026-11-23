#ifndef NOC_W10_MCPTOOLHA_1FDC12_H
#define NOC_W10_MCPTOOLHA_1FDC12_H
#include "hermes_json.h"

/* PoP port of tools/mcp_tool_handlers.py (gate w10_mcptoolha_1fdc12). */
char *w10_mcptoolha_1fdc12_trust_approval_key(const char *server_name, const char *tool_name);
int   w10_mcptoolha_1fdc12_result_is_error(const char *result_text);
int   w10_mcptoolha_1fdc12_json_type_stable_equal(json_t *left, json_t *right);
#endif /* NOC_W10_MCPTOOLHA_1FDC12_H */
