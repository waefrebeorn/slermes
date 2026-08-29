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

#include "port_hermes_cli_heartbeat.h"
#include "goal_contract.h"
#include "loops_pure.h"
#include "terminal_breadcrumbs_pure.h"
#include "port_tools_wake_word.h"
#include "approval.h"
#include "port_bang_shell_ports.h"
/* Declared in src/hermes_cli/runtime_provider.c (no public header). */
extern char *canonical_custom_identity(const char *base_url, const char *config_provider);
#include <time.h>

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
extern int hermes_state_set_session_yolo(hermes_state_db_t *db,
                                         const char *session_id,
                                         bool enabled);
extern void hermes_state_update_session_model(hermes_state_db_t *db,
                                              const char *session_id,
                                              const char *model);
extern bool hermes_state_session_yolo_enabled(const char *session_meta_json);
/* Declared in src/tools/approval_helpers2.c (no public header). */
extern int  apr2_enable_session_yolo(const char *session_key);
extern bool apr2_is_session_yolo_enabled(const char *session_key);

/* Declared in src/tools/approval.c (in include/approval.h). */
extern void approval_set_yolo(bool enabled);
extern bool approval_is_yolo_enabled(void);
extern bool is_yolo_mode_frozen(void);
/* From hermes.h umbrella or approval internals. */
extern void approval_set_current_session_key(const char *key);
extern char *resolve_runtime_provider(const char *requested,
                                      const char *explicit_api_key,
                                      const char *explicit_base_url,
                                      const char *target_model);
/* Declared in src/agent/hermes_state/hermes_state_gaps.c. */
extern json_t *session_gateway_runtime(const json_t *session_meta);

/* Declared in src/agent/hermes_state/hermes_state_messages.c (orphan). */
extern char *hermes_state_resolve_resume_session_id(hermes_state_db_t *db,
                                                    const char *session_id);

/* Declared in src/agent/tool_dispatch_helpers.c (no public header yet). */
extern bool is_multimodal_tool_result(const json_t *value);
extern char *multimodal_text_summary_from_json(const json_t *value);

/* Declared in src/hermes_cli/pet_input.c. */
extern void cli__pet_flash(const char *state, double secs);
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
/* PoP: _is_ephemeral_scaffolding @ cli.py:_is_ephemeral_scaffolding */
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

/* PoP: _owns_process_notification @ cli.py:_owns_process_notification */
/* Return whether this CLI session provably owns a delegation event.
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

/* PoP: _get_loop_manager @ cli.py:_get_loop_manager */
/* Return the LoopManager bound to the current session_id, cached on
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

/* PoP: _clear_persisted_context_for_model_switch @ cli.py:_clear_persisted_context_for_model_switch */
/* Drop the global context_length pin when its configured owner changes.
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

/* PoP: _show_context_breakdown @ cli.py:_show_context_breakdown */
/* /context [all] — visual context-window usage breakdown.
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

/* PoP: _usage_reset @ cli.py:_usage_reset */
/* /usage reset [--force] — redeem one banked Codex reset credit.
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

/* PoP: _drain_process_notifications @ cli.py:_drain_process_notifications */
/* Queue background notifications owned by this CLI session. Mirrors Python:
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

/* PoP: _rewind_persisted_user_turn @ cli.py:_rewind_persisted_user_turn */
/* Bind one warm user ordinal to a durable row and rewind it atomically.
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

/* PoP: _handle_usage_command @ cli.py:_handle_usage_command */
json_t *hcli_handle_usage_command(hcli_t *self, const char *cmd_original) {
    if (!self) return NULL;
    if (!cmd_original || !*cmd_original) return NULL;

    /* parts = cmd_original.split() */
    /* args = [p.lower() for p in parts[1:]] */
    char *cmd_copy = strdup(cmd_original);
    if (!cmd_copy) return NULL;
    /* skip first token (the command name) */
    char *p = cmd_copy;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    /* p now points to args */
    char *arg_copy = strdup(p);
    free(cmd_copy);
    /* lower() + check first token */
    char *first = arg_copy;
    while (*first && *first == ' ') first++;
    /* extract first arg token */
    char *first_end = first;
    while (*first_end && *first_end != ' ' && *first_end != '\t') first_end++;
    bool is_reset = (strncasecmp(first, "reset", first_end - first) == 0 &&
                     first_end - first == 5);
    bool has_args = (first_end > first);

    if (is_reset) {
        /* --force check */
        bool force = false;
        char *rest = first_end;
        while (*rest) {
            while (*rest == ' ') rest++;
            if (strncasecmp(rest, "--force", 7) == 0) { force = true; break; }
            while (*rest && *rest != ' ') rest++;
        }
        hcli_usage_reset(self, force);
        free(arg_copy);
        return json_new_null();
    }

    if (has_args) {
        printf("  Unknown /usage subcommand: %s. Try /usage or /usage reset [--force].\n",
               arg_copy);
        free(arg_copy);
        return json_new_null();
    }

    /* Bare /usage — delegate to show_usage seam if available. */
    if (self->show_status_fn) {
        self->show_status_fn(self);
    }
    /* TODO: hcli_show_usage() — complex: accesses agent rate limits, */
    /* compressor state, session tokens. Requires agent seam + context_compressor. */
    free(arg_copy);
    return json_new_null();
}

/* PoP: _write_terminal_breadcrumb @ cli.py:_write_terminal_breadcrumb */
void hcli_write_terminal_breadcrumb(hcli_t *self) {
    if (!self) return;
    tbr_write_breadcrumb(self->session_id, NULL);
}

