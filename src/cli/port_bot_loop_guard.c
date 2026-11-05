/*
 * port_bot_loop_guard.c — pure helpers from gateway/bot_loop_guard.py.
 *
 * _TRUTHY = {"true","1","yes","on"}; _FALSY = {"false","0","no","off"}.
 * BotLoopGuardSettings defaults: enabled=True, max_events=20,
 * window_seconds=300.0, cooldown_seconds=600.0.
 */

#include "port_bot_loop_guard.h"
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int blg_in_set(const char *t, const char *const *set, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (strcmp(t, set[i]) == 0)
            return 1;
    return 0;
}

int blg_as_bool(const char *raw_json, int default_val)
{
    static const char *const TRUTHY[] = {"true", "1", "yes", "on"};
    static const char *const FALSY[] = {"false", "0", "no", "off"};
    json_t *n = raw_json ? json_parse(raw_json, NULL) : NULL;
    int r = default_val;
    if (n) {
        switch (n->type) {
        case JSON_BOOL:
            r = n->bool_val ? 1 : 0;
            break;
        case JSON_STRING: {
            const char *s = n->str_val ? n->str_val : "";
            size_t len = strlen(s), i = 0, e = len;
            char word[64];
            while (i < len && isspace((unsigned char)s[i])) i++;
            while (e > i && isspace((unsigned char)s[e - 1])) e--;
            if (e - i >= sizeof(word)) break;
            for (size_t k = 0; k < e - i; k++)
                word[k] = (char)tolower((unsigned char)s[i + k]);
            word[e - i] = '\0';
            if (blg_in_set(word, TRUTHY, 4)) r = 1;
            else if (blg_in_set(word, FALSY, 4)) r = 0;
            else r = default_val;
            break;
        }
        case JSON_NUMBER: {
            double v = n->num_val;
            if (v == (double)(long long)v && fabs(v) < 9e15) {
                long long iv = (long long)v;
                if (iv == 1) r = 1;
                else if (iv == 0) r = 0;
                else r = default_val;
            } else {
                r = default_val;
            }
            break;
        }
        default:
            r = default_val;
        }
        json_free(n);
    }
    return r;
}

double blg_as_positive(const char *raw_json, double default_val)
{
    json_t *n = raw_json ? json_parse(raw_json, NULL) : NULL;
    double v = default_val;
    if (n) {
        switch (n->type) {
        case JSON_BOOL:
            v = default_val;
            break;
        case JSON_NUMBER:
            v = n->num_val;
            break;
        case JSON_STRING: {
            const char *s = n->str_val ? n->str_val : "";
            char *end = NULL;
            double p = strtod(s, &end);
            v = (end != s) ? p : default_val;
            break;
        }
        default:
            v = default_val;
        }
        json_free(n);
    }
    return (v > 0) ? v : default_val;
}

long long blg_as_positive_int(const char *raw_json, long long default_val)
{
    double v = blg_as_positive(raw_json, 0.0);
    if (v >= 1 && v == (double)(long long)v)
        return (long long)v;
    return default_val;
}

char *blg_settings_from_config(const char *block_json)
{
    json_t *b = block_json ? json_parse(block_json, NULL) : NULL;
    int enabled = 1;
    long long max_events = 20;
    double window = 300.0, cooldown = 600.0;
    char buf[192], *out;

    if (b && b->type == JSON_OBJECT) {
        json_t *e = json_obj_get(b, "enabled");
        json_t *m = json_obj_get(b, "max_events");
        json_t *w = json_obj_get(b, "window_seconds");
        json_t *c = json_obj_get(b, "cooldown_seconds");
        char *_r = NULL;
        if (e) {
            _r = json_serialize(e);
            enabled = blg_as_bool(_r, 1);
            free(_r);
        }
        if (m) {
            _r = json_serialize(m);
            max_events = blg_as_positive_int(_r, 20);
            free(_r);
        }
        if (w) {
            _r = json_serialize(w);
            window = blg_as_positive(_r, 300.0);
            free(_r);
        }
        if (c) {
            _r = json_serialize(c);
            cooldown = blg_as_positive(_r, 600.0);
            free(_r);
        }
    }
    if (b) json_free(b);

    snprintf(buf, sizeof(buf),
             "{\"enabled\":%s,\"max_events\":%lld,\"window_seconds\":%.17g,"
             "\"cooldown_seconds\":%.17g}",
             enabled ? "true" : "false", max_events, window, cooldown);
    out = strdup(buf);
    return out;
}
