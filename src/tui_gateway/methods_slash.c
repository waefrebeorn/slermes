/*
 * methods_slash.c — C port of upstream Hermes tui_gateway/methods_slash.py.
 *
 * PoP: w13_methodssla_fe7885_format_live_model_output @ tui_gateway/methods_slash.py:_format_live_model_output
 */
#include "noc_w13_methodssla_fe7885.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

static int py_bool(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

/* Python str() for the scalars these helpers actually receive. */
static const char *py_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return buf; }
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) { snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return buf; }
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (x == (double)(long long)x) snprintf(buf, n, "%lld", (long long)x);
        else snprintf(buf, n, "%g", x);
        return buf;
    }
    snprintf(buf, n, "");
    return buf;
}

struct json_t *w13_methodssla_fe7885_format_live_model_output(const struct json_t *agent) {
    if (!json_is_object(agent)) return json_string("Current model: (unknown)");
    const struct json_t *model = json_obj_get(agent, "model");
    const struct json_t *provider = json_obj_get(agent, "provider");
    if (!py_bool(model)) return json_string("Current model: (unknown)");
    char mb[256], pb[256], out[640];
    const char *m = py_str(model, mb, sizeof mb);
    if (py_bool(provider)) {
        const char *p = py_str(provider, pb, sizeof pb);
        snprintf(out, sizeof out, "Current model: %s (%s)", m, p);
    } else {
        snprintf(out, sizeof out, "Current model: %s", m);
    }
    return json_string(out);
}
