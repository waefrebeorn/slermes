/*
 * port_scale_to_zero_helpers.c
 *
 * Pure, portable helper functions ported from gateway/scale_to_zero.py.
 * All five take plain inputs (no live gateway, no config load) so they unit
 * test without side effects. Module prefix used by the scanner for
 * gateway/scale_to_zero.py is "scale_to_zero_".
 *
 * C name <- python name (scale_to_zero_ prefix):
 *   scale_to_zero_enabled, parse_idle_timeout_seconds,
 *   messaging_is_relay_only_or_absent, should_arm, is_idle
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* ---- lowercase helper ---- */
static void lc(char *s)
{
    for (; *s; s++) if (isupper((unsigned char)*s)) *s = (char)tolower((unsigned char)*s);
}

static int is_truthy(const char *v)
{
    if (!v) return 0;
    char b[32]; strncpy(b, v, sizeof(b)-1); b[sizeof(b)-1]='\0'; lc(b);
    return strcmp(b,"1")==0 || strcmp(b,"true")==0 || strcmp(b,"yes")==0 || strcmp(b,"on")==0;
}

/* ---------------------------------------------------------------------- */
/* PoP: scale_to_zero_enabled @ gateway/scale_to_zero.py:scale_to_zero_enabled
 * environ_json: JSON object {"HERMES_SCALE_TO_ZERO": "..."} or NULL (use empty). */
int scale_to_zero_enabled(const char *environ_json)
{
    const char *val = NULL;
    if (environ_json && environ_json[0]) {
        json_t *e = json_parse(environ_json, NULL);
        if (e && e->type == JSON_OBJECT) {
            json_t *v = json_object_get(e, "HERMES_SCALE_TO_ZERO");
            if (v && v->type == JSON_STRING) val = json_string_value(v);
        }
        if (e) json_free(e);
    }
    return is_truthy(val);
}

/* ---------------------------------------------------------------------- */
/* PoP: parse_idle_timeout_seconds @ gateway/scale_to_zero.py:parse_idle_timeout_seconds */
double scale_to_zero_parse_idle_timeout_seconds(double cfg_value, int has_cfg, int default_minutes)
{
    double minutes = (has_cfg && default_minutes >= 0) ? (double)default_minutes : 5.0;
    if (has_cfg) {
        minutes = cfg_value;
        if (minutes <= 0) minutes = (double)default_minutes;
    }
    return minutes * 60.0;
}

/* ---------------------------------------------------------------------- */
/* PoP: messaging_is_relay_only_or_absent @ gateway/scale_to_zero.py:messaging_is_relay_only_or_absent
 * platforms_json: JSON array of platform name strings. Returns 1 when the only
 * present name is "relay" (case-insensitive) or the set is empty. */
int scale_to_zero_messaging_is_relay_only_or_absent(const char *platforms_json)
{
    if (!platforms_json || !platforms_json[0]) return 1;
    json_t *root = json_parse(platforms_json, NULL);
    if (!root || root->type != JSON_ARRAY) { if (root) json_free(root); return 1; }
    int other = 0;
    for (size_t i = 0; i < json_array_size(root); i++) {
        json_t *e = json_array_get(root, i);
        if (!e || e->type != JSON_STRING) continue;
        char b[64]; strncpy(b, json_string_value(e), sizeof(b)-1); b[sizeof(b)-1]='\0'; lc(b);
        if (strcmp(b, "relay") != 0) { other = 1; break; }
    }
    json_free(root);
    return other ? 0 : 1;
}

/* ---------------------------------------------------------------------- */
/* PoP: should_arm @ gateway/scale_to_zero.py:should_arm */
int scale_to_zero_should_arm(int enabled, int relay_only_or_absent, const char *wake_url)
{
    return (enabled != 0) && (relay_only_or_absent != 0) && (wake_url && wake_url[0]);
}

/* ---------------------------------------------------------------------- */
/* PoP: is_idle @ gateway/scale_to_zero.py:is_idle */
int scale_to_zero_is_idle(int running_agent_count, double seconds_since_last_inbound,
                          double idle_timeout_seconds, int has_live_background_work)
{
    if (running_agent_count > 0) return 0;
    if (has_live_background_work) return 0;
    return seconds_since_last_inbound >= idle_timeout_seconds;
}
