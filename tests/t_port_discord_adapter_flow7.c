/* t_port_discord_adapter_flow7.c -- harness for the Discord adapter FLOW7
 * config/env + localization/classification remainder port
 * (src/gateway/platforms/discord_adapter_flow7.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret") and the state echo ("state" -- only the two forum-probe-cache defs
 * mutate state; all others leave the input copy unchanged). The echoed "in" is the
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
#include "gateway/platforms/discord_adapter_flow7.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da7_fn_t)(da7_ctx_t *, const json_t *);
typedef struct { const char *func; da7_fn_t fn; } da7_case_t;

static const da7_case_t CASES[] = {
    { "_standalone_response_encoding", da7_standalone_response_encoding },
    { "_standalone_close_response", da7_standalone_close_response },
    { "_remember_channel_is_forum", da7_remember_channel_is_forum },
    { "_probe_is_forum_cached", da7_probe_is_forum_cached },
    { "_build_allowed_mentions", da7_build_allowed_mentions },
    { "_load_discord_int_config", da7_load_discord_int_config },
    { "_load_voice_timeout", da7_load_voice_timeout },
    { "_load_playback_timeout", da7_load_playback_timeout },
    { "_voice_timeout_limit", da7_voice_timeout_limit },
    { "_playback_timeout_limit", da7_playback_timeout_limit },
    { "_exec_approval_cmd_budget", da7_exec_approval_cmd_budget },
    { "_text_batch_delay_for", da7_text_batch_delay_for },
    { "_resolve_channel_prompt", da7_resolve_channel_prompt },
    { "_playback_timeout_for_audio", da7_playback_timeout_for_audio },
    { "_attachment_message_type", da7_attachment_message_type },
    { "_guild_member_has_role", da7_guild_member_has_role },
    { "_reply_target", da7_reply_target },
    { "_stamp_auto_thread_name", da7_stamp_auto_thread_name },
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

        const da7_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            da7_ctx_t ctx;
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
