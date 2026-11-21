/* t_port_telegram_adapter_flow14.c -- harness for the Telegram adapter FLOW
 * part 14 port (src/gateway/platforms/telegram_adapter_flow14.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events ("trace"), the final return ("ret"), and the state echo ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the LIVE
 * reference from it).
 */
#include "gateway/platforms/telegram_adapter_flow14.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ta14_fn_t)(ta14_ctx_t *, const json_t *);
typedef struct { const char *func; ta14_fn_t fn; } ta14_case_t;

static const ta14_case_t CASES[] = {
    { "_should_drop_delayed_delivery", ta14_should_drop_delayed_delivery },
    { "_send_retry_is_final", ta14_send_retry_is_final },
    { "_observed_media_source", ta14_observed_media_source },
    { "_flush_media_group_event", ta14_flush_media_group_event },
    { "_flush_photo_batch", ta14_flush_photo_batch },
    { "_flush_text_batch", ta14_flush_text_batch },
    { "_get_dm_topic_info", ta14_get_dm_topic_info },
    { "_get_general_request_drain_lock", ta14_get_general_request_drain_lock },
    { "_go_fatal_network", ta14_go_fatal_network },
    { "_handle_command", ta14_handle_command },
    { "_handle_choice_picker_callback", ta14_handle_choice_picker_callback },
    { "_handle_callback_query", ta14_handle_callback_query },
    { "_handle_clarify_callback", ta14_handle_clarify_callback },
    { "_handle_exec_approval_callback", ta14_handle_exec_approval_callback },
    { "_handle_gmail_triage_callback", ta14_handle_gmail_triage_callback },
    { "_handle_inline_query", ta14_handle_inline_query },
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

        const ta14_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ta14_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            ctx.errors = in ? json_obj_get(in, "errors") : NULL;
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