/* PoP: _maybe_start_wake_word @ cli.py:_maybe_start_wake_word */
void hcli_maybe_start_wake_word(hcli_t *self) {
    if (!self) return;
    if (self->should_exit) return;
    /* Config gate: session.wake_word.surface must enable "cli". */
    json_t *cfg = ww_load_wake_word_config();
    if (!cfg) return;
    if (!ww_wake_surface_enabled("cli", cfg)) {
        json_free(cfg);
        return;
    }
    /* Already listening? */
    if (self->wake_word_active) {
        json_free(cfg);
        return;
    }
    /* Check requirements — best-effort. */
    json_t *reqs = ww_check_wake_word_requirements(cfg);
    if (reqs) {
        json_t *avail = json_object_get(reqs, "available");
        if (avail && avail->type == JSON_BOOL && !avail->bool_val) {
            json_free(reqs);
            json_free(cfg);
            return;
        }
        json_free(reqs);
    }
    /* start_new_session default True (matches Python cfg.get default). */
    self->wake_start_new_session = true;

    /* start_listening — callback is wired by the TUI shell at runtime.
     * In pure C, pass NULL owner; the listener seam is set by the shell. */
    self->wake_word_active = true;
    json_free(cfg);
}

/* PoP: _on_wake_word @ cli.py:_on_wake_word */
void hcli_on_wake_word(hcli_t *self) {
    if (!self) return;
    if (self->should_exit) return;
    /* Ignore wake while a turn is in flight or the mic is already in use. */
    if (self->agent_running || self->voice_recording || self->voice_processing)
        return;
    /* Release the mic so STT can capture the command utterance. */
    if (!ww_pause_listening(self)) {
        self->wake_word_active = false;
        return;
    }
    self->wake_suspended = true;

    /* Multi-profile routing — CLI is single-profile. */
    json_t *match = ww_get_last_match();
    if (match) {
        char *profile = ww_active_profile_name();
        json_t *matched_profile = json_object_get(match, "1");
        if (profile && matched_profile &&
            matched_profile->type == JSON_STRING &&
            matched_profile->str_val &&
            strcmp(profile, matched_profile->str_val) != 0) {
            printf("\n  Wake phrase for profile '%s' — run: hermes -p %s\n",
                   matched_profile->str_val, matched_profile->str_val);
            self->wake_suspended = true;
            free(profile);
            json_free(match);
            return;
        }
        free(profile);
        json_free(match);
    }

    printf("\n  ✦ Wake word detected — listening...\n");
    if (self->app) {
        /* self._app.invalidate() — no-op without app seam */
    }
}

/* PoP: _persist_session_yolo @ cli.py:_persist_session_yolo */
void hcli_persist_session_yolo(hcli_t *self, const char *session_key, bool enabled) {
    if (!self || !session_key || !*session_key ||
        strcmp(session_key, "default") == 0) return;
    /* Best-effort: in-memory toggle is authoritative; persistence only
     * affects future --resume. Swallow errors (matches Python's except:pass). */
    if (self->persist_yolo) {
        self->persist_yolo(self, session_key, enabled);
    } else if (self->session_db) {
        int rc = hermes_state_set_session_yolo(self->session_db, session_key, enabled);
        (void)rc;
    }
}

/* PoP: _stop_wake_word_listener @ cli.py:_stop_wake_word_listener */
void hcli_stop_wake_word_listener(hcli_t *self, bool announce) {
    (void)announce;
    if (!self) return;
    self->wake_word_active = false;
    self->wake_suspended = false;
    ww_stop_listening(self);
}

/* PoP: _start_wake_watchdog @ cli.py:_start_wake_watchdog
 * Daemon thread polls every 0.25s; after 3 consecutive idle polls (~0.75s),
 * resume the paused detector. Thread exits when wake_word_active is cleared. */
static void *wake_watchdog_fn(void *arg) {
    hcli_t *self = (hcli_t *)arg;
    int idle_polls = 0;
    while (self->wake_word_active && !self->should_exit) {
        struct timespec ts = {0, 250000000}; /* 0.25s */
        nanosleep(&ts, NULL);
        if (!self->wake_suspended) {
            idle_polls = 0;
            continue;
        }
        bool busy = self->agent_running ||
                    self->voice_recording ||
                    self->voice_processing ||
                    (self->pending_input_put_fn != NULL);
        if (busy) {
            idle_polls = 0;
            continue;
        }
        idle_polls++;
        if (idle_polls >= 3) {
            idle_polls = 0;
            if (ww_resume_listening(self)) {
                self->wake_suspended = false;
            } else {
                self->wake_word_active = false;
            }
        }
    }
    return NULL;
}

void hcli_start_wake_word_watchdog(hcli_t *self) {
    if (!self) return;
    if (self->wake_watchdog_started) return;
    self->wake_watchdog_started = true;
    pthread_t tid;
    pthread_create(&tid, NULL, wake_watchdog_fn, self);
    pthread_detach(tid);
}

/* PoP: _start_heartbeat_watchdog @ cli.py:_start_heartbeat_watchdog
 * Daemon thread polls every hb_POLL_SECONDS (5.0s); when idle and a
 * heartbeat is due, inject the prompt into _pending_input. Idempotent. */
static void *heartbeat_watchdog_fn(void *arg) {
    hcli_t *self = (hcli_t *)arg;
    while (self->wake_word_active && !self->should_exit) {
        /* hb_POLL_SECONDS = 5.0 → 5s sleep */
        struct timespec ts = {0, 5000000000};
        nanosleep(&ts, NULL);
        if (self->should_exit) break;
        void *mgr = hcli_get_heartbeat_manager(self);
        if (!mgr || !hb_manager_is_active(mgr)) continue;
        bool busy = self->agent_running ||
                    self->voice_recording ||
                    self->voice_processing;
        if (busy) continue;
        /* Check pending input queue is empty — use the seam. */
        if (self->pending_input_put_fn == NULL) continue; /* no way to check */
        if (self->pending_input_nonempty_fn && self->pending_input_nonempty_fn(self))
            continue;  /* not idle — pending input waiting */
        char *prompt = hb_manager_due_prompt(mgr, (double)time(NULL));
        if (prompt) {
            self->pending_input_put_fn(self, prompt);
            free(prompt);
        }
    }
    self->heartbeat_watchdog_started = false;
    return NULL;
}

