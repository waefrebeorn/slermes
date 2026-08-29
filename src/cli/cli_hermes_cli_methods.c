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
