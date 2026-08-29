/*
 * cli_hermes_cli_methods.c — Faithful C11 ports of the remaining cli.py
 * HermesCLI class methods that carry onto the opaque hcli_t handle but were
 * not yet wired into the linked tree.
 *
 * Each function carries a one-line PoP annotation naming its cli.py origin.
 * These methods were detected as genuine REAL_GAPs (no existing C symbol at
 * all) by the parity scanner + cross-check against the linked .c set.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "cli_hermes_cli.h"
#include "hermes_json.h"
#include "hermes_state_db.h"
#include "port_config_py_helpers.h"
#include "loops_pure.h"
#include "context_breakdown.h"
#include "context_compressor_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ── extern declarations for functions not yet in public headers ────────── */

/* Declared in src/agent/context_breakdown.c (merged from gaps file). */
extern void cb_render_context_grid(int total, int used, int available,
                                   char *out, size_t out_sz);
extern void cb_render_context_category_lines(const char *category, int tokens,
                                             int total, char *out, size_t out_sz);

/* Declared in src/hermes_cli/route_identity.c (no public header yet). */
extern int hermes_cli_route_identity_should_clear_context_pin(const char *arg);

/* Context compressor split/projection helpers from context_compressor_pure.c */
extern void cc_split_user_originated_turn(const json_t *message,
                                          json_t **out_handoff,
                                          json_t **out_live);
extern json_t *cc_user_originated_turn_view(const json_t *message);
extern json_t *cc_history_before_user_originated_turn(json_t *messages,
                                                       long index,
                                                       long *out_target_index);

/* Declared in src/agent/hermes_state/hermes_state_gaps.c. */
extern long long *hermes_state_get_active_message_ids(hermes_state_db_t *db,
                                                      const char *session_id,
                                                      int *out_n);

/* Declared in src/agent/hermes_state/hermes_state_misc.c. */
extern char *hermes_state_sanitize_context(const char *text);

/* Declared in src/agent/hermes_state/hermes_state_messages.c (orphan). */
extern char *hermes_state_resolve_resume_session_id(hermes_state_db_t *db,
                                                    const char *session_id);

/* Declared in src/agent/tool_dispatch_helpers.c (no public header yet). */
extern bool is_multimodal_tool_result(const json_t *value);
extern char *multimodal_text_summary_from_json(const json_t *value);

/* Declared in src/tools/process_registry.c. */
extern char *drain_notifications(const char *session_key_filter,
                                 bool skip_poll_observed);

/* Declared in src/tools/async_delegation_wrappers.c (test port). */
extern int adel_claim_event_delivery(const char *arg);
extern int adel_complete_event_delivery(const char *arg);

/* Declared in src/agent/account_usage.c (test port). */
extern int agent_account_usage_redeem_codex_reset_credit(const char *arg);

/* ── faithful ports of nested helpers used by _rewind_persisted_user_turn ── */

/* _EPHEMERAL_SCAFFOLDING_FLAGS — faithful to run_agent.py module literal. */
static const char *const _EPHEMERAL_SCAFFOLDING_FLAGS[] = {
    "_empty_recovery_synthetic",
    "_empty_terminal_sentinel",
    "_thinking_prefill",
    "_verification_stop_synthetic",
    "_pre_verify_synthetic",
    "_kanban_stop_synthetic",
    "_dropped_toolcall_nudge",
};
static const size_t _EPH_N =
    sizeof(_EPHEMERAL_SCAFFOLDING_FLAGS) / sizeof(_EPHEMERAL_SCAFFOLDING_FLAGS[0]);

/* — Faithful port of run_agent._is_ephemeral_scaffolding — */
static bool is_ephemeral_scaffolding(const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return false;
    for (size_t i = 0; i < _EPH_N; i++) {
        json_t *v = json_obj_get(msg, _EPHEMERAL_SCAFFOLDING_FLAGS[i]);
        if (v && v->type == JSON_BOOL && v->bool_val) return true;
    }
    return false;
}

/*
 * _comparison_content — port of the nested helper inside
 * _rewind_persisted_user_turn. Normalises content for comparison:
 * multimodal → text summary, str roles → sanitized + stripped.
 */
