/* t_port_discord_adapter_flow2.c — harness for the Discord adapter FLOW
 * remainder port (src/gateway/platforms/discord_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the state echo ("state" — the mutated adapter-state
 * copy; only _snapshot_gate_env / _record_bot_tag_debounce /
 * _is_bot_tag_debounce_continuation write state). The echoed "in" is the
 * ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/discord_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da2_fn_t)(da2_ctx_t *, const json_t *);
typedef struct { const char *func; da2_fn_t fn; } da2_case_t;

static const da2_case_t CASES[] = {
    { "_extra_or_env_flag", da2_extra_or_env_flag },
    { "_discord_require_mention", da2_discord_require_mention },
    { "_discord_free_response_auto_thread", da2_discord_free_response_auto_thread },
    { "_discord_thread_require_mention", da2_discord_thread_require_mention },
    { "_discord_history_backfill", da2_discord_history_backfill },
    { "_discord_max_attachment_bytes", da2_discord_max_attachment_bytes },
    { "_is_discord_voice_message_attachment", da2_is_discord_voice_message_attachment },
    { "_snapshot_gate_env", da2_snapshot_gate_env },
    { "_gate_env", da2_gate_env },
    { "_gate_raw", da2_gate_raw },
    { "_gate_csv_set", da2_gate_csv_set },
    { "_get_allowed_channels", da2_get_allowed_channels },
    { "_get_ignored_channels", da2_get_ignored_channels },
    { "_get_no_thread_channels", da2_get_no_thread_channels },
    { "resolved_allowlist_user_ids", da2_resolved_allowlist_user_ids },
    { "_discord_allow_all_users", da2_discord_allow_all_users },
    { "_gateway_allow_all_users", da2_gateway_allow_all_users },
    { "_get_allow_bots", da2_get_allow_bots },
    { "_bot_tag_debounce_key", da2_bot_tag_debounce_key },
    { "_bot_tag_window_seconds", da2_bot_tag_window_seconds },
    { "_record_bot_tag_debounce", da2_record_bot_tag_debounce },
    { "_is_bot_tag_debounce_continuation", da2_is_bot_tag_debounce_continuation },
    { "_discord_free_response_channels", da2_discord_free_response_channels },
    { "_raw_mentioned_user_ids", da2_raw_mentioned_user_ids },
    { "_self_is_explicitly_mentioned", da2_self_is_explicitly_mentioned },
    { "_self_is_raw_mentioned", da2_self_is_raw_mentioned },
    { "_discord_bots_require_inline_mention", da2_discord_bots_require_inline_mention },
    { "_discord_channel_keys", da2_discord_channel_keys },
    { "_discord_channel_keys_from_channel", da2_discord_channel_keys_from_channel },
    { "_discord_history_backfill_limit", da2_discord_history_backfill_limit },
    { "_get_discord_command_sync_policy", da2_get_discord_command_sync_policy },
    { "_normalize_permissions", da2_normalize_permissions },
    { "_canonicalize_app_command_option", da2_canonicalize_app_command_option },
    { "_canonicalize_app_command_payload", da2_canonicalize_app_command_payload },
    { "_existing_command_to_payload", da2_existing_command_to_payload },
    { "_patchable_app_command_payload", da2_patchable_app_command_payload },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    json_t *empty_args = json_object();
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *out = json_object();

        const da2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            da2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", in ? json_copy(in) : json_null());
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}