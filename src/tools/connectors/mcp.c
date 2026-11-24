/*
 * mcp.c - C11 port of upstream tools/connectors/mcp.py.
 * Gates: w31_mcp_de9cb4_p1, w31_mcp_de9cb4_p2.
 *
 * PoP: w31_mcp_de9cb4_p1_validate_names    @ tools/connectors/mcp.py:validate_mcp_names
 * PoP: w31_mcp_de9cb4_p1_misrouted_error   @ tools/connectors/mcp.py:misrouted_to_mcp_error (imported from targets.py)
 * PoP: w31_mcp_de9cb4_p1_bare_error_text   @ tools/connectors/mcp.py:_detail <_BARE_ERRORS chain>
 * PoP: w31_mcp_de9cb4_p1_detail            @ tools/connectors/mcp.py:_detail
 * PoP: w31_mcp_de9cb4_p1_tool_error_body   @ tools/connectors/mcp.py:run_mcp_operation <tool_error body>
 * PoP: w31_mcp_de9cb4_p1_answer_env        @ tools/connectors/mcp.py:_answer_env
 * PoP: w31_mcp_de9cb4_p1_catalog_instructions @ tools/connectors/mcp.py:_catalog_instructions
 * PoP: w31_mcp_de9cb4_p1_cancel_note       @ tools/connectors/mcp.py:_cancel_attempt
 * PoP: w31_mcp_de9cb4_p1_retry_error       @ tools/connectors/mcp.py:retry
 * PoP: w31_mcp_de9cb4_p1_constants         @ tools/connectors/mcp.py:<module notes and budgets>
 * PoP: w31_mcp_de9cb4_p2_missing_required  @ tools/connectors/mcp.py:_missing_required
 * PoP: w31_mcp_de9cb4_p2_install_now_message @ tools/connectors/mcp.py:_install_now <missing credentials>
 * PoP: w31_mcp_de9cb4_p2_late_key          @ tools/connectors/mcp.py:_late_key
 * PoP: w31_mcp_de9cb4_p2_target_snapshot   @ tools/connectors/operation.py:Target.snapshot
 * PoP: w31_mcp_de9cb4_p2_no_card_result    @ tools/connectors/mcp.py:_no_card_result
 * PoP: w31_mcp_de9cb4_p2_answer_plan       @ tools/connectors/mcp.py:apply_answer
 * PoP: w31_mcp_de9cb4_p2_installs_with_oauth @ tools/connectors/mcp.py:_CatalogBackend.installs_with_oauth
 * PoP: w31_mcp_de9cb4_p2_actor_for         @ tools/connectors/mcp.py:_actor
 * PoP: w31_mcp_de9cb4_p2_observable        @ tools/connectors/mcp.py:_Runner.observe <live-target read>
 *
 * mcp.py is the tool entry point for MCP connection work: it validates the
 * names an action may address, folds the card's answer into the operation,
 * starts one backend call per target and turns whatever the backend says into a
 * row and a message.  Every one of those backends is a seam here -- the catalog,
 * the installer, the OAuth flow, the connector gateway's account list, the tool
 * registry, the clock and the pattern redactor -- and each gate passes their
 * answers in on the request object.  What is ported is what the values decide:
 * the words of a refusal, the text of a failure, the coercion of the card's
 * credentials, which row an answer advances, and how a target is reported.
 *
 * Text is compared byte for byte, so a value whose Python ``str()`` differs from
 * the C rendering of the same JSON value is outside this gate: an integral
 * number renders as an int (``10``), a container as its JSON, and only ASCII
 * text is bounded by character count.  The fixtures stay inside that.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_mcp_de9cb4_p1.h"
#include "noc_w31_mcp_de9cb4_p2.h"

/* ------------------------------------------------------------------ text -- */

typedef struct {
    char *p;
    size_t n, cap;
} mcp_sb;

static int mcp_sb_reserve(mcp_sb *b, size_t extra) {
    size_t cap;
    char *p;
    if (b->n + extra + 1 <= b->cap) return 1;
    cap = b->cap ? b->cap : 128;
    while (cap < b->n + extra + 1) cap *= 2;
    p = (char *)realloc(b->p, cap);
    if (!p) return 0;
    b->p = p;
    b->cap = cap;
    return 1;
}

static void mcp_sb_addn(mcp_sb *b, const char *s, size_t len) {
    if (!mcp_sb_reserve(b, len)) return;
    if (len && s) memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}

static void mcp_sb_add(mcp_sb *b, const char *s) {
    if (!s) return;
    mcp_sb_addn(b, s, strlen(s));
}