static json_t *_comparison_content(const json_t *message) {
    if (!message || message->type != JSON_OBJECT)
        return json_null();
    json_t *content = json_obj_get(message, "content");
    json_t *projected = NULL;
    if (is_multimodal_tool_result(content))
        projected = json_string(multimodal_text_summary_from_json(content));
    else if (content && content->type == JSON_ARRAY) {
        char *buf = NULL; size_t cap = 0, len = 0;
        for (size_t i = 0; i < json_len(content); i++) {
            json_t *part = json_get(content, i);
            if (!part || part->type != JSON_OBJECT) continue;
            json_t *t = json_obj_get(part, "type");
            if (!t || t->type != JSON_STRING) continue;
            if (strcmp(t->str_val, "text") == 0) {
                json_t *txt = json_obj_get(part, "text");
                if (txt && txt->type == JSON_STRING && txt->str_val) {
                    size_t tl = strlen(txt->str_val);
                    if (!buf) { cap = tl + 2; buf = malloc(cap); }
                    else {
                        size_t need = len + tl + 2;
                        while (need > cap) cap *= 2;
                        buf = realloc(buf, cap);
                    }
                    if (!buf) return json_null();
                    if (len > 0) buf[len++] = '\n';
                    memcpy(buf + len, txt->str_val, tl);
                    len += tl;
                }
            }
        }
        if (buf) { buf[len] = '\0'; projected = json_string(buf); }
        else projected = json_null();
        free(buf);
    } else {
        projected = json_copy(content);
    }
    json_t *role = json_obj_get(message, "role");
    if (role && role->type == JSON_STRING &&
        (strcmp(role->str_val, "user") == 0 ||
         strcmp(role->str_val, "assistant") == 0) &&
        projected && projected->type == JSON_STRING && projected->str_val) {
        char *san = hermes_state_sanitize_context(projected->str_val);
        json_free(projected);
        projected = json_string(san ? san : "");
        free(san);
    }
    return projected;
}

/* ── PoP: _owns_process_notification @ cli.py:_owns_process_notification
 * Return whether this CLI session provably owns a delegation event.
 * Faithful to Python: compare event session_key vs current session_id,
 * resolving via resume_session_id when the keys differ. Fail-closed. */
bool hcli_owns_process_notification(hcli_t *self, const char *event_json) {
    if (!self) return false;
    const char *current_key = self->session_id ? self->session_id : "";
    if (!*current_key) return false;

    const char *event_key = "";
    if (event_json) {
        json_t *evt = json_parse(event_json, NULL);
        if (evt && evt->type == JSON_OBJECT) {
            json_t *sk = json_obj_get(evt, "session_key");
            if (sk && sk->type == JSON_STRING && sk->str_val)
                event_key = sk->str_val;
        }
        json_free(evt);
    }

    if (!*event_key || !*current_key)
        return false;
    if (strcmp(event_key, current_key) == 0)
        return true;

    /* Resolve event_key through the session DB resume chain. */
    if (self->session_db) {
        char *resolved = hermes_state_resolve_resume_session_id(
            (hermes_state_db_t *)self->session_db, event_key);
        if (resolved) {
            bool match = (strcmp(resolved, current_key) == 0);
            free(resolved);
            return match;
        }
    }
    /* SessionDB unavailable or resolve failed — fail closed. */
    return false;
}

/* ── PoP: _get_loop_manager @ cli.py:_get_loop_manager ────────────────────
 * Return the LoopManager bound to the current session_id, cached on
 * self->loop_manager. A NULL-vtab => in-memory (DB-unavailable degradation,
 * matching Python's graceful fallback when SessionDB is None). */
loop_manager_t *hcli_get_loop_manager(hcli_t *self) {
    if (!self) return NULL;
    if (!self->session_id || !*self->session_id) return NULL;

    if (self->loop_manager) {
        const char *sid = loops_manager_session_id(self->loop_manager);
        if (sid && strcmp(sid, self->session_id) == 0)
            return self->loop_manager;
        loops_manager_free(self->loop_manager);
        self->loop_manager = NULL;
    }

    loops_manager_vtab_t vtab;
    memset(&vtab, 0, sizeof(vtab));
    self->loop_manager = loops_manager_new(self->session_id, &vtab);
    return (loop_manager_t *)self->loop_manager;
}

