/*
 * gateway_session_store.c - C11 port of upstream hermes_cli/gateway_session_store.py
 * (gates w31_gatewayses_1bd734_p1 | w31_gatewayses_1bd734_p2 | w31_gatewayses_1bd734_p3).
 *
 * The upstream class is the classic CLI's session store: every write is a gateway
 * verb and every read goes through a read-only state.db reader.  Both are seams
 * (a GatewayConnection RPC, an ExitStack of SQLite handles) and stay in the
 * caller.  What is ported here is the part that is a pure function of values:
 * how a durable id labels itself, how a listing window and a transcript view are
 * normalised, which family an attribute falls into, how a source filter expands
 * into live/created labels, the banner inventory a snapshot yields, and how
 * launch flags become session.create policy (and a resume refusal).
 *
 * PoP: w31_gatewayses_1bd734_p1_session_label @ hermes_cli/gateway_session_store.py:session_label
 * PoP: w31_gatewayses_1bd734_p1_window @ hermes_cli/gateway_session_store.py:_window
 * PoP: w31_gatewayses_1bd734_p1_transcript_view @ hermes_cli/gateway_session_store.py:_transcript_view
 * PoP: w31_gatewayses_1bd734_p1_store_truthy @ hermes_cli/gateway_session_store.py:GatewaySessionStore.__bool__
 * PoP: w31_gatewayses_1bd734_p1_attr_dispatch @ hermes_cli/gateway_session_store.py:GatewaySessionStore.__getattr__
 * PoP: w31_gatewayses_1bd734_p1_retry_state @ hermes_cli/gateway_session_store.py:GatewaySessionStore._retry_state
 * PoP: w31_gatewayses_1bd734_p2_expand @ hermes_cli/gateway_session_store.py:SurfaceLabelReader._expand
 * PoP: w31_gatewayses_1bd734_p2_labelled @ hermes_cli/gateway_session_store.py:SurfaceLabelReader._labelled
 * PoP: w31_gatewayses_1bd734_p2_banner_inventory @ hermes_cli/gateway_session_store.py:GatewaySessionStore.banner_inventory
 * PoP: w31_gatewayses_1bd734_p3_classic_only_policy @ hermes_cli/gateway_session_store.py:_classic_only_policy
 * PoP: w31_gatewayses_1bd734_p3_creation_policy @ hermes_cli/gateway_session_store.py:_creation_policy
 * PoP: w31_gatewayses_1bd734_p3_resume_classic_differs @ hermes_cli/gateway_session_store.py:_check_resume_launch:686-689
 *                                                       + hermes_cli/gateway_chat.py:resume_policy_error
 *
 * Seams (not ported, left to the caller): GatewayConnection RPC (call/resume/
 * detach/start/reattach), the read-only SQLite reader (_db/_row_revision/
 * _title_holder/_has_created_source_column's SQL probe), uuid4 request ids,
 * os.getcwd/realpath in _launch_cwd, and gateway_chat.check_resume_policy
 * (the non-classic half of _check_resume_launch).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hermes_json.h"
#include "noc_w31_gatewayses_1bd734_p1.h"
#include "noc_w31_gatewayses_1bd734_p2.h"
#include "noc_w31_gatewayses_1bd734_p3.h"

/* ------------------------------------------------------------------ helpers */

/* Python truthiness of a JSON value. */
static int gs_truthy(const json_t *v) {
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

/* Python's numeric view: int, float or bool (None is none of those). */
static int gs_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

static const char *gs_str(const json_t *v) {
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val ? v->str_val : "";
}

static int gs_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }

static void gs_set_owned(json_t *o, const char *k, json_t *v) { json_set(o, k, v ? v : json_null()); }

/* obj[key] copied, an absent key and a null both as JSON null. */
static void gs_set_copy(json_t *o, const char *k, const json_t *v) {
    gs_set_owned(o, k, v ? json_copy(v) : json_null());
}

/* Python str() of a number: shortest round-tripping form, ints without ".0". */
static void gs_num_str(double v, char *buf, size_t cap) {
    if (!(v == v)) { snprintf(buf, cap, "nan"); return; }
    if (v == (double)(long long)v && fabs(v) < 1e18) {
        snprintf(buf, cap, "%lld", (long long)v);
        return;
    }
    snprintf(buf, cap, "%.15g", v);
    if (strtod(buf, NULL) != v) snprintf(buf, cap, "%.17g", v);
}

