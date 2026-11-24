/*
 * usage.c - C11 port of upstream hermes_cli/subcommands/usage.py
 * (gate w25_usage_61b537).
 *
 * PoP: w25_usage_61b537_snapshot_document @ hermes_cli/subcommands/usage.py:usage_snapshot_document
 * PoP: w25_usage_61b537_no_usage_message @ hermes_cli/subcommands/usage.py:cmd_usage
 * PoP: w25_usage_61b537_constants @ hermes_cli/subcommands/usage.py:build_usage_parser
 *
 * ``hermes usage`` is three layers of which only the middle is a pure value
 * function: the fetch (``fetch_account_usage``: singleton credential store,
 * credential pool, provider HTTP), the document built from its snapshot, and
 * the argparse surface.  The fetch and the renderer are seams; what is ported
 * is the document -- the schema cron jobs parse -- plus the advisory printed
 * when no snapshot came back, plus the flags the parser attaches.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_usage_61b537.h"

/* Python truthiness of a JSON value. */
static int us_truthy(const json_t *v) {
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

/* json_set with a None fallback: snapshot.get(k) is None for both absent/null. */
static void us_put(json_t *o, const char *k, json_t *v) {
    json_set(o, k, v ? v : json_null());
}

static json_t *us_copy_or_null(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    return json_copy(v);
}

/* datetime.isoformat() of an already-ISO timestamp string is the identity; a
 * falsy timestamp renders None (``window.reset_at if window.reset_at else None``). */
static json_t *us_isoformat(const json_t *v) {
    if (!us_truthy(v)) return json_null();
    if (v->type != JSON_STRING) return json_null();  /* not a datetime: the live code would raise */
    return json_string(v->str_val);
}

json_t *w25_usage_61b537_snapshot_document(const json_t *snapshot) {
    const json_t *windows, *details;
    json_t *out, *rows;
    size_t i, n;

    if (!snapshot || snapshot->type != JSON_OBJECT) return json_null();
    windows = json_obj_get(snapshot, "windows");
    if (windows && windows->type != JSON_NULL && windows->type != JSON_ARRAY) return json_null();
    details = json_obj_get(snapshot, "details");
    if (details && details->type != JSON_NULL && details->type != JSON_ARRAY) return json_null();

    out = json_object();
    us_put(out, "provider", us_copy_or_null(json_obj_get(snapshot, "provider")));
    us_put(out, "source", us_copy_or_null(json_obj_get(snapshot, "source")));
    us_put(out, "title", us_copy_or_null(json_obj_get(snapshot, "title")));
    us_put(out, "plan", us_copy_or_null(json_obj_get(snapshot, "plan")));
    us_put(out, "fetched_at", us_isoformat(json_obj_get(snapshot, "fetched_at")));

    rows = json_array();
    n = windows ? json_len(windows) : 0;
    for (i = 0; i < n; i++) {
        const json_t *w = json_get(windows, i);
        json_t *row;
        if (!w || w->type != JSON_OBJECT) { json_append(rows, json_null()); continue; }
        row = json_object();
        us_put(row, "label", us_copy_or_null(json_obj_get(w, "label")));
        us_put(row, "used_percent", us_copy_or_null(json_obj_get(w, "used_percent")));
        us_put(row, "resets_at", us_isoformat(json_obj_get(w, "reset_at")));
        us_put(row, "detail", us_copy_or_null(json_obj_get(w, "detail")));
        json_append(rows, row);
    }
    json_set(out, "windows", rows);
    json_set(out, "details", details ? json_copy(details) : json_array());
    us_put(out, "unavailable_reason", us_copy_or_null(json_obj_get(snapshot, "unavailable_reason")));
    return out;
}

/* f-string rendering of the provider: str() over the shapes a provider id takes. */
static void us_provider_text(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
    case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
    case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, cap, "%lld", (long long)v->num_val);
        else snprintf(buf, cap, "%g", v->num_val);
        return;
    case JSON_ARRAY:  snprintf(buf, cap, "[]"); return;
    case JSON_OBJECT: snprintf(buf, cap, "{}"); return;
    case JSON_NULL:   snprintf(buf, cap, "None"); return;
    }
    snprintf(buf, cap, "None");
}

json_t *w25_usage_61b537_no_usage_message(const json_t *provider) {
    static const char *const head = "No account usage available for provider '";
    static const char *const tail =
        "': no credential is configured for it, the provider has no usage endpoint, "
        "or the fetch failed.";
    char name[512];
    char *buf;
    size_t need;
    json_t *out;

    us_provider_text(provider, name, sizeof name);
    need = strlen(head) + strlen(name) + strlen(tail) + 1;
    buf = (char *)malloc(need);
    if (!buf) return json_null();
    snprintf(buf, need, "%s%s%s", head, name, tail);
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w25_usage_61b537_constants(void) {
    json_t *o = json_object();
    json_set(o, "name", json_string("usage"));
    json_set(o, "help", json_string(
        "Show account rate-limit windows (the /usage block) without starting a session"));
    json_set(o, "description", json_string(
        "Fetch the configured provider's account limits (Codex 5h/weekly windows, plan, banked "
        "resets; Anthropic OAuth windows; OpenRouter credits) \u2014 the same block the /usage slash "
        "command prints \u2014 and exit. Exit code 1 when no credential is configured or the fetch fails."));
    json_set(o, "provider_flag", json_string("--provider"));
    json_set(o, "provider_default", json_null());
    json_set(o, "provider_help", json_string(
        "Provider to query (default: the configured model provider)"));
    json_set(o, "json_flag", json_string("--json"));
    json_set(o, "json_default", json_bool(0));
    json_set(o, "json_action", json_string("_StoreTrueAction"));  /* type(parser action).__name__ */
    json_set(o, "json_help", json_string(
        "Print one JSON document instead of the human-readable block"));
    json_set(o, "defaults_func", json_string("cmd_usage"));
    json_set(o, "unavailable_exit", json_number(1.0));
    return o;
}