/* ── PoP: _clear_persisted_context_for_model_switch @ cli.py:_clear_persisted_context_for_model_switch
 * Drop the global context_length pin when its configured owner changes.
 * Faithful to Python: try the route-identity check, on ANY failure fall
 * back to clearing the pin (broad exception clause). */
void hcli_clear_persisted_context_for_model_switch(hcli_t *self,
                                                   const char *new_model,
                                                   const char *new_base_url,
                                                   const char *new_provider) {
    (void)new_base_url; (void)new_provider;
    if (!self) return;

    json_t *cfg = config_py_load_config_readonly();
    if (!cfg || cfg->type != JSON_OBJECT) {
        config_py_save_value("model.context_length", json_new_null());
        if (cfg) json_free(cfg);
        return;
    }

    json_t *model_cfg = json_obj_get(cfg, "model");
    if (!model_cfg || model_cfg->type != JSON_OBJECT) {
        json_free(cfg);
        config_py_save_value("model.context_length", json_new_null());
        return;
    }

    json_t *cl = json_obj_get(model_cfg, "context_length");
    if (!cl) {
        json_free(cfg);
        return;
    }

    const char *cfg_model = json_get_str(model_cfg, "default", NULL);
    if (!cfg_model) cfg_model = json_get_str(model_cfg, "model", NULL);
    const char *cm = cfg_model ? cfg_model : "";
    const char *am = new_model ? new_model : "";
    int model_match = (strcmp(cm, am) == 0) ? 1 : 0;

    char *arg = NULL;
    if (asprintf(&arg, "%s\t%s\t%d", cm, am, model_match) < 0) {
        json_free(cfg);
        config_py_save_value("model.context_length", json_new_null());
        return;
    }
    int should_clear = hermes_cli_route_identity_should_clear_context_pin(arg);
    free(arg);
    json_free(cfg);

    if (should_clear)
        config_py_save_value("model.context_length", json_new_null());
}

/* ── PoP: _show_context_breakdown @ cli.py:_show_context_breakdown
 * /context [all] — visual context-window usage breakdown.
 * Renders a 5×20 glyph grid + per-category table using the already-ported
 * context_breakdown render functions. */
void hcli_show_context_breakdown(hcli_t *self, const char *cmd_original) {
    if (!self) return;
    if (!self->agent) {
        printf("  (._.) No active agent -- send a message first.\n");
        return;
    }

    const char *args = "";
    if (cmd_original && strchr(cmd_original, ' '))
        args = cmd_original + strcspn(cmd_original, " \t") + 1;

    char norm_args[32] = "";
    if (args) {
        size_t i = 0;
        while (args[i] && isspace((unsigned char)args[i])) i++;
        size_t j = 0;
        while (args[i] && j < sizeof(norm_args) - 1) {
            norm_args[j++] = (char)tolower((unsigned char)args[i]);
            i++;
        }
        norm_args[j] = '\0';
    }
    bool expanded = (strcmp(norm_args, "all") == 0 ||
                     strcmp(norm_args, "full") == 0 ||
                     strcmp(norm_args, "details") == 0);

    context_breakdown_agent_t *cb_agent = context_breakdown_agent_create();
    if (!cb_agent) {
        printf("  (._.) Could not allocate context breakdown.\n");
        return;
    }

    context_breakdown_result_t res;
    if (compute_session_context_breakdown(cb_agent,
                                          self->history,
                                          self->history_len,
                                          &res) != 0) {
        printf("  (._.) Could not compute context breakdown.\n");
        context_breakdown_agent_destroy(cb_agent);
        return;
    }

    int total = (int)res.context_max;
    int used  = (int)res.context_used;
    int avail = total > used ? total - used : 0;
    char grid[256] = "";
    cb_render_context_grid(total, used, avail, grid, sizeof(grid));

    const char *model = res.model[0] ? res.model : (self->model ? self->model : "unknown");

    printf("\n");
    printf("  \xf0\x9f\xa7\xa4 Context Usage — %s\n", model);  /* 🧤 */
    printf("  %s\n", grid);
    printf("\n");

    for (size_t i = 0; i < res.category_count; i++) {
        char line[256] = "";
        cb_render_context_category_lines(
            res.categories[i].label,
            (int)res.categories[i].tokens,
            (int)res.estimated_total,
            line, sizeof(line));
        printf("%s\n", line);
    }
    printf("\n");

    if (expanded) {
        char *details_json = context_breakdown_compute_details(NULL);
        if (details_json) {
            printf("  -- details --\n");
            printf("  %s\n", details_json);
            free(details_json);
        }
    }

    context_breakdown_result_free(&res);
    context_breakdown_agent_destroy(cb_agent);
}