static void mcp_sb_addc(mcp_sb *b, char c) {
    if (!mcp_sb_reserve(b, 1)) return;
    b->p[b->n++] = c;
    b->p[b->n] = '\0';
}

static char *mcp_sb_take(mcp_sb *b) {
    char *out;
    if (b->p) return b->p;
    out = (char *)malloc(1);
    if (out) out[0] = '\0';
    return out;
}

static char *mcp_dup(const char *s) {
    size_t len = s ? strlen(s) : 0;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    if (len) memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* Python truthiness of a JSON value. */
static int mcp_truthy(const json_t *v) {
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

/* An integral number is an int to Python's json, and stays one here. */
static void mcp_num_text(double d, char *buf, size_t cap) {
    if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) {
        snprintf(buf, cap, "%.0f", d);
        return;
    }
    snprintf(buf, cap, "%.15g", d);
    if (!strpbrk(buf, ".eE") && strlen(buf) + 3 < cap) strcat(buf, ".0");
}

/* Python str() over the shapes a scalar can hold; containers fall back to JSON. */
static char *mcp_py_str(const json_t *v);

/* Python repr(), which differs from str() only in quoting a string. */
static char *mcp_py_repr(const json_t *v) {
    mcp_sb b;
    char num[64];
    if (v && v->type == JSON_STRING) {
        memset(&b, 0, sizeof b);
        mcp_sb_addc(&b, '\'');
        mcp_sb_add(&b, v->str_val ? v->str_val : "");
        mcp_sb_addc(&b, '\'');
        return mcp_sb_take(&b);
    }
    if (v && v->type == JSON_NUMBER) {
        mcp_num_text(v->num_val, num, sizeof num);
        return mcp_dup(num);
    }
    return mcp_py_str(v);
}

static char *mcp_py_str(const json_t *v) {
    mcp_sb b;
    char num[64];
    size_t i, n;
    if (!v || v->type == JSON_NULL) return mcp_dup("None");
    if (v->type == JSON_BOOL) return mcp_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        mcp_num_text(v->num_val, num, sizeof num);
        return mcp_dup(num);
    }
    if (v->type == JSON_STRING) return mcp_dup(v->str_val ? v->str_val : "");
    memset(&b, 0, sizeof b);
    if (v->type == JSON_ARRAY) {
        mcp_sb_addc(&b, '[');
        n = json_len(v);
        for (i = 0; i < n; i++) {
            char *item = mcp_py_repr(json_get(v, i));
            if (i) mcp_sb_add(&b, ", ");
            mcp_sb_add(&b, item ? item : "");
            free(item);
        }
        mcp_sb_addc(&b, ']');
        return mcp_sb_take(&b);
    }
    {
        char *s = json_serialize(v);
        if (!s) return mcp_dup("None");
        n = strlen(s);
        mcp_sb_addc(&b, '{');
        for (i = 0; i < n; i++) {
            if (s[i] == '"') mcp_sb_addc(&b, '\'');
            else mcp_sb_addc(&b, s[i]);
        }
        free(s);
        return mcp_sb_take(&b);
    }
}

/* ', '.join(items) over a JSON array. */
static char *mcp_join(const json_t *items) {
    mcp_sb b;
    size_t i, n;
    memset(&b, 0, sizeof b);
    n = items && items->type == JSON_ARRAY ? json_len(items) : 0;
    for (i = 0; i < n; i++) {
        char *s = mcp_py_str(json_get(items, i));
        if (i) mcp_sb_add(&b, ", ");
        mcp_sb_add(&b, s ? s : "");
        free(s);
    }
    return mcp_sb_take(&b);
}

static int mcp_list_has(const json_t *list, const char *s) {
    size_t i, n;
    if (!list || list->type != JSON_ARRAY) return 0;
    n = json_len(list);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(list, i);
        if (v && v->type == JSON_STRING && strcmp(v->str_val ? v->str_val : "", s) == 0) return 1;
    }
    return 0;
}

static json_t *mcp_string_of(const char *s) { return json_string(s ? s : ""); }

/* The two message-less errors reach the user as words, not class names. */
static const char *mcp_bare_error(const char *key) {
    if (!key) return NULL;
    if (strcmp(key, "CancelledError") == 0)
        return "tool discovery was interrupted; run the same action again to list the tools";
    if (strcmp(key, "TimeoutError") == 0) return "the server did not answer in time";
    return NULL;
}

/* A JSON value that is a plain string, for the places the upstream returns one. */
static char *mcp_value_text(const json_t *v) { return mcp_py_str(v); }