/* Python str() of a scalar (what the launch-flag join/split does to an item). */
static char *gs_pystr(const json_t *v) {
    char buf[64], *s;
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) {
        s = (char *)malloc(strlen(v->str_val ? v->str_val : "") + 1);
        if (s) strcpy(s, v->str_val ? v->str_val : "");
        return s;
    }
    if (v->type == JSON_BOOL) {
        s = (char *)malloc(v->bool_val ? 5 : 6);
        if (s) strcpy(s, v->bool_val ? "True" : "False");
        return s;
    }
    if (v->type == JSON_NUMBER) {
        gs_num_str(v->num_val, buf, sizeof buf);
        s = (char *)malloc(strlen(buf) + 1);
        if (s) strcpy(s, buf);
        return s;
    }
    return json_dumps(v, 0); /* libjson text, close enough for structured items */
}

/* A bounded string set with insertion order (Python set/dict.fromkeys). */
#define GS_SET_CAP 64
typedef struct { const char *items[GS_SET_CAP]; size_t n; } gs_set;

static int gs_set_has(const gs_set *s, const char *v) {
    size_t i;
    for (i = 0; i < s->n; i++)
        if (s->items[i] == v || (s->items[i] && v && strcmp(s->items[i], v) == 0)) return 1;
    return 0;
}

static void gs_set_add(gs_set *s, const char *v) {
    if (!v || s->n >= GS_SET_CAP || gs_set_has(s, v)) return;
    s->items[s->n++] = v;
}

static int gs_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* sorted(set) as a JSON array of the strings held. */
static json_t *gs_set_array(gs_set *s, int sorted) {
    json_t *arr = json_array();
    size_t i;
    if (sorted) qsort(s->items, s->n, sizeof s->items[0], gs_cmp_str);
    for (i = 0; i < s->n; i++) json_append(arr, json_string(s->items[i]));
    return arr;
}

/* ------------------------------------------------------- p1: identity/window */

/* session_label: the 8-char label of a canonical ``local-`` + 64 hex id, else
 * the id as given (None stays None).  ``$`` also matches before one final \n. */
json_t *w31_gatewayses_1bd734_p1_session_label(const json_t *session_id) {
    const char *s;
    size_t n, len;
    int ok = 1;
    char label[9];

    if (!session_id) return json_null();
    if (session_id->type != JSON_STRING) return json_copy((json_t *)session_id);
    s = session_id->str_val ? session_id->str_val : "";
    n = strlen(s);
    len = (n == 71 && s[70] == '\n') ? 70 : n;
    if (len != 70 || memcmp(s, "local-", 6) != 0) return json_copy((json_t *)session_id);
    for (size_t i = 6; i < 70; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { ok = 0; break; }
    }
    if (!ok) return json_copy((json_t *)session_id);
    memcpy(label, s + 6, 8);
    label[8] = '\0';
    return json_string(label);
}

/* _window + SessionWindow.__post_init__: a negative limit is no limit (SQL
 * LIMIT -1); a negative offset is the refusal the window type raises. */
json_t *w31_gatewayses_1bd734_p1_window(const json_t *limit, const json_t *offset) {
    json_t *o = json_object();
    int has_limit = limit && limit->type != JSON_NULL;
    double lv = 0.0, ov = 0.0;
    char lim_s[64], off_s[64];

    if (has_limit && gs_number(limit, &lv) && lv < 0) has_limit = 0;
    if (!has_limit) {
        snprintf(lim_s, sizeof lim_s, "None");
    } else if (limit->type == JSON_BOOL) {
        snprintf(lim_s, sizeof lim_s, "%s", limit->bool_val ? "True" : "False");
    } else {
        gs_num_str(limit->num_val, lim_s, sizeof lim_s);
    }
    if (offset && gs_number(offset, &ov) && ov < 0) {
        if (offset->type == JSON_BOOL)
            snprintf(off_s, sizeof off_s, "%s", offset->bool_val ? "True" : "False");
        else
            gs_num_str(offset->num_val, off_s, sizeof off_s);
        {
            size_t need = strlen("SessionWindow needs limit >= 0 and offset >= 0, got , ") + strlen(lim_s) + strlen(off_s) + 1;
            char *msg = (char *)malloc(need);
            if (!msg) return o;
            snprintf(msg, need, "SessionWindow needs limit >= 0 and offset >= 0, got %s, %s", lim_s, off_s);
            json_set(o, "error", json_string(msg));
            free(msg);
        }
        return o;
    }
    json_set(o, "limit", has_limit ? json_copy((json_t *)limit) : json_null());
    gs_set_copy(o, "offset", offset);
    return o;
}

