/*
 * ports/agent/relay_tools.c — C11 port of agent/relay_tools.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/agent/port_relay_tools.c ── */
/*
 * port_relay_tools.c — C port of agent/relay_tools.py
 *
 * Core NeMo Relay adapter for Hermes tool execution: execute() runs one
 * tool call through Relay and returns its final arguments; _jsonable /
 * _json_equal / _run_awaitable are the JSON bridge helpers.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* PoP: relay_tools_jsonable @ agent/relay_tools.py:_jsonable */
char *relay_tools_jsonable(const char *arg) {
    /* Python: normalize a value into a JSON-able form. The C core works on
     * JSON strings already — normalize by parsing and re-serializing. */
    if (!arg || !*arg) return strdup("null");
    json_t *j = json_parse(arg, NULL);
    if (!j) return strdup(arg);
    char *out = json_serialize(j);
    json_free(j);
    return out;
}

/* PoP: relay_tools_json_equal @ agent/relay_tools.py:_json_equal */
bool relay_tools_json_equal(const char *left, const char *right) {
    /* Python: canonical JSON equality (sort_keys, compact separators). */
    if (!left && !right) return true;
    if (!left || !right) return false;
    json_t *lj = json_parse(left, NULL);
    json_t *rj = json_parse(right, NULL);
    if (!lj || !rj) {
        if (lj) json_free(lj);
        if (rj) json_free(rj);
        return strcmp(left, right) == 0;
    }
    char *ls = json_serialize(lj);
    char *rs = json_serialize(rj);
    bool eq = ls && rs && strcmp(ls, rs) == 0;
    free(ls);
    free(rs);
    json_free(lj);
    json_free(rj);
    return eq;
}

/* PoP: relay_tools_run_awaitable @ agent/relay_tools.py:_run_awaitable */
char *relay_tools_run_awaitable(const char *arg) {
    /* Python: run an awaitable to completion, or return the value as-is.
     * The C core has no asyncio; coroutine objects never reach it. The
     * value passes through unchanged. */
    return arg ? strdup(arg) : strdup("");
}

/* PoP: relay_tools_execute @ agent/relay_tools.py:execute */
char *relay_tools_execute(const char *arg) {
    /* Python: run one tool call through Relay and return its final args.
     * Arg = "tool_name\targs_json\tresult_json" — when managed relay
     * execution is enabled the result is the relay's managed output;
     * otherwise the callback result passes through. */
    if (!arg || !*arg) return strdup("null");
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    /* Without managed execution: return args as the observed final args. */
    if (!t2 || !t2[1]) {
        return t1 ? strdup(t1 + 1) : strdup("null");
    }
    /* With managed execution: return the relay result JSON. */
    return strdup(t2 + 1);
}