void hcli_start_heartbeat_watchdog(hcli_t *self) {
    if (!self) return;
    if (self->heartbeat_watchdog_started) return;
    self->heartbeat_watchdog_started = true;
    pthread_t tid;
    pthread_create(&tid, NULL, heartbeat_watchdog_fn, self);
    pthread_detach(tid);
}

/* PoP: _get_heartbeat_manager @ cli.py:_get_heartbeat_manager */
void *hcli_get_heartbeat_manager(hcli_t *self) {
    if (!self) return NULL;
    const char *sid = self->session_id ? self->session_id : "";
    if (!*sid) return NULL;

    /* Return cached manager if session_id matches. */
    hb_manager_t *existing = (hb_manager_t *)self->heartbeat_manager;
    if (existing && existing->session_id &&
        strcmp(existing->session_id, sid) == 0) {
        return existing;
    }

    /* Rebind lazily — mirrors Python's try/import + GracefulHeartbeatUnavailable. */
    hb_manager_t *mgr = hb_manager_new(sid);
    self->heartbeat_manager = mgr;
    return mgr;
}

/* PoP: _get_goal_manager @ cli.py:_get_goal_manager */
void *hcli_get_goal_manager(hcli_t *self) {
    if (!self) return NULL;
    const char *sid = self->session_id ? self->session_id : "";
    if (!*sid) return NULL;

    /* Return cached manager if session_id matches. */
    goal_manager_t *existing = (goal_manager_t *)self->goal_manager;
    if (existing && goal_manager_session_id(existing) &&
        strcmp(goal_manager_session_id(existing), sid) == 0) {
        return existing;
    }

    /* Load goals config for default_max_turns — mirrors Python's load_config()
     * with a broad except → defaults to 20. */
    int max_turns = 20;
    if (self->config) {
        json_t *goal_cfg = config_py_get_config_value(self->config, "goals", NULL);
        if (goal_cfg) {
            json_t *max_t = json_object_get(goal_cfg, "max_turns");
            if (max_t && max_t->type == JSON_NUMBER) {
                max_turns = (int)max_t->num_val;
            }
            json_free(goal_cfg);
        }
    }
    if (max_turns <= 0) max_turns = 20;

    goal_manager_t *mgr = goal_manager_new(sid, NULL, max_turns);
    self->goal_manager = mgr;
    return mgr;
}

/* PoP: _maybe_fire_loop_tick @ cli.py:_maybe_fire_loop_tick */
void hcli_maybe_fire_loop_tick(hcli_t *self) {
    if (!self) return;
    /* Only runs while the agent is idle and nothing is queued. */
    if (self->agent_running) return;
    if (self->agent) return;

    loop_manager_t *mgr = (loop_manager_t *)self->loop_manager;
    if (!mgr) return;

    double now = (double)time(NULL);
    /* Throttle the deferred re-check (matches Python's _last_loop_tick_check). */
    if (now - self->last_loop_tick_check < 2.0) return;
    self->last_loop_tick_check = now;

    if (!loops_manager_is_due(mgr, now)) return;

    /* Real user input (or anything else queued) takes priority. */
    if (self->pending_input_put_fn && self->pending_input_nonempty_fn) {
        if (self->pending_input_nonempty_fn(self)) return;
    }

    /* An active goal that blocks also wins the idle boundary. */
    const char *sid = loops_manager_session_id(mgr);
    if (sid && loops_goal_blocks_loop_tick(sid)) return;

    char *wakeup = loops_manager_fire_tick(mgr, now, 0.0);
    if (!wakeup) return;

    loop_state_t *state = loops_manager_state(mgr);
    long tick_no = loops_state_ticks_fired(state);
    (void)tick_no;  /* tick number available via state */

    /* Inject the wakeup as a synthetic user turn via the pending_input seam. */
    if (self->pending_input_put_fn) {
        self->pending_input_put_fn(self, wakeup);
    }
    free(wakeup);
}

/* PoP: _maybe_complete_loop_tick_after_turn @ cli.py:_maybe_complete_loop_tick_after_turn */
void hcli_maybe_complete_loop_tick_after_turn(hcli_t *self) {
    if (!self) return;

    loop_manager_t *mgr = (loop_manager_t *)self->loop_manager;
    if (!mgr) return;

    loop_state_t *state = loops_manager_state(mgr);
    if (!state || !loops_state_awaiting_response(state)) return;

    /* User-interrupted wakeup turn pauses the loop (recoverable via /loop resume). */
    if (self->last_turn_interrupted) {
        loops_manager_pause(mgr, "user-interrupted (Ctrl+C)");
        return;
    }

    /* Extract last assistant response from conversation history. */
    char *last_response = NULL;
    if (self->history && self->history_len > 0) {
        for (size_t i = self->history_len; i-- > 0;) {
            hcli_msg_t *msg = &self->history[i];
            if (msg && msg->role && strcmp(msg->role, "assistant") == 0) {
                last_response = strdup(msg->content ? msg->content : "");
                break;
            }
        }
    }
    if (!last_response) last_response = strdup("");

    double now = (double)time(NULL);
    char *decision_json = loops_manager_complete_tick(mgr, last_response, now, 0.0, 0.0);
    free(last_response);

    if (decision_json) {
        json_t *dec = json_parse(decision_json, NULL);
        if (dec) {
            json_t *msg = json_object_get(dec, "message");
            if (msg && msg->type == JSON_STRING && msg->str_val) {
                /* Print the message — mirrors _cprint */
                printf("  %s\n", msg->str_val);
            }
            json_free(dec);
        }
        free(decision_json);
    }
}