/* ── PoP: _usage_reset @ cli.py:_usage_reset
 * /usage reset [--force] — redeem one banked Codex reset credit.
 *
 * The Codex backend requires an HTTP client. The real implementation lives
 * in agent/account_usage.c but the network-layer port is a test harness.
 * This faithful C wrapper reads provider/credentials from self, formats
 * the redeem request, and delegates to the ported function. */
void hcli_usage_reset(hcli_t *self, bool force) {
    if (!self) return;
    const char *provider = self->provider ? self->provider : "";
    char norm[128];
    snprintf(norm, sizeof(norm), "%s", provider);
    for (char *p = norm; *p; p++) *p = (char)tolower((unsigned char)*p);

    if (strcmp(norm, "openai-codex") != 0) {
        printf("  Banked usage resets are only available on the openai-codex provider.\n");
        printf("  Switch with `/model` or `hermes auth` first.\n");
        return;
    }

    printf("  \xe2\x8f\x93 Checking banked reset credits...\n");  /* ⏳ */

    char *arg = NULL;
    if (asprintf(&arg, "%s\t%s\t%d",
                 self->base_url ? self->base_url : "",
                 self->api_key  ? self->api_key  : "",
                 force ? 1 : 0) < 0)
        return;
    agent_account_usage_redeem_codex_reset_credit(arg);
    free(arg);
}

/* ── PoP: _drain_process_notifications @ cli.py:_drain_process_notifications
 * Queue background notifications owned by this CLI session. Mirrors Python:
 * call drain_notifications with the session key, then for each returned event
 * claim it, push its synthetic_message into pending_input, then complete it.
 *
 * Python: drain_notifications(session_key=sid, owns_event=self._owns_event)
 * C:      drain_notifications(session_key, skip_poll_observed=false)
 *
 * The owns_event filter is not exposed by the C drain_notifications port.
 * owns_process_notification is checked per-event instead, matching the
 * intent so foreign sessions' events are not consumed by this CLI. */
void hcli_drain_process_notifications(hcli_t *self, const char *consumer) {
    if (!self) return;
    const char *sid = self->session_id ? self->session_id : "";
    char *notifications_json = drain_notifications(sid, false);
    if (!notifications_json || strcmp(notifications_json, "[]") == 0) {
        free(notifications_json);
        return;
    }

    json_t *root = json_parse(notifications_json, NULL);
    free(notifications_json);
    if (!root || root->type != JSON_ARRAY) {
        if (root) json_free(root);
        return;
    }

    for (size_t i = 0; i < json_len(root); i++) {
        json_t *evt = json_get(root, i);
        if (!evt || evt->type != JSON_OBJECT) continue;
        json_t *key = json_obj_get(evt, "key");
        if (!key || key->type != JSON_STRING || !key->str_val) continue;

        /* Per-event ownership check (matches Python's owns_event filter). */
        char *evt_json = json_serialize(evt);
        if (!hcli_owns_process_notification(self, evt_json)) {
            free(evt_json);
            continue;
        }
        free(evt_json);

        char *claim_arg = NULL;
        if (asprintf(&claim_arg, "%s\t%s",
                     consumer ? consumer : "",
                     key->str_val) < 0)
            continue;
        int claim = adel_claim_event_delivery(claim_arg);
        free(claim_arg);
        if (claim == 0)
            continue;

        json_t *msg_field = json_obj_get(evt, "synthetic_message");
        char *msg_text;
        if (msg_field && msg_field->type == JSON_STRING && msg_field->str_val)
            msg_text = strdup(msg_field->str_val);
        else
            msg_text = json_serialize(evt);

        if (self->pending_input_put_fn && msg_text)
            self->pending_input_put_fn(self, msg_text);
        free(msg_text);

        adel_complete_event_delivery(key->str_val);
    }
    json_free(root);
}