#define MCP_NOTE \
    "Settled once; do not re-ask on your own for any target the user skipped or that " \
    "timed out, but a later request from the USER for that same app is not a re-ask " \
    "\xE2\x80\x94 run it. Connected targets' tools are available now through " \
    "tool_describe/tool_call and are named under tools_listing. A target with " \
    "discovery_error is authorized but its tools are unavailable; retry discovery with " \
    "manage_connections using that target's authorize or install action without asking " \
    "for consent again."

#define MCP_NO_CARD_NOTE \
    "No connection card is drawn for this turn. Show any connect_url to the user so they " \
    "open it in a browser, then ask them to say when they are done. Connected targets' " \
    "tools are available now through tool_describe/tool_call and are named under " \
    "tools_listing. A target with discovery_error is authorized but its tools are " \
    "unavailable; retry discovery with manage_connections using that target's authorize " \
    "or install action without asking for consent again. Do not re-ask on your own for " \
    "skipped or timed-out targets, but a later request from the USER for that same app " \
    "is not a re-ask \xE2\x80\x94 run it."

#define MCP_AUTHORIZATION_KEPT \
    "the authorization had already completed when this was canceled, so it was kept; " \
    "run the same action again to list the tools"

/* ------------------------------------------------------------- p1: names -- */

json_t *w31_mcp_de9cb4_p1_misrouted_error(const json_t *req) {
    char *name = mcp_py_str(json_obj_get(req, "name"));
    char *action = mcp_py_str(json_obj_get(req, "action"));
    mcp_sb b;
    char *text;
    json_t *out;
    memset(&b, 0, sizeof b);
    mcp_sb_add(&b, name ? name : "");
    mcp_sb_add(&b, " is a hosted connector account, not a local MCP server, so '");
    mcp_sb_add(&b, action ? action : "");
    mcp_sb_add(&b, "' does not apply. Call manage_connections with action connect and connectors [\"");
    mcp_sb_add(&b, name ? name : "");
    mcp_sb_add(&b, "\"].");
    free(name);
    free(action);
    text = mcp_sb_take(&b);
    out = mcp_string_of(text);
    free(text);
    return out;
}