/* _transcript_view: inactive wins, then compacted. */
json_t *w31_gatewayses_1bd734_p1_transcript_view(const json_t *include_inactive,
                                                 const json_t *include_compacted) {
    if (gs_truthy(include_inactive)) return json_string("ALL");
    if (gs_truthy(include_compacted)) return json_string("DISPLAY");
    return json_string("ACTIVE");
}

/* __bool__: truthy until the connection is final. */
json_t *w31_gatewayses_1bd734_p1_store_truthy(const json_t *final) {
    return json_bool(gs_truthy(final) ? 0 : 1);
}

/* The read-only SessionDB methods main's CLI calls. */
static const char *const GS_READS[] = {
    "get_session", "get_session_title", "get_session_by_title", "resolve_resume_session_id",
    "get_messages_as_conversation", "get_resume_conversations", "assert_resume_safe",
    "is_kanban_owned_session", "list_sessions_rich", "search_sessions", "get_next_title_in_lineage",
    "get_handoff_state", "get_meta", "get_compression_tip", "resolve_session_by_title", "get_messages",
    "session_count", "sanitize_title", "session_yolo_enabled", "get_session_model_config_value",
};
/* Writes the gateway already performs itself. */
static const char *const GS_GATEWAY_OWNED[] = {
    "create_session", "reopen_session", "append_messages_batch", "flush_token_counts",
    "update_session_model", "patch_session_model_config", "maybe_auto_archive",
    "maybe_auto_prune_and_vacuum", "set_meta", "update_token_counts", "close",
};
/* Mutations the owner counts rather than performs here. */
static const char *const GS_COUNTED_REPAIRS[] = {
    "prune_empty_ghost_sessions", "finalize_orphaned_compression_sessions",
};

static int gs_in(const char *name, const char *const *tab, size_t n) {
    size_t i;
    if (!name) return 0;
    for (i = 0; i < n; i++)
        if (strcmp(name, tab[i]) == 0) return 1;
    return 0;
}

/* __getattr__: which family a missing attribute falls into. */
json_t *w31_gatewayses_1bd734_p1_attr_dispatch(const json_t *name) {
    const char *n = gs_str(name);
    if (!n) return json_string("refused");
    if (n[0] == '_' && n[1] == '_') return json_string("attribute_error");
    if (gs_in(n, GS_READS, sizeof GS_READS / sizeof GS_READS[0])) {
        if (strcmp(n, "sanitize_title") == 0) return json_string("sanitize_title");
        if (strcmp(n, "session_yolo_enabled") == 0) return json_string("session_yolo_enabled");
        return json_string("read");
    }
    if (gs_in(n, GS_GATEWAY_OWNED, sizeof GS_GATEWAY_OWNED / sizeof GS_GATEWAY_OWNED[0]))
        return json_string("noop");
    if (gs_in(n, GS_COUNTED_REPAIRS, sizeof GS_COUNTED_REPAIRS / sizeof GS_COUNTED_REPAIRS[0]))
        return json_string("zero");
    return json_string("refused");
}

/* _retry_state: (prepared, abandon) of the owner-restart wait, or (None, None).
 * The abandon poll is a callable in the truth, so here it is its presence. */
json_t *w31_gatewayses_1bd734_p1_retry_state(const json_t *retry) {
    json_t *o = json_object();
    if (!retry || retry->type == JSON_NULL) {
        json_set(o, "prepared", json_null());
        json_set(o, "abandon", json_bool(0));
        return o;
    }
    gs_set_copy(o, "prepared", json_get(retry, 0));
    json_set(o, "abandon", json_bool(gs_truthy(json_get(retry, 1)) ? 1 : 0));
    return o;
}

/* --------------------------------------------- p2: surface labels and banner */

/* SURFACE_GROUPS: the other name of a gui/desktop pair, else the label itself.
 * ``tool`` and ``oneshot`` are not cli aliases (they keep their stored label). */
static size_t gs_group(const char *name, const char **out) {
    if (!name) return 0;
    if (strcmp(name, "cli") == 0) { out[0] = "cli"; return 1; }
    if (strcmp(name, "tui") == 0) { out[0] = "tui"; return 1; }
    if (strcmp(name, "acp") == 0) { out[0] = "acp"; return 1; }
    if (strcmp(name, "gui") == 0 || strcmp(name, "desktop") == 0) {
        out[0] = "gui";
        out[1] = "desktop";
        return 2;
    }
    return 0;
}

