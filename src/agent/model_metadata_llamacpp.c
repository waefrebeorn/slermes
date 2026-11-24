/*
 * model_metadata_llamacpp.c — real C11 port of selected behavior of upstream
 * Hermes agent/model_metadata_llamacpp.py.
 *
 * PoP: w17_modelmetad_923e37_llamacpp_context @ agent/model_metadata_llamacpp.py:llamacpp_context
 * PoP: w17_modelmetad_923e37_router_launch_ctx @ agent/model_metadata_llamacpp.py:router_launch_ctx
 */
#include "noc_w17_modelmetad_923e37.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { json_t *resps; size_t idx; } RespQueue;

/* Next scripted response spec, or NULL when the queue is exhausted. */
static json_t *resp_next(RespQueue *q) {
    json_t *arr = q->resps;
    if (!arr || arr->type != JSON_ARRAY || q->idx >= arr->c.count) return NULL;
    return arr->c.items[q->idx++];
}

/* Python str() coercion for values that may appear in /models args. */
static const char *py_str(json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) return "None";
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d)
            snprintf(buf, cap, "%lld", (long long)d);
        else
            snprintf(buf, cap, "%g", d);
        return buf;
    }
    if (v->type == JSON_ARRAY) return "[]";
    if (v->type == JSON_OBJECT) return "{}";
    return "";
}

/* agent.model_metadata._model_id_matches, verbatim semantics. */
static int model_id_matches(const char *candidate, const char *lookup) {
    const char *slash;
    if (!candidate) candidate = "";
    if (!lookup) lookup = "";
    if (!strcmp(candidate, lookup)) return 1;
    slash = strrchr(candidate, '/');
    return slash && !strcmp(slash + 1, lookup);
}

/* agent.model_metadata._positive_int: int(value) if number-ish and truthy. */
static int positive_int(json_t *v, long long *out) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_NUMBER) {
        d = v->num_val;
    } else if (v->type == JSON_BOOL) {
        d = v->bool_val ? 1.0 : 0.0; /* bool is an int subclass in Python */
    } else {
        return 0;
    }
    if (d == 0.0) return 0; /* falsy -> None */
    *out = (long long)d;    /* int(): truncate toward zero */
    return 1;
}

static int all_digits(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++) if (*s < '0' || *s > '9') return 0;
    return 1;
}

static json_t *router_impl(const char *model, RespQueue *q) {
    json_t *spec, *body, *obj, *entries;
    int status;
    size_t i, n;

    spec = resp_next(q);
    if (!spec || json_get_bool(spec, "error", 0)) return json_null();
    status = (int)json_get_num(spec, "status", 200.0);
    if (status != 200) return json_null();

    body = json_get_bool(spec, "json_raises", 0) ? NULL : json_obj_get(spec, "body");
    obj = (body && body->type == JSON_OBJECT) ? body : NULL; /* _json_object */
    entries = obj ? json_obj_get(obj, "data") : NULL;
    if (!entries || entries->type != JSON_ARRAY) return json_null();

    n = entries->c.count;
    for (i = 0; i < n; i++) {
        json_t *entry = entries->c.items[i];
        json_t *idnode, *statusnode, *raw_args;
        char idbuf[64];
        const char *id;
        size_t j, m;

        if (!entry || entry->type != JSON_OBJECT) continue;
        idnode = json_obj_get(entry, "id");
        id = idnode ? py_str(idnode, idbuf, sizeof(idbuf)) : "";
        if (!model_id_matches(id, model)) continue;

        statusnode = json_obj_get(entry, "status");
        raw_args = (statusnode && statusnode->type == JSON_OBJECT)
                   ? json_obj_get(statusnode, "args") : NULL;
        if (!raw_args || raw_args->type != JSON_ARRAY) continue;

        m = raw_args->c.count;
        /* pairwise(): overlapping (args[i], args[i+1]) pairs. */
        for (j = 0; j + 1 < m; j++) {
            char fbuf[64], vbuf[64];
            const char *flag = py_str(raw_args->c.items[j], fbuf, sizeof(fbuf));
            const char *value = py_str(raw_args->c.items[j + 1], vbuf, sizeof(vbuf));
            if ((!strcmp(flag, "--ctx-size") || !strcmp(flag, "-c")) &&
                all_digits(value)) {
                long long n_ctx = strtoll(value, NULL, 10);
                if (n_ctx > 0) return json_number((double)n_ctx);
            }
        }
    }
    return json_null();
}

json_t *w17_modelmetad_923e37_router_launch_ctx(const char *model, json_t *responses) {
    RespQueue q;
    q.resps = responses;
    q.idx = 0;
    return router_impl(model ? model : "", &q);
}

json_t *w17_modelmetad_923e37_llamacpp_context(const char *model, json_t *responses) {
    RespQueue q;
    int pass;
    q.resps = responses;
    q.idx = 0;

    for (pass = 0; pass < 2; pass++) {
        json_t *spec = resp_next(&q);
        json_t *body, *obj, *settings;
        long long n_ctx;
        int status;
        if (!spec) break;
        if (json_get_bool(spec, "error", 0)) return json_null();
        status = (int)json_get_num(spec, "status", 200.0);
        if (status != 200) continue;

        body = json_get_bool(spec, "json_raises", 0) ? NULL : json_obj_get(spec, "body");
        obj = (body && body->type == JSON_OBJECT) ? body : NULL; /* _json_object */
        settings = obj ? json_obj_get(obj, "default_generation_settings") : NULL;
        if (settings && settings->type == JSON_OBJECT &&
            positive_int(json_obj_get(settings, "n_ctx"), &n_ctx)) {
            return json_number((double)n_ctx);
        }
        /* no usable n_ctx -> try the next /props path */
    }
    return router_impl(model ? model : "", &q);
}
