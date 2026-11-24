/*
 * bridge.c - C11 port of upstream tools/connectors/gateway/bridge.py
 * (gate w26_bridge_ca0da8).
 *
 * PoP: w26_bridge_ca0da8_leg_failure @ tools/connectors/gateway/bridge.py:_leg_failure
 * PoP: w26_bridge_ca0da8_search_leg @ tools/connectors/gateway/bridge.py:connector_search_hits
 * PoP: w26_bridge_ca0da8_describe_leg @ tools/connectors/gateway/bridge.py:connector_describe
 * PoP: w26_bridge_ca0da8_fallback_plan @ tools/connectors/gateway/bridge.py:run_remote:<literal retry selection>
 * PoP: w26_bridge_ca0da8_constants @ tools/connectors/gateway/bridge.py:<module literals>
 *
 * What is ported is the bridge's decision surface: which failure a raised
 * gateway error becomes (the D32 code the model sees), the search and describe
 * legs with the gateway client reduced to a fixed answer, the model-facing
 * parameters a describe returns (a file_uploadable input becomes a path
 * string), and the literal-vendor-slug retry plan run_remote builds for
 * confirmed misses.  The client, the availability probe, the file staging and
 * the execute round-trip are seams and stay with the caller.  Name parsing and
 * the two slug candidates are re-spelled here because this gate links alone.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_bridge_ca0da8.h"

#define BR_PREFIX "connectors__"
#define BR_PREFIX_HEAD "connectors"
#define BR_FILE_INPUT_NOTE "Pass a local file path."
#define BR_SIGN_IN_EXPIRED "sign_in_expired"
#define BR_UNREACHABLE "unreachable"
#define BR_NOT_SET_UP "not_set_up"
#define BR_NOT_SET_UP_MESSAGE \
    "Hosted connectors are not set up for this user yet. " \
    "Call manage_connections to connect the app first; that sets them up."

typedef enum { BR_AUTH, BR_UNAVAILABLE, BR_OTHER } br_kind_t;

static const char *const BR_TOKEN_REJECTED[3] = {
    "UNAUTHORIZED", "INVALID_TOKEN", "TOKEN_EXPIRED"
};

/* --- string helpers ----------------------------------------------------- */

static char *br_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *br_upper(const char *s) {
    char *p = br_dup(s);
    if (!p) return NULL;
    for (char *q = p; *q; q++)
        if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 'a' + 'A');
    return p;
}

static int br_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int br_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int br_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

/* Python truthiness of a JSON value. */
static int br_truthy(const json_t *v) {
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

/* str() of a number: shortest text that round-trips (what Python prints). */
static void br_num_text(double v, char *out, size_t cap) {
    char tmp[64];
    int p;
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) {
        snprintf(out, cap, "%.0f", v);
        return;
    }
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) { snprintf(out, cap, "%s", tmp); return; }
    }
    snprintf(out, cap, "%.17g", v);
}

typedef struct { char *b; size_t n, cap; int oom; } br_buf;

static void br_bput(br_buf *s, const char *t, size_t len) {
    if (s->oom) return;
    if (s->n + len + 1 > s->cap) {
        size_t cap = s->cap ? s->cap : 64;
        char *nb;
        while (cap < s->n + len + 1) cap *= 2;
        nb = (char *)realloc(s->b, cap);
        if (!nb) { s->oom = 1; return; }
        s->b = nb;
        s->cap = cap;
    }
    memcpy(s->b + s->n, t, len);
    s->n += len;
    s->b[s->n] = '\0';
}

static void br_bputs(br_buf *s, const char *t) { br_bput(s, t, strlen(t)); }

static void br_repr(br_buf *s, const json_t *v);

static void br_repr_str(br_buf *s, const char *t) {
    br_bput(s, "'", 1);
    for (const char *p = t; *p; p++) {
        if (*p == '\'') br_bputs(s, "\\'");
        else if (*p == '\\') br_bputs(s, "\\\\");
        else if (*p == '\n') br_bputs(s, "\\n");
        else if (*p == '\t') br_bputs(s, "\\t");
        else br_bput(s, p, 1);
    }
    br_bput(s, "'", 1);
}

static void br_repr(br_buf *s, const json_t *v) {
    size_t i, n;
    char num[64];
    if (!v) { br_bputs(s, "None"); return; }
    switch (v->type) {
    case JSON_NULL: br_bputs(s, "None"); return;
    case JSON_BOOL: br_bputs(s, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: br_num_text(v->num_val, num, sizeof(num)); br_bputs(s, num); return;
    case JSON_STRING: br_repr_str(s, v->str_val ? v->str_val : ""); return;
    case JSON_ARRAY:
        br_bput(s, "[", 1);
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) br_bputs(s, ", ");
            br_repr(s, json_get(v, i));
        }
        br_bput(s, "]", 1);
        return;
    case JSON_OBJECT:
        br_bput(s, "{", 1);
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            if (i) br_bputs(s, ", ");
            br_repr_str(s, json_object_get_key_at(v, i));
            br_bputs(s, ": ");
            br_repr(s, json_object_get_at(v, i));
        }
        br_bput(s, "}", 1);
        return;
    }
}