/* _expand: (live labels, created_source labels), or None for a raw filter. */
json_t *w31_gatewayses_1bd734_p2_expand(const json_t *requested, const json_t *has_column) {
    gs_set wanted = {{0}, 0}, live = {{0}, 0}, created = {{0}, 0}, out_live = {{0}, 0};
    const char *group[2];
    size_t i, g;
    json_t *o;

    if (requested && requested->type == JSON_ARRAY) {
        for (i = 0; i < json_len(requested); i++) gs_set_add(&wanted, gs_str(json_get(requested, i)));
    } else if (gs_str(requested)) {
        gs_set_add(&wanted, gs_str(requested));
    }
    /* live starts as every requested label, then grows by each group (so a
     * bare ``tool``/``unknown`` next to an expandable name stays a live label). */
    for (i = 0; i < wanted.n; i++) gs_set_add(&live, wanted.items[i]);
    for (i = 0; i < wanted.n; i++) {
        g = gs_group(wanted.items[i], group);
        for (size_t k = 0; k < g; k++) {
            gs_set_add(&live, group[k]);
            gs_set_add(&created, group[k]);
        }
    }
    if (created.n == 0 || !gs_truthy(has_column)) return json_null();

    for (i = 0; i < live.n; i++) gs_set_add(&out_live, live.items[i]);
    gs_set_add(&out_live, "local");
    o = json_object();
    json_set(o, "live", gs_set_array(&out_live, 1));
    json_set(o, "created", gs_set_array(&created, 1));
    return o;
}

/* _labelled: the expanded call, or the raw filter when nothing expands. */
json_t *w31_gatewayses_1bd734_p2_labelled(const json_t *requested, const json_t *has_column,
                                          const json_t *limit, const json_t *offset) {
    json_t *expanded = w31_gatewayses_1bd734_p2_expand(requested, has_column);
    json_t *o = json_object();
    gs_set_copy(o, "limit", limit);
    gs_set_copy(o, "offset", offset);
    if (!expanded || expanded->type != JSON_OBJECT) {
        gs_set_copy(o, "sources", requested);
        json_set(o, "surfaces", json_null());
        if (expanded) json_free(expanded);
        return o;
    }
    json_set(o, "sources", json_copy(json_obj_get(expanded, "live")));
    json_set(o, "surfaces", json_copy(json_obj_get(expanded, "created")));
    json_free(expanded);
    return o;
}

/* The default tool availability the banner shows when the snapshot has none. */
static json_t *gs_default_availability(void) {
    json_t *a = json_object();
    json_set(a, "available_toolsets", json_array());
    json_set(a, "unavailable_toolsets", json_array());
    json_set(a, "lazy_tools", json_array());
    json_set(a, "disabled_tools", json_array());
    return a;
}

/* banner_inventory: the snapshot's banner fields as build_welcome_banner
 * kwargs, None until a session with an ``info.tools`` key is open.  The
 * truth's ``get_toolset_for_tool`` is the bound ``toolset_of.get``; here it is
 * the mapping itself ("toolset_of"), which is what that callable answers. */