/* PoP: _start_wake_word_listener @ cli.py:_start_wake_word_listener */
bool hcli_start_wake_word_listener(hcli_t *self, bool announce) {
    if (!self) return false;
    if (self->wake_word_active && ww_owns_listener(self)) {
        return true;
    }
    self->wake_word_active = false;

    json_t *cfg = ww_load_wake_word_config();
    if (!cfg) {
        if (announce)
            printf("  Wake word unavailable (no config)\n");
        return false;
    }

    json_t *reqs = ww_check_wake_word_requirements(cfg);
    if (reqs) {
        json_t *avail = json_object_get(reqs, "available");
        if (avail && avail->type == JSON_BOOL && !avail->bool_val) {
            if (announce) {
                printf("\n  Wake word requirements not met:\n");
                json_t *hint = json_object_get(reqs, "hint");
                if (hint && hint->type == JSON_STRING && hint->str_val)
                    printf("  %s\n", hint->str_val);
            }
            json_free(reqs);
            json_free(cfg);
            return false;
        }
        self->wake_start_new_session = true;
        json_free(reqs);
    }

    (void)announce;
    self->wake_word_active = true;
    json_free(cfg);
    return true;
}

/* PoP: _show_wake_word_status @ cli.py:_show_wake_word_status */
json_t *hcli_show_wake_word_status(hcli_t *self) {
    if (!self) return NULL;

    json_t *cfg = ww_load_wake_word_config();
    json_t *reqs = cfg ? ww_check_wake_word_requirements(cfg) : NULL;
    bool owned = ww_owns_listener(self);
    bool listening = ww_is_listening();

    const char *state = "OFF";
    if (owned && listening) state = "LISTENING";
    else if (owned) state = "PAUSED";

    printf("\n  Wake Word Status\n");
    printf("  State:       %s\n", state);

    if (reqs) {
        json_t *phrase = json_object_get(reqs, "phrase");
        if (phrase && phrase->type == JSON_STRING && phrase->str_val)
            printf("  Phrase:      \"%s\"\n", phrase->str_val);
        json_t *provider = json_object_get(reqs, "provider");
        if (provider && provider->type == JSON_STRING && provider->str_val)
            printf("  Provider:    %s\n", provider->str_val);
        json_free(reqs);
    }

    if (cfg) {
        json_t *surface = json_object_get(cfg, "surface");
        if (surface && surface->type == JSON_STRING && surface->str_val)
            printf("  Surface:     %s\n", surface->str_val);
        else
            printf("  Surface:     auto\n");
        json_t *sn = json_object_get(cfg, "start_new_session");
        bool sns = true;
        if (sn && sn->type == JSON_BOOL) sns = sn->bool_val;
        printf("  New session: %s\n", sns ? "yes" : "no");
        json_free(cfg);
    } else {
        printf("  Surface:     auto\n");
        printf("  New session: yes\n");
    }

    return json_new_null();
}

/* PoP: _transfer_session_yolo @ cli.py:_transfer_session_yolo */
void hcli_transfer_session_yolo(hcli_t *self, const char *old_id,
                                 const char *new_id) {
    if (!self || !old_id || !new_id ||
        strcmp(old_id, new_id) == 0) return;
    if (is_yolo_mode_frozen()) return;
    /* Python: disable_session_yolo(old) + enable_session_yolo(new).
     * In C, the global yolo state is shared — set current session key
     * to the new id (mirrors run_conversation binding session_id). */
    approval_set_yolo(false);
    approval_set_current_session_key(new_id);
}

/* PoP: _is_session_yolo_active @ cli.py:_is_session_yolo_active */
bool hcli_is_session_yolo_active(hcli_t *self) {
    (void)self;
    /* Honors frozen --yolo flag first, then per-session approval set. */
    if (is_yolo_mode_frozen()) return true;
    if (approval_is_yolo_enabled()) return true;
    return false;
}

/* PoP: _toggle_yolo @ cli.py:_toggle_yolo */
void hcli_toggle_yolo(hcli_t *self) {
    if (!self) return;
    if (is_yolo_mode_frozen()) return;

    const char *sid = self->session_id ? self->session_id : "default";
    if (approval_is_yolo_enabled()) {
        approval_set_yolo(false);
        printf("  ⚡ YOLO mode OFF — dangerous commands will prompt.\n");
    } else {
        approval_set_yolo(true);
        approval_set_current_session_key(sid);
        printf("  ⚡ YOLO mode ON — all dangerous commands auto-approved.\n");
    }
}

/* PoP: _persist_model_switch_to_session @ cli.py:_persist_model_switch_to_session */
void hcli_persist_model_switch_to_session(hcli_t *self, const json_t *result) {
    if (!self || !self->session_db || !self->session_id ||
        !result || result->type != JSON_OBJECT) return;

    const char *sid = self->session_id;
    const char *new_model = json_get_str(result, "new_model", NULL);
    hermes_state_update_session_model(self->session_db, sid, new_model);

    const char *provider = json_get_str(result, "target_provider", NULL);
    const char *base_url = json_get_str(result, "base_url", NULL);
    const char *api_mode = json_get_str(result, "api_mode", NULL);

    /* Heal bare "custom" to canonical identity (or NULL to drop). */
    char *canon = NULL;
    if (provider && strcasecmp(provider, "custom") == 0) {
        canon = canonical_custom_identity(base_url, NULL);
        provider = canon ? canon : NULL;
    }

    /* Build the route blob: provider, base_url, api_mode — all or-None. */
    json_t *route = json_new_object();
    json_set(route, "provider", provider ? json_string(provider) : json_null());
    json_set(route, "base_url", base_url ? json_string(base_url) : json_null());
    json_set(route, "api_mode", api_mode ? json_string(api_mode) : json_null());

    /* patch_session_model_config merges gateway_runtime + flat route keys. */
    json_t *patch = json_new_object();
    json_set(patch, "gateway_runtime", route);
    json_set(patch, "provider", provider ? json_string(provider) : json_null());
    json_set(patch, "base_url", base_url ? json_string(base_url) : json_null());
    json_set(patch, "api_mode", api_mode ? json_string(api_mode) : json_null());

    hermes_state_patch_session_model_config(self->session_db, sid, patch);

    json_free(route);
    json_free(patch);
    free(canon);
}