/* str(value); caller frees. */
static char *br_pystr(const json_t *v) {
    br_buf s = {NULL, 0, 0, 0};
    if (!v) return br_dup("None");
    if (v->type == JSON_STRING) return br_dup(v->str_val ? v->str_val : "");
    br_repr(&s, v);
    if (s.oom) { free(s.b); return NULL; }
    if (!s.b) return br_dup("");
    return s.b;
}

/* Python str.strip() over ASCII whitespace. */
static char *br_strip(char *s) {
    char *end;
    while (*s && isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

/* --- the failure mapping ------------------------------------------------ */

static int br_in_token_set(const char *upper) {
    size_t i;
    for (i = 0; i < sizeof(BR_TOKEN_REJECTED) / sizeof(BR_TOKEN_REJECTED[0]); i++)
        if (strcmp(upper, BR_TOKEN_REJECTED[i]) == 0) return 1;
    return 0;
}

static json_t *br_leg_failure(br_kind_t kind, const json_t *code, const json_t *status) {
    char *text, *upper;
    double st;

    if (kind == BR_OTHER || kind == BR_UNAVAILABLE) return json_string(BR_UNREACHABLE);
    /* isinstance(exc, GatewayAuthError) and exc.code == NOT_SET_UP */
    if (br_is_str(code) && strcmp(code->str_val ? code->str_val : "", BR_NOT_SET_UP) == 0)
        return json_string(BR_NOT_SET_UP);
    if (status && (status->type == JSON_NUMBER || status->type == JSON_BOOL)) {
        st = status->type == JSON_BOOL ? (status->bool_val ? 1.0 : 0.0) : status->num_val;
        if (st == 401.0) return json_string(BR_SIGN_IN_EXPIRED);
    }
    text = br_pystr(code);
    if (!text) return json_null();
    upper = br_upper(text);
    free(text);
    if (!upper) return json_null();
    if (br_in_token_set(upper)) { free(upper); return json_string(BR_SIGN_IN_EXPIRED); }
    free(upper);
    return json_null();
}

static br_kind_t br_kind_of(const json_t *raise) {
    const char *k = json_obj_get(raise, "kind") ? json_get_str(raise, "kind", "") : "";
    if (!strcmp(k, "auth")) return BR_AUTH;
    if (!strcmp(k, "unavailable")) return BR_UNAVAILABLE;
    return BR_OTHER;
}

json_t *w26_bridge_ca0da8_leg_failure(const json_t *req) {
    return br_leg_failure(br_kind_of(req), json_obj_get(req, "code"), json_obj_get(req, "status"));
}

/* --- connector names ---------------------------------------------------- */

typedef struct {
    char *raw, *connector, *tool;
} br_name;

/* name.split("__", 2) with the prefix and both parts non-empty. */
static int br_parse_name(const json_t *v, br_name *out) {
    const char *s, *p1, *p2;
    size_t plen, clen, tlen;

    memset(out, 0, sizeof(*out));
    if (!br_is_str(v)) return 0;
    s = v->str_val ? v->str_val : "";
    p1 = strstr(s, "__");
    if (!p1) return 0;
    p2 = strstr(p1 + 2, "__");
    if (!p2) return 0;
    plen = (size_t)(p1 - s);
    clen = (size_t)(p2 - (p1 + 2));
    tlen = strlen(p2 + 2);
    if (plen != sizeof(BR_PREFIX_HEAD) - 1 || memcmp(s, BR_PREFIX_HEAD, plen) != 0) return 0;
    if (clen == 0 || tlen == 0) return 0;
    out->raw = br_dup(s);
    out->connector = (char *)malloc(clen + 1);
    out->tool = (char *)malloc(tlen + 1);
    if (!out->raw || !out->connector || !out->tool) return 0;
    memcpy(out->connector, p1 + 2, clen);
    out->connector[clen] = '\0';
    memcpy(out->tool, p2 + 2, tlen);
    out->tool[tlen] = '\0';
    return 1;
}

static void br_name_free(br_name *n) {
    free(n->raw);
    free(n->connector);
    free(n->tool);
    memset(n, 0, sizeof(*n));
}

/* vendor_slug_candidates: (f"{connector.upper()}_{tool}", tool). */
static void br_candidates(const char *connector, const json_t *tool, char **primary, char **literal) {
    char *up = br_upper(connector);
    char *text = br_pystr(tool);
    size_t need;
    *primary = NULL;
    *literal = text;
    if (!up || !text) { free(up); return; }
    need = strlen(up) + 1 + strlen(text) + 1;
    *primary = (char *)malloc(need);
    if (*primary) snprintf(*primary, need, "%s_%s", up, text);
    free(up);
}

/* --- model-facing parameters -------------------------------------------- */

/* _as_model_schema: a file_uploadable node becomes a path string. */
static json_t *br_model_schema(const json_t *node) {
    const json_t *flag;
    json_t *out;
    size_t i, n;

    if (!node) return json_null();
    if (node->type == JSON_ARRAY) {
        out = json_array();
        n = json_len(node);
        for (i = 0; i < n; i++) json_append(out, br_model_schema(json_get(node, i)));
        return out;
    }
    if (node->type != JSON_OBJECT) return json_copy(node);

    flag = json_obj_get(node, "file_uploadable");
    if (flag && flag->type == JSON_BOOL && flag->bool_val) {
        const json_t *desc = json_obj_get(node, "description");
        char *text = br_truthy(desc) ? br_pystr(desc) : br_dup("");
        char *note;
        size_t need;
        if (!text) return json_null();
        need = strlen(text) + 1 + strlen(BR_FILE_INPUT_NOTE) + 1;
        note = (char *)malloc(need);
        if (!note) { free(text); return json_null(); }
        snprintf(note, need, "%s %s", text, BR_FILE_INPUT_NOTE);
        free(text);
        out = json_object();
        json_set(out, "type", json_string("string"));
        json_set(out, "description", json_string(br_strip(note)));
        free(note);
        return out;
    }

    out = json_object();
    n = json_object_size(node);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(node, i);
        if (!k) continue;
        json_set(out, k, br_model_schema(json_object_get_at(node, i)));
    }
    return out;
}

/* model_parameters(name, input_schema). */
static json_t *br_model_parameters(const json_t *schema) {
    if (!br_is_obj(schema)) return json_object();
    return br_model_schema(schema);
}

/* --- the legs ----------------------------------------------------------- */

static json_t *br_leg(json_t *payload, json_t *failure) {
    json_t *out = json_object();
    json_set(out, "payload", payload);
    json_set(out, "failure", failure);
    return out;
}

/* The leg's failure for a raised exception object, or NULL for a silent skip. */
static json_t *br_raise_failure(const json_t *raise, int *silent) {
    br_kind_t kind;
    *silent = 0;
    if (!br_is_obj(raise)) return json_null();
    kind = br_kind_of(raise);
    if (kind == BR_UNAVAILABLE) { *silent = 1; return json_null(); }
    return br_leg_failure(kind, json_obj_get(raise, "code"), json_obj_get(raise, "status"));
}

json_t *w26_bridge_ca0da8_search_leg(const json_t *req) {
    const json_t *raise = json_obj_get(req, "raise");
    const json_t *result;
    int silent = 0;
    json_t *failure;

    if (!br_truthy(json_obj_get(req, "available")) || !br_truthy(json_obj_get(req, "queries")))
        return br_leg(json_object(), json_null());

    failure = br_raise_failure(raise, &silent);
    if (br_is_obj(raise) && (silent || br_truthy(failure)))
        return br_leg(json_object(), failure);

    /* payload=client.search(list(queries)) or {} */
    result = json_obj_get(req, "result");
    return br_leg(br_truthy(result) ? json_copy(result) : json_object(), json_null());
}

json_t *w26_bridge_ca0da8_describe_leg(const json_t *req) {
    const json_t *names = json_obj_get(req, "names");
    const json_t *raise = json_obj_get(req, "raise");
    const json_t *response, *schemas;
    json_t *tools, *out;
    br_name *wanted = NULL;
    size_t *wanted_n = NULL;
    size_t nwanted = 0, cap = 0, i, n;
    int silent = 0;
    json_t *failure;

    if (!br_truthy(json_obj_get(req, "available")))
        return br_leg(json_object(), json_null());

    /* Resolve each name independently; candidates may overlap across names. */
    n = br_is_arr(names) ? json_len(names) : 0;
    for (i = 0; i < n; i++) {
        br_name parsed;
        size_t j;
        int seen = 0;
        if (!br_parse_name(json_get(names, i), &parsed)) continue;
        for (j = 0; j < nwanted; j++)
            if (!strcmp(wanted[j].raw, parsed.raw)) { seen = 1; break; }
        if (seen) { br_name_free(&parsed); continue; }
        if (nwanted == cap) {
            size_t ncap = cap ? cap * 2 : 4;
            br_name *np = (br_name *)realloc(wanted, ncap * sizeof(br_name));
            size_t *nn;
            if (!np) { br_name_free(&parsed); break; }
            wanted = np;
            nn = (size_t *)realloc(wanted_n, ncap * sizeof(size_t));
            if (!nn) { br_name_free(&parsed); break; }
            wanted_n = nn;
            cap = ncap;
        }
        wanted[nwanted] = parsed;
        wanted_n[nwanted] = 0;
        nwanted++;
    }
    if (nwanted == 0) {
        free(wanted);
        free(wanted_n);
        return br_leg(json_object(), json_null());
    }

    failure = br_raise_failure(raise, &silent);
    if (br_is_obj(raise) && (silent || br_truthy(failure))) {
        for (i = 0; i < nwanted; i++) br_name_free(&wanted[i]);
        free(wanted);
        free(wanted_n);
        return br_leg(json_object(), failure);
    }

    /* response = client.schemas(request_slugs) or {} */
    response = json_obj_get(req, "schemas_response");
    if (!br_truthy(response)) {
        response = NULL;
    } else if (!br_is_obj(response)) {
        /* a truthy non-mapping has no .get: AttributeError -> "unreachable" */
        for (i = 0; i < nwanted; i++) br_name_free(&wanted[i]);
        free(wanted);
        free(wanted_n);
        return br_leg(json_object(), json_string(BR_UNREACHABLE));
    }
    schemas = response ? json_obj_get(response, "schemas") : NULL;
    if (!br_is_obj(schemas)) schemas = json_object();

    tools = json_object();
    for (i = 0; i < nwanted; i++) {
        char *primary = NULL, *literal = NULL;
        json_t *literal_val;
        const json_t *schema = NULL;

        literal_val = json_string(wanted[i].tool);
        br_candidates(wanted[i].connector, literal_val, &primary, &literal);
        json_free(literal_val);
        if (primary) {
            const json_t *hit = json_obj_get(schemas, primary);
            if (br_is_obj(hit)) schema = hit;
        }
        if (!schema && literal) {
            const json_t *hit = json_obj_get(schemas, literal);
            if (br_is_obj(hit)) schema = hit;
        }
        free(primary);
        free(literal);
        if (!schema) continue;

        {
            json_t *entry = json_object();
            const json_t *desc = json_obj_get(schema, "description");
            char *text = br_truthy(desc) ? br_pystr(desc) : br_dup("");
            json_set(entry, "description", json_string(text ? text : ""));
            free(text);
            json_set(entry, "parameters", br_model_parameters(json_obj_get(schema, "input_schema")));
            json_set(tools, wanted[i].raw, entry);
        }
    }
    for (i = 0; i < nwanted; i++) br_name_free(&wanted[i]);
    free(wanted);
    free(wanted_n);

    out = json_object();
    json_set(out, "tools", tools);
    return br_leg(out, json_null());
}

json_t *w26_bridge_ca0da8_fallback_plan(const json_t *req) {
    const json_t *planned = json_obj_get(req, "planned");
    const json_t *entries = json_obj_get(req, "entries");
    json_t *out = json_array();
    size_t n, i;

    n = br_is_arr(planned) ? json_len(planned) : 0;
    if (br_is_arr(entries) && json_len(entries) < n) n = json_len(entries);
    for (i = 0; i < n; i++) {
        const json_t *plan = json_get(planned, i);
        const json_t *entry = json_get(entries, i);
        const json_t *error = br_is_obj(entry) ? json_obj_get(entry, "error") : NULL;
        const json_t *connector, *tool;
        char *primary = NULL, *literal = NULL;
        int differs;

        if (!br_is_obj(plan)) continue;
        connector = json_obj_get(plan, "connector");
        tool = json_obj_get(plan, "tool");
        if (!br_is_str(connector)) continue;   /* .upper() would raise */

        /* primary, literal = vendor_slug_candidates(...) -- before the check. */
        br_candidates(connector->str_val ? connector->str_val : "", tool, &primary, &literal);
        differs = primary && literal && strcmp(primary, literal) != 0;
        free(primary);
        if (!differs) { free(literal); continue; }

        if (br_is_obj(error) && br_is_str(json_obj_get(error, "code")) &&
            strcmp(json_get_str(error, "code", ""), "TOOL_NOT_FOUND") == 0) {
            json_t *row = json_object();
            json_set(row, "slot", json_number((double)i));
            json_set(row, "tool", json_string(literal));
            json_append(out, row);
        }
        free(literal);
    }
    return out;
}

json_t *w26_bridge_ca0da8_constants(void) {
    json_t *o = json_object();
    json_set(o, "SIGN_IN_EXPIRED", json_string(BR_SIGN_IN_EXPIRED));
    json_set(o, "UNREACHABLE", json_string(BR_UNREACHABLE));
    json_set(o, "NOT_SET_UP", json_string(BR_NOT_SET_UP));
    json_set(o, "NOT_SET_UP_MESSAGE", json_string(BR_NOT_SET_UP_MESSAGE));
    return o;
}