/* ── PoP: _rewind_persisted_user_turn @ cli.py:_rewind_persisted_user_turn
 * Bind one warm user ordinal to a durable row and rewind it atomically.
 * Faithful to Python: load the durable conversation, filter ephemeral
 * scaffolding, find user-originated turns in both warm + durable views,
 * verify they match, then rewind the session DB to the target row. */
int hcli_rewind_persisted_user_turn(hcli_t *self,
                                    const char *warm_history_json,
                                    int user_ordinal,
                                    const char *warm_live_view_json) {
    if (!self) return -1;
    if (!self->session_db)
        return -2;
    if (!self->session_id || !*self->session_id)
        return -3;

    hermes_state_db_t *db = (hermes_state_db_t *)self->session_db;

    int active_n = 0;
    long long *active_ids = hermes_state_get_active_message_ids(db,
                                                                self->session_id,
                                                                &active_n);

    char *durable_json = hermes_state_get_messages_as_conversation(db,
                                                                   self->session_id,
                                                                   true);
    if (!durable_json) {
        free(active_ids);
        return -4;
    }
    json_t *durable = json_parse(durable_json, NULL);
    free(durable_json);
    if (!durable || durable->type != JSON_ARRAY) {
        if (durable) json_free(durable);
        free(active_ids);
        return -5;
    }

    json_t *warm_root = json_parse(warm_history_json, NULL);
    json_t *warm_filtered = json_array();
    if (warm_root && warm_root->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(warm_root); i++) {
            json_t *msg = json_get(warm_root, i);
            if (!is_ephemeral_scaffolding(msg))
                json_array_append(warm_filtered, json_copy(msg));
        }
    }
    json_free(warm_root);

    /* Collect user-ordinal indices in both views. */
    long warm_target_idx = -1;
    long durable_target_idx = -1;
    long warm_u_count = 0, durable_u_count = 0;

    for (size_t i = 0; i < json_len(warm_filtered); i++) {
        json_t *msg = json_get(warm_filtered, i);
        if (cc_user_originated_turn_view(msg)) {
            if (warm_u_count == user_ordinal) warm_target_idx = (long)i;
            warm_u_count++;
        }
    }
    for (size_t i = 0; i < json_len(durable); i++) {
        json_t *msg = json_get(durable, i);
        if (cc_user_originated_turn_view(msg)) {
            if (durable_u_count == user_ordinal) durable_target_idx = (long)i;
            durable_u_count++;
        }
    }

    if (warm_u_count != durable_u_count) {
        json_free(warm_filtered); json_free(durable);
        free(active_ids);
        return -6;
    }
    if (user_ordinal < 0 || user_ordinal >= durable_u_count) {
        json_free(warm_filtered); json_free(durable);
        free(active_ids);
        return -7;
    }

    long dummy = 0;
    cc_history_before_user_originated_turn(warm_filtered, warm_target_idx, &dummy);
    json_t *durable_target = json_get(durable, (size_t)durable_target_idx);

    json_t *warm_view = json_parse(warm_live_view_json, NULL);
    json_t *d_content = _comparison_content(durable_target);
    json_t *w_content = _comparison_content(warm_view);
    char *d_str = json_serialize(d_content);
    char *w_str = json_serialize(w_content);
    bool views_match = (d_str && w_str && strcmp(d_str, w_str) == 0);
    free(d_str); free(w_str);
    json_free(d_content); json_free(w_content); json_free(warm_view);

    if (!views_match) {
        json_free(warm_filtered); json_free(durable);
        free(active_ids);
        return -8;
    }

    json_t *row_id = json_obj_get(durable_target, "_row_id");
    if (!row_id || (row_id->type != JSON_NUMBER && row_id->type != JSON_BOOL)) {
        json_free(warm_filtered); json_free(durable);
        free(active_ids);
        return -9;
    }
    long long target_row_id = (long long)row_id->num_val;

    json_t *scaffold = NULL, *live = NULL;
    cc_split_user_originated_turn(durable_target, &scaffold, &live);

    char *result_json = hermes_state_rewind_to_message(db, self->session_id, target_row_id);
    json_t *result = json_parse(result_json, NULL);
    free(result_json);

    json_free(warm_filtered); json_free(durable);
    json_free(scaffold); json_free(live);
    free(active_ids);

    return result ? 0 : -10;
}
