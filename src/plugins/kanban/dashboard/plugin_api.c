/*
 * plugin_api.c — C11 home for upstream Hermes
 *   plugins/kanban/dashboard/plugin_api.py  (1845 lines, 113 defs, 20 classes)
 *
 * STATUS: behaviour port of the module's PURE half. Every symbol below is a
 * differential port of one upstream function or constant table; the DB
 * connection, the FastAPI router/HTTPException surface, the config files, the
 * auxiliary LLM client, the gateway config loader and the filesystem stat stay
 * seams. Not linked into the build.
 *
 * GATES (source order, one partition each):
 *   w27_pluginapi_1215d9_p1  lines    1-410   slug / payload / error plumbing
 *   w27_pluginapi_1215d9_p2  lines  531-960   status verbs, overrides, bulk
 *   w27_pluginapi_1215d9_p3  lines 1064-1300  estimate / config / home / diags
 *   w27_pluginapi_1215d9_p4  lines 1301-1845  boards / profiles / orchestration / ws
 *
 * Result envelope shared by every entry point:
 *   {"ok":true,  "value":<v>}
 *   {"ok":false, "kind":"http", "status":<int>, "detail":<str>}
 *   {"ok":false, "kind":"exc",  "exc":<str>,     "detail":<str>}
 *
 * PoP:
 *   plugins_kanban_dashboard_plugin_api_normalize_slug
 *     @ plugins/kanban/dashboard/plugin_api.py:_normalize_slug_or_400
 *   plugins_kanban_dashboard_plugin_api_resolve_board
 *     @ plugins/kanban/dashboard/plugin_api.py:_resolve_board
 *   plugins_kanban_dashboard_plugin_api_existing_board_slug
 *     @ plugins/kanban/dashboard/plugin_api.py:_existing_board_slug
 *   plugins_kanban_dashboard_plugin_api_placeholders
 *     @ plugins/kanban/dashboard/plugin_api.py:_placeholders
 *   plugins_kanban_dashboard_plugin_api_attachment_dict
 *     @ plugins/kanban/dashboard/plugin_api.py:_attachment_dict
 *   plugins_kanban_dashboard_plugin_api_task_dict
 *     @ plugins/kanban/dashboard/plugin_api.py:_task_dict
 *   plugins_kanban_dashboard_plugin_api_warnings_summary
 *     @ plugins/kanban/dashboard/plugin_api.py:_warnings_summary_from_diagnostics
 *   plugins_kanban_dashboard_plugin_api_attach_diagnostics
 *     @ plugins/kanban/dashboard/plugin_api.py:_attach_diagnostics
 *   plugins_kanban_dashboard_plugin_api_require_obj
 *     @ plugins/kanban/dashboard/plugin_api.py:_require
 *   plugins_kanban_dashboard_plugin_api_require_ok
 *     @ plugins/kanban/dashboard/plugin_api.py:_require_ok
 *   plugins_kanban_dashboard_plugin_api_conflict
 *     @ plugins/kanban/dashboard/plugin_api.py:_conflict
 *   plugins_kanban_dashboard_plugin_api_map_errors
 *     @ plugins/kanban/dashboard/plugin_api.py:_map_errors
 *   plugins_kanban_dashboard_plugin_api_errors_to_500
 *     @ plugins/kanban/dashboard/plugin_api.py:_errors_to_500
 *   plugins_kanban_dashboard_plugin_api_link_tasks
 *     @ plugins/kanban/dashboard/plugin_api.py:_link_tasks
 *   plugins_kanban_dashboard_plugin_api_module_constants
 *     @ plugins/kanban/dashboard/plugin_api.py:BOARD_COLUMNS,
 *       _CARD_SUMMARY_PREVIEW_CHARS, _EVENT_POLL_SECONDS, _RUNNING_DIRECT_MSG
 *   plugins_kanban_dashboard_plugin_api_status_refusal
 *     @ plugins/kanban/dashboard/plugin_api.py:_apply_status
 *   plugins_kanban_dashboard_plugin_api_status_table
 *     @ plugins/kanban/dashboard/plugin_api.py:_STATUS_HANDLERS
 *   plugins_kanban_dashboard_plugin_api_patch_status_route
 *     @ plugins/kanban/dashboard/plugin_api.py:_patch_status
 *   plugins_kanban_dashboard_plugin_api_open_parent_refusal
 *     @ plugins/kanban/dashboard/plugin_api.py:_open_parent_refusal
 *   plugins_kanban_dashboard_plugin_api_model_override_value
 *     @ plugins/kanban/dashboard/plugin_api.py:_apply_model_override
 *   plugins_kanban_dashboard_plugin_api_reasoning_effort_value
 *     @ plugins/kanban/dashboard/plugin_api.py:_apply_reasoning_effort
 *   plugins_kanban_dashboard_plugin_api_override_wanted
 *     @ plugins/kanban/dashboard/plugin_api.py:_OVERRIDE_OPS
 *   plugins_kanban_dashboard_plugin_api_drag_route
 *     @ plugins/kanban/dashboard/plugin_api.py:_drag_to
 *   plugins_kanban_dashboard_plugin_api_bulk_ids
 *     @ plugins/kanban/dashboard/plugin_api.py:bulk_update
 *   plugins_kanban_dashboard_plugin_api_comment_body_gate
 *     @ plugins/kanban/dashboard/plugin_api.py:add_comment
 *   plugins_kanban_dashboard_plugin_api_review_assignee_deferred
 *     @ plugins/kanban/dashboard/plugin_api.py:update_task
 *   plugins_kanban_dashboard_plugin_api_cap
 *     @ plugins/kanban/dashboard/plugin_api.py:_cap
 *   plugins_kanban_dashboard_plugin_api_estimate
 *     @ plugins/kanban/dashboard/plugin_api.py:_run_estimate
 *   plugins_kanban_dashboard_plugin_api_config_shape
 *     @ plugins/kanban/dashboard/plugin_api.py:get_config
 *   plugins_kanban_dashboard_plugin_api_home_channels
 *     @ plugins/kanban/dashboard/plugin_api.py:_configured_home_channels
 *   plugins_kanban_dashboard_plugin_api_home_for_platform
 *     @ plugins/kanban/dashboard/plugin_api.py:_home_for_platform
 *   plugins_kanban_dashboard_plugin_api_home_channels_subscribed
 *     @ plugins/kanban/dashboard/plugin_api.py:get_home_channels
 *   plugins_kanban_dashboard_plugin_api_active_profile
 *     @ plugins/kanban/dashboard/plugin_api.py:_active_profile_name
 *   plugins_kanban_dashboard_plugin_api_model_options
 *     @ plugins/kanban/dashboard/plugin_api.py:model_options
 *   plugins_kanban_dashboard_plugin_api_diagnostics_list
 *     @ plugins/kanban/dashboard/plugin_api.py:list_diagnostics
 *   plugins_kanban_dashboard_plugin_api_board_display_kwargs
 *     @ plugins/kanban/dashboard/plugin_api.py:_board_display_kwargs
 *   plugins_kanban_dashboard_plugin_api_resolve_project
 *     @ plugins/kanban/dashboard/plugin_api.py:_resolve_project
 *   plugins_kanban_dashboard_plugin_api_default_workspace_kind
 *     @ plugins/kanban/dashboard/plugin_api.py:_default_workspace_kind
 *   plugins_kanban_dashboard_plugin_api_annotate_board_meta
 *     @ plugins/kanban/dashboard/plugin_api.py:_annotate_board_meta
 *   plugins_kanban_dashboard_plugin_api_validated_profile_name
 *     @ plugins/kanban/dashboard/plugin_api.py:_validated_profile_name
 *   plugins_kanban_dashboard_plugin_api_orchestration_settings
 *     @ plugins/kanban/dashboard/plugin_api.py:get_orchestration_settings
 *   plugins_kanban_dashboard_plugin_api_since_param
 *     @ plugins/kanban/dashboard/plugin_api.py:_since_param
 *   plugins_kanban_dashboard_plugin_api_ws_board
 *     @ plugins/kanban/dashboard/plugin_api.py:_ws_board
 *   plugins_kanban_dashboard_plugin_api_ws_board_live
 *     @ plugins/kanban/dashboard/plugin_api.py:_ws_board_live
 *   plugins_kanban_dashboard_plugin_api_list_boards_annotate
 *     @ plugins/kanban/dashboard/plugin_api.py:list_boards
 *   plugins_kanban_dashboard_plugin_api_profile_roster
 *     @ plugins/kanban/dashboard/plugin_api.py:list_profile_roster
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w27_pluginapi_1215d9_p1.h"
#include "noc_w27_pluginapi_1215d9_p2.h"
#include "noc_w27_pluginapi_1215d9_p3.h"
#include "noc_w27_pluginapi_1215d9_p4.h"

/* ======================================================================== */
/* Shared helpers (not part of any gate's surface)                          */
/* ======================================================================== */

typedef struct {
    char  *p;
    size_t len;
    size_t cap;
} sb_t;