json_t *w31_mcp_de9cb4_p1_validate_names(const json_t *req) {
    const json_t *names = json_obj_get(req, "names");
    const json_t *catalog = json_obj_get(req, "catalog");
    const json_t *configured = json_obj_get(req, "configured");
    const json_t *hosted = json_obj_get(req, "hosted");
    const json_t *fail = json_obj_get(req, "catalog_error");
    char *action;
    json_t *unknown, *foreign;
    size_t i, n;
    int install;

    if (!catalog || catalog->type != JSON_ARRAY) catalog = json_null();
    if (!configured || configured->type != JSON_ARRAY) configured = json_null();
    if (fail && fail->type == JSON_NULL) fail = NULL;
    if (!fail) {
        fail = json_obj_get(req, "configured_error");
        if (fail && fail->type == JSON_NULL) fail = NULL;
    }
    if (fail) {
        /* the catalog read failed before any name could be judged */
        char *msg = mcp_py_str(fail);
        mcp_sb b;
        char *text;
        json_t *out;
        memset(&b, 0, sizeof b);
        mcp_sb_add(&b, "could not read the MCP catalog: ");
        mcp_sb_add(&b, msg ? msg : "");
        free(msg);
        text = mcp_sb_take(&b);
        out = mcp_string_of(text);
        free(text);
        return out;
    }

    action = mcp_py_str(json_obj_get(req, "action"));
    install = strcmp(action ? action : "", "install") == 0;

    unknown = json_array();
    foreign = json_array();
    n = names && names->type == JSON_ARRAY ? json_len(names) : 0;
    for (i = 0; i < n; i++) {
        char *name = mcp_py_str(json_get(names, i));
        const json_t *allowed = install ? catalog : configured;
        const char *text = name ? name : "";
        if (!mcp_list_has(allowed, text)) {
            json_append(unknown, mcp_string_of(text));
            if (!mcp_list_has(catalog, text) && !mcp_list_has(configured, text))
                json_append(foreign, mcp_string_of(text));
        }
        free(name);
    }

    if (json_len(unknown) == 0) {
        json_free(unknown);
        json_free(foreign);
        free(action);
        return json_null();
    }

    if (json_len(foreign) > 0 && hosted && hosted->type == JSON_ARRAY) {
        /* a name that is a connector account was addressed to the wrong tool */
        json_t *misrouted = json_array();
        size_t j, m = json_len(foreign);
        for (j = 0; j < m; j++) {
            const json_t *v = json_get(foreign, j);
            const char *name = v && v->type == JSON_STRING ? v->str_val : "";
            if (mcp_list_has(hosted, name ? name : "")) json_append(misrouted, mcp_string_of(name));
        }
        if (json_len(misrouted) > 0) {
            mcp_sb b;
            char *text;
            json_t *out;
            memset(&b, 0, sizeof b);
            m = json_len(misrouted);
            for (j = 0; j < m; j++) {
                const json_t *v = json_get(misrouted, j);
                json_t *one = json_object();
                json_t *part;
                char *msg;
                json_set(one, "name", mcp_string_of(v && v->type == JSON_STRING ? v->str_val : ""));
                json_set(one, "action", mcp_string_of(action));
                part = w31_mcp_de9cb4_p1_misrouted_error(one);
                msg = mcp_py_str(part);
                json_free(part);
                json_free(one);
                if (j) mcp_sb_add(&b, " ");
                mcp_sb_add(&b, msg ? msg : "");
                free(msg);
            }
            json_free(misrouted);
            json_free(unknown);
            json_free(foreign);
            free(action);
            text = mcp_sb_take(&b);
            out = mcp_string_of(text);
            free(text);
            return out;
        }
        json_free(misrouted);
    }

    {
        char *joined_unknown = mcp_join(unknown);
        char *joined_catalog = mcp_join(catalog);
        char *joined_configured = mcp_join(configured);
        mcp_sb b;
        char *text;
        json_t *out;
        memset(&b, 0, sizeof b);
        if (install) {
            mcp_sb_add(&b, "unknown MCP server(s) for install: ");
            mcp_sb_add(&b, joined_unknown ? joined_unknown : "");
            mcp_sb_add(&b, ". Install works for catalog entries only: ");
            mcp_sb_add(&b, joined_catalog && joined_catalog[0] ? joined_catalog : "(empty catalog)");
            mcp_sb_add(&b, ".");
            if (joined_configured && joined_configured[0]) {
                mcp_sb_add(&b, " Already configured (use enable/authorize): ");
                mcp_sb_add(&b, joined_configured);
                mcp_sb_add(&b, ".");
            }
        } else {
            mcp_sb_add(&b, "unknown MCP server(s) for ");
            mcp_sb_add(&b, action ? action : "");
            mcp_sb_add(&b, ": ");
            mcp_sb_add(&b, joined_unknown ? joined_unknown : "");
            mcp_sb_add(&b, ". ");
            mcp_sb_add(&b, action ? action : "");
            mcp_sb_add(&b, " works for servers already in mcp_servers: ");
            mcp_sb_add(&b, joined_configured && joined_configured[0] ? joined_configured
                                                                   : "(none configured)");
            mcp_sb_add(&b, ".");
            if (joined_catalog && joined_catalog[0]) {
                mcp_sb_add(&b, " Catalog entries you can install: ");
                mcp_sb_add(&b, joined_catalog);
                mcp_sb_add(&b, ".");
            }
        }
        free(joined_unknown);
        free(joined_catalog);
        free(joined_configured);
        json_free(unknown);
        json_free(foreign);
        free(action);
        text = mcp_sb_take(&b);
        out = mcp_string_of(text);
        free(text);
        return out;
    }
}

/* ------------------------------------------------------------- p1: text -- */

json_t *w31_mcp_de9cb4_p1_bare_error_text(const json_t *req) {
    const json_t *cls = json_obj_get(req, "exc_class");
    const json_t *msgv = json_obj_get(req, "exc_message");
    int is_exception = cls && cls->type != JSON_NULL;
    char *text = mcp_value_text(msgv);
    const char *mapped;
    json_t *out;

    if (!text || !text[0]) {
        /* str(exc) was empty: a message-less error names its class instead */
        free(text);
        if (is_exception) {
            text = mcp_value_text(cls);
            mapped = mcp_bare_error(text);
            if (mapped) {
                free(text);
                text = mcp_dup(mapped);
            }
        } else {
            text = mcp_dup("error");
        }
    } else {
        mapped = mcp_bare_error(text);
        if (mapped) {
            free(text);
            text = mcp_dup(mapped);
        }
    }
    out = mcp_string_of(text);
    free(text);
    return out;
}

/* The approved credentials of one target, replaced by exact match. */
static char *mcp_redact_approved(const char *text, const json_t *req) {
    const json_t *approved = json_obj_get(req, "approved_env");
    const json_t *values;
    char *name;
    char *cur = mcp_dup(text ? text : "");
    size_t i, n;

    if (!cur || !approved || approved->type != JSON_OBJECT) return cur;
    name = mcp_value_text(json_obj_get(req, "target"));
    values = name ? json_obj_get(approved, name) : NULL;
    free(name);
    if (!values || values->type != JSON_OBJECT) return cur;

    n = json_object_size(values);
    for (i = 0; i < n; i++) {
        const json_t *v = json_object_get_at(values, i);
        char *value;
        mcp_sb b;
        char *found;
        if (!mcp_truthy(v)) continue;
        value = mcp_value_text(v);
        if (!value || !value[0]) {
            free(value);
            continue;
        }
        memset(&b, 0, sizeof b);
        found = cur;
        while (found) {
            char *hit = strstr(found, value);
            if (!hit) break;
            mcp_sb_addn(&b, found, (size_t)(hit - found));
            mcp_sb_add(&b, "[REDACTED]");
            found = hit + strlen(value);
        }
        if (b.p) {
            mcp_sb_add(&b, found ? found : "");
            free(cur);
            cur = mcp_sb_take(&b);
        }
        free(value);
    }
    return cur;
}