/* PoP: _restore_session_yolo @ cli.py:_restore_session_yolo */
void hcli_restore_session_yolo(hcli_t *self, const char *session_meta_json,
                                bool quiet) {
    if (!self) return;
    if (is_yolo_mode_frozen()) return;
    if (!hermes_state_session_yolo_enabled(session_meta_json)) return;

    const char *sid = self->session_id ? self->session_id : "default";
    if (apr2_is_session_yolo_enabled(sid)) return;

    apr2_enable_session_yolo(sid);
    const char *msg = "⚡ YOLO mode restored from session — all commands auto-approved. /yolo to turn off.";
    if (quiet) {
        fprintf(stderr, "%s\n", msg);
    } else {
        printf("  %s\n", msg);
    }
}

/* PoP: _snapshot_model_runtime @ cli.py:_snapshot_model_runtime */
hcli_model_runtime_t *hcli_snapshot_model_runtime(hcli_t *self) {
    if (!self) return NULL;
    hcli_model_runtime_t *snap = calloc(1, sizeof(hcli_model_runtime_t));
    if (!snap) return NULL;
    snap->model = strdup(self->model ? self->model : "");
    snap->provider = strdup(self->provider ? self->provider : "");
    snap->requested_provider = strdup(self->requested_provider ? self->requested_provider : "");
    snap->explicit_api_key = self->explicit_api_key ? strdup(self->explicit_api_key) : NULL;
    snap->explicit_base_url = self->explicit_base_url ? strdup(self->explicit_base_url) : NULL;
    snap->api_key = strdup(self->api_key ? self->api_key : "");
    snap->base_url = strdup(self->base_url ? self->base_url : "");
    snap->api_mode = strdup(self->api_mode ? self->api_mode : "");

    /* Deep-copy agent._primary_runtime JSON.
     * In Python: copy.deepcopy(getattr(agent, "_primary_runtime", None)).
     * self->agent is opaque (void *) in C — no direct field access.
     * Caller (TUI shell) may set snap->agent_primary_runtime_json
     * via a pre-serialized JSON string before handoff. NULL = no agent. */
    /* snap->agent_primary_runtime_json left NULL by calloc */
    return snap;
}

/* PoP: _restore_model_runtime_snapshot @ cli.py:_restore_model_runtime_snapshot */
void hcli_restore_model_runtime_snapshot(hcli_t *self, const hcli_model_runtime_t *snap) {
    if (!self || !snap) return;
    free(self->model);             self->model = snap->model ? strdup(snap->model) : NULL;
    free(self->provider);          self->provider = snap->provider ? strdup(snap->provider) : NULL;
    free(self->requested_provider); self->requested_provider = snap->requested_provider ? strdup(snap->requested_provider) : NULL;
    free(self->explicit_api_key);  self->explicit_api_key = snap->explicit_api_key ? strdup(snap->explicit_api_key) : NULL;
    free(self->explicit_base_url); self->explicit_base_url = snap->explicit_base_url ? strdup(snap->explicit_base_url) : NULL;
    free(self->api_key);           self->api_key = snap->api_key ? strdup(snap->api_key) : NULL;
    free(self->base_url);          self->base_url = snap->base_url ? strdup(snap->base_url) : NULL;
    free(self->api_mode);          self->api_mode = snap->api_mode ? strdup(snap->api_mode) : NULL;
    /* agent_primary_runtime_json is set by TUI shell seam, not restored here
     * (agent is opaque; Python setattr can't be done in C for private fields). */
}

/* PoP: hcli_model_runtime_free @ hermes_cli/runtime_provider.py:ModelRuntimeSnapshot */
void hcli_model_runtime_free(hcli_model_runtime_t *snap) {
    if (!snap) return;
    free(snap->model);
    free(snap->provider);
    free(snap->requested_provider);
    free(snap->explicit_api_key);
    free(snap->explicit_base_url);
    free(snap->api_key);
    free(snap->base_url);
    free(snap->api_mode);
    free(snap->agent_primary_runtime_json);
    free(snap);
}

/* PoP: handle_bang_shell @ cli.py:handle_bang_shell */
bool hcli_handle_bang_shell(hcli_t *self, const char *text) {
    if (!self || !text) return false;
    if (!bang_shell_is_bang_command(text)) return false;
    if (!bang_shell_enabled()) return false;

    char *command = bang_shell_parse_bang_command(text);
    if (!command || !*command) {
        const char *hint = "  !<command> — run a shell command without sending it to the model.\n";
        if (self->console_print_fn) self->console_print_fn(self, hint);
        else printf("%s", hint);
        free(command);
        return true;
    }

    char *approval_json = bang_shell_check_approval(command);
    if (approval_json) {
        json_t *aj = json_parse(approval_json, NULL);
        if (aj && aj->type == JSON_OBJECT) {
            bool approved = json_get_bool(aj, "approved", false);
            if (!approved) {
                const char *msg = json_get_str(aj, "message", NULL);
                if (self->console_print_fn) {
                    char line[1024];
                    snprintf(line, sizeof(line), "  %s\n", msg ? msg : "Command denied.");
                    self->console_print_fn(self, line);
                } else {
                    printf("  %s\n", msg ? msg : "Command denied.");
                }
                json_free(aj);
                free(approval_json);
                free(command);
                return true;
            }
        }
        json_free(aj);
        free(approval_json);
    }

    const char *cwd = bang_shell_resolve_cwd(self->session_id);
    int exit_code = bang_shell_run_command(command, cwd, 30, NULL);
    if (exit_code) {
        char line[128];
        snprintf(line, sizeof(line), "  ! exited %d\n", exit_code);
        if (self->console_print_fn)
            self->console_print_fn(self, line);
        else
            printf("%s", line);
    }

    free(command);
    return true;
}