static void sb_init(sb_t *b) {
    b->cap = 256;
    b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_reserve(sb_t *b, size_t extra) {
    size_t cap;
    char *p;
    if (!b->p) return;
    if (b->len + extra + 1 <= b->cap) return;
    cap = b->cap;
    while (cap < b->len + extra + 1) cap *= 2;
    p = (char *)realloc(b->p, cap);
    if (!p) return;
    b->p = p;
    b->cap = cap;
}

static void sb_putn(sb_t *b, const char *s, size_t n) {
    if (!b->p || !s) return;
    sb_reserve(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void sb_puts(sb_t *b, const char *s) { if (s) sb_putn(b, s, strlen(s)); }

static void sb_putc(sb_t *b, char c) { sb_putn(b, &c, 1); }

static void sb_free(sb_t *b) {
    if (b->p) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
}

/* --- json accessors that follow Python's dict.get / None semantics ------ */

static const json_t *jget(const json_t *o, const char *k) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    return json_obj_get(o, k);
}

/* Python None: absent key or JSON null. */
static bool jis_none(const json_t *v) { return v == NULL || v->type == JSON_NULL; }

/* str value, or NULL when absent / null / not a string. */
static const char *jstr(const json_t *o, const char *k) {
    const json_t *v = jget(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static double jnum(const json_t *o, const char *k, double def) {
    const json_t *v = jget(o, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

/* Python truthiness of a decoded value. */
static bool jtruthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:  return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* Python str(n) for an int-ish double. */
static const char *jnum_str(double d, char *buf, size_t n) {
    if (d == (double)(long long)d) snprintf(buf, n, "%lld", (long long)d);
    else snprintf(buf, n, "%g", d);
    return buf;
}

/* Python str(v) for f-string interpolation of a scalar. */
static const char *jpy_str(const json_t *v, char *buf, size_t n) {
    if (jis_none(v)) { snprintf(buf, n, "None"); return buf; }
    switch (v->type) {
        case JSON_BOOL:   snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return buf;
        case JSON_NUMBER: return jnum_str(v->num_val, buf, n);
        case JSON_STRING: return v->str_val;
        default:          snprintf(buf, n, "<?>"); return buf;
    }
}

/* --- Python repr("...") for f-strings that use {x!r} ------------------- */

static void sb_repr(sb_t *b, const char *s) {
    const unsigned char *p;
    char quote = '\'';
    if (!s) { sb_puts(b, "None"); return; }
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    sb_putc(b, quote);
    for (p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)quote) { sb_putc(b, '\\'); sb_putc(b, (char)c); }
        else if (c == '\\') { sb_puts(b, "\\\\"); }
        else if (c == '\n') { sb_puts(b, "\\n"); }
        else if (c == '\r') { sb_puts(b, "\\r"); }
        else if (c == '\t') { sb_puts(b, "\\t"); }
        else if (c < 0x20 || c == 0x7f) { char t[8]; snprintf(t, sizeof(t), "\\x%02x", (unsigned)c); sb_puts(b, t); }
        else { sb_putc(b, (char)c); }
    }
    sb_putc(b, quote);
}

/* --- result envelopes -------------------------------------------------- */

static json_t *env_ok(json_t *v) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", v ? v : json_null());
    return o;
}

static json_t *env_http(int status, const char *detail) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "kind", json_string("http"));
    json_set(o, "status", json_number((double)status));
    json_set(o, "detail", json_string(detail ? detail : ""));
    return o;
}

static json_t *env_exc(const char *exc, const char *detail) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "kind", json_string("exc"));
    json_set(o, "exc", json_string(exc ? exc : "Exception"));
    json_set(o, "detail", json_string(detail ? detail : ""));
    return o;
}

/* Python str.strip() over ASCII whitespace (the slugs/names this port sees). */
static bool py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *py_strip_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    while (*s && py_isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n > 0 && py_isspace((unsigned char)s[n - 1])) n--;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *py_lower_dup(const char *s) {
    char *out = (char *)malloc(strlen(s ? s : "") + 1);
    if (!out) return NULL;
    {
        const char *src = s ? s : "";
        char *p = out;
        for (; *src; src++, p++) *p = (*src >= 'A' && *src <= 'Z') ? (char)(*src - 'A' + 'a') : *src;
        *p = '\0';
    }
    return out;
}

/* Python len() over UTF-8 code points, and a code-point-bounded prefix. */
static size_t py_char_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

static char *py_char_prefix(const char *s, size_t limit) {
    size_t n = 0, bytes;
    const unsigned char *p = (const unsigned char *)s;
    char *out;
    while (*p) {
        if ((*p & 0xC0) != 0x80) {
            if (n == limit) break;
            n++;
        }
        p++;
    }
    bytes = (size_t)((const char *)p - s);
    out = (char *)malloc(bytes + 1);
    if (!out) return NULL;
    memcpy(out, s, bytes);
    out[bytes] = '\0';
    return out;
}

/* Python int() on a decoded value; 0 on the ValueError/TypeError path. */
static long long jint_coerce(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        int sign = 1;
        long long acc = 0;
        while (*s && py_isspace((unsigned char)*s)) s++;
        if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
        if (*s < '0' || *s > '9') return 0;
        while (*s >= '0' && *s <= '9') { acc = acc * 10 + (*s - '0'); s++; }
        while (*s && py_isspace((unsigned char)*s)) s++;
        if (*s) return 0;
        return sign * acc;
    }
    return 0;
}

/* ======================================================================== */
/* Gate p1 — slug / payload / error plumbing                                 */
/* ======================================================================== */

/* plugins_kanban_dashboard_plugin_api_normalize_slug
 * @ plugins/kanban/dashboard/plugin_api.py:_normalize_slug_or_400 */
static json_t *p1_normalize_node(const json_t *slug) {
    char *stripped = NULL;
    char *normed = NULL;

    if (!jis_none(slug)) {
        char buf[64];
        const char *raw = (slug->type == JSON_STRING) ? slug->str_val
                                                     : jpy_str(slug, buf, sizeof(buf));
        stripped = py_strip_dup(raw);
        normed = py_lower_dup(stripped);
    }
    if (!normed || normed[0] == '\0') {
        free(stripped);
        free(normed);
        return env_ok(json_null());
    }
    {
        /* ^[a-z0-9][a-z0-9\-_]{0,63}$ */
        size_t n = strlen(normed);
        bool ok = n >= 1 && n <= 64;
        for (size_t i = 0; ok && i < n; i++) {
            char c = normed[i];
            bool good = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        (i > 0 && (c == '-' || c == '_'));
            if (!good) ok = false;
        }
        if (ok) {
            json_t *out;
            free(stripped);
            out = env_ok(json_string(normed));
            free(normed);
            return out;
        }
    }
    /* ValueError -> HTTPException(400, str(exc)); repr() uses the RAW value. */
    {
        sb_t b;
        json_t *out;
        char buf[64];
        const char *raw = (slug && slug->type == JSON_STRING) ? slug->str_val
                                                             : (slug ? jpy_str(slug, buf, sizeof(buf)) : NULL);
        sb_init(&b);
        sb_puts(&b, "invalid board slug ");
        sb_repr(&b, raw);
        sb_puts(&b, ": must be 1-64 chars, lowercase alphanumerics / hyphens / underscores,"
                    " not starting with '-' or '_'");
        out = env_http(400, b.p);
        sb_free(&b);
        free(stripped);
        free(normed);
        return out;
    }
}

json_t *w27p1_normalize_slug(const json_t *req) {
    return p1_normalize_node(jget(req, "slug"));
}

/* plugins_kanban_dashboard_plugin_api_resolve_board
 * @ plugins/kanban/dashboard/plugin_api.py:_resolve_board */
json_t *w27p1_resolve_board(const json_t *req) {
    const json_t *board = jget(req, "board");
    json_t *normed_env;
    const char *normed;

    if (jis_none(board) || (board->type == JSON_STRING && board->str_val[0] == '\0'))
        return env_ok(json_null());

    normed_env = p1_normalize_node(board);
    if (!json_get_bool(normed_env, "ok", false)) return normed_env;
    normed = json_get_str(normed_env, "value", NULL);

    if (normed && strcmp(normed, "default") != 0) {
        sb_t b;
        json_t *out = NULL;
        sb_init(&b);
        if (!json_get_bool(req, "exists", false)) {
            sb_puts(&b, "board ");
            sb_repr(&b, normed);
            sb_puts(&b, " does not exist");
            out = env_http(404, b.p);
        } else if (json_get_bool(req, "archived", false)) {
            sb_puts(&b, "board ");
            sb_repr(&b, normed);
            sb_puts(&b, " is archived");
            out = env_http(404, b.p);
        }
        if (out) {
            sb_free(&b);
            json_free(normed_env);
            return out;
        }
        sb_free(&b);
    }
    {
        json_t *out = env_ok(normed ? json_string(normed) : json_null());
        json_free(normed_env);
        return out;
    }
}

