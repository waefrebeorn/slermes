/*
 * doctor_connectivity.c - C home for upstream Hermes hermes_cli/doctor_connectivity.py.
 *
 * PoP: w18_doctorconn_8245d8_row @ hermes_cli/doctor_connectivity.py:_row
 * PoP: w18_doctorconn_8245d8_openrouter_status @ hermes_cli/doctor_connectivity.py:_OPENROUTER_STATUS
 * PoP: w18_doctorconn_8245d8_probe_openrouter @ hermes_cli/doctor_connectivity.py:_probe_openrouter
 *
 * Only the deterministic HTTP-decision core is ported: _row's status->glyph
 * mapping, the OpenRouter status table, and _probe_openrouter's branching.
 * color() is injected as identity in the oracle, so the C side emits the raw
 * glyphs (UTF-8 escapes keep the source encoding-independent).
 */
#include "noc_w18_doctorconn_8245d8.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unicode glyphs from _GLYPH: OK / warn / fail. */
static const char *DC_GLYPH_OK = "\xE2\x9C\x93";
static const char *DC_GLYPH_WARN = "\xE2\x9A\xA0";
static const char *DC_GLYPH_FAIL = "\xE2\x9C\x97";

static const char *dc_glyph(const char *status) {
    if (!status) status = "";
    if (!strcmp(status, "ok")) return DC_GLYPH_OK;
    if (!strcmp(status, "warn")) return DC_GLYPH_WARN;
    if (!strcmp(status, "fail")) return DC_GLYPH_FAIL;
    return DC_GLYPH_OK;
}

static json_t *dc_row(const char *name, const char *status, const char *detail,
                      const struct json_t *label, const struct json_t *issues) {
    if (!name) name = "";
    if (!detail) detail = "";
    const char *shown = name;
    if (label && label->type == JSON_STRING && label->str_val) shown = label->str_val;

    json_t *line = json_array();
    json_append(line, json_string(dc_glyph(status)));
    json_append(line, json_string(shown));
    json_append(line, json_string(detail[0] ? detail : ""));

    json_t *lines = json_array();
    json_append(lines, line);

    json_t *res = json_object();
    json_set(res, "label", json_string(name));
    json_set(res, "lines", lines);
    if (issues && issues->type == JSON_ARRAY) json_set(res, "issues", json_copy(issues));
    else json_set(res, "issues", json_array());
    return res;
}

struct json_t *w18_doctorconn_8245d8_row(const char *name, const char *status,
                                          const char *detail, const struct json_t *label) {
    return dc_row(name, status, detail, label, NULL);
}

struct json_t *w18_doctorconn_8245d8_openrouter_status(int status) {
    json_t *out = json_array();
    if (status == 401) {
        json_append(out, json_string("(invalid API key)"));
        json_append(out, json_string("Check OPENROUTER_API_KEY in .env"));
    } else if (status == 402) {
        json_append(out, json_string("(out of credits \xE2\x80\x94 payment required)"));
        json_append(out, json_string(
            "OpenRouter account has insufficient credits. "
            "Fix: run 'hermes config set model.provider <provider>' "
            "to switch providers, or fund your OpenRouter account "
            "at https://openrouter.ai/settings/credits"));
    } else if (status == 429) {
        json_append(out, json_string("(rate limited)"));
        json_append(out, json_string(
            "OpenRouter rate limit hit \xE2\x80\x94 consider switching to a "
            "different provider or waiting"));
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "(HTTP %d)", status);
        json_append(out, json_string(buf));
        json_append(out, json_null());
    }
    return out;
}

struct json_t *w18_doctorconn_8245d8_probe_openrouter(const char *key,
                                                       const char *error, int status) {
    const char *name = "OpenRouter API";
    if (!key || !*key) return dc_row(name, "warn", "(not configured)", NULL, NULL);

    if (error && *error) {
        char msg[2048];
        snprintf(msg, sizeof(msg), "(%s)", error);
        json_t *issues = json_array();
        json_append(issues, json_string("Check network connectivity"));
        json_t *r = dc_row(name, "fail", msg, NULL, issues);
        json_free(issues);
        return r;
    }

    if (status == 200) return dc_row(name, "ok", "", NULL, NULL);

    json_t *st = w18_doctorconn_8245d8_openrouter_status(status);
    const json_t *dv = json_get(st, 0);
    const json_t *iv = json_get(st, 1);
    char detail[2048];
    snprintf(detail, sizeof(detail), "%s", (dv && dv->type == JSON_STRING && dv->str_val) ? dv->str_val : "");
    json_t *issues = json_array();
    if (iv && iv->type == JSON_STRING && iv->str_val) json_append(issues, json_string(iv->str_val));
    json_t *r = dc_row(name, "fail", detail, NULL, issues);
    json_free(issues);
    json_free(st);
    return r;
}