json_t *w31_gatewayses_1bd734_p2_banner_inventory(const json_t *current) {
    const json_t *info, *grouped, *names, *item, *tools;
    const char *name_str;
    json_t *o, *tools_out, *toolset_of, *mcp, *availability;
    size_t i, j;

    if (current && current->type != JSON_OBJECT) return json_null();
    info = current ? json_obj_get(current, "info") : NULL;
    if (!gs_truthy(info)) info = NULL;
    if (!info) return json_null();
    if (json_obj_get(info, "tools") == NULL) return json_null();
    grouped = json_obj_get(info, "tools");
    if (!gs_truthy(grouped)) grouped = NULL;

    o = json_object();
    tools_out = json_array();
    toolset_of = json_object();
    if (grouped && grouped->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(grouped); i++) {
            const char *toolset = json_object_get_key_at(grouped, i);
            names = json_object_get_at(grouped, i);
            if (!names || names->type != JSON_ARRAY) continue;
            for (j = 0; j < json_len(names); j++) {
                const char *n = gs_str(json_get(names, j));
                json_t *fn, *entry;
                if (!n) continue;
                entry = json_object();
                json_set(entry, "type", json_string("function"));
                fn = json_object();
                json_set(fn, "name", json_string(n));
                json_set(entry, "function", fn);
                json_append(tools_out, entry);
                json_set(toolset_of, n, json_string(toolset ? toolset : ""));
            }
        }
    }
    json_set(o, "tools", tools_out);

    availability = json_obj_get(info, "tool_availability");
    if (!gs_truthy(availability)) availability = gs_default_availability();
    else availability = json_copy(availability);
    json_set(o, "availability", availability);

    if (availability->type == JSON_OBJECT) {
        const json_t *unavail = json_obj_get(availability, "unavailable_toolsets");
        if (unavail && unavail->type == JSON_ARRAY) {
            for (i = 0; i < json_len(unavail); i++) {
                const char *fallback, *value;
                item = json_get(unavail, i);
                if (!item || item->type != JSON_OBJECT) continue;
                tools = json_obj_get(item, "tools");
                if (!tools || tools->type != JSON_ARRAY) continue;
                fallback = gs_str(json_obj_get(item, "name"));
                value = gs_str(json_obj_get(item, "id"));
                if (!value) value = fallback ? fallback : "";
                for (j = 0; j < json_len(tools); j++) {
                    name_str = gs_str(json_get(tools, j));
                    if (!name_str || json_obj_get(toolset_of, name_str) != NULL) continue;
                    json_set(toolset_of, name_str, json_string(value));
                }
            }
        }
    }
    json_set(o, "toolset_of", toolset_of);

    {
        const json_t *skills = json_obj_get(info, "skills");
        json_set(o, "skills_by_category", gs_truthy(skills) ? json_copy(skills) : json_object());
    }
    mcp = json_obj_get(info, "mcp_servers");
    if (mcp && mcp->type == JSON_ARRAY) {
        json_set(o, "mcp_status", json_copy(mcp));
    } else if (mcp && mcp->type == JSON_OBJECT) {
        json_t *keys = json_array();
        for (i = 0; i < json_object_size(mcp); i++) {
            const char *k = json_object_get_key_at(mcp, i);
            if (k) json_append(keys, json_string(k));
        }
        json_set(o, "mcp_status", keys);
    } else {
        json_set(o, "mcp_status", json_array());
    }
    gs_set_copy(o, "enabled_toolsets", json_obj_get(info, "enabled_toolsets"));
    gs_set_copy(o, "context_length", json_obj_get(info, "context_length"));
    gs_set_copy(o, "cwd", json_obj_get(info, "cwd"));
    gs_set_copy(o, "tool_availability_warnings", json_obj_get(info, "tool_availability_warnings"));
    return o;
}

/* ------------------------------------------- p3: launch policy / resume diff */

/* gateway_chat._POLICY: _LAUNCH_POLICY (the create-policy subset) plus the
 * bypass flags, which are never in the Namespace _creation_policy builds and so
 * are always absent there. */
static const char *const GS_POLICY_FIELDS[] = {
    "model", "provider", "reasoning", "toolsets", "max_turns", "base_url", "ignore_rules",
    "api_key", "yolo", "safe_mode", "ignore_user_config", "skills", "checkpoints",
    "accept_hooks", "pass_session_id",
};
/* gateway_chat._ENV_LAUNCH_FLAGS */
static const char *const GS_ENV_FLAGS[][2] = {
    {"yolo", "HERMES_YOLO_MODE"},
    {"ignore_rules", "HERMES_IGNORE_RULES"},
    {"safe_mode", "HERMES_SAFE_MODE"},
    {"ignore_user_config", "HERMES_IGNORE_USER_CONFIG"},
    {"accept_hooks", "HERMES_ACCEPT_HOOKS"},
};
/* gateway_chat._RESUME_FLAG_HINTS: as typed, and the slash command that changes it. */
static const char *const GS_RESUME_HINTS[][3] = {
    {"model", "-m/--model", "/model"},
    {"provider", "--provider", "/model"},
    {"base_url", "--base-url", "/model"},
    {"reasoning", "--reasoning", "/reasoning"},
    {"yolo", "--yolo", "/yolo"},
    {"toolsets", "-t/--toolsets", NULL},
    {"skills", "-s/--skills", NULL},
    {"verbose", "-v/--verbose", NULL},
    {"cwd", "--in", NULL},
};
#define GS_RESUME_POLICY_MISMATCH \
    "Resume retains gateway session policy; creation overrides are unsupported on resume."