/* plugins_kanban_dashboard_plugin_api_existing_board_slug
 * @ plugins/kanban/dashboard/plugin_api.py:_existing_board_slug */
json_t *w27p1_existing_board_slug(const json_t *req) {
    const char *slug = jstr(req, "slug");
    json_t *normed_env = p1_normalize_node(jget(req, "slug"));
    const char *normed;

    if (!json_get_bool(normed_env, "ok", false)) return normed_env;
    normed = json_get_str(normed_env, "value", NULL);
    if (!normed || !json_get_bool(req, "exists", false)) {
        sb_t b;
        json_t *out;
        sb_init(&b);
        sb_puts(&b, "board ");
        sb_repr(&b, slug); /* repr() of the RAW path slug */
        sb_puts(&b, " does not exist");
        out = env_http(404, b.p);
        sb_free(&b);
        json_free(normed_env);
        return out;
    }
    {
        json_t *out = env_ok(json_string(normed));
        json_free(normed_env);
        return out;
    }
}

/* plugins_kanban_dashboard_plugin_api_placeholders
 * @ plugins/kanban/dashboard/plugin_api.py:_placeholders */
json_t *w27p1_placeholders(const json_t *req) {
    const json_t *ids = jget(req, "ids");
    sb_t b;
    json_t *out;
    size_t n = (ids && ids->type == JSON_ARRAY) ? ids->c.count : 0;

    sb_init(&b);
    for (size_t i = 0; i < n; i++) {
        if (i) sb_putc(&b, ',');
        sb_putc(&b, '?');
    }
    out = env_ok(json_string(b.p));
    sb_free(&b);
    return out;
}

/* plugins_kanban_dashboard_plugin_api_attachment_dict
 * @ plugins/kanban/dashboard/plugin_api.py:_attachment_dict */
json_t *w27p1_attachment_dict(const json_t *req) {
    static const char *const keys[] = {"id", "task_id", "filename", "content_type",
                                       "size", "uploaded_by", "stored_path", "created_at"};
    const json_t *att = jget(req, "attachment");
    json_t *o = json_object();
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const json_t *v = jget(att, keys[i]);
        json_set(o, keys[i], v ? json_copy(v) : json_null());
    }
    return env_ok(o);
}

/* The `asdict(Task)` field order, which _task_dict emits verbatim. */
static const char *const P1_TASK_FIELDS[] = {
    "id", "title", "body", "assignee", "status", "priority", "created_by", "created_at",
    "started_at", "completed_at", "workspace_kind", "workspace_path", "claim_lock",
    "claim_expires", "tenant", "branch_name", "project_id", "result", "idempotency_key",
    "consecutive_failures", "worker_pid", "last_failure_error", "max_runtime_seconds",
    "last_heartbeat_at", "current_run_id", "workflow_template_id", "current_step_key",
    "skills", "model_override", "provider_override", "reasoning_effort", "max_retries",
    "goal_mode", "goal_max_turns", "session_id", "block_kind", "block_recurrences",
    "completion_contract"};

static json_t *p1_task_field_default(const char *k) {
    if (!strcmp(k, "priority") || !strcmp(k, "consecutive_failures") ||
        !strcmp(k, "block_recurrences"))
        return json_number(0);
    if (!strcmp(k, "goal_mode")) return json_bool(false);
    return json_null();
}

/* plugins_kanban_dashboard_plugin_api_task_dict
 * @ plugins/kanban/dashboard/plugin_api.py:_task_dict */
json_t *w27p1_task_dict(const json_t *req) {
    const json_t *task = jget(req, "task");
    const json_t *c = jget(task, "created_at");
    const json_t *s = jget(task, "started_at");
    const json_t *co = jget(task, "completed_at");
    json_t *o = json_object();
    json_t *age = json_object();
    long long now = (long long)jnum(req, "now", 0);
    bool fallback = false;

    for (size_t i = 0; i < sizeof(P1_TASK_FIELDS) / sizeof(P1_TASK_FIELDS[0]); i++) {
        const char *k = P1_TASK_FIELDS[i];
        const json_t *v = jget(task, k);
        json_set(o, k, v ? json_copy(v) : p1_task_field_default(k));
    }
    if (jis_none(c)) json_set(age, "created_age_seconds", json_null());
    else if (c->type == JSON_NUMBER)
        json_set(age, "created_age_seconds", json_number((double)now - c->num_val));
    else fallback = true;

    if (jis_none(s)) json_set(age, "started_age_seconds", json_null());
    else if (s->type == JSON_NUMBER)
        json_set(age, "started_age_seconds", json_number((double)now - s->num_val));
    else fallback = true;

    if (jis_none(co)) {
        json_set(age, "time_to_complete_seconds", json_null());
    } else if (co->type != JSON_NUMBER) {
        fallback = true;
    } else {
        /* (_s or _c): a zero/None start falls back to created_at. */
        const json_t *base = (s && s->type == JSON_NUMBER && s->num_val != 0.0) ? s
                            : (c && c->type == JSON_NUMBER && c->num_val != 0.0) ? c : NULL;
        if (!base) fallback = true; /* TypeError -> the except branch nulls all three */
        else json_set(age, "time_to_complete_seconds", json_number(co->num_val - base->num_val));
    }
    if (fallback) {
        json_free(age);
        age = json_object();
        json_set(age, "created_age_seconds", json_null());
        json_set(age, "started_age_seconds", json_null());
        json_set(age, "time_to_complete_seconds", json_null());
    }
    json_set(o, "age", age);
    {
        const json_t *ls = jget(req, "latest_summary");
        const json_t *rs = jget(req, "current_run_started_at");
        json_set(o, "latest_summary", ls ? json_copy(ls) : json_null());
        json_set(o, "current_run_started_at", rs ? json_copy(rs) : json_null());
    }
    return env_ok(o);
}

static int p3_severity_index(const json_t *v) {
    if (!v || v->type != JSON_STRING) return -1;
    if (!strcmp(v->str_val, "warning")) return 0;
    if (!strcmp(v->str_val, "error")) return 1;
    if (!strcmp(v->str_val, "critical")) return 2;
    return -1;
}

static json_t *p1_warnings_value(const json_t *diags) {
    json_t *out = json_object();
    json_t *kinds = json_object();
    double count = 0, latest = 0;
    int highest_idx = -1;
    const char *highest_sev = NULL;

    for (size_t i = 0; i < diags->c.count; i++) {
        const json_t *d = diags->c.items[i];
        const json_t *cnt, *kind, *seen, *sev;
        const char *kname;
        double n, prev = 0, sv;
        int idx;
        if (!d || d->type != JSON_OBJECT) continue;
        cnt = jget(d, "count");
        n = jis_none(cnt) ? 1.0 : ((cnt->type == JSON_NUMBER) ? cnt->num_val : 1.0);
        kind = jget(d, "kind");
        kname = (kind && kind->type == JSON_STRING) ? kind->str_val : NULL;
        if (kname) {
            json_t *kv = json_obj_get(kinds, kname);
            if (kv && kv->type == JSON_NUMBER) prev = kv->num_val;
            json_set(kinds, kname, json_number(prev + n));
        }
        count += n;
        seen = jget(d, "last_seen_at");
        sv = (seen && seen->type == JSON_NUMBER) ? seen->num_val : 0.0;
        if (sv > latest) latest = sv;
        sev = jget(d, "severity");
        idx = p3_severity_index(sev);
        if (idx >= 0 && idx > highest_idx) { highest_idx = idx; highest_sev = sev->str_val; }
    }
    json_set(out, "count", json_number(count));
    json_set(out, "kinds", kinds);
    json_set(out, "latest_at", json_number(latest));
    json_set(out, "highest_severity", highest_sev ? json_string(highest_sev) : json_null());
    return out;
}

/* plugins_kanban_dashboard_plugin_api_warnings_summary
 * @ plugins/kanban/dashboard/plugin_api.py:_warnings_summary_from_diagnostics */
json_t *w27p1_warnings_summary(const json_t *req) {
    const json_t *diags = jget(req, "diagnostics");
    if (!diags || diags->type != JSON_ARRAY || diags->c.count == 0) return env_ok(json_null());
    return env_ok(p1_warnings_value(diags));
}

/* plugins_kanban_dashboard_plugin_api_attach_diagnostics
 * @ plugins/kanban/dashboard/plugin_api.py:_attach_diagnostics */
json_t *w27p1_attach_diagnostics(const json_t *req) {
    const json_t *task = jget(req, "task");
    const json_t *diags = jget(req, "diagnostics");
    json_t *o = (task && task->type == JSON_OBJECT) ? json_copy(task) : json_object();

    if (jtruthy(diags) && diags->type == JSON_ARRAY) {
        json_set(o, "diagnostics", json_copy(diags));
        json_set(o, "warnings", p1_warnings_value(diags));
    }
    return env_ok(o);
}

/* plugins_kanban_dashboard_plugin_api_require_obj
 * @ plugins/kanban/dashboard/plugin_api.py:_require */
