/*
 * port_turn_liveness.c — Port of Python: agent/turn_liveness.py (settings half).
 *
 * Faithful to the Python resolution contract:
 *   * a typo, NaN or Inf warns and falls back to the default (never raises);
 *   * a non-positive poll falls back to the default;
 *   * a non-positive timeout opts the watchdog out entirely (None);
 *   * a non-dict `agent.turn_liveness` section warns and is ignored.
 *
 * The watchdog state machine is NOT ported: it samples a live agent object
 * through a shared lock, a scheduler and a commit callback, and no C caller
 * exists for it yet. An empty C stand-in would be the fake-parity this repo
 * has already burned months on.
 */

#include "port_turn_liveness.h"
#include "hermes_json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TL_CONFIG_TIMEOUT_KEY "agent.turn_liveness.timeout_s"
#define TL_CONFIG_POLL_KEY    "agent.turn_liveness.poll_s"

/* Python float(raw) for the textual form of a config value. Anything that is
 * not a complete finite-parseable number becomes NaN so the caller's
 * isfinite() test drives the fallback, exactly like float() raising. */
static double parse_float(const char *raw)
{
    if (!raw) return NAN;
    char *end = NULL;
    double v = strtod(raw, &end);
    if (!end) return NAN;
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    if (end == raw || *end != '\0') return NAN;
    return v;
}

/* Python repr() of a string value, as it appears inside the log message. */
static char *repr_str(const char *s)
{
    if (!s) return NULL;
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = quote;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\' || c == (unsigned char)quote || c == '\n' ||
            c == '\r' || c == '\t') {
            out[o++] = '\\';
            switch (c) {
            case '\n': out[o++] = 'n'; break;
            case '\r': out[o++] = 'r'; break;
            case '\t': out[o++] = 't'; break;
            default:   out[o++] = (char)c;
            }
        } else {
            out[o++] = (char)c;
        }
    }
    out[o++] = quote;
    out[o] = '\0';
    return out;
}

/* Textual form of a config value, for the float() coercion. Python's float()
 * accepts numbers, numeric strings and bools; None/containers become NaN and
 * therefore fall back to the default. Returns malloc'd text or NULL. */
static char *node_to_text(const json_t *n)
{
    if (!n) return NULL;
    char buf[64];
    switch (n->type) {
    case JSON_STRING: return strdup(n->str_val ? n->str_val : "");
    case JSON_NUMBER: snprintf(buf, sizeof(buf), "%.17g", n->num_val); return strdup(buf);
    case JSON_BOOL:   return strdup(n->bool_val ? "True" : "False");
    default:          return NULL;
    }
}

/* PoP: turn_liveness_invalid_value_message @ agent/turn_liveness.py:_warn_invalid_value */
char *turn_liveness_invalid_value_message(const char *key, const char *raw,
                                          double def)
{
    char *rep = repr_str(raw);
    if (!rep) rep = strdup("None");
    size_t cap = strlen(key ? key : "") + strlen(rep) + 96;
    char *msg = (char *)malloc(cap);
    if (!msg) { free(rep); return NULL; }
    snprintf(msg, cap,
             "Invalid %s in config.yaml: %s \xe2\x80\x94 falling back to default %.1f.",
             key ? key : "", rep, def);
    free(rep);
    return msg;
}

/* PoP: turn_liveness_resolve_finite_seconds @ agent/turn_liveness.py:_resolve_finite_seconds */
int turn_liveness_resolve_finite_seconds(const char *raw, double def,
                                         double *out)
{
    double value;
    if (!raw) {                     /* key absent -> Python passes the default */
        if (out) *out = def;
        return 0;
    }
    value = parse_float(raw);
    if (!isfinite(value)) {
        if (out) *out = def;
        return 1;                   /* warning due */
    }
    if (out) *out = value;
    return 0;
}

/* PoP: turn_liveness_resolve_settings @ agent/turn_liveness.py:resolve_turn_liveness_settings */
int turn_liveness_resolve_settings(const char *config_json, int *has_timeout,
                                   double *timeout_s, double *poll_s)
{
    int warnings = 0;
    double timeout = TL_DEFAULT_TIMEOUT_S;
    double poll = TL_DEFAULT_POLL_S;
    const char *raw_timeout = NULL;
    const char *raw_poll = NULL;
    int section_is_bad = 0;
    const char *raw_section_repr = NULL;
    json_t *cfg = NULL;
    json_t *agent = NULL;
    json_t *section = NULL;

    if (config_json) cfg = json_parse(config_json, NULL);
    agent = cfg ? json_obj_get(cfg, "agent") : NULL;
    if (agent && json_type(agent) != JSON_OBJECT) agent = NULL;
    if (agent) {
        section = json_obj_get(agent, "turn_liveness");
        if (section && json_type(section) != JSON_OBJECT) {
            /* Python warns with the offending raw value and uses {} after. */
            section_is_bad = 1;
            raw_section_repr = json_get_str(agent, "turn_liveness", NULL);
            section = NULL;   /* Python replaces a non-dict section with {} */
        }
    }

    if (section_is_bad) warnings++;

    if (section) {
        char *t = node_to_text(json_obj_get(section, "timeout_s"));
        char *p = node_to_text(json_obj_get(section, "poll_s"));
        raw_timeout = t;
        raw_poll = p;
        warnings += turn_liveness_resolve_finite_seconds(raw_timeout,
                                                         TL_DEFAULT_TIMEOUT_S,
                                                         &timeout);
        warnings += turn_liveness_resolve_finite_seconds(raw_poll,
                                                         TL_DEFAULT_POLL_S,
                                                         &poll);
        free(t);
        free(p);
    } else {
        warnings += turn_liveness_resolve_finite_seconds(NULL,
                                                         TL_DEFAULT_TIMEOUT_S,
                                                         &timeout);
        warnings += turn_liveness_resolve_finite_seconds(NULL,
                                                         TL_DEFAULT_POLL_S,
                                                         &poll);
    }

    if (poll <= 0.0) {
        /* Python warns, then restores the default. */
        warnings++;
        poll = TL_DEFAULT_POLL_S;
    }
    if (timeout <= 0.0) {
        if (has_timeout) *has_timeout = 0;
        if (timeout_s) *timeout_s = 0.0;
    } else {
        if (has_timeout) *has_timeout = 1;
        if (timeout_s) *timeout_s = timeout;
    }
    if (poll_s) *poll_s = poll;

    (void)raw_section_repr;
    if (cfg) json_free(cfg);
    return warnings;
}
