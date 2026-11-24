/*
 * policy.c - C11 port of upstream tools/connectors/portal/policy.py
 * (gate w24_policy_2a6f06).
 *
 * PoP: w24_policy_2a6f06_layer @ tools/connectors/portal/policy.py:_layer
 * PoP: w24_policy_2a6f06_connector_write @ tools/connectors/portal/policy.py:compose_connector_write
 * PoP: w24_policy_2a6f06_tools_write @ tools/connectors/portal/policy.py:compose_tools_write
 * PoP: w24_policy_2a6f06_constants @ tools/connectors/portal/policy.py:_LAYERS
 *
 * Ported: the whole composition surface, including ``validate_slug`` (a pure
 * regex predicate) and both validators' messages.  The seams are the models
 * and the transport: a PolicyBody/ConnectorChange/ToolsChange arrives as a
 * plain object, and no request is ever built here -- the dict this returns is
 * exactly what the portal client would put on the wire.
 *
 * Raises become error strings (see the header), because a C caller has no
 * exception channel and the oracle compares them one-for-one.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_policy_2a6f06.h"

#define ERR_SLUG   "ERROR:policy:connector must be a slug"
#define ERR_TOOLS  "ERROR:policy:disabled_tools must contain non-empty tool slugs up to 256 characters"
#define ERR_OFF    "ERROR:policy:connector is off in the member policy"
#define ERR_KEY    "ERROR:keyerror"
#define ERR_TYPE   "ERROR:type"
#define ERR_ATTR   "ERROR:attribute"

#define MAX_TOOL_SLUG_CHARS 256

static const char *as_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* ^[a-z0-9][a-z0-9_-]*$ (re.fullmatch: the whole string, newline included). */
static int slug_ok(const char *s) {
    size_t i;
    if (!s || !*s) return 0;
    if (!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'))) return 0;
    for (i = 1; s[i]; i++) {
        char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') continue;
        return 0;
    }
    return 1;
}

/* Python '==' over the shapes a bool field can hold: True == 1, None == None. */
static int py_equal(const json_t *a, const json_t *b) {
    double x, y;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

typedef struct {
    const char *key;      /* connectors | disabledConnectors */
    int allow;
    json_t *values;       /* owned list */
    const char *err;      /* non-NULL: return this error token instead */
} layer_t;

/* _LAYERS + _layer(): body = member_body or UnrestrictedPolicyBody("unrestricted"). */
static layer_t compute_layer(const json_t *member) {
    layer_t L;
    const char *mode = "unrestricted";
    const json_t *src = NULL;
    size_t i, n;

    L.key = "disabledConnectors";
    L.allow = 0;
    L.values = json_array();      /* the empty layer still writes its key */
    L.err = NULL;

    if (member && member->type == JSON_NULL) member = NULL;
    if (member) {
        if (member->type != JSON_OBJECT) { L.err = ERR_ATTR; return L; }
        mode = as_str(json_obj_get(member, "mode"));
        if (!mode) { L.err = ERR_KEY; return L; }      /* _LAYERS[None] -> KeyError */
    }

    if (strcmp(mode, "unrestricted") == 0) {
        L.key = "disabledConnectors"; L.allow = 0; return L;
    }
    if (strcmp(mode, "deny-all") == 0) {
        L.key = "connectors"; L.allow = 1; return L;
    }
    if (strcmp(mode, "allow") == 0) {
        L.key = "connectors"; L.allow = 1;
        src = json_obj_get(member, "connectors");
    } else if (strcmp(mode, "deny") == 0) {
        L.key = "disabledConnectors"; L.allow = 0;
        src = json_obj_get(member, "disabled_connectors");
    } else {
        L.err = ERR_KEY;
        return L;
    }

    if (!src || src->type == JSON_NULL) { L.err = ERR_ATTR; return L; }
    if (src->type != JSON_ARRAY) { L.err = ERR_TYPE; return L; }

    n = json_len(src);
    for (i = 0; i < n; i++) json_append(L.values, json_copy(json_get(src, i)));
    return L;
}

json_t *w24_policy_2a6f06_layer(const json_t *member) {
    layer_t L = compute_layer(member);
    json_t *out;
    if (L.err) {
        if (L.values) json_free(L.values);
        return json_string(L.err);
    }
    out = json_object();
    json_set(out, "key", json_string(L.key));
    json_set(out, "values", L.values ? L.values : json_array());
    json_set(out, "allow", json_bool(L.allow));
    return out;
}

/* First string element of `values` equal to `connector`, or -1. */
static long index_of(const json_t *values, const char *connector) {
    size_t i, n = values ? json_len(values) : 0;
    for (i = 0; i < n; i++) {
        const char *s = as_str(json_get(values, i));
        if (s && strcmp(s, connector) == 0) return (long)i;
    }
    return -1;
}

json_t *w24_policy_2a6f06_connector_write(const json_t *member, const json_t *change) {
    const json_t *connector_node, *enabled;
    const char *connector;
    layer_t L;
    json_t *out, *kept;
    size_t i, n;
    int flip;

    connector_node = (change && change->type == JSON_OBJECT)
                         ? json_obj_get(change, "connector") : NULL;
    if (!connector_node) return json_string(ERR_ATTR);   /* change.connector missing */
    connector = as_str(connector_node);
    if (!connector) return json_string(ERR_TYPE);
    if (!slug_ok(connector)) return json_string(ERR_SLUG);

    enabled = (change && change->type == JSON_OBJECT) ? json_obj_get(change, "enabled") : NULL;
    if (!enabled) return json_string(ERR_ATTR);      /* change.enabled missing */

    L = compute_layer(member);
    if (L.err) {
        if (L.values) json_free(L.values);
        return json_string(L.err);
    }

    kept = json_array();
    n = json_len(L.values);
    for (i = 0; i < n; i++) {
        const char *s = as_str(json_get(L.values, i));
        if (s && strcmp(s, connector) == 0) continue;   /* retained = values != connector */
        json_append(kept, json_copy(json_get(L.values, i)));
    }
    json_free(L.values);

    flip = py_equal(enabled, json_bool(L.allow));
    if (flip) json_append(kept, json_string(connector));

    out = json_object();
    json_set(out, "scope", json_string("member"));
    json_set(out, L.key, kept);
    return out;
}

json_t *w24_policy_2a6f06_tools_write(const json_t *member, const json_t *change) {
    const json_t *connector_node;
    const char *connector;
    const json_t *tools;
    layer_t L;
    json_t *out, *validated, *tools_obj;
    size_t i, n;
    int in_values;

    /* _slug(change.connector) */
    connector_node = (change && change->type == JSON_OBJECT)
                         ? json_obj_get(change, "connector") : NULL;
    if (!connector_node) return json_string(ERR_ATTR);
    connector = as_str(connector_node);
    if (!connector) return json_string(ERR_TYPE);
    if (!slug_ok(connector)) return json_string(ERR_SLUG);

    /* _layer(member_body) */
    L = compute_layer(member);
    if (L.err) {
        if (L.values) json_free(L.values);
        return json_string(L.err);
    }

    /* if (connector in values) != allow: raise */
    in_values = index_of(L.values, connector) >= 0;
    if (in_values != L.allow) {
        json_free(L.values);
        return json_string(ERR_OFF);
    }

    /* change.disabled_tools, then _validated_tools -- evaluated last upstream. */
    tools = (change && change->type == JSON_OBJECT) ? json_obj_get(change, "disabled_tools") : NULL;
    if (!tools) { json_free(L.values); return json_string(ERR_ATTR); }
    if (tools->type != JSON_ARRAY) { json_free(L.values); return json_string(ERR_TYPE); }

    n = json_len(tools);
    for (i = 0; i < n; i++) {
        const json_t *t = json_get(tools, i);
        const char *s = as_str(t);
        if (!s) { json_free(L.values); return json_string(ERR_TYPE); }  /* len(non-str) -> TypeError */
        if (!*s || strlen(s) > MAX_TOOL_SLUG_CHARS) {
            json_free(L.values);
            return json_string(ERR_TOOLS);
        }
    }
    validated = json_copy(tools);

    tools_obj = json_object();
    json_set(tools_obj, connector, validated);
    out = json_object();
    json_set(out, "scope", json_string("member"));
    json_set(out, L.key, L.values);
    json_set(out, "tools", tools_obj);
    return out;
}

json_t *w24_policy_2a6f06_constants(void) {
    static const char *const LAYERS[] = {"unrestricted", "deny-all", "allow", "deny"};
    json_t *o = json_object(), *layers = json_array();
    size_t i;
    for (i = 0; i < 4; i++) json_append(layers, json_string(LAYERS[i]));
    json_set(o, "layers", layers);
    json_set(o, "slug_pattern", json_string("^[a-z0-9][a-z0-9_-]*$"));
    json_set(o, "max_tool_slug_chars", json_number(MAX_TOOL_SLUG_CHARS));
    return o;
}