json_t *w27p1_require_obj(const json_t *req) {
    if (!json_get_bool(req, "found", false)) {
        sb_t b;
        json_t *out;
        char ibuf[64];
        const char *ident = jpy_str(jget(req, "ident"), ibuf, sizeof(ibuf));
        const char *label = jstr(req, "label");
        sb_init(&b);
        sb_puts(&b, label ? label : "");
        sb_putc(&b, ' ');
        sb_puts(&b, ident ? ident : "None");
        sb_puts(&b, " not found");
        out = env_http(404, b.p);
        sb_free(&b);
        return out;
    }
    {
        const json_t *obj = jget(req, "obj");
        return env_ok(obj ? json_copy(obj) : json_null());
    }
}

/* plugins_kanban_dashboard_plugin_api_require_ok
 * @ plugins/kanban/dashboard/plugin_api.py:_require_ok */
json_t *w27p1_require_ok(const json_t *req) {
    if (!json_get_bool(req, "ok", false)) return env_http(404, "task not found");
    return env_ok(json_null()); /* the Python helper returns None */
}

/* plugins_kanban_dashboard_plugin_api_conflict
 * @ plugins/kanban/dashboard/plugin_api.py:_conflict */
json_t *w27p1_conflict(const json_t *req) {
    const char *detail = jstr(req, "detail");
    return env_http(409, detail ? detail : "");
}

/* plugins_kanban_dashboard_plugin_api_map_errors
 * @ plugins/kanban/dashboard/plugin_api.py:_map_errors */
json_t *w27p1_map_errors(const json_t *req) {
    const char *exc = jstr(req, "exc");
    const json_t *types = jget(req, "types");
    const char *message = jstr(req, "message");

    if (!exc) exc = "Exception";
    if (types && types->type == JSON_ARRAY) {
        for (size_t i = 0; i < types->c.count; i++) {
            const json_t *t = types->c.items[i];
            if (t && t->type == JSON_STRING && !strcmp(t->str_val, exc))
                return env_http((int)jnum(req, "status", 500), message ? message : "");
        }
    }
    if (!strcmp(exc, "HTTPException")) {
        const char *hd = jstr(req, "http_detail");
        return env_http((int)jnum(req, "http_status", 500), hd ? hd : "");
    }
    return env_exc(exc, message ? message : "");
}

/* plugins_kanban_dashboard_plugin_api_errors_to_500
 * @ plugins/kanban/dashboard/plugin_api.py:_errors_to_500 */
json_t *w27p1_errors_to_500(const json_t *req) {
    const char *exc = jstr(req, "exc");
    const char *prefix = jstr(req, "prefix");
    const char *message = jstr(req, "message");
    sb_t b;
    json_t *out;

    if (exc && !strcmp(exc, "HTTPException")) {
        const char *hd = jstr(req, "http_detail");
        return env_http((int)jnum(req, "http_status", 500), hd ? hd : "");
    }
    sb_init(&b);
    sb_puts(&b, prefix ? prefix : "");
    sb_puts(&b, ": ");
    sb_puts(&b, message ? message : "");
    out = env_http(500, b.p);
    sb_free(&b);
    return out;
}

/* plugins_kanban_dashboard_plugin_api_link_tasks
 * @ plugins/kanban/dashboard/plugin_api.py:_link_tasks */
json_t *w27p1_link_tasks(const json_t *req) {
    const json_t *links = jget(req, "links");
    const json_t *tasks = jget(req, "tasks");
    const json_t *parents = jget(links, "parents");
    const json_t *children = jget(links, "children");
    json_t *rows = json_array();
    const char **order;
    size_t total, seen = 0;

    total = (parents && parents->type == JSON_ARRAY ? parents->c.count : 0) +
            (children && children->type == JSON_ARRAY ? children->c.count : 0);
    order = (const char **)calloc(total ? total : 1, sizeof(char *));
    if (!order) return env_ok(rows);

    /* dict.fromkeys([*parents, *children]) — first appearance wins. */
    for (int pass = 0; pass < 2; pass++) {
        const json_t *list = pass ? children : parents;
        if (!list || list->type != JSON_ARRAY) continue;
        for (size_t i = 0; i < list->c.count; i++) {
            const json_t *v = list->c.items[i];
            const char *id = (v && v->type == JSON_STRING) ? v->str_val : NULL;
            bool dup = false;
            if (!id) continue;
            for (size_t j = 0; j < seen; j++) if (!strcmp(order[j], id)) { dup = true; break; }
            if (!dup) order[seen++] = id;
        }
    }
    for (size_t i = 0; i < seen; i++) {
        const json_t *task = jget(tasks, order[i]);
        json_t *row;
        if (!task || task->type != JSON_OBJECT) continue; /* dropped/foreign rows absent */
        row = json_object();
        {
            const json_t *title = jget(task, "title");
            const json_t *status = jget(task, "status");
            json_set(row, "id", json_string(order[i]));
            json_set(row, "title", title ? json_copy(title) : json_null());
            json_set(row, "status", status ? json_copy(status) : json_null());
        }
        json_append(rows, row);
    }
    free(order);
    return env_ok(rows);
}

/* plugins_kanban_dashboard_plugin_api_module_constants
 * @ plugins/kanban/dashboard/plugin_api.py:BOARD_COLUMNS,
 *   _CARD_SUMMARY_PREVIEW_CHARS, _EVENT_POLL_SECONDS, _RUNNING_DIRECT_MSG */
json_t *w27p1_module_constants(const json_t *req) {
    static const char *const cols[W27_P1_BOARD_COLUMN_COUNT] = {
        "triage", "todo", "scheduled", "ready", "running", "blocked", "review", "done"};
    json_t *o = json_object();
    json_t *arr = json_array();
    (void)req;
    for (size_t i = 0; i < W27_P1_BOARD_COLUMN_COUNT; i++) json_append(arr, json_string(cols[i]));
    json_set(o, "board_columns", arr);
    json_set(o, "card_summary_preview_chars", json_number(W27_P1_CARD_SUMMARY_PREVIEW_CHARS));
    json_set(o, "event_poll_seconds", json_number(0.3));
    json_set(o, "running_direct_msg",
             json_string("Cannot set status to 'running' directly; use the dispatcher/claim path"));
    return env_ok(o);
}

/* ======================================================================== */
/* Gate p2 — status verbs, overrides, bulk                                   */
/* ======================================================================== */

static const char *const P2_STATUS_HANDLERS[W27_P2_STATUS_HANDLER_COUNT] = {
    "done", "blocked", "scheduled", "review", "ready", "todo", "triage"};

static int p2_handler_index(const char *s) {
    if (!s) return -1;
    for (size_t i = 0; i < W27_P2_STATUS_HANDLER_COUNT; i++)
        if (!strcmp(P2_STATUS_HANDLERS[i], s)) return (int)i;
    return -1;
}

/* plugins_kanban_dashboard_plugin_api_status_refusal
 * @ plugins/kanban/dashboard/plugin_api.py:_apply_status */
json_t *w27p2_status_refusal(const json_t *req) {
    const char *s = jstr(req, "status");
    if (s && !strcmp(s, "running"))
        return env_exc("_StatusRejected",
                       "Cannot set status to 'running' directly; use the dispatcher/claim path");
    if (p2_handler_index(s) < 0) {
        const char *detail = jstr(req, "unknown_detail");
        return env_exc("_StatusRejected", detail ? detail : "");
    }
    return env_ok(json_bool(true));
}

/* plugins_kanban_dashboard_plugin_api_status_table
 * @ plugins/kanban/dashboard/plugin_api.py:_STATUS_HANDLERS */
json_t *w27p2_status_table(const json_t *req) {
    json_t *arr = json_array();
    (void)req;
    for (size_t i = 0; i < W27_P2_STATUS_HANDLER_COUNT; i++)
        json_append(arr, json_string(P2_STATUS_HANDLERS[i]));
    return env_ok(arr);
}

/* plugins_kanban_dashboard_plugin_api_patch_status_route
 * @ plugins/kanban/dashboard/plugin_api.py:_patch_status */
json_t *w27p2_patch_status_route(const json_t *req) {
    const char *s = jstr(req, "status");
    return env_ok(json_string((s && !strcmp(s, "archived")) ? "archive" : "verb"));
}

/* plugins_kanban_dashboard_plugin_api_open_parent_refusal
 * @ plugins/kanban/dashboard/plugin_api.py:_open_parent_refusal */
