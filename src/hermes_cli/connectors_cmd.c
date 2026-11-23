/*
 * connectors_cmd.c — C home for upstream Hermes hermes_cli/connectors_cmd.py.
 *
 * PoP: w12_connectors_5682b3_account_name  @ hermes_cli/connectors_cmd.py:_account_name
 * PoP: w12_connectors_5682b3_display_name  @ hermes_cli/connectors_cmd.py:_display_name
 * PoP: w12_connectors_5682b3_names_hint    @ hermes_cli/connectors_cmd.py:_names_hint
 * PoP: w12_connectors_5682b3_valid_alias   @ hermes_cli/connectors_cmd.py:_valid_alias
 *
 * Pure CLI display/validation helpers. No network / portal access.
 */
#include "hermes_json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w12_connectors_5682b3.h"

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY) return json_len(v) ? 1 : 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) ? 1 : 0;
    return 1;
}

/* str(value) for the scalar types the CLI's row fields carry. */
static void py_str(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); return; }
    if (v->type == JSON_STRING) { snprintf(out, cap, "%s", v->str_val ? v->str_val : ""); return; }
    if (v->type == JSON_BOOL) { snprintf(out, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.0e18) snprintf(out, cap, "%.0f", d);
        else snprintf(out, cap, "%.17g", d);
        return;
    }
    char *s = json_dumps(v, 0);
    snprintf(out, cap, "%s", s ? s : "");
    free(s);
}

/* Python repr() of a str (quote choice + escaping). */
static void py_repr_str(const char *s, char *out, size_t cap) {
    char q = (strchr(s, '\'') && !strchr(s, '"')) ? '"' : '\'';
    size_t n = 0;
    if (n < cap) out[n++] = q;
    for (; *s && n + 1 < cap; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '\\' || c == (unsigned char)q) {
            if (n + 2 < cap) { out[n++] = '\\'; out[n++] = (char)c; }
        } else if (c == '\n') { if (n + 2 < cap) { out[n++] = '\\'; out[n++] = 'n'; } }
        else if (c == '\t') { if (n + 2 < cap) { out[n++] = '\\'; out[n++] = 't'; } }
        else if (c == '\r') { if (n + 2 < cap) { out[n++] = '\\'; out[n++] = 'r'; } }
        else out[n++] = (char)c;
    }
    if (n < cap) out[n++] = q;
    out[n] = 0;
}

json_t *w12_connectors_5682b3_account_name(const json_t *row) {
    const json_t *cand = json_is_object(row) ? json_obj_get(row, "alias") : NULL;
    if (!py_truthy(cand)) cand = json_is_object(row) ? json_obj_get(row, "label") : NULL;
    if (!py_truthy(cand)) cand = json_is_object(row) ? json_obj_get(row, "connectionId") : NULL;
    if (!py_truthy(cand)) return json_string("");
    char buf[2048];
    py_str(cand, buf, sizeof buf);
    return json_string(buf);
}

json_t *w12_connectors_5682b3_display_name(const json_t *row) {
    const json_t *alias = json_is_object(row) ? json_obj_get(row, "alias") : NULL;
    if (py_truthy(alias)) {
        char buf[2048];
        py_str(alias, buf, sizeof buf);
        return json_string(buf);
    }
    const json_t *cand = json_is_object(row) ? json_obj_get(row, "label") : NULL;
    if (!py_truthy(cand)) cand = json_is_object(row) ? json_obj_get(row, "connectionId") : NULL;
    char base[2048], out[4096];
    py_str(cand, base, sizeof base); /* NULL -> "None" */
    snprintf(out, sizeof out, "%s (unnamed)", base);
    return json_string(out);
}

json_t *w12_connectors_5682b3_names_hint(const char *app, const json_t *rows) {
    char **names = NULL;
    size_t n = 0, cap = 0;
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); ++i) {
            json_t *nm = w12_connectors_5682b3_account_name(json_get(rows, i));
            const char *s = nm->str_val ? nm->str_val : "";
            int seen = 0;
            for (size_t j = 0; j < n; ++j) if (!strcmp(names[j], s)) { seen = 1; break; }
            if (!seen) {
                if (n == cap) {
                    cap = cap ? cap * 2 : 8;
                    char **tmp = (char **)realloc(names, cap * sizeof *tmp);
                    if (!tmp) { json_free(nm); break; }
                    names = tmp;
                }
                names[n++] = strdup(s);
            }
            json_free(nm);
        }
    }
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            if (strcmp(names[i], names[j]) > 0) { char *t = names[i]; names[i] = names[j]; names[j] = t; }
    char *joined = (char *)calloc(32768, 1);
    size_t used = 0;
    for (size_t i = 0; i < n; ++i) {
        size_t l = strlen(names[i]);
        if (used + l + 2 < 32768) {
            if (used) { joined[used++] = ','; joined[used++] = ' '; }
            memcpy(joined + used, names[i], l); used += l;
        }
        free(names[i]);
    }
    free(names);
    char out[32768];
    if (n) snprintf(out, sizeof out, "Its accounts are: %s.", joined);
    else snprintf(out, sizeof out, "You have no %s account.", app ? app : "");
    free(joined);
    return json_string(out);
}

json_t *w12_connectors_5682b3_valid_alias(const char *alias) {
    const char *s = alias ? alias : "";
    size_t n = strlen(s);
    int ok = (n >= 1 && n <= 32);
    if (ok) {
        char c = s[0];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) ok = 0;
    }
    for (size_t i = 1; ok && i < n; ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) ok = 0;
    }
    json_t *out = json_object();
    if (ok) {
        json_set(out, "ok", json_bool(1));
        json_set(out, "alias", json_string(s));
    } else {
        char repr[2048], msg[4096];
        py_repr_str(s, repr, sizeof repr);
        snprintf(msg, sizeof msg,
                 "Invalid name %s. Use 1 to 32 lowercase letters, digits or hyphens, "
                 "starting with a letter or digit.", repr);
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string(msg));
    }
    return out;
}
