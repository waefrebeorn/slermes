/*
 * sync_diagnostics.c — C home for upstream Hermes agent/lsp/sync_diagnostics.py.
 * STATUS: PORTED (gate w25_syncdiagno_87bd05) — differential oracle green.
 * PoP: w25_syncdiagno_87bd05_supported @ agent/lsp/sync_diagnostics.py:supported
 * PoP: w25_syncdiagno_87bd05_from_tsserver @ agent/lsp/sync_diagnostics.py:_from_tsserver
 * PoP: w25_syncdiagno_87bd05_request @ agent/lsp/sync_diagnostics.py:request
 * PoP: w25_syncdiagno_87bd05_constants @ agent/lsp/sync_diagnostics.py:<module tables>
 *
 * The edit check asks this module first and falls back to the push/pull wait
 * when the server has no synchronous request or the request fails.  Sending the
 * request, the wait budget and the logger are seams: the replies are fixed
 * inputs here, and what is ported is the support gate, the tsserver -> LSP
 * mapping (0-based range, numeric severity, ``source``) and the per-command
 * failure ladder that decides between "no verdict", "no time" and diagnostics.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_syncdiagno_87bd05.h"

/* tsserver category -> LSP DiagnosticSeverity, as typescript-language-server maps it. */
static const struct {
    const char *category;
    int severity;
} SD_SEVERITY[] = {
    {"error", 1}, {"warning", 2}, {"message", 3}, {"suggestion", 4},
};

/* _TS_COMMANDS */
static const char *const SD_COMMANDS[W25_SYNCDIAGNO_87BD05_COMMANDS] = {
    "syntacticDiagnosticsSync", "semanticDiagnosticsSync",
};

#define SD_UNSUPPORTED "_hermes_sync_diagnostics_unsupported"
#define SD_TS_SERVER "typescript"

static int sd_truthy(const json_t *v) {
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

static char *sd_dup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *out = (char *)malloc(n);
    if (out) memcpy(out, s ? s : "", n);
    return out;
}

/* Python str() over the shapes a tsserver field holds. */
static char *sd_py_str(const json_t *v) {
    char buf[64];
    if (!v) return sd_dup("");
    switch (v->type) {
    case JSON_NULL:  return sd_dup("");
    case JSON_BOOL:  return sd_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.12g", v->num_val);
        return sd_dup(buf);
    case JSON_STRING: return sd_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY: {
        size_t i, n = json_len(v), need = 3;
        char *out, *q;
        for (i = 0; i < n; i++) {
            char *item = sd_py_str(json_get(v, i));
            need += strlen(item) + 3;
            free(item);
        }
        out = (char *)malloc(need);
        if (!out) return sd_dup("");
        q = out;
        *q++ = '[';
        for (i = 0; i < n; i++) {
            char *item = sd_py_str(json_get(v, i));
            if (i) { *q++ = ','; *q++ = ' '; }
            q += snprintf(q, need - (size_t)(q - out), "%s", item);
            free(item);
        }
        *q++ = ']';
        *q = '\0';
        return out;
    }
    case JSON_OBJECT:
        return sd_dup("{}");
    }
    return sd_dup("");
}

static int sd_is_supported(const json_t *server_id, int flag) {
    if (flag) return 0;
    if (!server_id || server_id->type != JSON_STRING) return 0;
    return strcmp(server_id->str_val ? server_id->str_val : "", SD_TS_SERVER) == 0;
}

json_t *w25_syncdiagno_87bd05_supported(const json_t *server_id, const json_t *unsupported) {
    return json_bool(sd_is_supported(server_id, sd_truthy(unsupported)) ? 1 : 0);
}

/* start.get(key, 1) - 1 ; *ok cleared for a position Python would refuse. */
static double sd_offset(const json_t *loc, const char *key, int *ok) {
    const json_t *v = json_obj_get(loc, key);
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val - 1.0;
    if (v->type == JSON_BOOL) return (v->bool_val ? 1.0 : 0.0) - 1.0;
    *ok = 0;
    return 0.0;
}

