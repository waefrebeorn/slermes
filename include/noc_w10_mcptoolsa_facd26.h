#ifndef NOC_W10_MCPTOOLSA_FACD26_H
#define NOC_W10_MCPTOOLSA_FACD26_H
#include "hermes_json.h"

/* PoP port of tools/mcp_tool_sampling.py (gate w10_mcptoolsa_facd26). */
json_t *w10_mcptoolsa_facd26_parse_tool_call_arguments(json_t *args);
json_t *w10_mcptoolsa_facd26_accepted_consent_content(json_t *schema);
char   *w10_mcptoolsa_facd26_format_elicitation_schema_summary(json_t *schema,
                                                              const char *server_name);
#endif /* NOC_W10_MCPTOOLSA_FACD26_H */