json_t *w31_mcp_de9cb4_p1_detail(const json_t *req) {
    json_t *bare = w31_mcp_de9cb4_p1_bare_error_text(req);
    char *text = mcp_redact_approved(bare ? bare->str_val : "", req);
    json_t *out;
    json_free(bare);
    /* redact_sensitive_text(..., force=True) or "error" */
    if (!text || !text[0]) {
        free(text);
        text = mcp_dup("error");
    }
    out = mcp_string_of(text);
    free(text);
    return out;
}

json_t *w31_mcp_de9cb4_p1_tool_error_body(const json_t *req) {
    const json_t *message = json_obj_get(req, "message");
    char *text = (message && message->type != JSON_NULL) ? mcp_value_text(message) : mcp_dup("");
    json_t *out = json_object();

    if (text && strlen(text) > W31_MCP_P1_MAX_TOOL_ERROR_CHARS) {
        /* the body is cut, and the cut says so */
        char *cut = (char *)malloc(W31_MCP_P1_MAX_TOOL_ERROR_CHARS + 1);
        if (cut) {
            memcpy(cut, text, W31_MCP_P1_MAX_TOOL_ERROR_CHARS);
            cut[W31_MCP_P1_MAX_TOOL_ERROR_CHARS] = '\0';
            free(text);
            text = (char *)realloc(cut, strlen(cut) + strlen(W31_MCP_P1_TRUNCATION_MARKER) + 1);
            if (text) strcat(text, W31_MCP_P1_TRUNCATION_MARKER);
        }
    }
    json_set(out, "error", mcp_string_of(text));
    free(text);
    return out;
}

/* ------------------------------------------------------------ p1: card -- */

json_t *w31_mcp_de9cb4_p1_answer_env(const json_t *req) {
    const json_t *raw = req && req->type == JSON_OBJECT ? json_obj_get(req, "env") : req;
    json_t *out = json_object();
    size_t i, n;
    if (!raw || raw->type != JSON_OBJECT) return out;
    n = json_object_size(raw);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(raw, i);
        char *v = mcp_value_text(json_object_get_at(raw, i));
        json_set(out, k ? k : "", mcp_string_of(v));
        free(v);
    }
    return out;
}

json_t *w31_mcp_de9cb4_p1_catalog_instructions(const json_t *req) {
    const json_t *post;
    char *text;
    json_t *out;
    if (!mcp_truthy(json_obj_get(req, "present"))) return json_string("");
    post = json_obj_get(req, "post_install");
    /* str(entry.post_install or ""): an empty note stays empty, not "None" */
    text = mcp_truthy(post) ? mcp_value_text(post) : mcp_dup("");
    out = mcp_string_of(text);
    free(text);
    return out;
}

json_t *w31_mcp_de9cb4_p1_cancel_note(const json_t *req) {
    if (!mcp_truthy(json_obj_get(req, "has_flow"))) return json_string("");
    /* cancel_attempt answered: a committed authorization is kept, not lost */
    if (mcp_truthy(json_obj_get(req, "cancelled"))) return json_string(MCP_AUTHORIZATION_KEPT);
    return json_string("");
}

json_t *w31_mcp_de9cb4_p1_retry_error(const json_t *req) {
    if (!mcp_truthy(json_obj_get(req, "runner")))
        return json_string("this operation has no MCP work to re-run");
    if (mcp_truthy(json_obj_get(req, "settled")))
        return json_string("this operation has settled; its result is frozen");
    return json_null();
}

json_t *w31_mcp_de9cb4_p1_constants(void) {
    json_t *out = json_object();
    json_t *bare = json_object();
    json_set(bare, "CancelledError", json_string(mcp_bare_error("CancelledError")));
    json_set(bare, "TimeoutError", json_string(mcp_bare_error("TimeoutError")));
    json_set(out, "prepare_wait_seconds", json_number(30.0));
    json_set(out, "note", json_string(MCP_NOTE));
    json_set(out, "no_card_note", json_string(MCP_NO_CARD_NOTE));
    json_set(out, "authorization_kept", json_string(MCP_AUTHORIZATION_KEPT));
    json_set(out, "bare_errors", bare);
    return out;
}

