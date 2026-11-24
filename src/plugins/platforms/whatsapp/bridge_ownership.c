/*
 * bridge_ownership.c - C11 port of upstream
 * plugins/platforms/whatsapp/bridge_ownership.py (gate w24_bridgeowne_2dcbff).
 *
 * PoP: w24_bridgeowne_2dcbff_standalone_port @ plugins/platforms/whatsapp/bridge_ownership.py:standalone_bridge_port
 * PoP: w24_bridgeowne_2dcbff_secondary_port @ plugins/platforms/whatsapp/bridge_ownership.py:secondary_bridge_port
 * PoP: w24_bridgeowne_2dcbff_read_port_record @ plugins/platforms/whatsapp/bridge_ownership.py:_read_port_record
 * PoP: w24_bridgeowne_2dcbff_constants @ plugins/platforms/whatsapp/bridge_ownership.py:SECONDARY_PORT_FIRST/SECONDARY_PORT_LAST
 *
 * The filesystem and the socket are seams: a record is (exists, text), the bind
 * probe is the caller's busy set, and the sibling glob is the caller's claimed
 * set.  What is ported is every decision those values drive -- explicit beats
 * record beats default, a record beats the scan, the scan takes the lowest free
 * port in the window, and the 1..65535 gate runs after resolution.  Error texts
 * are the ValueError messages, as {"error": "..."}; the record-parse failure is
 * classified because its message names the profile path.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_bridgeowne_2dcbff.h"

/* Python truthiness of a JSON value; NULL is an absent key (None/false). */
static int bo_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int bo_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* int(value) for the shapes a port arrives in: bool, int/float (truncated
 * toward zero, as int() does) and a decimal string with surrounding space.
 * Writes Python's ValueError text into err when the value is not a number. */
static int bo_int(const json_t *v, long long *out, char *err, size_t errcap) {
    const char *s, *p;
    int neg = 0, digits = 0;
    long long acc = 0;

    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v && v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
    p = s;
    while (bo_space(*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') {
        if (acc < 1000000000000000000LL) acc = acc * 10 + (*p - '0');
        digits++;
        p++;
    }
    while (bo_space(*p)) p++;
    if (!digits || *p) {
        snprintf(err, errcap, "invalid literal for int() with base 10: '%s'", s);
        return 0;
    }
    *out = neg ? -acc : acc;
    return 1;
}

static json_t *bo_error(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    return o;
}

static json_t *bo_invalid_port(long long port) {
    char buf[64];
    snprintf(buf, sizeof buf, "Invalid bridge port %lld", port);
    return bo_error(buf);
}

static int bo_in_window(long long port) {
    return port >= 1 && port <= 65535;
}

static int bo_list_has(const json_t *list, long long want) {
    size_t i, n;
    if (!list || list->type != JSON_ARRAY) return 0;
    n = json_len(list);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(list, i);
        double d;
        if (!v) continue;
        if (v->type == JSON_NUMBER) d = v->num_val;
        else if (v->type == JSON_BOOL) d = v->bool_val ? 1.0 : 0.0;
        else continue;
        if (d == (double)want) return 1;
    }
    return 0;
}

/* Resolve explicit -> record ("record" meaning exists + text).  Returns 1 and
 * the port on success; 0 with *err set, or with *err left NULL to say "no
 * record and no explicit: the caller scans". */
static int bo_resolve(const json_t *req, long long *port, json_t **err,
                      int allow_default) {
    const json_t *explicit = json_obj_get(req, "explicit");
    const json_t *text;

    if (explicit && explicit->type != JSON_NULL) {
        char buf[512];
        if (!bo_int(explicit, port, buf, sizeof buf)) { *err = bo_error(buf); return 0; }
        return 1;
    }
    if (!bo_truthy(json_obj_get(req, "record_exists"))) {
        if (!allow_default) { *err = NULL; return 0; }
        *port = W24_BRIDGEOWNE_2DCBFF_DEFAULT_PORT;
        return 1;
    }
    text = json_obj_get(req, "record_text");
    {
        char buf[512];
        if (!bo_int(text, port, buf, sizeof buf)) { *err = bo_error("not_a_port"); return 0; }
    }
    return 1;
}

json_t *w24_bridgeowne_2dcbff_standalone_port(const json_t *req) {
    long long port = 0;
    json_t *err = NULL;

    if (!bo_resolve(req, &port, &err, 1)) return err ? err : bo_error("not_a_port");
    if (!bo_in_window(port)) return bo_invalid_port(port);
    return json_number((double)port);
}

json_t *w24_bridgeowne_2dcbff_secondary_port(const json_t *req) {
    long long port = 0;
    json_t *err = NULL;
    const json_t *explicit = json_obj_get(req, "explicit");
    int has_record = bo_truthy(json_obj_get(req, "record_exists"));
    long long p;

    if ((explicit && explicit->type != JSON_NULL) || has_record) {
        if (!bo_resolve(req, &port, &err, 0)) return err ? err : bo_error("not_a_port");
        if (!bo_in_window(port)) return bo_invalid_port(port);
        return json_number((double)port);
    }
    /* no explicit, no record: the first port in the window that no sibling
     * claims and nothing is listening on */
    for (p = W24_BRIDGEOWNE_2DCBFF_SECONDARY_PORT_FIRST;
         p <= W24_BRIDGEOWNE_2DCBFF_SECONDARY_PORT_LAST; p++) {
        if (bo_list_has(json_obj_get(req, "claimed"), p)) continue;
        if (bo_list_has(json_obj_get(req, "busy"), p)) continue;
        return json_number((double)p);
    }
    {
        char buf[96];
        snprintf(buf, sizeof buf, "No free bridge port in %d..%d",
                 W24_BRIDGEOWNE_2DCBFF_SECONDARY_PORT_FIRST,
                 W24_BRIDGEOWNE_2DCBFF_SECONDARY_PORT_LAST);
        return bo_error(buf);
    }
}

json_t *w24_bridgeowne_2dcbff_read_port_record(const json_t *req) {
    long long port = 0;
    char buf[512];
    if (!bo_int(json_obj_get(req, "text"), &port, buf, sizeof buf))
        return bo_error("not_a_port");
    return json_number((double)port);
}

json_t *w24_bridgeowne_2dcbff_constants(void) {
    json_t *o = json_object();
    json_set(o, "secondary_port_first",
             json_number((double)W24_BRIDGEOWNE_2DCBFF_SECONDARY_PORT_FIRST));
    json_set(o, "secondary_port_last",
             json_number((double)W24_BRIDGEOWNE_2DCBFF_SECONDARY_PORT_LAST));
    json_set(o, "default_port", json_number((double)W24_BRIDGEOWNE_2DCBFF_DEFAULT_PORT));
    return o;
}