/* utils.is_truthy_value over an environment string. */
static int gs_is_truthy_value(const json_t *v) {
    char *buf;
    size_t n, i, a, b;
    int hit;
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type != JSON_STRING) return gs_truthy(v);
    buf = (char *)malloc(strlen(v->str_val ? v->str_val : "") + 1);
    if (!buf) return 0;
    strcpy(buf, v->str_val ? v->str_val : "");
    n = strlen(buf);
    a = 0;
    b = n;
    while (a < b && (buf[a] == ' ' || buf[a] == '\t' || buf[a] == '\n' || buf[a] == '\r' ||
                     buf[a] == '\v' || buf[a] == '\f')) a++;
    while (b > a && (buf[b - 1] == ' ' || buf[b - 1] == '\t' || buf[b - 1] == '\n' ||
                     buf[b - 1] == '\r' || buf[b - 1] == '\v' || buf[b - 1] == '\f')) b--;
    for (i = a; i < b; i++) if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] = (char)(buf[i] - 'A' + 'a');
    buf[b] = '\0';
    hit = !strcmp(buf + a, "1") || !strcmp(buf + a, "true") || !strcmp(buf + a, "yes") ||
          !strcmp(buf + a, "on");
    free(buf);
    return hit;
}

/* _classic_only_policy: main's --run-budget and -v, which the thin client refuses. */
json_t *w31_gatewayses_1bd734_p3_classic_only_policy(const json_t *launch) {
    json_t *o = json_object();
    const json_t *rb, *verbose;
    double v = 0.0;

    rb = gs_is_obj(launch) ? json_obj_get(launch, "run_budget") : NULL;
    if (rb && rb->type != JSON_NULL) {
        if (rb->type == JSON_BOOL) v = rb->bool_val ? 1.0 : 0.0;
        else if (rb->type == JSON_NUMBER) v = rb->num_val;
        else if (rb->type == JSON_STRING) v = strtod(rb->str_val ? rb->str_val : "", NULL);
        json_set(o, "run_budget", json_number(v));
    }
    verbose = gs_is_obj(launch) ? json_obj_get(launch, "verbose") : NULL;
    if (gs_truthy(verbose)) json_set(o, "verbose", json_bool(1));
    return o;
}

/* "a,b" -> ["a","b"] (stripped, empties dropped), the shape _requested_policy builds. */
static json_t *gs_split_toolsets(const json_t *v) {
    json_t *arr = json_array();
    const char *s = gs_str(v);
    size_t len, i, start;
    if (!s) return arr;
    len = strlen(s);
    i = 0;
    while (i <= len) {
        size_t end;
        start = i;
        while (i < len && s[i] != ',') i++;
        end = i;
        while (start < end && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' ||
                               s[start] == '\r' || s[start] == '\v' || s[start] == '\f')) start++;
        while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' ||
                               s[end - 1] == '\r' || s[end - 1] == '\v' || s[end - 1] == '\f')) end--;
        if (end > start) {
            size_t n = end - start;
            char *item = (char *)malloc(n + 1);
            if (item) {
                memcpy(item, s + start, n);
                item[n] = '\0';
                json_append(arr, json_string(item));
                free(item);
            }
        }
        i++;
    }
    return arr;
}

/* Does the built array already hold this name? */
static int gs_arr_has(const json_t *arr, const char *name) {
    size_t i;
    for (i = 0; i < json_len(arr); i++) {
        const char *have = gs_str(json_get(arr, i));
        if (have && strcmp(have, name) == 0) return 1;
    }
    return 0;
}

/* Append every comma-separated, stripped, non-empty name not already there. */
static void gs_push_names(json_t *arr, const char *s) {
    size_t len, k = 0;
    if (!s) return;
    len = strlen(s);
    while (k <= len) {
        size_t start = k, end;
        while (k < len && s[k] != ',') k++;
        end = k;
        while (start < end && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' ||
                               s[start] == '\r' || s[start] == '\v' || s[start] == '\f')) start++;
        while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' ||
                               s[end - 1] == '\r' || s[end - 1] == '\v' || s[end - 1] == '\f')) end--;
        if (end > start) {
            char *name = (char *)malloc(end - start + 1);
            if (name) {
                memcpy(name, s + start, end - start);
                name[end - start] = '\0';
                if (!gs_arr_has(arr, name)) json_append(arr, json_string(name));
                free(name);
            }
        }
        k++;
    }
}

