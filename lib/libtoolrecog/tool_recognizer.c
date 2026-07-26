/*
 * tool_recognizer.c — Centralized tool-call recognition/normalization.
 * See tool_recognizer.h for the stage breakdown and rationale.
 *
 * Self-contained: depends only on provider.h (tool_call_t /
 * provider_response_t), libjson (argument parsing), and the registry
 * (Tool-Search scope). No god headers, opaque struct, C11.
 */

#include "tool_recognizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "json.h"          /* libjson: json_node_t, json_parse, json_free,
                            * json_object_get_string, JSON_OBJECT */
#include "registry.h"      /* registry_find */

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Does `args` parse as a JSON object ({ ... })? Mirrors Python's
 * `_parse_tool_arguments`: json.loads must succeed AND be a dict. */
static bool args_is_json_object(const char *args) {
    if (!args || !*args) return false;
    const char *p = skip_ws(args);
    if (*p != '{') return false;
    json_node_t *node = json_parse(args, NULL);
    if (!node) return false;
    bool ok = (node->type == JSON_OBJECT);
    json_free(node);
    return ok;
}

/* Hermes _parse_tool_arguments error shape. */
static void set_malformed_error(tool_call_t *tc, const char *message) {
    tc->malformed = true;
    int n = snprintf(tc->arguments, sizeof(tc->arguments),
        "{\"error\":\"Invalid tool arguments\",\"message\":\"%s\"}", message);
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof(tc->arguments)) tc->arguments[sizeof(tc->arguments) - 1] = '\0';
}

/* Deterministic, stable id when the model omits one (Hermes
 * _deterministic_call_id: derived from name + args + position so the
 * same call yields the same id across retries). */
void tool_recognizer_ensure_id(tool_call_t *tc, int index) {
    if (!tc) return;
    if (tc->id[0] != '\0') return; /* already present */

    /* FNV-1a over name + arguments for a short stable digest. */
    unsigned long hash = 1469598103934665603UL;
    const char *s = tc->name;
    while (*s) { hash ^= (unsigned char)*s++; hash *= 1099511628211UL; }
    s = tc->arguments;
    while (*s) { hash ^= (unsigned char)*s++; hash *= 1099511628211UL; }

    snprintf(tc->id, sizeof(tc->id), "call-%s-%d-%lx",
             tc->name[0] ? tc->name : "unknown", index, hash & 0xffffff);
}

/* Stage 4. */
void tool_recognizer_validate_args(tool_call_t *tc) {
    if (!tc) return;
    if (tc->malformed) return; /* already flagged (e.g. scope) */
    if (!args_is_json_object(tc->arguments)) {
        set_malformed_error(tc,
            "Tool arguments must be a valid JSON object; tool was not executed.");
    }
}

/* Stage 5. */
bool tool_recognizer_unwrap_tool_search(tool_call_t *tc, bool enforce_scope) {
    if (!tc) return false;
    if (strcmp(tc->name, TOOL_RECOGNIZER_BRIDGE_NAME) != 0) return false;

    /* Parse the bridge arguments: {"name": <underlying>, "arguments": <json>}. */
    json_node_t *node = json_parse(tc->arguments, NULL);
    if (!node || node->type != JSON_OBJECT) {
        json_free(node);
        set_malformed_error(tc,
            "Tool-Search bridge call had no parseable arguments; tool was not executed.");
        return true;
    }

    const char *underlying = json_object_get_string(node, "name", NULL);
    const char *underlying_args = json_object_get_string(node, "arguments", NULL);

    if (!underlying || !*underlying) {
        json_free(node);
        set_malformed_error(tc,
            "Tool-Search bridge call omitted the underlying tool name; tool was not executed.");
        return true;
    }

    /* Scope gate: the underlying tool must be registered (granted to this
     * session). Mirrors Hermes _tool_search_scoped_names check. */
    if (enforce_scope && !registry_find(underlying)) {
        json_free(node);
        int n = snprintf(tc->arguments, sizeof(tc->arguments),
            "{\"error\":\"'%s' is not available in this session. "
            "Use tool_search to find tools you can call.\"}", underlying);
        if (n < 0) n = 0;
        if ((size_t)n >= sizeof(tc->arguments)) tc->arguments[sizeof(tc->arguments) - 1] = '\0';
        tc->malformed = true;
        return true;
    }

    /* Peel open: replace name + arguments with the underlying call. The
     * original tool_call entry is intentionally overwritten so every
     * downstream stage (guardrails, hooks, dispatch) observes the REAL tool,
     * exactly as Hermes does for the live transcript/tool_call_id. */
    snprintf(tc->name, sizeof(tc->name), "%s", underlying);
    if (underlying_args) {
        snprintf(tc->arguments, sizeof(tc->arguments), "%s", underlying_args);
    } else {
        tc->arguments[0] = '\0';
    }
    tc->unwrapped = true;
    json_free(node);
    return true;
}

tool_recognizer_opts_t tool_recognizer_default_opts(void) {
    tool_recognizer_opts_t o;
    o.unwrap_tool_search = true;
    o.enforce_scope = true;
    return o;
}

/* ---------------------------------------------------------------------------
 * Aggregate entry point
 * ------------------------------------------------------------------------- */

void tool_recognizer_process_response(provider_response_t *resp,
                                       const tool_recognizer_opts_t *opts) {
    if (!resp) return;
    tool_recognizer_opts_t o = opts ? *opts : tool_recognizer_default_opts();

    for (int i = 0; i < resp->tool_calls_count && i < 64; i++) {
        tool_call_t *tc = &resp->tool_calls[i];
        tc->malformed = false;
        tc->unwrapped = false;

        tool_recognizer_ensure_id(tc, i);                       /* stage 2 */
        if (o.unwrap_tool_search)
            tool_recognizer_unwrap_tool_search(tc, o.enforce_scope); /* stage 5 */
        tool_recognizer_validate_args(tc);                      /* stage 4 */
    }
}