json_t *w25_syncdiagno_87bd05_from_tsserver(const json_t *item) {
    const json_t *start, *end, *v;
    json_t *diag, *range, *rs, *re;
    char *msg, *cat;
    int ok = 1, severity = 1;
    double sl, sc, el, ec;
    size_t i;

    if (!item || item->type != JSON_OBJECT) return json_null();
    start = json_obj_get(item, "startLocation");
    end = json_obj_get(item, "endLocation");
    if (!start || start->type != JSON_OBJECT) return json_null();
    if (!end || end->type != JSON_OBJECT) return json_null();

    sl = sd_offset(start, "line", &ok);
    sc = sd_offset(start, "offset", &ok);
    el = sd_offset(end, "line", &ok);
    ec = sd_offset(end, "offset", &ok);
    if (!ok) return json_null();

    range = json_object();
    rs = json_object();
    re = json_object();
    json_set(rs, "line", json_number(sl));
    json_set(rs, "character", json_number(sc));
    json_set(re, "line", json_number(el));
    json_set(re, "character", json_number(ec));
    json_set(range, "start", rs);
    json_set(range, "end", re);

    diag = json_object();
    json_set(diag, "range", range);

    v = json_obj_get(item, "message");
    msg = sd_py_str(sd_truthy(v) ? v : NULL);
    json_set(diag, "message", json_string(msg ? msg : ""));
    free(msg);

    /* _TS_SEVERITY.get(str(item.get("category") or "error"), 1) */
    v = json_obj_get(item, "category");
    cat = sd_py_str(sd_truthy(v) ? v : NULL);
    if (!cat || !cat[0]) { free(cat); cat = sd_dup("error"); }
    for (i = 0; i < sizeof(SD_SEVERITY) / sizeof(SD_SEVERITY[0]); i++) {
        if (strcmp(cat, SD_SEVERITY[i].category) == 0) { severity = SD_SEVERITY[i].severity; break; }
    }
    free(cat);
    json_set(diag, "severity", json_number((double)severity));

    v = json_obj_get(item, "source");
    if (sd_truthy(v)) json_set(diag, "source", json_copy(v));
    else json_set(diag, "source", json_string("typescript"));

    v = json_obj_get(item, "code");
    if (v && v->type != JSON_NULL) json_set(diag, "code", json_copy(v));
    return diag;
}

static json_t *sd_result(const char *status, int flag) {
    json_t *out = json_object();
    json_set(out, "status", json_string(status));
    json_set(out, "unsupported", json_bool(flag ? 1 : 0));
    return out;
}

json_t *w25_syncdiagno_87bd05_request(const json_t *req) {
    const json_t *server_id = json_obj_get(req, "server_id");
    const json_t *replies = json_obj_get(req, "replies");
    const json_t *timeout = json_obj_get(req, "timeout");
    int flag = sd_truthy(json_obj_get(req, "unsupported"));
    double budget = timeout && timeout->type == JSON_NUMBER ? timeout->num_val : 0.0;
    json_t *out, *diagnostics;
    size_t i;

    if (!sd_is_supported(server_id, flag) || budget <= 0.0) return sd_result("none", flag);

    diagnostics = json_array();
    for (i = 0; i < W25_SYNCDIAGNO_87BD05_COMMANDS; i++) {
        const json_t *entry = replies && replies->type == JSON_ARRAY ? json_get(replies, i) : NULL;
        const char *kind = entry && entry->type == JSON_OBJECT
                               ? json_get_str(entry, "kind", "ok") : "ok";
        const json_t *reply = entry && entry->type == JSON_OBJECT
                                  ? json_obj_get(entry, "reply") : NULL;

        if (!strcmp(kind, "timeout")) {
            char buf[192];
            snprintf(buf, sizeof(buf), "%s did not answer in %.1fs", SD_COMMANDS[i], budget);
            out = sd_result("timeout", flag);
            json_set(out, "message", json_string(buf));
            json_free(diagnostics);
            return out;
        }
        if (!strcmp(kind, "request_error")) { /* the server rejects the command */
            json_free(diagnostics);
            return sd_result("none", 1);
        }
        if (!strcmp(kind, "protocol_error")) { /* connection trouble: the push wait reports it */
            json_free(diagnostics);
            return sd_result("none", flag);
        }
        {
            /* body = reply["body"] if isinstance(reply, dict) and reply.get("success", True) */
            const json_t *body = NULL;
            if (reply && reply->type == JSON_OBJECT) {
                const json_t *success = json_obj_get(reply, "success");
                if (!success || sd_truthy(success)) body = json_obj_get(reply, "body");
            }
            if (!body || body->type != JSON_ARRAY) {
                json_free(diagnostics);
                return sd_result("none", 1);
            }
            {
                size_t j, n = json_len(body);
                for (j = 0; j < n; j++) {
                    json_t *d = w25_syncdiagno_87bd05_from_tsserver(json_get(body, j));
                    if (d && d->type != JSON_NULL) json_append(diagnostics, d);
                    else json_free(d);
                }
            }
        }
    }
    out = sd_result("diagnostics", flag);
    json_set(out, "diagnostics", diagnostics);
    return out;
}

json_t *w25_syncdiagno_87bd05_constants(void) {
    json_t *o = json_object();
    json_t *commands = json_array();
    json_t *servers = json_array();
    json_t *severity = json_object();
    size_t i;

    for (i = 0; i < W25_SYNCDIAGNO_87BD05_COMMANDS; i++)
        json_append(commands, json_string(SD_COMMANDS[i]));
    json_append(servers, json_string(SD_TS_SERVER));
    for (i = 0; i < sizeof(SD_SEVERITY) / sizeof(SD_SEVERITY[0]); i++)
        json_set(severity, SD_SEVERITY[i].category, json_number((double)SD_SEVERITY[i].severity));
    json_set(o, "ts_servers", servers);
    json_set(o, "ts_commands", commands);
    json_set(o, "ts_severity", severity);
    json_set(o, "unsupported_attr", json_string(SD_UNSUPPORTED));
    return o;
}
