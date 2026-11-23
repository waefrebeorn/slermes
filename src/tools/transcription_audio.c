/*
 * transcription_audio.c — C port of upstream Hermes tools/transcription_audio.py.
 *
 * PoP: w13_transcripti_b9c624_cloud_trim_settings @ tools/transcription_audio.py:_cloud_trim_settings
 *
 * Faithful copies of the two helpers the live function imports:
 *   utils.is_truthy_value                 (default=True at the call site)
 *   tools.transcription_common._config_number
 * The differential oracle injects identical stubs for these into the live module.
 */
#include "noc_w13_transcripti_b9c624.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* utils.is_truthy_value(value, default): None -> default; str -> stripped/lower
 * in {1,true,yes,on}; otherwise bool(value). */
static int truthy(const struct json_t *v, int dflt) {
    static const char *yes[] = {"1", "true", "yes", "on", NULL};
    if (!v || v->type == JSON_NULL) return dflt;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) {
        const char *p = v->str_val;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1])) e--;
        for (int i = 0; yes[i]; i++) {
            size_t n = strlen(yes[i]);
            if ((size_t)(e - p) == n && strncasecmp(p, yes[i], n) == 0) return 1;
        }
        return 0;
    }
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return dflt;
}

/* _config_number(cfg, key, default, cast): cast(cfg.get(key, default)), falling
 * back to default on TypeError/ValueError. as_int mirrors cast=int. */
static double config_number(const struct json_t *cfg, const char *key, double dflt, int as_int) {
    const struct json_t *v = cfg ? json_obj_get(cfg, key) : NULL;
    if (!v) return dflt;
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        return as_int ? (double)(long long)x : x;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) {
        char *end = NULL;
        if (as_int) {
            long long iv = strtoll(v->str_val, &end, 10);
            if (end == v->str_val) return dflt;
            while (*end && isspace((unsigned char)*end)) end++;
            if (*end) return dflt;
            return (double)iv;
        }
        double x = strtod(v->str_val, &end);
        if (end == v->str_val) return dflt;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return dflt;
        return x;
    }
    return dflt; /* None / list / dict -> cast raises -> default */
}

struct json_t *w13_transcripti_b9c624_cloud_trim_settings(const struct json_t *cfg) {
    const struct json_t *c = json_is_object(cfg) ? cfg : NULL; /* isinstance dict guard */

    /* enabled = is_truthy_value(cfg.get("cloud_trim_silence", True), default=True) */
    const struct json_t *raw = c ? json_obj_get(c, "cloud_trim_silence") : NULL;
    int enabled = raw ? truthy(raw, 1) : 1;

    double threshold_db = config_number(c, "cloud_trim_threshold_db", -40.0, 1);
    double keep_ms = config_number(c, "cloud_trim_keep_ms", 300.0, 1);
    if (keep_ms < 0) keep_ms = 0; /* max(keep_ms, 0) */

    struct json_t *out = json_array();
    json_append(out, json_bool(enabled ? true : false));
    json_append(out, json_number(threshold_db));
    json_append(out, json_number(keep_ms));
    return out;
}