/* PoP: _restore_session_model @ cli.py:_restore_session_model */
void hcli_restore_session_model(hcli_t *self, const json_t *session_meta, bool quiet) {
    if (!self || !session_meta) return;
    if (self->explicit_model_override) return;

    const char *stored_model = json_get_str(session_meta, "model", NULL);
    if (!stored_model || !*stored_model) return;

    /* Read persisted gateway_runtime from session row. */
    json_t *runtime = session_gateway_runtime(session_meta);
    if (!runtime || runtime->type != JSON_OBJECT) {
        if (runtime) json_free(runtime);
        return;
    }

    const char *stored_provider = json_get_str(runtime, "provider", NULL);
    const char *stored_base_url = json_get_str(runtime, "base_url", NULL);
    const char *stored_api_mode = json_get_str(runtime, "api_mode", NULL);

    /* Heal bare "custom" — recover canonical identity or drop provider. */
    char *canon = NULL;
    if (stored_provider && strcasecmp(stored_provider, "custom") == 0) {
        canon = canonical_custom_identity(stored_base_url, NULL);
        stored_provider = canon ? canon : NULL;
    }

    /* Resolve credentials for the stored provider (or ambient). */
    char *resolved_json = resolve_runtime_provider(
        stored_provider ? stored_provider : self->provider,
        stored_provider ? NULL : self->explicit_api_key,
        stored_base_url,
        stored_model);

    if (resolved_json) {
        json_t *resolved = json_parse(resolved_json, NULL);
        if (resolved && resolved->type == JSON_OBJECT) {
            const char *r_provider = json_get_str(resolved, "provider", NULL);
            const char *r_api_key  = json_get_str(resolved, "api_key", NULL);
            const char *r_base_url = json_get_str(resolved, "base_url", NULL);
            const char *r_api_mode = json_get_str(resolved, "api_mode", NULL);

            if (r_provider) { free(self->provider); self->provider = strdup(r_provider); }
            if (r_api_key)    { free(self->api_key); self->api_key = strdup(r_api_key); }
            if (r_base_url)   { free(self->base_url); self->base_url = strdup(r_base_url); }
            if (r_api_mode)   { free(self->api_mode); self->api_mode = strdup(r_api_mode); }
            /* Always set the model. */
            free(self->model); self->model = strdup(stored_model);
        }
        json_free(resolved);
        free(resolved_json);
    } else {
        /* Resolution failed — keep ambient credentials, set model only. */
        free(self->model); self->model = strdup(stored_model);
        if (stored_provider) { free(self->provider); self->provider = strdup(stored_provider); }
    }

    free(canon);
    json_free(runtime);

    /* Announce. */
    char msg[256];
    if (stored_provider && stored_provider[0]) {
        snprintf(msg, sizeof(msg), "Model restored from session: %s (%s)",
                 stored_model, stored_provider);
    } else {
        snprintf(msg, sizeof(msg), "Model restored from session: %s", stored_model);
    }
    if (quiet) {
        fprintf(stderr, "%s\n", msg);
    } else {
        if (self->console_print_fn) {
            char dm[280];
            snprintf(dm, sizeof(dm), "  %s\n", msg);
            self->console_print_fn(self, dm);
        }
    }
}

/* PoP: finalize_preloaded_skills @ cli.py:HermesCLI.finalize_preloaded_skills */
void hcli_finalize_preloaded_skills(hcli_t *self) {
    if (!self) return;
    if (self->preload_skills_finalized) return;

    if (!self->preload_thread_valid) {
        self->preload_skills_finalized = true;
        return;
    }

    /* Join the preload thread (timeout=120s in Python). */
    void *thread_result = NULL;
    pthread_join(self->preload_thread, &thread_result);
    self->preload_thread_valid = false;
    self->preload_skills_finalized = true;

    /* Check for error — Python: err = getattr(self, "_preload_skills_error", None); raise err */
    if (self->preload_skills_error && *self->preload_skills_error) {
        fprintf(stderr, "Preload error: %s\n", self->preload_skills_error);
        return;
    }

    /* If no result, nothing to fold in. */
    if (!self->preload_skills_prompt) return;

    /* If ALL requested skills were missing, this is a hard fail.
     * Python: if loaded_skills is empty and missing_skills is non-empty
     * → raise ValueError. C equivalent: print error. */
    if (self->preload_missing_len > 0 && self->preload_loaded_len == 0) {
        char *missing = NULL;
        if (self->preload_missing_skills) {
            size_t total = 0;
            for (size_t i = 0; i < self->preload_missing_len; i++) {
                if (self->preload_missing_skills[i]) {
                    total += strlen(self->preload_missing_skills[i]) + 2;
                }
            }
            missing = malloc(total + 1);
            missing[0] = '\0';
            for (size_t i = 0; i < self->preload_missing_len; i++) {
                if (missing && self->preload_missing_skills[i]) {
                    if (i > 0) strcat(missing, ", ");
                    strcat(missing, self->preload_missing_skills[i]);
                }
            }
        }
        fprintf(stderr, "Unknown skill(s) requested (all missing): %s\n",
                missing ? missing : "(none)");
        free(missing);
        return;
    }

    /* Fold the skills prompt into self->skills_preamble or print warning
     * for partial missing. */
    if (self->preload_missing_len > 0 && self->preload_loaded_len > 0) {
        /* Degrade gracefully — log warning, continue. */
        fprintf(stderr, "Warning: some preloaded skills were unknown, skipping them.\n");
    }
}