json_t *w27p2_open_parent_refusal(const json_t *req) {
    const char *s = jstr(req, "status");
    const json_t *blockers = jget(req, "blockers");
    const char *task_id = jstr(req, "task_id");
    sb_t b;
    json_t *out;

    if (!s || (strcmp(s, "done") && strcmp(s, "review"))) return env_ok(json_null());
    if (!blockers || blockers->type != JSON_ARRAY || blockers->c.count == 0)
        return env_ok(json_null());
    sb_init(&b);
    sb_puts(&b, "cannot move ");
    sb_puts(&b, task_id ? task_id : "");
    sb_puts(&b, " to ");
    sb_repr(&b, s);
    sb_puts(&b, ": unsatisfied parent dependencies: ");
    for (size_t i = 0; i < blockers->c.count; i++) {
        const json_t *pair = blockers->c.items[i];
        const json_t *pid = (pair && pair->type == JSON_ARRAY) ? json_get(pair, 0) : NULL;
        const json_t *st = (pair && pair->type == JSON_ARRAY) ? json_get(pair, 1) : NULL;
        char ibuf[64], sbuf[64];
        if (i) sb_puts(&b, ", ");
        sb_puts(&b, jpy_str(pid, ibuf, sizeof(ibuf)));
        sb_puts(&b, " (");
        sb_puts(&b, jpy_str(st, sbuf, sizeof(sbuf)));
        sb_putc(&b, ')');
    }
    sb_puts(&b, "; complete the parents first (done or archived)");
    out = env_ok(json_string(b.p));
    sb_free(&b);
    return out;
}

/* plugins_kanban_dashboard_plugin_api_model_override_value
 * @ plugins/kanban/dashboard/plugin_api.py:_apply_model_override */
json_t *w27p2_model_override_value(const json_t *req) {
    bool clear = json_get_bool(req, "clear", false);
    const json_t *model = jget(req, "model_override");
    const json_t *provider = jget(req, "provider_override");
    json_t *o = json_object();

    if (clear) {
        json_set(o, "model", json_null());
    } else {
        const char *m = (model && model->type == JSON_STRING) ? model->str_val : NULL;
        char *stripped = py_strip_dup(m ? m : "");
        if (stripped && stripped[0]) json_set(o, "model", json_string(stripped));
        else json_set(o, "model", json_null());
        free(stripped);
    }
    json_set(o, "provider", provider ? json_copy(provider) : json_null());
    return env_ok(o);
}

/* plugins_kanban_dashboard_plugin_api_reasoning_effort_value
 * @ plugins/kanban/dashboard/plugin_api.py:_apply_reasoning_effort */
json_t *w27p2_reasoning_effort_value(const json_t *req) {
    const json_t *eff;
    if (json_get_bool(req, "clear", false)) return env_ok(json_null());
    eff = jget(req, "reasoning_effort");
    return env_ok(eff ? json_copy(eff) : json_null());
}

/* plugins_kanban_dashboard_plugin_api_override_wanted
 * @ plugins/kanban/dashboard/plugin_api.py:_OVERRIDE_OPS */
json_t *w27p2_override_wanted(const json_t *req) {
    bool clear = json_get_bool(req, "clear", false);
    bool present = !jis_none(jget(req, "value"));
    return env_ok(json_bool(clear || present));
}

/* plugins_kanban_dashboard_plugin_api_drag_route
 * @ plugins/kanban/dashboard/plugin_api.py:_drag_to */
json_t *w27p2_drag_route(const json_t *req) {
    const char *target = jstr(req, "target");
    const char *current = jstr(req, "current_status"); /* NULL = task vanished */

    if (target && !strcmp(target, "triage")) current = NULL; /* no current-state query */
    if (target && !strcmp(target, "ready") && current &&
        (!strcmp(current, "blocked") || !strcmp(current, "scheduled")))
        return env_ok(json_string("unblock"));
    if (current && !strcmp(current, "review")) return env_ok(json_string("reopen_review"));
    return env_ok(json_string("direct"));
}

/* plugins_kanban_dashboard_plugin_api_bulk_ids
 * @ plugins/kanban/dashboard/plugin_api.py:bulk_update */
json_t *w27p2_bulk_ids(const json_t *req) {
    const json_t *ids = jget(req, "ids");
    json_t *arr = json_array();

    if (ids && ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < ids->c.count; i++) {
            const json_t *v = ids->c.items[i];
            if (!jtruthy(v)) continue;
            json_append(arr, json_copy(v));
        }
    }
    if (json_len(arr) == 0) {
        json_free(arr);
        return env_http(400, "ids is required");
    }
    return env_ok(arr);
}

/* plugins_kanban_dashboard_plugin_api_comment_body_gate
 * @ plugins/kanban/dashboard/plugin_api.py:add_comment */
json_t *w27p2_comment_body_gate(const json_t *req) {
    const char *body = jstr(req, "body");
    char *stripped = py_strip_dup(body ? body : "");
    bool blank = !stripped || stripped[0] == '\0';
    free(stripped);
    if (blank) return env_http(400, "body is required");
    return env_ok(json_bool(true));
}

/* plugins_kanban_dashboard_plugin_api_review_assignee_deferred
 * @ plugins/kanban/dashboard/plugin_api.py:update_task */
json_t *w27p2_review_assignee_deferred(const json_t *req) {
    const char *status = jstr(req, "status");
    bool has_assignee = !jis_none(jget(req, "assignee"));
    bool deferred = status && !strcmp(status, "review") && has_assignee;
    return env_ok(json_bool(deferred));
}

/* ======================================================================== */
/* Gate p3 — estimate / config / home channels / diagnostics                 */
/* ======================================================================== */

/* plugins_kanban_dashboard_plugin_api_cap
 * @ plugins/kanban/dashboard/plugin_api.py:_cap */
json_t *w27p3_cap(const json_t *req) {
    const char *s = jstr(req, "s");
    long long n = (long long)jnum(req, "n", 0);
    char *stripped = py_strip_dup(s ? s : "");
    json_t *out;

    if (!stripped) return env_ok(json_string(""));
    if (n < 0) n = 0;
    if (py_char_len(stripped) <= (size_t)n) {
        out = env_ok(json_string(stripped));
        free(stripped);
        return out;
    }
    {
        char *prefix = py_char_prefix(stripped, (size_t)n);
        sb_t b;
        sb_init(&b);
        sb_puts(&b, prefix ? prefix : "");
        sb_puts(&b, "\xE2\x80\xA6"); /* U+2026 HORIZONTAL ELLIPSIS */
        out = env_ok(json_string(b.p));
        sb_free(&b);
        free(prefix);
        free(stripped);
        return out;
    }
}

static json_t *p3_estimate_result(const json_t *parsed, const json_t *model) {
    const json_t *tokens = jget(parsed, "est_tokens");
    const json_t *complexity = jget(parsed, "complexity");
    const json_t *rationale = jget(parsed, "rationale");
    json_t *o = json_object();
    long long est = 0;
    char cbuf[64];
    const char *cstr = NULL;
    char *cstripped;
    char *rstripped;

    if (!jis_none(tokens) && tokens->type != JSON_ARRAY && tokens->type != JSON_OBJECT)
        est = jint_coerce(tokens);
    json_set(o, "ok", json_bool(true));
    json_set(o, "est_tokens", json_number((double)est));

    if (!jis_none(complexity) && complexity->type != JSON_ARRAY && complexity->type != JSON_OBJECT)
        cstr = jpy_str(complexity, cbuf, sizeof(cbuf));
    cstripped = py_strip_dup(cstr ? cstr : "");
    if (cstripped)
        for (char *p = cstripped; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    if (cstripped && (!strcmp(cstripped, "S") || !strcmp(cstripped, "M") || !strcmp(cstripped, "L")))
        json_set(o, "complexity", json_string(cstripped));
    else
        json_set(o, "complexity", json_null());

    {
        const char *rstr = NULL;
        if (!jis_none(rationale) && rationale->type != JSON_ARRAY && rationale->type != JSON_OBJECT)
            rstr = jpy_str(rationale, cbuf, sizeof(cbuf));
        rstripped = py_strip_dup(rstr ? rstr : "");
        if (rstripped && rstripped[0]) json_set(o, "rationale", json_string(rstripped));
        else json_set(o, "rationale", json_null());
    }
    json_set(o, "model", model ? json_copy(model) : json_null());
    free(cstripped);
    free(rstripped);
    return o;
}

static json_t *p3_estimate_fail(const char *reason) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "reason", json_string(reason));
    return env_ok(o);
}

/* plugins_kanban_dashboard_plugin_api_estimate
 * @ plugins/kanban/dashboard/plugin_api.py:_run_estimate */
