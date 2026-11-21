/* t_port_discord_adapter_flow13.c -- harness for the Discord adapter FLOW
 * part 13 port (src/gateway/platforms/discord_adapter_flow13.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events ("trace"), the final return ("ret"), and the state echo ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the LIVE
 * reference from it).
 */
#include "gateway/platforms/discord_adapter_flow13.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da13_fn_t)(da13_ctx_t *, const json_t *);
typedef struct { const char *func; da13_fn_t fn; } da13_case_t;

static const da13_case_t CASES[] = {
    { "_on_platform_message_edit", da13_on_platform_message_edit },
    { "_on_platform_message_delete", da13_on_platform_message_delete },
    { "_on_platform_thread_create", da13_on_platform_thread_create },
    { "_on_platform_thread_update", da13_on_platform_thread_update },
    { "_read_websocket_health", da13_read_websocket_health },
    { "_read_command_sync_state", da13_read_command_sync_state },
    { "_record_command_sync_rate_limit", da13_record_command_sync_rate_limit },
    { "_probe_audio_duration_seconds", da13_probe_audio_duration_seconds },
    { "_process_voice_input", da13_process_voice_input },
    { "_read_attachment_bytes", da13_read_attachment_bytes },
    { "_read_url_image_with_redirect_guard", da13_read_url_image_with_redirect_guard },
    { "_observe_exec_approval_settle", da13_observe_exec_approval_settle },
    { "_on_packet", da13_on_packet },
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

        const da13_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            da13_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            /* Inject the mutable doubles the oracle materializes from args. */
            if (strcmp(func, "_observe_exec_approval_settle") == 0) {
                if (!da13_has(ctx.state, "view") && args) {
                    const json_t *v = json_obj_get(args, "view");
                    if (v) json_set(ctx.state, "view", json_copy((json_t *)v));
                }
            } else if (strcmp(func, "_process_voice_input") == 0) {
                if (!da13_has(ctx.state, "_voice_text_channels") && args) {
                    const json_t *v = json_obj_get(args, "voice_text_channels");
                    json_set(ctx.state, "_voice_text_channels", v ? json_copy((json_t *)v) : json_object());
                }
            }
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
