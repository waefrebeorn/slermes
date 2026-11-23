/*
 * mcp_tool_costs.c — faithful C11 port of the pure cost helpers in
 * tools/mcp_tool_costs.py (gate w7_mcptoolcos_c8e8a5).
 *
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_fmt @ tools/mcp_tool_costs.py:_fmt
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_qty @ tools/mcp_tool_costs.py:_qty
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_parse_cost @ tools/mcp_tool_costs.py:_parse_cost
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_parse_cost_policy @ tools/mcp_tool_costs.py:parse_cost_policy
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_unknown_cost_keys @ tools/mcp_tool_costs.py:unknown_cost_keys
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_charge_sentence @ tools/mcp_tool_costs.py:Charge.cost_sentence
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_spend_sentence @ tools/mcp_tool_costs.py:Charge.spend_sentence
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_charge_lines @ tools/mcp_tool_costs.py:Charge.lines
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_over_limit @ tools/mcp_tool_costs.py:Charge.over_limit
 * PoP: w7_mcptoolcos_c8e8a5_mcptcost_charge_refusal @ tools/mcp_tool_costs.py:Charge.refusal
 *
 * Validation failures render as {"error": true} (the oracle does the same
 * for a raised ValueError), so exact Python exception text is not required.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w7_mcptoolcos_c8e8a5.h"

static int is_type(const json_t *v, json_type_t t) { return v && v->type == t; }
static json_t *err_obj(void) { json_t *o = json_object(); json_set(o, "error", json_bool(1)); return o; }

static void trim_str(const char *s, char *out, size_t cap) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n >= cap) n = cap - 1;
    memcpy(out, s, n); out[n] = '\0';
}

/* Python f"{int(amount):,}" / f"{amount:,.2f}".rstrip("0").rstrip(".") */
static void fmt(double amount, char *out, size_t cap) {
    char raw[160];
    if (amount == floor(amount)) {
        snprintf(raw, sizeof(raw), "%lld", (long long)llround(amount));
    } else {
        snprintf(raw, sizeof(raw), "%.2f", amount);
        size_t n = strlen(raw);
        while (n > 0 && raw[n - 1] == '0') n--;
        if (n > 0 && raw[n - 1] == '.') n--;
        raw[n] = '\0';
    }
    const char *dot = strchr(raw, '.');
    size_t intlen = dot ? (size_t)(dot - raw) : strlen(raw);
    size_t o = 0;
    for (size_t i = 0; i < intlen && o + 1 < cap; i++) {
        if (i > 0 && (intlen - i) % 3 == 0 && o + 1 < cap) out[o++] = ',';
        if (o + 1 < cap) out[o++] = raw[i];
    }
    if (dot) {
        size_t n = strlen(dot);
        if (o + n >= cap) n = cap - o - 1;
        memcpy(out + o, dot, n); o += n;
    }
    out[o] = '\0';
}

/* _qty: a plural unit loses its "s" for exactly one. */
static void qty(double amount, const char *unit, char *out, size_t cap) {
    char fbuf[160], ubuf[256];
    fmt(amount, fbuf, sizeof(fbuf));
    if (!unit) unit = "";
    size_t ulen = strlen(unit);
    if (amount == 1 && ulen > 0 && unit[ulen - 1] == 's') ulen--;
    if (ulen >= sizeof(ubuf)) ulen = sizeof(ubuf) - 1;
    memcpy(ubuf, unit, ulen); ubuf[ulen] = '\0';
    snprintf(out, cap, "%s %s", fbuf, ubuf);
}

static int number(const json_t *v, double *out) {
    if (!is_type(v, JSON_NUMBER)) return 0; /* bool is JSON_BOOL, rejected */
    double d = v->num_val;
    if (!isfinite(d) || d < 0) return 0;
    *out = d;
    return 1;
}

