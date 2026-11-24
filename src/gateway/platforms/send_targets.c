/*
 * send_targets.c - C11 port of upstream gateway/platforms/send_targets.py
 * (gate w24_sendtarget_a3cd9e).
 *
 * PoP: w24_sendtarget_a3cd9e_parse_nonempty @ gateway/platforms/send_targets.py:parse_nonempty
 * PoP: w24_sendtarget_a3cd9e_regex_stripped @ gateway/platforms/send_targets.py:regex_stripped_parser
 * PoP: w24_sendtarget_a3cd9e_regex_groups @ gateway/platforms/send_targets.py:regex_groups_parser
 * PoP: w24_sendtarget_a3cd9e_policy @ gateway/platforms/send_targets.py:StandaloneSendPolicy
 * PoP: w24_sendtarget_a3cd9e_constants @ gateway/platforms/send_targets.py:<module constants>
 *
 * Nothing here sends: core imports this module before any adapter exists, so
 * the whole surface is pure. The two parser factories are closures over a
 * compiled regex; they are modelled as (pattern, ref) -> match groups, using
 * POSIX ERE anchored at both ends (Python's re.fullmatch semantics). The policy
 * is a frozen dataclass with no methods, so it ports as its field defaults with
 * the declared overrides applied verbatim -- a dataclass does not coerce.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_sendtarget_a3cd9e.h"

/* StandaloneSendPolicy fields, in declaration order. */
static const char *const ST_FIELDS[W24_SENDTARGET_A3CD9E_POLICY_FIELDS] = {
    "label", "media", "all_sends", "captionable", "empty_list_between_chunks",
    "forward_force_document", "mentions", "text_direct", "reply_file_over",
};

static const json_t *st_arg(const json_t *req, const char *key) {
    return req && req->type == JSON_OBJECT ? json_obj_get(req, key) : NULL;
}

static const char *st_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python truthiness of a JSON value. */
static int st_truthy(const json_t *v) {
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

static int st_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over the ASCII whitespace set. */
static char *st_strip(const char *s) {
    const char *a, *b;
    char *out;
    size_t n;
    if (!s) s = "";
    a = s;
    while (*a && st_isspace(*a)) a++;
    b = a + strlen(a);
    while (b > a && st_isspace(b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

json_t *w24_sendtarget_a3cd9e_parse_nonempty(const json_t *req) {
    const char *ref = st_str(st_arg(req, "ref"));
    char *stripped;
    json_t *out;

    if (!ref) return json_null();
    stripped = st_strip(ref);
    if (!stripped) return json_null();
    if (stripped[0] == '\0') { free(stripped); return json_null(); }
    out = json_array();
    json_append(out, json_string(stripped));
    json_append(out, json_null());
    free(stripped);
    return out;
}

json_t *w24_sendtarget_a3cd9e_regex_stripped(const json_t *req) {
    const json_t *matched = st_arg(req, "matched");
    const char *ref = st_str(st_arg(req, "ref"));
    char *stripped;
    json_t *out;

    if (!st_truthy(matched) || !ref) return json_null();
    stripped = st_strip(ref);
    if (!stripped) return json_null();
    out = json_array();
    json_append(out, json_string(stripped));
    json_append(out, json_null());
    free(stripped);
    return out;
}

json_t *w24_sendtarget_a3cd9e_regex_groups(const json_t *req) {
    const json_t *group1 = st_arg(req, "group1");
    const json_t *group2 = st_arg(req, "group2");
    int thread_group = st_truthy(st_arg(req, "thread_group"));
    json_t *out;

    if (!st_truthy(st_arg(req, "matched"))) return json_null();
    out = json_array();
    json_append(out, group1 ? json_copy(group1) : json_null());
    json_append(out, (thread_group && group2) ? json_copy(group2) : json_null());
    return out;
}

static json_t *st_default(size_t index) {
    switch (index) {
    case 0:  return json_string("");        /* label */
    case 8:  return json_number(0);         /* reply_file_over */
    default: return json_bool(false);
    }
}

json_t *w24_sendtarget_a3cd9e_policy(const json_t *req) {
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < W24_SENDTARGET_A3CD9E_POLICY_FIELDS; i++) {
        const json_t *v = st_arg(req, ST_FIELDS[i]);
        json_set(out, ST_FIELDS[i], v ? json_copy(v) : st_default(i));
    }
    return out;
}

json_t *w24_sendtarget_a3cd9e_constants(void) {
    json_t *o = json_object();
    json_t *fields = json_object();
    size_t i;
    json_set(o, "not_explicit_type", json_string("object"));
    for (i = 0; i < W24_SENDTARGET_A3CD9E_POLICY_FIELDS; i++)
        json_set(fields, ST_FIELDS[i], st_default(i));
    json_set(o, "fields", fields);
    return o;
}
