/* Port gate harness for tools/mcp_tool_costs.py (gate w7_mcptoolcos_c8e8a5). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_mcptoolcos_c8e8a5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *where = json_get_str(in, "where", "srv");
        const char *unit = json_get_str(in, "unit", "credits");
        const char *note = json_get_str(in, "note", "");
        const char *server = json_get_str(in, "server", "");
        const char *tool = json_get_str(in, "tool", "tool");
        double amount = json_get_num(in, "amount", 0);
        double low = json_get_num(in, "low", 0);
        double high = json_get_num(in, "high", 0);
        double spent = json_get_num(in, "spent", 0);
        double limit = json_get_num(in, "limit", 0);
        json_t *out = NULL;
        if (!strcmp(func, "fmt")) {
            out = mcptcost_fmt(amount);
        } else if (!strcmp(func, "qty")) {
            out = mcptcost_qty(amount, unit);
        } else if (!strcmp(func, "parse_cost")) {
            out = mcptcost_parse_cost(json_obj_get(in, "value"), where);
        } else if (!strcmp(func, "parse_cost_policy")) {
            out = mcptcost_parse_cost_policy(json_obj_get(in, "costs"),
                                             json_obj_get(in, "spend_limit"),
                                             json_obj_get(in, "cost_unit"), where);
        } else if (!strcmp(func, "unknown_cost_keys")) {
            out = mcptcost_unknown_cost_keys(json_obj_get(in, "costs"), json_obj_get(in, "tool_names"));
        } else if (!strcmp(func, "charge_sentence")) {
            out = mcptcost_charge_sentence(low, high, note, unit);
        } else if (!strcmp(func, "spend_sentence")) {
            out = mcptcost_spend_sentence(spent, server, limit, unit);
        } else if (!strcmp(func, "charge_lines")) {
            out = mcptcost_charge_lines(low, high, note, unit, spent, server, limit);
        } else if (!strcmp(func, "over_limit")) {
            out = mcptcost_over_limit(limit, spent, high);
        } else if (!strcmp(func, "charge_refusal")) {
            out = mcptcost_charge_refusal(tool, server, spent, limit, high, unit);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