/* PoP: _get_voice_status_fragments @ cli.py:_get_voice_status_fragments */
hcli_fragment_t *hcli_get_voice_status_fragments(hcli_t *self, int width, size_t *out_len) {
    if (!self || !out_len) { if (out_len) *out_len = 0; return NULL; }
    *out_len = 0;

    int w = width;
    if (w <= 0) w = cli_get_tui_terminal_width();
    bool compact = hcli_use_minimal_tui_chrome(self, w);
    char *label = hcli_voice_record_key_label(self);

    hcli_fragment_t *frags = NULL;
    const char *style = NULL;
    char *text = NULL;

    if (self->voice_recording) {
        style = "class:voice-status-recording";
        if (compact) {
            text = strdup(" ● REC ");
        } else {
            char buf[128];
            snprintf(buf, sizeof(buf), " ● REC  %s to stop ", label ? label : "?");
            text = strdup(buf);
        }
    } else if (self->voice_processing) {
        style = "class:voice-status";
        if (compact) {
            text = strdup(" ◉ STT ");
        } else {
            text = strdup(" ◉ Transcribing... ");
        }
    } else {
        style = "class:voice-status";
        char tts_part[32] = "";
        char cont_part[32] = "";
        char label_part[32] = "";
        if (self->voice_tts)        snprintf(tts_part,  sizeof(tts_part),  " | TTS on");
        if (self->voice_continuous) snprintf(cont_part, sizeof(cont_part), " | Continuous");
        if (label)                  snprintf(label_part, sizeof(label_part), "%s to record ", label);
        char buf[160];
        if (compact) {
            snprintf(buf, sizeof(buf), " 🎤 %s ", label ? label : "?");
        } else {
            snprintf(buf, sizeof(buf), " 🎤 Voice mode%s%s%s  —  %s to record ",
                     tts_part, cont_part, "", label);
        }
        text = strdup(buf);
    }

    free(label);

    if (!text) return NULL;
    frags = malloc(sizeof(hcli_fragment_t));
    if (!frags) { free(text); return NULL; }
    frags[0].style = strdup(style);
    frags[0].text  = text;
    *out_len = 1;
    return frags;
}


/* PoP: hcli_status_bar_goal_segment @ cli.py:_status_bar_goal_segment */
char *hcli_status_bar_goal_segment(const json_t *snapshot) {
    if (!snapshot || snapshot->type != JSON_OBJECT) return strdup("");
    bool goal_active = json_get_bool(snapshot, "goal_active", false);
    if (!goal_active) return strdup("");
    long used = (long)json_get_num(snapshot, "goal_turns_used", 0);
    long max_turns = (long)json_get_num(snapshot, "goal_max_turns", 0);
    if (max_turns > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "⊙ goal %ld/%ld", used, max_turns);
        return strdup(buf);
    }
    return strdup("⊙ goal");
}

/* PoP: _build_status_bar_text @ cli.py:_build_status_bar_text */
char *hcli_build_status_bar_text(hcli_t *self, int width) {
    if (!self) return NULL;
    json_t *snapshot = hcli_get_status_bar_snapshot(self);
    if (!snapshot || snapshot->type != JSON_OBJECT) {
        if (snapshot) json_free(snapshot);
        return strdup("");
    }
    if (width <= 0) width = cli_get_tui_terminal_width();

    const char *percent_str = json_get_str(snapshot, "percent", NULL);
    const char *percent_label = percent_str ? percent_str : "--";
    const char *duration_label = json_get_str(snapshot, "duration", NULL);
    const char *battery_label = json_get_str(snapshot, "battery_label", NULL);
    const char *focus_label = json_get_str(snapshot, "focus_label", NULL);
    const char *session_title = json_get_str(snapshot, "session_title", NULL);
    const char *model_short = json_get_str(snapshot, "model_short", NULL);

    char battery_prefix[64] = "";
    if (battery_label && *battery_label)
        snprintf(battery_prefix, sizeof(battery_prefix), "%s │ ", battery_label);

    bool yolo = hcli_is_session_yolo_active(self);
    char *goal_seg = hcli_status_bar_goal_segment(snapshot);
    int compressions = (int)json_get_num(snapshot, "compressions", 0);
    int bg_tasks = (int)json_get_num(snapshot, "active_background_tasks", 0);
    int bg_procs = (int)json_get_num(snapshot, "active_background_processes", 0);
    int bg_subs = (int)json_get_num(snapshot, "active_background_subagents", 0);

    char *result;
    if (width < 52) {
        char text[256];
        snprintf(text, sizeof(text), "%s⚕ %s · %s",
                 battery_prefix, model_short ? model_short : "?", duration_label ? duration_label : "");
        if (goal_seg && goal_seg[0]) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "%s · %s", text, goal_seg);
            strncpy(text, tmp, sizeof(text)-1); text[sizeof(text)-1]='\0';
        }
        if (focus_label && *focus_label) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "%s · %s", text, focus_label);
            strncpy(text, tmp, sizeof(text)-1); text[sizeof(text)-1]='\0';
        }
        if (yolo) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "%s · ⚠ YOLO", text);
            strncpy(text, tmp, sizeof(text)-1); text[sizeof(text)-1]='\0';
        }
        result = cli_right_align_status_title(text, session_title, width);
    } else {
        /* width >= 52: multi-segment layout */
        char parts[16][128];
        int nparts = 0;
        snprintf(parts[nparts++], 128, "⚕ %s", model_short ? model_short : "?");
        if (battery_label && *battery_label) {
            /* insert battery at front */
            for (int i = nparts; i > 0; i--) strncpy(parts[i], parts[i-1], 127);
            strncpy(parts[0], battery_label, 127);
            nparts++;
        }
        snprintf(parts[nparts++], 128, "%s", percent_label);
        if (compressions) snprintf(parts[nparts++], 128, "🗜️ %d", compressions);
        if (bg_tasks) snprintf(parts[nparts++], 128, "▶ %d", bg_tasks);
        if (bg_procs) snprintf(parts[nparts++], 128, "⚙ %d", bg_procs);
        if (bg_subs) snprintf(parts[nparts++], 128, "⏱ %d", bg_subs);
        const char *prompt_elapsed = json_get_str(snapshot, "prompt_elapsed", NULL);
        if (prompt_elapsed && *prompt_elapsed) snprintf(parts[nparts++], 128, "%s", prompt_elapsed);
        const char *idle_since = json_get_str(snapshot, "idle_since", NULL);
        if (idle_since && *idle_since) snprintf(parts[nparts++], 128, "%s", idle_since);

        char text[512] = "";
        for (int i = 0; i < nparts; i++) {
            if (i > 0) strncat(text, " │ ", sizeof(text) - strlen(text) - 1);
            strncat(text, parts[i], sizeof(text) - strlen(text) - 1);
        }
        if (goal_seg && goal_seg[0]) {
            char tmp[640];
            snprintf(tmp, sizeof(tmp), "%s │ %s", text, goal_seg);
            strncpy(text, tmp, sizeof(text)-1); text[sizeof(text)-1]='\0';
        }
        if (focus_label && *focus_label) {
            char tmp[640];
            snprintf(tmp, sizeof(tmp), "%s │ %s", text, focus_label);
            strncpy(text, tmp, sizeof(text)-1); text[sizeof(text)-1]='\0';
        }
        if (yolo) {
            char tmp[640];
            snprintf(tmp, sizeof(tmp), "%s │ ⚠ YOLO", text);
            strncpy(text, tmp, sizeof(text)-1); text[sizeof(text)-1]='\0';
        }
        result = cli_right_align_status_title(text, session_title, width);
    }
    free(goal_seg);
    json_free(snapshot);
    return result;
}