/* _requested_policy's skills fold: one deduplicated list from -s a,b -s c. */
static json_t *gs_fold_skills(const json_t *v) {
    json_t *arr = json_array();
    size_t i;

    if (v && v->type == JSON_ARRAY) {
        for (i = 0; i < json_len(v); i++) {
            char *s = gs_pystr(json_get(v, i));
            gs_push_names(arr, s);
            free(s);
        }
    } else if (v && v->type != JSON_NULL) {
        char *s = gs_pystr(v);
        gs_push_names(arr, s);
        free(s);
    }
    return arr;
}

/* _creation_policy: the explicit launch flags as session.create policy.
 * ``env`` is this shell's environment for the five HERMES_* launch opt-ins
 * (gateway_chat.env_launch_flags is the seam that reads it). */
json_t *w31_gatewayses_1bd734_p3_creation_policy(const json_t *launch, const json_t *env) {
    json_t *policy = json_object();
    const json_t *fields[sizeof GS_POLICY_FIELDS / sizeof GS_POLICY_FIELDS[0]];
    size_t i, n = sizeof GS_POLICY_FIELDS / sizeof GS_POLICY_FIELDS[0];

    for (i = 0; i < n; i++) {
        const json_t *v = gs_is_obj(launch) ? json_obj_get(launch, GS_POLICY_FIELDS[i]) : NULL;
        fields[i] = v;
    }
    for (i = 0; i < n; i++) {
        const char *key = GS_POLICY_FIELDS[i];
        const json_t *v = fields[i];
        if (!v || v->type == JSON_NULL) continue;
        if (v->type == JSON_BOOL && !v->bool_val) continue; /* value is False */
        if (strcmp(key, "toolsets") == 0) {
            if (v->type == JSON_ARRAY) {
                size_t j, total = 0;
                char *joined;
                for (j = 0; j < json_len(v); j++) {
                    char *s = gs_pystr(json_get(v, j));
                    total += (s ? strlen(s) : 0) + 1;
                    free(s);
                }
                joined = (char *)malloc(total + 1);
                if (!joined) continue;
                joined[0] = '\0';
                for (j = 0; j < json_len(v); j++) {
                    char *s = gs_pystr(json_get(v, j));
                    if (j) strcat(joined, ",");
                    if (s) strcat(joined, s);
                    free(s);
                }
                {
                    json_t *tmp = json_string(joined);
                    json_set(policy, key, gs_split_toolsets(tmp));
                    json_free(tmp);
                }
                free(joined);
            } else if (v->type == JSON_STRING) {
                json_set(policy, key, gs_split_toolsets(v));
            } else {
                gs_set_copy(policy, key, v);
            }
        } else if (strcmp(key, "skills") == 0) {
            json_t *folded = gs_fold_skills(v);
            if (json_len(folded) > 0) json_set(policy, key, folded);
            else json_free(folded); /* empty list is popped */
        } else {
            gs_set_copy(policy, key, v);
        }
    }

    {
        json_t *classic = w31_gatewayses_1bd734_p3_classic_only_policy(launch);
        for (i = 0; i < json_object_size(classic); i++) {
            const char *k = json_object_get_key_at(classic, i);
            json_set(policy, k, json_copy(json_object_get_at(classic, i)));
        }
        json_free(classic);
    }
    for (i = 0; i < sizeof GS_ENV_FLAGS / sizeof GS_ENV_FLAGS[0]; i++) {
        const json_t *v = gs_is_obj(env) ? json_obj_get(env, GS_ENV_FLAGS[i][1]) : NULL;
        if (gs_is_truthy_value(v)) json_set(policy, GS_ENV_FLAGS[i][0], json_bool(1));
    }
    return policy;
}

