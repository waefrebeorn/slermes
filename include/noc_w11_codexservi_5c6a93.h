#ifndef NOC_W11_CODEXSERVI_5C6A93_H
#define NOC_W11_CODEXSERVI_5C6A93_H
#include "hermes_json.h"

/* PoP port of agent/codex_service_tiers.py (gate w11_codexservi_5c6a93). */
/* Returns a sorted JSON array of strings, or JSON null for Python None. */
json_t *w11_codexservi_5c6a93_advertised_tiers(json_t *entry);
json_t *w11_codexservi_5c6a93_advertised_cyber_programs(json_t *entry);
const char *w11_codexservi_5c6a93_daybreak_program(void);
#endif /* NOC_W11_CODEXSERVI_5C6A93_H */