/* ------------------------------------------------------------- p2: rows -- */

static const char *mcp_field_str(const json_t *obj, const char *key, const char *def) {
    const json_t *v = obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
    return v && v->type == JSON_STRING ? v->str_val : def;
}

/* contract.resolves: every mcp row is resolved once it is connected or skipped;
 * a catalog install (plugin/skill) also counts a failed row as done. */
static int mcp_resolves(const char *kind, const char *state) {
    if (!state) return 0;
    if (strcmp(state, "connected") == 0 || strcmp(state, "skipped") == 0) return 1;
    if (kind && (strcmp(kind, "plugin") == 0 || strcmp(kind, "skill") == 0))
        return strcmp(state, "failed") == 0;
    return 0;
}

/* Target.snapshot(with_url=...): only the fields that are set are drawn. */
static json_t *mcp_snapshot_of(const json_t *fields, int with_url) {
    const char *name = mcp_field_str(fields, "name", "");
    const char *kind = mcp_field_str(fields, "kind", "mcp");
    const char *action = mcp_field_str(fields, "action", "authorize");
    const char *state = mcp_field_str(fields, "state", "pending");
    const json_t *v;
    json_t *out = json_object();
    size_t i, n;

    json_set(out, "name", mcp_string_of(name));
    json_set(out, "kind", mcp_string_of(kind));
    json_set(out, "action", mcp_string_of(action));
    json_set(out, "state", mcp_string_of(state));
    json_set(out, "resolved", json_bool(mcp_resolves(kind, state) != 0));

    v = json_obj_get(fields, "alias");
    if (mcp_truthy(v)) json_set(out, "alias", json_copy(v));
    if (strcmp(state, "initiated") == 0) {
        v = json_obj_get(fields, "phase");
        if (mcp_truthy(v)) json_set(out, "phase", json_copy(v));
    }
    v = json_obj_get(fields, "detail");
    if (mcp_truthy(v)) json_set(out, "detail", json_copy(v));
    v = json_obj_get(fields, "instructions");
    if (mcp_truthy(v)) json_set(out, "instructions", json_copy(v));
    v = json_obj_get(fields, "connect_url");
    if (with_url && mcp_truthy(v)) json_set(out, "connect_url", json_copy(v));
    v = json_obj_get(fields, "connection_id");
    if (mcp_truthy(v)) json_set(out, "connection_id", json_copy(v));
    v = json_obj_get(fields, "attempt");
    if (mcp_truthy(v)) json_set(out, "attempt", json_copy(v));
    v = json_obj_get(fields, "required_env");
    if (mcp_truthy(v)) json_set(out, "required_env", json_copy(v));
    v = json_obj_get(fields, "launch");
    if (mcp_truthy(v)) json_set(out, "launch", json_copy(v));

    /* out.update(self.extra) */
    v = json_obj_get(fields, "extra");
    if (v && v->type == JSON_OBJECT) {
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            json_obj_del(out, k);
            json_set(out, k ? k : "", json_copy(json_object_get_at(v, i)));
        }
    }
    return out;
}

json_t *w31_mcp_de9cb4_p2_target_snapshot(const json_t *req) {
    const json_t *fields = json_obj_get(req, "target");
    const json_t *with_url = json_obj_get(req, "with_url");
    /* the default is True, but an explicit null is False, as bool(None) is */
    int keep_url = with_url ? mcp_truthy(with_url) : 1;
    if (!fields || fields->type != JSON_OBJECT) return mcp_snapshot_of(json_null(), keep_url);
    return mcp_snapshot_of(fields, keep_url);
}