/* _parse_cost -> 1 (out = ToolCost object) or 0 (error). */
static int parse_cost(const json_t *v, json_t **out) {
    const json_t *value = v;
    char notebuf[512]; const char *note = "";
    if (is_type(v, JSON_OBJECT)) {
        json_t *credits = json_obj_get(v, "credits");
        if (!credits) return 0;
        for (size_t i = 0; i < json_object_size(v); i++) {
            const char *k = json_object_get_key_at(v, i);
            if (!k || (strcmp(k, "credits") && strcmp(k, "note"))) return 0;
        }
        json_t *nv = json_obj_get(v, "note");
        if (nv) {
            if (!is_type(nv, JSON_STRING)) return 0;
            note = nv->str_val ? nv->str_val : "";
        }
        value = credits;
    }
    double low, high;
    if (is_type(value, JSON_ARRAY)) {
        if (json_len(value) != 2) return 0;
        if (!number(json_get(value, 0), &low)) return 0;
        if (!number(json_get(value, 1), &high)) return 0;
        if (low > high) return 0;
    } else {
        if (!number(value, &low)) return 0;
        high = low;
    }
    trim_str(note, notebuf, sizeof(notebuf));
    json_t *obj = json_object();
    json_set(obj, "low", json_number(low));
    json_set(obj, "high", json_number(high));
    json_set(obj, "note", json_string(notebuf));
    *out = obj;
    return 1;
}

struct json_t *mcptcost_fmt(double amount) {
    char buf[256]; fmt(amount, buf, sizeof(buf));
    return json_string(buf);
}

struct json_t *mcptcost_qty(double amount, const char *unit) {
    char buf[512]; qty(amount, unit, buf, sizeof(buf));
    return json_string(buf);
}

struct json_t *mcptcost_parse_cost(const struct json_t *value, const char *where) {
    (void)where;
    json_t *cost = NULL;
    if (!parse_cost(value, &cost)) return err_obj();
    return cost;
}

struct json_t *mcptcost_parse_cost_policy(const struct json_t *costs,
                                          const struct json_t *spend_limit,
                                          const struct json_t *cost_unit,
                                          const char *where) {
    (void)where;
    if (costs && costs->type != JSON_NULL && costs->type != JSON_OBJECT) return err_obj();
    json_t *table = json_object();
    if (is_type(costs, JSON_OBJECT)) {
        for (size_t i = 0; i < json_object_size(costs); i++) {
            const char *key = json_object_get_key_at(costs, i);
            if (!key || !*key) { json_free(table); return err_obj(); }
            json_t *cost = NULL;
            if (!parse_cost(json_object_get_at(costs, i), &cost)) { json_free(table); return err_obj(); }
            json_set(table, key, cost);
        }
    }
    double limit = 0.0;
    if (spend_limit && spend_limit->type != JSON_NULL) {
        if (!number(spend_limit, &limit)) { json_free(table); return err_obj(); }
    }
    char ubuf[128]; const char *unit = "credits";
    if (cost_unit && cost_unit->type != JSON_NULL) {
        if (!is_type(cost_unit, JSON_STRING) || strlen(cost_unit->str_val ? cost_unit->str_val : "") > 32) {
            json_free(table); return err_obj();
        }
        trim_str(cost_unit->str_val, ubuf, sizeof(ubuf));
        if (!*ubuf) { json_free(table); return err_obj(); }
        unit = ubuf;
    }
    int any = 0;
    for (size_t i = 0; i < json_object_size(table); i++) {
        json_t *c = json_object_get_at(table, i);
        if (c && c->type == JSON_OBJECT) {
            json_t *h = json_obj_get(c, "high");
            if (h && h->type == JSON_NUMBER && h->num_val > 0) { any = 1; break; }
        }
    }
    if (!any) { json_free(table); return json_null(); }
    json_t *out = json_object();
    json_set(out, "costs", table);
    json_set(out, "limit", json_number(limit));
    json_set(out, "unit", json_string(unit));
    return out;
}