json_t *w27p3_estimate(const json_t *req) {
    const char *title = jstr(req, "title");
    const char *raw = jstr(req, "raw");
    const char *llm_error = jstr(req, "llm_error");
    const json_t *model = jget(req, "model");
    char *tstripped = py_strip_dup(title ? title : "");
    char *stripped;
    const char *scan;
    json_t *parsed = NULL;

    if (!tstripped || tstripped[0] == '\0') {
        free(tstripped);
        return p3_estimate_fail("a title is required to estimate");
    }
    free(tstripped);
    if (!json_get_bool(req, "aux", true)) return p3_estimate_fail("auxiliary client unavailable");
    if (llm_error) {
        sb_t b;
        json_t *out;
        sb_init(&b);
        sb_puts(&b, "LLM error: ");
        sb_puts(&b, llm_error);
        out = p3_estimate_fail(b.p);
        sb_free(&b);
        return out;
    }
    /* raw = (resp.choices[0].message.content or "").strip() */
    stripped = py_strip_dup(raw ? raw : "");
    scan = stripped ? stripped : "";
    while (*scan && py_isspace((unsigned char)*scan)) scan++;
    if (stripped && *scan == '{') {
        parsed = json_parse(stripped, NULL);
    } else {
        /* re.search(r"\{.*\}", raw, re.DOTALL) — leftmost '{' to last '}'. */
        const char *first = strchr(scan, '{');
        const char *last = strrchr(scan, '}');
        if (first && last && last > first) {
            size_t n = (size_t)(last - first) + 1;
            char *blob = (char *)malloc(n + 1);
            if (blob) {
                memcpy(blob, first, n);
                blob[n] = '\0';
                parsed = json_parse(blob, NULL);
                free(blob);
            }
        }
    }
    free(stripped);
    if (!parsed || parsed->type != JSON_OBJECT) {
        if (parsed) json_free(parsed);
        return p3_estimate_fail("could not parse an estimate from the model");
    }
    {
        json_t *o = p3_estimate_result(parsed, model);
        json_free(parsed);
        return env_ok(o);
    }
}

/* plugins_kanban_dashboard_plugin_api_config_shape
 * @ plugins/kanban/dashboard/plugin_api.py:get_config */
json_t *w27p3_config_shape(const json_t *req) {
    const json_t *cfg = jget(req, "config");
    const json_t *dashboard = jget(cfg, "dashboard");
    const json_t *kanban = (dashboard && dashboard->type == JSON_OBJECT) ? jget(dashboard, "kanban") : NULL;
    const json_t *tenant = jget(kanban, "default_tenant");
    const json_t *lane = jget(kanban, "lane_by_profile");
    const json_t *arch = jget(kanban, "include_archived_by_default");
    const json_t *md = jget(kanban, "render_markdown");
    json_t *o = json_object();

    if (jtruthy(tenant)) json_set(o, "default_tenant", json_copy(tenant));
    else json_set(o, "default_tenant", json_string(""));
    /* dict.get(k, default): an ABSENT key takes the default, an explicit null does not. */
    json_set(o, "lane_by_profile", json_bool(lane == NULL ? true : jtruthy(lane)));
    json_set(o, "include_archived_by_default", json_bool(arch == NULL ? false : jtruthy(arch)));
    json_set(o, "render_markdown", json_bool(md == NULL ? true : jtruthy(md)));
    return env_ok(o);
}

/* plugins_kanban_dashboard_plugin_api_home_channels
 * @ plugins/kanban/dashboard/plugin_api.py:_configured_home_channels */
json_t *w27p3_home_channels(const json_t *req) {
    const json_t *platforms = jget(req, "platforms");
    json_t *rows = json_array();

    for (size_t i = 0; platforms && platforms->type == JSON_ARRAY && i < platforms->c.count; i++) {
        const json_t *p = platforms->c.items[i];
        const json_t *home = jget(p, "home_channel");
        const json_t *chat, *thread, *name;
        json_t *row;
        if (!jtruthy(p) || !jtruthy(home)) continue;
        chat = jget(home, "chat_id");
        thread = jget(home, "thread_id");
        name = jget(home, "name");
        row = json_object();
        json_set(row, "platform", jstr(p, "value") ? json_string(jstr(p, "value")) : json_null());
        json_set(row, "chat_id", chat ? json_copy(chat) : json_null());
        json_set(row, "thread_id", jtruthy(thread) ? json_copy(thread) : json_string(""));
        json_set(row, "name", jtruthy(name) ? json_copy(name) : json_string("Home"));
        json_append(rows, row);
    }
    /* result.sort(key=lambda r: r["platform"]) — stable insertion sort. */
    for (size_t i = 1; i < json_len(rows); i++) {
        json_t *cur = json_copy(json_get(rows, i));
        const char *ck = json_get_str(json_get(rows, i), "platform", "");
        size_t j = i;
        while (j > 0) {
            const char *pk = json_get_str(json_get(rows, j - 1), "platform", "");
            if (strcmp(pk, ck) <= 0) break;
            json_array_set(rows, j, json_copy(json_get(rows, j - 1)));
            j--;
        }
        json_array_set(rows, j, cur);
    }
    return env_ok(rows);
}

/* plugins_kanban_dashboard_plugin_api_home_for_platform
 * @ plugins/kanban/dashboard/plugin_api.py:_home_for_platform */
json_t *w27p3_home_for_platform(const json_t *req) {
    const json_t *homes = jget(req, "homes");
    const char *platform = jstr(req, "platform");
    const char *detail = jstr(req, "detail");

    if (homes && homes->type == JSON_ARRAY && platform) {
        for (size_t i = 0; i < homes->c.count; i++) {
            const json_t *h = homes->c.items[i];
            const char *p = jstr(h, "platform");
            if (p && !strcmp(p, platform)) return env_ok(json_copy(h));
        }
    }
    return env_http(404, detail ? detail : "");
}

/* plugins_kanban_dashboard_plugin_api_home_channels_subscribed
 * @ plugins/kanban/dashboard/plugin_api.py:get_home_channels */
json_t *w27p3_home_channels_subscribed(const json_t *req) {
    const json_t *homes = jget(req, "homes");
    const json_t *subs = jget(req, "subs");
    const json_t *task_id = jget(req, "task_id");
    json_t *out = json_object();
    json_t *list = json_array();

    for (size_t i = 0; homes && homes->type == JSON_ARRAY && i < homes->c.count; i++) {
        const json_t *home = homes->c.items[i];
        const char *hp = jstr(home, "platform");
        const json_t *hchat = jget(home, "chat_id");
        const json_t *hthread = jget(home, "thread_id");
        json_t *row = json_copy(home);
        bool subbed = false;

        if (jtruthy(task_id) && subs && subs->type == JSON_ARRAY) {
            for (size_t j = 0; j < subs->c.count; j++) {
                const json_t *sub = subs->c.items[j];
                const char *sp = jstr(sub, "platform");
                const char *sc = jstr(sub, "chat_id");
                const char *st = jstr(sub, "thread_id");
                /* str(sub.get(k) or "") — a null thread_id coerces to "". */
                if (!st) st = "";
                /* The tuple test uses the HOME values raw: a non-string never matches
                 * the str()-coerced subscription tuple. */
                if (!hp || !sp || strcmp(hp, sp)) continue;
                if (!hchat || hchat->type != JSON_STRING || !sc || strcmp(hchat->str_val, sc)) continue;
                if (hthread && hthread->type == JSON_STRING) {
                    if (strcmp(hthread->str_val, st)) continue;
                } else if (st[0]) continue;
                subbed = true;
                break;
            }
        }
        json_set(row, "subscribed", json_bool(subbed));
        json_append(list, row);
    }
    json_set(out, "home_channels", list);
    return env_ok(out);
}

/* plugins_kanban_dashboard_plugin_api_active_profile
 * @ plugins/kanban/dashboard/plugin_api.py:_active_profile_name */
json_t *w27p3_active_profile(const json_t *req) {
    const json_t *p;
    if (json_get_bool(req, "raise", false)) return env_ok(json_string("default"));
    p = jget(req, "profile");
    if (jtruthy(p)) return env_ok(json_copy(p));
    return env_ok(json_string("default"));
}

/* plugins_kanban_dashboard_plugin_api_model_options
 * @ plugins/kanban/dashboard/plugin_api.py:model_options */
json_t *w27p3_model_options(const json_t *req) {
    const json_t *payload = jget(req, "payload");
    const json_t *providers = jget(payload, "providers");
    json_t *out = json_object();
    json_t *list = json_array();

    if (json_get_bool(req, "raise", false) || !providers || providers->type != JSON_ARRAY) {
        json_set(out, "providers", list);
        return env_ok(out);
    }
    for (size_t i = 0; i < providers->c.count; i++) {
        const json_t *row = providers->c.items[i];
        const json_t *models = jget(row, "models");
        const json_t *label = jget(row, "label");
        const json_t *slug = jget(row, "slug");
        json_t *entry;
        if (!jtruthy(models)) continue;
        entry = json_object();
        if (slug) json_set(entry, "slug", json_copy(slug));
        else json_set(entry, "slug", json_string(""));
        json_set(entry, "label", jtruthy(label) ? json_copy(label)
                               : (slug ? json_copy(slug) : json_string("")));
        json_set(entry, "models", models->type == JSON_ARRAY ? json_copy(models) : json_array());
        json_append(list, entry);
    }
    json_set(out, "providers", list);
    return env_ok(out);
}

static bool p3_severity_at_or_above(const json_t *sev, const char *threshold) {
    int si = p3_severity_index(sev);
    int ti = -1;
    if (threshold) {
        if (!strcmp(threshold, "warning")) ti = 0;
        else if (!strcmp(threshold, "error")) ti = 1;
        else if (!strcmp(threshold, "critical")) ti = 2;
    }
    if (si < 0 || ti < 0) return false;
    return si >= ti;
}