json_t *w31_mcp_de9cb4_p2_no_card_result(const json_t *req) {
    const json_t *targets = json_obj_get(req, "targets");
    const json_t *deadline = json_obj_get(req, "deadline_at");
    const char *action = mcp_field_str(req, "action", "install");
    json_t *out = json_object();
    json_t *rows = json_array();
    size_t i, n;
    int any_initiated = 0;

    n = targets && targets->type == JSON_ARRAY ? json_len(targets) : 0;
    for (i = 0; i < n; i++) {
        const json_t *fields = json_get(targets, i);
        json_t *merged;
        json_t *row;
        if (!fields || fields->type != JSON_OBJECT) continue;
        /* every row is built from the names and the one action of the call */
        merged = json_copy(fields);
        json_obj_del(merged, "action");
        json_set(merged, "action", mcp_string_of(action));
        if (strcmp(mcp_field_str(fields, "state", "pending"), "initiated") == 0) any_initiated = 1;
        row = mcp_snapshot_of(merged, 1);
        json_free(merged);
        /* result() drops the card's own fields before the model sees the row */
        json_obj_del(row, "connection_id");
        json_obj_del(row, "resolved");
        json_obj_del(row, "launch");
        json_append(rows, row);
    }

    {
        const json_t *op_id = json_obj_get(req, "op_id");
        json_set(out, "op_id", op_id && op_id->type != JSON_NULL ? json_copy(op_id) : json_null());
    }
    json_set(out, "seq", json_number(0.0));
    json_set(out, "deadline_at",
             deadline && deadline->type == JSON_NUMBER ? json_number(deadline->num_val)
                                                       : json_number(0.0));
    json_set(out, "settled_at", json_null());
    json_set(out, "settled_by", json_null());
    json_set(out, "targets", rows);
    json_set(out, "status", json_string(any_initiated ? "initiated" : "settled"));
    json_set(out, "note", json_string(MCP_NO_CARD_NOTE));
    return out;
}

