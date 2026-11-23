/*
 * PoP: w10_kanbanpars_0b0fba_nonnegative_int @ hermes_cli/kanban_parser.py:_nonnegative_int
 * PoP: w10_kanbanpars_0b0fba_bulk_ids        @ hermes_cli/kanban_parser.py:_bulk_ids
 * PoP: w10_kanbanpars_0b0fba_reason          @ hermes_cli/kanban_parser.py:_reason
 * PoP: w10_kanbanpars_0b0fba_json_flag       @ hermes_cli/kanban_parser.py:_json_flag
 */
#include "hermes_json.h"
#include "noc_w10_kanbanpars_0b0fba.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ws(char c) { return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'; }

static json_t *err_obj(const char *msg) {
    json_t *o = json_object();
    json_set(o, "err", json_string(msg));
    return o;
}

json_t *w10_kanbanpars_0b0fba_nonnegative_int(const char *value) {
    if (!value) value = "";
    const char *p = value, *e = value + strlen(value);
    while (p < e && ws(*p)) p++;
    while (e > p && ws(e[-1])) e--;
    int neg = 0;
    if (p < e && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
    if (p >= e) return err_obj("must be an integer");
    /* Python int(): digits with underscores allowed strictly between digits. */
    char *digits = (char *)malloc((size_t)(e - p) + 1);
    if (!digits) return err_obj("must be an integer");
    size_t dn = 0;
    int prev_digit = 0;
    for (const char *q = p; q < e; q++) {
        if (isdigit((unsigned char)*q)) { digits[dn++] = *q; prev_digit = 1; }
        else if (*q == '_') {
            if (!prev_digit || q + 1 >= e || !isdigit((unsigned char)q[1])) {
                free(digits); return err_obj("must be an integer");
            }
            prev_digit = 0;
        } else { free(digits); return err_obj("must be an integer"); }
    }
    digits[dn] = 0;
    if (dn == 0) { free(digits); return err_obj("must be an integer"); }
    size_t z = 0;
    while (z < dn && digits[z] == '0') z++;
    const char *canon = (z == dn) ? "0" : digits + z;
    json_t *out = json_object();
    if (neg && strcmp(canon, "0")) {
        json_set(out, "err", json_string("retention days must be >= 0 (0 disables that sweep)"));
    } else {
        json_set(out, "ok", json_string(canon));
    }
    free(digits);
    return out;
}

static json_t *arg_spec(const char *flag, const char *nargs, const char *help) {
    json_t *spec = json_array();
    json_append(spec, json_array());
    json_append(json_array_get(spec, 0), json_string(flag));
    json_t *kw = json_object();
    if (nargs) json_set(kw, "nargs", json_string(nargs));
    if (help) json_set(kw, "help", json_string(help));
    json_append(spec, kw);
    return spec;
}

json_t *w10_kanbanpars_0b0fba_bulk_ids(const char *verb) {
    char help[256];
    snprintf(help, sizeof(help),
             "Additional task ids to %s with the same reason (bulk mode)",
             verb ? verb : "");
    return arg_spec("--ids", "+", help);
}

json_t *w10_kanbanpars_0b0fba_reason(const char *help) {
    return arg_spec("--reason", NULL, help ? help : "");
}

json_t *w10_kanbanpars_0b0fba_json_flag(json_t *kw) {
    json_t *spec = json_array();
    json_append(spec, json_array());
    json_append(json_array_get(spec, 0), json_string("--json"));
    json_t *d = json_object();
    json_set(d, "action", json_string("store_true"));
    if (kw && kw->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(kw); i++)
            json_set(d, json_object_get_key_at(kw, i),
                     json_copy(json_object_get_at(kw, i)));
    json_append(spec, d);
    return spec;
}
