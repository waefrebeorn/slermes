#ifndef NOC_W7_MCPTOOLCOS_C8E8A5_H
#define NOC_W7_MCPTOOLCOS_C8E8A5_H

#include "hermes_json.h"

/* Public C API for the pure cost helpers of tools/mcp_tool_costs.py
 * (gate w7_mcptoolcos_c8e8a5). mcptcost_ prefix avoids collisions with the
 * other mcp_tool_* symbols. Validation failures render as {"error": true}. */
struct json_t *mcptcost_fmt(double amount);
struct json_t *mcptcost_qty(double amount, const char *unit);
struct json_t *mcptcost_parse_cost(const struct json_t *value, const char *where);
struct json_t *mcptcost_parse_cost_policy(const struct json_t *costs,
                                          const struct json_t *spend_limit,
                                          const struct json_t *cost_unit,
                                          const char *where);
struct json_t *mcptcost_unknown_cost_keys(const struct json_t *costs,
                                          const struct json_t *tool_names);
struct json_t *mcptcost_charge_sentence(double low, double high, const char *note, const char *unit);
struct json_t *mcptcost_spend_sentence(double spent, const char *server, double limit, const char *unit);
struct json_t *mcptcost_charge_lines(double low, double high, const char *note, const char *unit,
                                     double spent, const char *server, double limit);
struct json_t *mcptcost_over_limit(double limit, double spent, double high);
struct json_t *mcptcost_charge_refusal(const char *tool, const char *server, double spent,
                                       double limit, double high, const char *unit);

#endif /* NOC_W7_MCPTOOLCOS_C8E8A5_H */