struct json_t *mcptcost_unknown_cost_keys(const struct json_t *costs,
                                          const struct json_t *tool_names) {
    json_t *out = json_array();
    if (!is_type(costs, JSON_OBJECT) || !is_type(tool_names, JSON_ARRAY) || json_len(tool_names) == 0)
        return out;
    for (size_t i = 0; i < json_object_size(costs); i++) {
        const char *k = json_object_get_key_at(costs, i);
        int offered = 0;
        for (size_t j = 0; j < json_len(tool_names); j++) {
            const char *n = json_string_value(json_get(tool_names, j));
            if (n && k && !strcmp(n, k)) { offered = 1; break; }
        }
        if (!offered) json_append(out, json_string(k ? k : ""));
    }
    /* sorted */
    for (size_t i = 0; i + 1 < json_len(out); i++)
        for (size_t j = 0; j + 1 + i < json_len(out); j++)
            if (strcmp(json_string_value(json_get(out, j)),
                       json_string_value(json_get(out, j + 1))) > 0) {
                json_t *a = json_copy(json_get(out, j));
                json_t *b = json_copy(json_get(out, j + 1));
                json_array_set(out, j, b);
                json_array_set(out, j + 1, a);
            }
    return out;
}

struct json_t *mcptcost_charge_sentence(double low, double high, const char *note, const char *unit) {
    char q[256], text[512], out[1024];
    qty(high, unit, q, sizeof(q));
    if (low == high) snprintf(text, sizeof(text), "Costs about %s", q);
    else {
        char f[256]; fmt(low, f, sizeof(f));
        snprintf(text, sizeof(text), "Costs %s-%s", f, q);
    }
    if (note && *note) snprintf(out, sizeof(out), "%s (%s).", text, note);
    else snprintf(out, sizeof(out), "%s.", text);
    return json_string(out);
}

struct json_t *mcptcost_spend_sentence(double spent, const char *server, double limit, const char *unit) {
    char out[1024]; const char *srv = server ? server : "";
    if (spent > 0) {
        char q[256], cap[256];
        qty(spent, unit, q, sizeof(q));
        if (limit > 0) { char f[256]; fmt(limit, f, sizeof(f)); snprintf(cap, sizeof(cap), " (limit %s)", f); }
        else cap[0] = '\0';
        snprintf(out, sizeof(out), "This chat has used about %s on '%s'%s.", q, srv, cap);
    } else if (limit > 0) {
        char q[256]; qty(limit, unit, q, sizeof(q));
        snprintf(out, sizeof(out), "Spend limit for this chat on '%s': %s.", srv, q);
    } else {
        out[0] = '\0';
    }
    return json_string(out);
}

struct json_t *mcptcost_charge_lines(double low, double high, const char *note, const char *unit,
                                     double spent, const char *server, double limit) {
    json_t *out = json_array();
    json_t *a = mcptcost_charge_sentence(low, high, note, unit);
    if (a->type == JSON_STRING && a->str_val && *a->str_val) json_append(out, a); else json_free(a);
    json_t *b = mcptcost_spend_sentence(spent, server, limit, unit);
    if (b->type == JSON_STRING && b->str_val && *b->str_val) json_append(out, b); else json_free(b);
    return out;
}

struct json_t *mcptcost_over_limit(double limit, double spent, double high) {
    return json_bool(limit > 0 && (spent + high) > limit);
}

struct json_t *mcptcost_charge_refusal(const char *tool, const char *server, double spent,
                                       double limit, double high, const char *unit) {
    char qs[256], ql[256], qh[256];
    qty(spent, unit, qs, sizeof(qs));
    qty(limit, unit, ql, sizeof(ql));
    qty(high, unit, qh, sizeof(qh));
    char out[2048];
    snprintf(out, sizeof(out),
             "MCP tool '%s' on server '%s' was NOT run: this chat has used about %s on '%s' and its spend limit is %s. "
             "This call (about %s) needed an explicit approval and did not get one. Do not retry; tell the user the spend limit was reached.",
             tool ? tool : "", server ? server : "", qs, server ? server : "", ql, qh);
    return json_string(out);
}
