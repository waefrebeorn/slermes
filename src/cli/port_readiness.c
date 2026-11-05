/*
 * port_readiness.c — pure probes from gateway/readiness.py.
 *
 * _CONNECTED_STATES = {"connected","running","ok"}.
 * _DISK_DEGRADED_PERCENT = 90.0.
 */

#include "port_readiness.h"
#include "hermes_json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *rd_dict(const char *status, const char *detail_json,
                     const char *extra_json)
{
    /* detail_json/extra_json already JSON-encoded fragments (or NULL/none). */
    size_t sd = detail_json ? strlen(detail_json) : 0;
    size_t se = extra_json ? strlen(extra_json) : 0;
    char *out = (char *)malloc(32 + sd + se + 40);
    char *p;
    if (!out) return NULL;
    p = out;
    p += sprintf(p, "{\"status\":");
    /* status is a plain string we wrap in quotes here */
    p += sprintf(p, "\"%s\"", status);
    if (detail_json) {
        p += sprintf(p, ",\"detail\":%s", detail_json);
    }
    if (extra_json) {
        p += sprintf(p, ",%s", extra_json);
    }
    *p++ = '}';
    *p = '\0';
    return out;
}

static char *rd_json_str(const char *s)
{
    /* JSON-encode a plain string (no escaping needed for our domain). */
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 3);
    if (!o) return NULL;
    o[0] = '"';
    memcpy(o + 1, s, n);
    o[n + 1] = '"';
    o[n + 2] = '\0';
    return o;
}

char *rd_probe_state_db(int exists, int corrupt, const char *error_name)
{
    if (!exists) {
        char *d = rd_json_str("not initialized");
        char *r = rd_dict("ok", d, NULL);
        free(d);
        return r;
    }
    if (corrupt) {
        char *d = rd_json_str("corrupt");
        char *r = rd_dict("degraded", d, NULL);
        free(d);
        return r;
    }
    if (error_name && *error_name) {
        char *d = rd_json_str(error_name);
        char *r = rd_dict("degraded", d, NULL);
        free(d);
        return r;
    }
    return rd_dict("ok", NULL, NULL);
}

char *rd_probe_config(int config_exists, int backend_unavailable,
                      const char *exc_name, int raw_is_mapping)
{
    static const char F_INVALID[] = "invalid config (%s)";
    char detail[120], *d, *r;
    if (!config_exists) {
        d = rd_json_str("using defaults");
        r = rd_dict("ok", d, NULL);
        free(d);
        return r;
    }
    if (backend_unavailable) {
        d = rd_json_str("config backend unavailable");
        r = rd_dict("degraded", d, NULL);
        free(d);
        return r;
    }
    if (exc_name && *exc_name) {
        snprintf(detail, sizeof(detail), F_INVALID, exc_name);
        d = rd_json_str(detail);
        r = rd_dict("degraded", d, NULL);
        free(d);
        return r;
    }
    if (!raw_is_mapping) {
        d = rd_json_str("top level is not a mapping");
        r = rd_dict("degraded", d, NULL);
        free(d);
        return r;
    }
    return rd_dict("ok", NULL, NULL);
}

char *rd_probe_disk(double used_percent, long long free_bytes,
                    const char *error_name)
{
    if (error_name && *error_name) {
        char *d = rd_json_str(error_name);
        char *r = rd_dict("degraded", d, NULL);
        free(d);
        return r;
    }
    {
        const char *st = used_percent >= 90.0 ? "degraded" : "ok";
        char extra[80];
        snprintf(extra, sizeof(extra), "\"used_percent\":%.1f,\"free_bytes\":%lld",
                 used_percent, (long long)free_bytes);
        return rd_dict(st, NULL, extra);
    }
}

static int rd_connected(const json_t *v)
{
    /* A platform is connected when v is a dict and its state|status lowercased
     * is in _CONNECTED_STATES = {"connected","running","ok"}. */
    if (!v || v->type != JSON_OBJECT)
        return 0;
    json_t *raw = json_obj_get(v, "state");
    if (!raw)
        raw = json_obj_get(v, "status");
    if (!raw || raw->type != JSON_STRING || !raw->str_val)
        return 0;
    const char *p = raw->str_val;
    while (*p == ' ') p++;
    return (strcasecmp(p, "connected") == 0 ||
            strcasecmp(p, "running") == 0 || strcasecmp(p, "ok") == 0);
}

char *rd_probe_gateway(const char *gateway_state, const char *platforms_json)
{
    const char *state = (gateway_state && *gateway_state) ? gateway_state : "unknown";
    json_t *platforms = platforms_json ? json_parse(platforms_json, NULL) : NULL;
    long long connected = 0, count = 0;
    const char *st = (strcmp(state, "running") == 0 ||
                      strcmp(state, "draining") == 0) ? "ok" : "degraded";
    char extra[160];

    if (platforms && platforms->type == JSON_OBJECT) {
        size_t i;
        count = (long long)platforms->c.count;
        for (i = 0; i < platforms->c.count; i++) {
            json_t *item = platforms->c.items[i];
            if (item && rd_connected(item))
                connected++;
        }
    }
    snprintf(extra, sizeof(extra),
             "\"state\":\"%s\",\"connected_platforms\":%lld,\"platforms\":%lld",
             state, connected, count);
    if (platforms) json_free(platforms);
    return rd_dict(st, NULL, extra);
}

static int rd_streq(const char *a, const char *b)
{
    return a && b && strcmp(a, b) == 0;
}

char *rd_probe_session_store(const char *state_db_detail,
                             const char *state_db_status,
                             const char *store_state)
{
    if (rd_streq(state_db_detail, "corrupt")) {
        char *d = rd_json_str("corrupt");
        char *r = rd_dict("unavailable", d, NULL);
        free(d);
        return r;
    }
    if (rd_streq(store_state, "ok") || rd_streq(store_state, "unavailable") ||
        rd_streq(store_state, "retrying")) {
        return rd_dict(store_state, NULL, NULL); /* _check(state): no detail */
    }
    {
        const char *st = rd_streq(state_db_status, "ok") ? "ok" : "unavailable";
        return rd_dict(st, NULL, NULL);
    }
}

char *rd_overall(const char *statuses_json)
{
    json_t *arr = statuses_json ? json_parse(statuses_json, NULL) : NULL;
    const char *overall = "ok";
    if (arr && arr->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < arr->c.count; i++) {
            json_t *it = arr->c.items[i];
            if (it && it->type == JSON_OBJECT) {
                json_t *s = json_obj_get(it, "status");
                if (!s || s->type != JSON_STRING || !s->str_val ||
                    strcmp(s->str_val, "ok") != 0) {
                    overall = "degraded";
                    break;
                }
            } else {
                overall = "degraded";
                break;
            }
        }
    } else if (arr) {
        /* non-array is not the shape collect produces */
    }
    if (arr) json_free(arr);
    return strdup(overall);
}