/* plugins_kanban_dashboard_plugin_api_diagnostics_list
 * @ plugins/kanban/dashboard/plugin_api.py:list_diagnostics */
json_t *w27p3_diagnostics_list(const json_t *req) {
    const json_t *diags = jget(req, "diags_by_task");
    const json_t *rows_in = jget(req, "rows");
    const char *severity = jstr(req, "severity");
    json_t *out = json_object();
    json_t *list = json_array();
    json_t *kept = json_object(); /* filtered {task_id: [diags]}, insertion order kept */
    size_t total = 0;

    if (diags && diags->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(diags); i++) {
            const char *tid = json_object_get_key_at(diags, i);
            const json_t *dl = json_object_get_at(diags, i);
            json_t *keep = json_array();
            for (size_t j = 0; dl && dl->type == JSON_ARRAY && j < dl->c.count; j++) {
                const json_t *d = dl->c.items[j];
                if (severity && severity[0] &&
                    !p3_severity_at_or_above(jget(d, "severity"), severity))
                    continue;
                json_append(keep, json_copy(d));
            }
            if (json_len(keep) > 0) json_set(kept, tid, keep);
            else json_free(keep);
        }
    }
    if (json_object_size(kept) == 0) {
        json_set(out, "diagnostics", list);
        json_set(out, "count", json_number(0));
        json_free(kept);
        return env_ok(out);
    }
    for (size_t i = 0; i < json_object_size(kept); i++) {
        const char *tid = json_object_get_key_at(kept, i);
        const json_t *dl = json_object_get_at(kept, i);
        const json_t *r = jget(rows_in, tid);
        const json_t *title = jget(r, "title");
        const json_t *status = jget(r, "status");
        const json_t *assignee = jget(r, "assignee");
        json_t *entry = json_object();
        if (!jtruthy(r)) { title = status = assignee = NULL; }
        json_set(entry, "task_id", json_string(tid));
        json_set(entry, "task_title", title ? json_copy(title) : json_null());
        json_set(entry, "task_status", status ? json_copy(status) : json_null());
        json_set(entry, "task_assignee", assignee ? json_copy(assignee) : json_null());
        json_set(entry, "diagnostics", json_copy(dl));
        total += json_len(dl);
        json_append(list, entry);
    }
    /* sort(key=(-sev_idx(first.severity, -1), -(first.last_seen_at or 0))) — stable. */
    for (size_t i = 1; i < json_len(list); i++) {
        json_t *cur = json_copy(json_get(list, i));
        const json_t *cd = json_get(json_obj_get(cur, "diagnostics"), 0);
        double k1 = -(double)p3_severity_index(jget(cd, "severity"));
        double k2 = -jnum(cd, "last_seen_at", 0);
        size_t j = i;
        for (;;) {
            const json_t *prev, *pd;
            double p1, p2;
            if (j == 0) break;
            prev = json_get(list, j - 1);
            pd = json_get(json_obj_get(prev, "diagnostics"), 0);
            p1 = -(double)p3_severity_index(jget(pd, "severity"));
            p2 = -jnum(pd, "last_seen_at", 0);
            if (p1 < k1 || (p1 == k1 && p2 <= k2)) break;
            json_array_set(list, j, json_copy(prev));
            j--;
        }
        json_array_set(list, j, cur);
    }
    json_set(out, "diagnostics", list);
    json_set(out, "count", json_number((double)total));
    json_free(kept);
    return env_ok(out);
}

/* ======================================================================== */
/* Gate p4 — boards / profiles / orchestration / websocket                   */
/* ======================================================================== */

/* plugins_kanban_dashboard_plugin_api_board_display_kwargs
 * @ plugins/kanban/dashboard/plugin_api.py:_board_display_kwargs */
json_t *w27p4_board_display_kwargs(const json_t *req) {
    static const char *const keys[] = {"name", "description", "icon", "color"};
    const json_t *p = jget(req, "payload");
    json_t *o = json_object();
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const json_t *v = jget(p, keys[i]);
        json_set(o, keys[i], v ? json_copy(v) : json_null());
    }
    return env_ok(o);
}

/* plugins_kanban_dashboard_plugin_api_resolve_project
 * @ plugins/kanban/dashboard/plugin_api.py:_resolve_project */
json_t *w27p4_resolve_project(const json_t *req) {
    const char *ref = jstr(req, "ref");
    char *stripped = py_strip_dup(ref ? ref : "");
    json_t *out = json_array();

    if (!ref || !stripped || stripped[0] == '\0') {
        free(stripped);
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, json_null());
        return env_ok(out);
    }
    if (json_get_bool(req, "connect_error", false)) {
        sb_t b;
        json_t *err;
        sb_init(&b);
        sb_puts(&b, "projects unavailable: ");
        sb_puts(&b, jstr(req, "connect_message") ? jstr(req, "connect_message") : "");
        err = env_http(500, b.p);
        sb_free(&b);
        free(stripped);
        return err;
    }
    {
        const json_t *projects = jget(req, "projects");
        const json_t *proj = jget(projects, stripped);
        if (!jtruthy(proj)) {
            sb_t b;
            json_t *err;
            sb_init(&b);
            sb_puts(&b, "project ");
            sb_repr(&b, ref);
            sb_puts(&b, " does not exist");
            err = env_http(400, b.p);
            sb_free(&b);
            free(stripped);
            return err;
        }
        {
            const json_t *id = jget(proj, "id");
            const json_t *name = jget(proj, "name");
            const json_t *path = jget(proj, "primary_path");
            json_append(out, id ? json_copy(id) : json_null());
            json_append(out, name ? json_copy(name) : json_null());
            json_append(out, jtruthy(path) ? json_copy(path) : json_null());
        }
    }
    free(stripped);
    return env_ok(out);
}

/* plugins_kanban_dashboard_plugin_api_default_workspace_kind
 * @ plugins/kanban/dashboard/plugin_api.py:_default_workspace_kind */
json_t *w27p4_default_workspace_kind(const json_t *req) {
    const json_t *board = jget(req, "board");
    const json_t *workdir = jget(board, "default_workdir");
    char buf[64];
    const char *w = NULL;
    char *stripped;

    if (!jis_none(workdir)) w = (workdir->type == JSON_STRING) ? workdir->str_val
                                                               : jpy_str(workdir, buf, sizeof(buf));
    stripped = py_strip_dup(w ? w : "");
    if (!stripped || stripped[0] == '\0') {
        free(stripped);
        return env_ok(json_string("scratch"));
    }
    free(stripped);
    /* "git": true -> git toplevel found; false -> None; null -> OSError/ValueError. */
    return env_ok(json_string(json_get_bool(req, "git", false) ? "worktree" : "dir"));
}

/* plugins_kanban_dashboard_plugin_api_annotate_board_meta
 * @ plugins/kanban/dashboard/plugin_api.py:_annotate_board_meta */
json_t *w27p4_annotate_board_meta(const json_t *req) {
    const json_t *meta = jget(req, "meta");
    const json_t *kind = jget(req, "workspace_kind");
    const json_t *project = jget(req, "project");
    json_t *o = (meta && meta->type == JSON_OBJECT) ? json_copy(meta) : json_object();

    json_set(o, "default_workspace_kind", kind ? json_copy(kind) : json_null());
    if (project && project->type == JSON_ARRAY && json_len(project) >= 2)
        json_set(o, "project_name", json_copy(json_get(project, 1)));
    else
        json_set(o, "project_name", json_null());
    return env_ok(o);
}

/* plugins_kanban_dashboard_plugin_api_validated_profile_name
 * @ plugins/kanban/dashboard/plugin_api.py:_validated_profile_name */
json_t *w27p4_validated_profile_name(const json_t *req) {
    const char *raw = jstr(req, "raw");
    char *name = py_strip_dup(raw ? raw : "");

    if (!name) return env_ok(json_string(""));
    if (name[0] != '\0' && json_get_bool(req, "mod", true) &&
        !json_get_bool(req, "exists_raises", false) &&
        !json_get_bool(req, "exists", true)) {
        sb_t b;
        json_t *out;
        sb_init(&b);
        sb_puts(&b, "profile ");
        sb_repr(&b, name);
        sb_puts(&b, " does not exist");
        out = env_http(400, b.p);
        sb_free(&b);
        free(name);
        return out;
    }
    {
        json_t *out = env_ok(json_string(name));
        free(name);
        return out;
    }
}

/* plugins_kanban_dashboard_plugin_api_orchestration_settings
 * @ plugins/kanban/dashboard/plugin_api.py:get_orchestration_settings */