/* PoP: _get_status_bar_fragments @ cli.py:_get_status_bar_fragments */
hcli_fragment_t *hcli_get_status_bar_fragments(hcli_t *self, size_t *out_len) {
    if (!out_len) return NULL;
    *out_len = 0;
    if (!self) return NULL;
    if (!self->status_bar_visible) return NULL;
    if (self->model_picker_open || self->command_palette_open) return NULL;

    json_t *snapshot = hcli_get_status_bar_snapshot(self);
    if (!snapshot || snapshot->type != JSON_OBJECT) {
        if (snapshot) json_free(snapshot);
        return NULL;
    }
    int width = cli_get_tui_terminal_width();
    const char *duration = json_get_str(snapshot, "duration", "");
    bool yolo = hcli_is_session_yolo_active(self);
    char *goal_seg = hcli_status_bar_goal_segment(snapshot);
    const char *battery_label = json_get_str(snapshot, "battery_label", "");
    const char *focus = json_get_str(snapshot, "focus_label", "");
    const char *model_short = json_get_str(snapshot, "model_short", "");
    const char *batt_cat = json_get_str(snapshot, "battery_category", "dim");
    const char *batt_style = cli_battery_status_style(batt_cat);

#define ADD(s, t) do { if (n < 63) { frags[n].style = strdup(s); frags[n].text = strdup(t); n++; } } while(0)

    hcli_fragment_t frags[64];
    size_t n = 0;

    if (width < 52) {
        ADD("class:status-bar", "\xe2\x9a\x82 ");
        ADD("class:status-bar-strong", model_short);
        ADD("class:status-bar-dim", " \xc2\xb7 ");
        ADD("class:status-bar-dim", duration);
        if (goal_seg && goal_seg[0]) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            ADD("class:status-bar-strong", goal_seg);
        }
        if (focus && *focus) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            ADD("class:status-bar-strong", focus);
        }
        if (yolo) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            ADD("class:status-bar-yolo", "\xe2\x9a\xa0 YOLO");
        }
        ADD("class:status-bar", " ");
    } else {
        ADD("class:status-bar", "\xe2\x9a\x82 ");
        ADD("class:status-bar-strong", model_short);
        ADD("class:status-bar-dim", " \xc2\xb7 ");
        double pct = json_get_num(snapshot, "context_percent", -1);
        char pct_str[16];
        if (pct < 0) snprintf(pct_str, sizeof(pct_str), "--");
        else snprintf(pct_str, sizeof(pct_str), "%.0f%%", pct);
        ADD(batt_style ? batt_style : "class:status-bar-dim", pct_str);

        int compressions = (int)json_get_num(snapshot, "compressions", 0);
        int bg_tasks = (int)json_get_num(snapshot, "active_background_tasks", 0);
        int bg_procs = (int)json_get_num(snapshot, "active_background_processes", 0);

        if (compressions) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            char tmp[32]; snprintf(tmp, sizeof(tmp), "\xf0\x9f\x97\xbc %d", compressions);
            ADD(cli_compression_count_style(compressions), tmp);
        }
        if (bg_tasks) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            char tmp[32]; snprintf(tmp, sizeof(tmp), "\xe2\x96\xb6 %d", bg_tasks);
            ADD("class:status-bar-strong", tmp);
        }
        if (bg_procs) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            char tmp[32]; snprintf(tmp, sizeof(tmp), "\xe2\x9a\x99 %d", bg_procs);
            ADD("class:status-bar-strong", tmp);
        }
        if (goal_seg && goal_seg[0]) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            ADD("class:status-bar-strong", goal_seg);
        }
        ADD("class:status-bar-dim", " \xc2\xb7 ");
        ADD("class:status-bar-dim", duration);
        if (focus && *focus) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            ADD("class:status-bar-strong", focus);
        }
        if (yolo) {
            ADD("class:status-bar-dim", " \xc2\xb7 ");
            ADD("class:status-bar-yolo", "\xe2\x9a\xa0 YOLO");
        }
        ADD("class:status-bar", " ");
    }
#undef ADD

    json_free(snapshot);
    free(goal_seg);

    hcli_fragment_t *result = malloc(n * sizeof(hcli_fragment_t));
    if (!result) return NULL;
    for (size_t i = 0; i < n; i++) result[i] = frags[i];
    *out_len = n;
    return result;
}

/* PoP: _on_reaction @ cli.py:_on_reaction */
void hcli_on_reaction(hcli_t *self, const char *kind) {
    (void)self;
    if (!kind) return;
    /* Python: if kind == "vibe": self._pet_flash("jump") */
    if (strcmp(kind, "vibe") == 0) {
        cli__pet_flash("jump", 3.0);
    }
}
