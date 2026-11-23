/*
 * session_rescue.c — real C11 port of session_rescue.py.
 *
 * PoP: sessionresc_divert_plan @ session_rescue.py:divert_session_transcript_jsonl
 *
 * The live function's only filesystem-independent observable is (a) whether
 * it writes at all and (b) the exact JSONL records it appends. This leaf
 * computes both, mirroring sid = str(session_id or "").strip(), the empty
 * guards, dict-vs-str coercion, and the skip-None rule. The directory
 * creation/open is intentionally left to the live module in the oracle.
 */
#include "noc_w5_sessionresc_d445b0.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str() coercion of a JSON scalar, as a malloc'd C string. */
static char *sr_str(json_t *v) {
    char buf[128];
    const char *s;
    if (!v) return strdup("");
    if (v->type == JSON_STRING) {
        s = v->str_val ? v->str_val : "";
        return strdup(s);
    }
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            v->num_val >= -9.0e15 && v->num_val <= 9.0e15)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return strdup(buf);
    }
    if (v->type == JSON_NULL) return strdup("None");
    return strdup("");
}

/* Strip leading/trailing ASCII whitespace (Python str.strip for our inputs). */
static char *sr_strip(const char *in) {
    size_t n, a, b;
    char *out;
    const char *s = in ? in : "";
    n = strlen(s);
    a = 0;
    while (a < n && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' ||
                     s[a] == '\r' || s[a] == '\v' || s[a] == '\f')) a++;
    if (a == n) return strdup("");
    b = n;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                     s[b - 1] == '\r' || s[b - 1] == '\v' || s[b - 1] == '\f')) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/*
 * PoP: sessionresc_divert_plan @ session_rescue.py:divert_session_transcript_jsonl
 */
json_t *sessionresc_divert_plan(const char *session_id, json_t *messages) {
    char *sid = sr_strip(session_id);
    size_t count, i;
    json_t *plan, *records, *name;
    char *fname;

    if (!sid || sid[0] == '\0' || !messages || messages->type != JSON_ARRAY ||
        messages->c.count == 0) {
        free(sid);
        return json_null();
    }
    count = messages->c.count;
    plan = json_object();
    records = json_array();

    fname = (char *)malloc(strlen(sid) + 7); /* sid + ".jsonl" + NUL */
    if (!fname) { free(sid); json_free(plan); json_free(records); return json_null(); }
    sprintf(fname, "%s.jsonl", sid);
    free(sid);
    name = json_string(fname);
    free(fname);
    json_set(plan, "name", name);

    for (i = 0; i < count; i++) {
        json_t *msg = json_get(messages, i);
        if (!msg || msg->type == JSON_NULL) continue; /* `if msg is not None` */
        if (msg->type == JSON_OBJECT) {
            json_append(records, json_copy(msg));
        } else {
            char *text = sr_str(msg);
            json_t *rec = json_object();
            json_set(rec, "content", json_string(text ? text : ""));
            free(text);
            json_append(records, rec);
        }
    }
    json_set(plan, "records", records);
    return plan;
}