json_t *w27p4_orchestration_settings(const json_t *req) {
    static const char *const PROFILE_SETTINGS[W27_P4_PROFILE_SETTING_COUNT] = {
        "orchestrator_profile", "default_assignee"};
    const json_t *cfg = jget(req, "config");
    const json_t *kanban = jget(cfg, "kanban");
    const json_t *exists = jget(req, "exists");
    const json_t *ad, *ap;
    char *explicit_[W27_P4_PROFILE_SETTING_COUNT];
    char *resolved[W27_P4_PROFILE_SETTING_COUNT];
    const char *active = "default";
    json_t *o;

    for (size_t i = 0; i < W27_P4_PROFILE_SETTING_COUNT; i++) {
        const json_t *v = jget(kanban, PROFILE_SETTINGS[i]);
        explicit_[i] = py_strip_dup((v && v->type == JSON_STRING) ? v->str_val : "");
    }
    if (json_get_bool(req, "active_raises", false)) {
        for (size_t i = 0; i < W27_P4_PROFILE_SETTING_COUNT; i++)
            resolved[i] = py_strip_dup(explicit_[i][0] ? explicit_[i] : "default");
    } else {
        const json_t *a = jget(req, "active");
        if (jtruthy(a) && a->type == JSON_STRING) active = a->str_val;
        for (size_t i = 0; i < W27_P4_PROFILE_SETTING_COUNT; i++) {
            bool known = false;
            if (explicit_[i][0] && exists && exists->type == JSON_ARRAY) {
                for (size_t j = 0; j < exists->c.count; j++) {
                    const json_t *e = exists->c.items[j];
                    if (e && e->type == JSON_STRING && !strcmp(e->str_val, explicit_[i])) {
                        known = true;
                        break;
                    }
                }
            }
            resolved[i] = py_strip_dup((explicit_[i][0] && known) ? explicit_[i] : active);
        }
    }
    o = json_object();
    json_set(o, "orchestrator_profile", json_string(explicit_[0]));
    json_set(o, "default_assignee", json_string(explicit_[1]));
    ad = jget(kanban, "auto_decompose");
    ap = jget(kanban, "auto_promote_children");
    json_set(o, "auto_decompose", json_bool(ad == NULL ? true : jtruthy(ad)));
    json_set(o, "auto_promote_children", json_bool(ap == NULL ? true : jtruthy(ap)));
    json_set(o, "resolved_orchestrator_profile", json_string(resolved[0]));
    json_set(o, "resolved_default_assignee", json_string(resolved[1]));
    json_set(o, "active_profile", json_string(active));
    for (size_t i = 0; i < W27_P4_PROFILE_SETTING_COUNT; i++) {
        free(explicit_[i]);
        free(resolved[i]);
    }
    return env_ok(o);
}

/* plugins_kanban_dashboard_plugin_api_since_param
 * @ plugins/kanban/dashboard/plugin_api.py:_since_param */
json_t *w27p4_since_param(const json_t *req) {
    const json_t *query = jget(req, "query");
    const json_t *raw = jget(query, "since");
    long long v;

    if (jis_none(raw)) return env_ok(json_null());
    if (raw->type == JSON_STRING) {
        const char *s = raw->str_val;
        char *stripped = py_strip_dup(s);
        bool empty = !stripped || stripped[0] == '\0';
        free(stripped);
        if (empty) return env_ok(json_null());
        while (*s && py_isspace((unsigned char)*s)) s++;
        if (*s == '+' || *s == '-') s++;
        if (*s < '0' || *s > '9') return env_ok(json_null()); /* int("x") -> ValueError */
        while (*s >= '0' && *s <= '9') s++;
        while (*s && py_isspace((unsigned char)*s)) s++;
        if (*s) return env_ok(json_null()); /* int("1.5") -> ValueError */
        v = jint_coerce(raw);
        return env_ok(json_number((double)(v < 0 ? 0 : v)));
    }
    if (raw->type == JSON_ARRAY || raw->type == JSON_OBJECT) return env_ok(json_null());
    v = jint_coerce(raw);
    return env_ok(json_number((double)(v < 0 ? 0 : v)));
}

/* plugins_kanban_dashboard_plugin_api_ws_board
 * @ plugins/kanban/dashboard/plugin_api.py:_ws_board */
json_t *w27p4_ws_board(const json_t *req) {
    const json_t *raw = jget(req, "raw");
    json_t *env;

    if (jis_none(raw) || (raw->type == JSON_STRING && raw->str_val[0] == '\0'))
        return env_ok(json_null());
    env = p1_normalize_node(raw);
    if (!json_get_bool(env, "ok", false)) { /* ValueError -> None */
        json_free(env);
        return env_ok(json_null());
    }
    return env;
}

/* plugins_kanban_dashboard_plugin_api_ws_board_live
 * @ plugins/kanban/dashboard/plugin_api.py:_ws_board_live */
json_t *w27p4_ws_board_live(const json_t *req) {
    const char *slug = jstr(req, "slug");
    const json_t *boards = jget(req, "boards");

    if (!slug || slug[0] == '\0') return env_ok(json_null());
    if (!strcmp(slug, "default")) return env_ok(json_string(slug));
    if (boards && boards->type == JSON_ARRAY) {
        for (size_t i = 0; i < boards->c.count; i++) {
            const json_t *b = boards->c.items[i];
            if (b && b->type == JSON_STRING && !strcmp(b->str_val, slug))
                return env_ok(json_string(slug));
        }
    }
    return env_ok(json_null());
}

/* plugins_kanban_dashboard_plugin_api_list_boards_annotate
 * @ plugins/kanban/dashboard/plugin_api.py:list_boards */
json_t *w27p4_list_boards_annotate(const json_t *req) {
    const json_t *boards = jget(req, "boards");
    const json_t *projects = jget(req, "projects");
    const json_t *counts_map = jget(req, "counts");
    const json_t *kinds = jget(req, "kinds");
    const char *current = jstr(req, "current");
    json_t *out = json_object();
    json_t *list = json_array();

    for (size_t i = 0; boards && boards->type == JSON_ARRAY && i < boards->c.count; i++) {
        const json_t *b = boards->c.items[i];
        const char *slug = jstr(b, "slug");
        const json_t *counts = jget(counts_map, slug ? slug : "");
        const json_t *pid_v;
        const json_t *proj;
        const char *pid;
        json_t *row = json_copy(b);
        double total = 0;

        json_set(row, "is_current", json_bool(slug && current && !strcmp(slug, current)));
        if (counts && counts->type == JSON_OBJECT) {
            json_set(row, "counts", json_copy(counts));
            for (size_t j = 0; j < json_object_size(counts); j++) {
                const char *st = json_object_get_key_at(counts, j);
                const json_t *nv = json_object_get_at(counts, j);
                if (st && !strcmp(st, "archived")) continue;
                if (nv && nv->type == JSON_NUMBER) total += nv->num_val;
            }
        } else {
            json_set(row, "counts", json_object());
        }
        json_set(row, "total", json_number(total));
        {
            const json_t *k = jget(kinds, slug ? slug : "");
            json_set(row, "default_workspace_kind", k ? json_copy(k) : json_string("scratch"));
        }
        pid_v = jget(b, "project_id");
        pid = (pid_v && pid_v->type == JSON_STRING && pid_v->str_val[0]) ? pid_v->str_val : NULL;
        json_set(row, "project_id", pid ? json_string(pid) : json_null());
        proj = pid ? jget(projects, pid) : NULL;
        {
            const json_t *name = jget(proj, "name");
            json_set(row, "project_name", name ? json_copy(name) : json_null());
        }
        json_append(list, row);
    }
    json_set(out, "boards", list);
    json_set(out, "current", current ? json_string(current) : json_null());
    return env_ok(out);
}

/* plugins_kanban_dashboard_plugin_api_profile_roster
 * @ plugins/kanban/dashboard/plugin_api.py:list_profile_roster */
json_t *w27p4_profile_roster(const json_t *req) {
    const json_t *profiles = jget(req, "profiles");
    json_t *out = json_object();
    json_t *list = json_array();

    for (size_t i = 0; profiles && profiles->type == JSON_ARRAY && i < profiles->c.count; i++) {
        const json_t *p = profiles->c.items[i];
        const json_t *model = jget(p, "model");
        const json_t *provider = jget(p, "provider");
        const json_t *desc = jget(p, "description");
        const json_t *sc = jget(p, "skill_count");
        json_t *row = json_object();
        json_set(row, "name", jstr(p, "name") ? json_string(jstr(p, "name")) : json_null());
        json_set(row, "is_default", json_bool(json_get_bool(p, "is_default", false)));
        json_set(row, "model", jtruthy(model) ? json_copy(model) : json_string(""));
        json_set(row, "provider", jtruthy(provider) ? json_copy(provider) : json_string(""));
        json_set(row, "description", jtruthy(desc) ? json_copy(desc) : json_string(""));
        json_set(row, "description_auto", json_bool(json_get_bool(p, "description_auto", false)));
        json_set(row, "skill_count", json_number((double)(jtruthy(sc) ? jint_coerce(sc) : 0)));
        json_append(list, row);
    }
    json_set(out, "profiles", list);
    return env_ok(out);
}