/* str(entry.get("name") or "").strip().lower() */
static char *mcp_target_name(const json_t *entry) {
    const json_t *v = json_obj_get(entry, "name");
    char *raw = mcp_truthy(v) ? mcp_value_text(v) : mcp_dup("");
    char *start = raw, *end;
    size_t i, len;
    char *out;

    if (!raw) return mcp_dup("");
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' ||
           *start == '\f' || *start == '\v')
        start++;
    end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                           end[-1] == '\r' || end[-1] == '\f' || end[-1] == '\v'))
        end--;
    len = (size_t)(end - start);
    out = (char *)malloc(len + 1);
    if (!out) {
        free(raw);
        return NULL;
    }
    for (i = 0; i < len; i++) {
        char c = start[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[len] = '\0';
    free(raw);
    return out;
}

json_t *w31_mcp_de9cb4_p2_answer_plan(const json_t *req) {
    const json_t *raw = json_obj_get(req, "answer");
    const json_t *targets = json_obj_get(req, "targets");
    int has_runner = mcp_truthy(json_obj_get(req, "runner"));
    json_t *answer = NULL;
    json_t *log = json_array();
    json_t *out = json_object();
    size_t i, n, t, ntargets;
    int all_resolved;

    if (raw && raw->type == JSON_STRING) answer = json_parse(raw->str_val ? raw->str_val : "", NULL);
    if (!answer || answer->type != JSON_OBJECT) {
        json_free(answer);
        answer = json_object();
    }

    ntargets = targets && targets->type == JSON_ARRAY ? json_len(targets) : 0;
    all_resolved = ntargets > 0;
    for (t = 0; t < ntargets; t++) {
        const json_t *fields = json_get(targets, t);
        if (!mcp_resolves(mcp_field_str(fields, "kind", "mcp"),
                          mcp_field_str(fields, "state", "pending")))
            all_resolved = 0;
    }

    {
        const json_t *entries = json_obj_get(answer, "targets");
        n = entries && entries->type == JSON_ARRAY ? json_len(entries) : 0;
        for (i = 0; i < n; i++) {
            const json_t *entry = json_get(entries, i);
            char *name;
            const json_t *match = NULL;
            const char *action = "none";
            json_t *row;
            if (!entry || entry->type != JSON_OBJECT) continue;
            name = mcp_target_name(entry);
            for (t = 0; t < ntargets; t++) {
                const json_t *fields = json_get(targets, t);
                if (fields && fields->type == JSON_OBJECT &&
                    strcmp(mcp_field_str(fields, "name", ""), name ? name : "") == 0) {
                    match = fields;
                    break;
                }
            }
            row = json_object();
            json_set(row, "name", mcp_string_of(name));
            json_set(row, "matched", json_bool(match != NULL));
            if (match) {
                const json_t *statusv = json_obj_get(entry, "status");
                char *status = mcp_truthy(statusv) ? mcp_value_text(statusv) : mcp_dup("");
                const char *state = mcp_field_str(match, "state", "pending");
                size_t k;
                for (k = 0; status && status[k]; k++)
                    if (status[k] >= 'A' && status[k] <= 'Z') status[k] = (char)(status[k] + 32);
                if (status && strcmp(status, "skipped") == 0) {
                    action = "skip";
                } else if (status && strcmp(status, "approved") == 0 && has_runner) {
                    if (strcmp(state, "pending") == 0) action = "approve";
                    else if (strcmp(state, "failed") == 0 || strcmp(state, "expired") == 0)
                        action = "retry";
                }
                free(status);
            }
            json_set(row, "action", json_string(action));
            json_append(log, row);
            free(name);
        }
    }

    {
        const json_t *by = json_obj_get(answer, "settled_by");
        int settles = by && by->type == JSON_STRING && by->str_val &&
                      strcmp(by->str_val, "continue") == 0 && !all_resolved;
        json_set(out, "targets", log);
        json_set(out, "settled", json_bool(settles != 0));
    }
    json_free(answer);
    return out;
}

/* ------------------------------------------------- p2: one-line decisions -- */

json_t *w31_mcp_de9cb4_p2_missing_required(const json_t *req) {
    const json_t *declared = json_obj_get(req, "declared");
    const json_t *env = json_obj_get(req, "env");
    json_t *out = json_array();
    size_t i, n;

    n = declared && declared->type == JSON_ARRAY ? json_len(declared) : 0;
    for (i = 0; i < n; i++) {
        const json_t *spec = json_get(declared, i);
        const json_t *required;
        char *name;
        const json_t *value;
        if (!spec || spec->type != JSON_OBJECT) continue;
        required = json_obj_get(spec, "required");
        /* spec.get("required", True): only a present, falsy value lets it pass */
        if (required && !mcp_truthy(required)) continue;
        /* str(spec.get("name") or ""): an absent name is the empty key, not "None" */
        {
            const json_t *nv = json_obj_get(spec, "name");
            name = mcp_truthy(nv) ? mcp_value_text(nv) : mcp_dup("");
        }
        value = env && env->type == JSON_OBJECT ? json_obj_get(env, name ? name : "") : NULL;
        if (!mcp_truthy(value)) json_append(out, json_copy(spec));
        free(name);
    }
    return out;
}

json_t *w31_mcp_de9cb4_p2_install_now_message(const json_t *req) {
    const json_t *declared = json_obj_get(req, "required");
    json_t *missing = json_array();
    json_t *names;
    char *home;
    char *joined;
    mcp_sb b;
    size_t i, n;
    json_t *out;

    n = declared && declared->type == JSON_ARRAY ? json_len(declared) : 0;
    for (i = 0; i < n; i++) {
        const json_t *spec = json_get(declared, i);
        const json_t *required;
        char *name;
        if (!spec || spec->type != JSON_OBJECT) continue;
        required = json_obj_get(spec, "required");
        if (required && !mcp_truthy(required)) continue;
        name = mcp_value_text(json_obj_get(spec, "name"));
        json_append(missing, mcp_string_of(name));
        free(name);
    }
    if (json_len(missing) == 0) {
        /* nothing is missing: the install runs instead of answering */
        json_free(missing);
        return json_null();
    }
    names = json_copy(missing);
    joined = mcp_join(names);
    home = mcp_value_text(json_obj_get(req, "home"));
    memset(&b, 0, sizeof b);
    mcp_sb_add(&b, "set ");
    mcp_sb_add(&b, joined ? joined : "");
    mcp_sb_add(&b, " in the environment or ");
    mcp_sb_add(&b, home ? home : "");
    mcp_sb_add(&b, "/.env, then install again");
    free(names);
    free(joined);
    free(home);
    json_free(missing);
    {
        char *text = mcp_sb_take(&b);
        out = mcp_string_of(text);
        free(text);
    }
    return out;
}

json_t *w31_mcp_de9cb4_p2_late_key(const json_t *req) {
    const json_t *profile = json_obj_get(req, "profile_key");
    char *first = mcp_truthy(profile) ? mcp_value_text(profile)
                                      : mcp_value_text(json_obj_get(req, "home_key"));
    char *session = mcp_value_text(json_obj_get(req, "session_key"));
    json_t *out = json_array();
    json_append(out, mcp_string_of(first));
    json_append(out, mcp_string_of(session));
    free(first);
    free(session);
    return out;
}

json_t *w31_mcp_de9cb4_p2_installs_with_oauth(const json_t *req) {
    const char *type = mcp_field_str(req, "auth_type", "");
    const json_t *provider = json_obj_get(req, "auth_provider");
    int own_oauth = strcmp(type, "oauth") == 0 && !mcp_truthy(provider);
    return json_bool(own_oauth != 0);
}

json_t *w31_mcp_de9cb4_p2_actor_for(const json_t *req) {
    const char *state = mcp_field_str(req, "state", "pending");
    if (strcmp(state, "failed") == 0) return json_string("user");
    return json_string("backend_watcher");
}

json_t *w31_mcp_de9cb4_p2_observable(const json_t *req) {
    const char *state = mcp_field_str(req, "state", "pending");
    int settled = mcp_truthy(json_obj_get(req, "settled"));
    int live = strcmp(state, "pending") == 0 || strcmp(state, "initiated") == 0;
    return json_bool((!settled && live) != 0);
}