/* resume_policy_error: the refusal naming each flag that differs. */
static char *gs_resume_policy_error(json_t *keys) {
    const char *flags[8], *switch_phrases[8];
    char *owned[8];
    size_t i, nf = 0, ns = 0, nowned = 0, need;
    char *msg;

    for (i = 0; i < json_len(keys) && nf < 8; i++) {
        const char *key = gs_str(json_get(keys, i));
        const char *flag = NULL, *cmd = NULL;
        char *fallback = NULL;
        size_t j;
        if (!key) continue;
        for (j = 0; j < sizeof GS_RESUME_HINTS / sizeof GS_RESUME_HINTS[0]; j++) {
            if (strcmp(GS_RESUME_HINTS[j][0], key) == 0) {
                flag = GS_RESUME_HINTS[j][1];
                cmd = GS_RESUME_HINTS[j][2];
                break;
            }
        }
        if (!flag) {
            size_t k, len = strlen(key);
            fallback = (char *)malloc(len + 3);
            if (!fallback) continue;
            strcpy(fallback, "--");
            for (k = 0; k < len; k++) fallback[2 + k] = key[k] == '_' ? '-' : key[k];
            fallback[2 + len] = '\0';
            flag = fallback;
            owned[nowned++] = fallback; /* freed once the message is built */
        }
        {
            int seen = 0;
            size_t k;
            for (k = 0; k < nf; k++) if (strcmp(flags[k], flag) == 0) seen = 1;
            if (!seen) flags[nf++] = flag;
        }
        if (cmd && ns < 8) {
            int seen = 0;
            size_t k;
            for (k = 0; k < ns; k++) if (strcmp(switch_phrases[k], cmd) == 0) seen = 1;
            if (!seen) switch_phrases[ns++] = cmd;
        }
    }

    need = strlen(GS_RESUME_POLICY_MISMATCH) + 256;
    for (i = 0; i < nf; i++) need += strlen(flags[i]) + 2;
    for (i = 0; i < ns; i++) need += strlen(switch_phrases[i]) + 4;
    msg = (char *)malloc(need);
    if (!msg) return NULL;
    strcpy(msg, GS_RESUME_POLICY_MISMATCH);
    strcat(msg, " ");
    for (i = 0; i < nf; i++) {
        if (i) strcat(msg, ", ");
        strcat(msg, flags[i]);
    }
    strcat(msg, json_len(keys) == 1 ? " differs from this session's launch: resume without it"
                                    : " differ from this session's launch: resume without them");
    if (ns > 0) {
        strcat(msg, " and use ");
        for (i = 0; i < ns; i++) {
            if (i) strcat(msg, " or ");
            strcat(msg, switch_phrases[i]);
        }
        strcat(msg, " after resuming to switch.");
    } else {
        strcat(msg, ", or start a new session to use ");
        strcat(msg, json_len(keys) == 1 ? "it." : "them.");
    }
    for (i = 0; i < nowned; i++) free(owned[i]);
    return msg;
}

/* _check_resume_launch's classic-only tail: main's --run-budget / -v against the
 * session's frozen launch_request.  None when they agree, else the exact
 * GatewayClientError text.  (gateway_chat.check_resume_policy, which runs first
 * and owns the _LAUNCH_POLICY flags, is a seam: fixtures here carry only the
 * classic-only flags, for which that check passes.) */
json_t *w31_gatewayses_1bd734_p3_resume_classic_differs(const json_t *launch,
                                                        const json_t *snapshot) {
    json_t *classic, *differs, *out;
    const json_t *info, *frozen;
    size_t i;

    if (!gs_is_obj(snapshot)) return json_null();
    info = json_obj_get(snapshot, "info");
    if (!gs_truthy(info)) info = NULL;
    frozen = info ? json_obj_get(info, "launch_request") : NULL;
    if (!gs_truthy(frozen)) frozen = NULL;

    classic = w31_gatewayses_1bd734_p3_classic_only_policy(launch);
    differs = json_array();
    for (i = 0; i < json_object_size(classic); i++) {
        const char *key = json_object_get_key_at(classic, i);
        const json_t *value = json_object_get_at(classic, i);
        const json_t *have = frozen ? json_obj_get(frozen, key) : NULL;
        int same = 0;
        if (!have || have->type == JSON_NULL) same = !value || value->type == JSON_NULL;
        else if (have->type == JSON_NUMBER && value->type == JSON_NUMBER)
            same = have->num_val == value->num_val;
        else if (have->type == JSON_BOOL && value->type == JSON_BOOL)
            same = have->bool_val == value->bool_val;
        else if (have->type == JSON_STRING && value->type == JSON_STRING)
            same = strcmp(have->str_val ? have->str_val : "", value->str_val ? value->str_val : "") == 0;
        else if (have->type == JSON_NUMBER && value->type == JSON_BOOL)
            same = have->num_val == (value->bool_val ? 1.0 : 0.0);
        else if (have->type == JSON_BOOL && value->type == JSON_NUMBER)
            same = value->num_val == (have->bool_val ? 1.0 : 0.0);
        if (!same) json_append(differs, json_string(key));
    }
    out = json_object();
    if (json_len(differs) > 0) {
        char *msg = gs_resume_policy_error(differs);
        json_set(out, "error", msg ? json_string(msg) : json_null());
        free(msg);
    } else {
        json_set(out, "error", json_null());
    }
    json_free(differs);
    json_free(classic);
    return out;
}
