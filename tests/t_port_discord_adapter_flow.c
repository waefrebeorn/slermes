/* t_port_discord_adapter_flow.c — harness for the Discord adapter FLOW port
 * (src/gateway/platforms/discord_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the state echo ("state" — no ported function mutates
 * adapter state). The echoed "in" is the ORIGINAL input (the oracle re-drives
 * the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/discord_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da_fn_t)(da_ctx_t *, const json_t *);
typedef struct { const char *func; da_fn_t fn; } da_case_t;

static const da_case_t CASES[] = {
    { "_image_ext_from_content_type", da_image_ext_from_content_type },
    { "_format_discord_markdown_link", da_format_discord_markdown_link },
    { "_needs_server_members_intent", da_needs_server_members_intent },
    { "_format_privileged_intents_guidance", da_format_privileged_intents_guidance },
    { "_discord_snowflake_time", da_discord_snowflake_time },
    { "_metadata_marks_nonconversational", da_metadata_marks_nonconversational },
    { "_prompt_target_id", da_prompt_target_id },
    { "_clean_discord_id", da_clean_discord_id },
    { "_command_sync_state_key", da_command_sync_state_key },
    { "_command_sync_mutation_interval_seconds", da_command_sync_mutation_interval_seconds },
    { "_truncate_discord_component_text", da_truncate_discord_component_text },
    { "_discord_ready_timeout_seconds", da_discord_ready_timeout_seconds },
    { "_text_batch_flush_deadline_seconds", da_text_batch_flush_deadline_seconds },
    { "_multiplex_active", da_multiplex_active },
    { "discord_deps_present", da_discord_deps_present },
    { "_is_discord_transport_error", da_is_discord_transport_error },
    { "_config_value", da_config_value },
    { "_warn_liveness_config_disabled", da_warn_liveness_config_disabled },
    { "_liveness_knob", da_liveness_knob },
    { "_finite_positive_config_float", da_finite_positive_config_float },
    { "_config_int", da_config_int },
    { "_classify_connect_exception", da_classify_connect_exception },
    { "_thread_id_and_chat_for_channel", da_thread_id_and_chat_for_channel },
    { "_source_for_platform_event", da_source_for_platform_event },
    { "_platform_events_subscribed", da_platform_events_subscribed },
    { "_fire_platform_event", da_fire_platform_event },
    { "_emit_platform_event", da_emit_platform_event },
    { "_message_event_parts", da_message_event_parts },
    { "_thread_event_parts", da_thread_event_parts },
    { "_command_sync_skip_reason", da_command_sync_skip_reason },
    { "_extract_discord_retry_after", da_extract_discord_retry_after },
    { "_is_discord_rate_limit", da_is_discord_rate_limit },
    { "_is_discord_unknown_interaction", da_is_discord_unknown_interaction },
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

        const da_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            da_ctx_t ctx;
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