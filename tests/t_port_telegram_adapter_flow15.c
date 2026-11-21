/* t_port_telegram_adapter_flow15.c -- harness for the Telegram adapter FLOW
 * part 15 port (src/gateway/platforms/telegram_adapter_flow15.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events ("trace"), the final return ("ret"), and the state echo ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the LIVE
 * reference from it).
 */
#include "gateway/platforms/telegram_adapter_flow15.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ta15_fn_t)(ta15_ctx_t *, const json_t *);
typedef struct { const char *func; ta15_fn_t fn; } ta15_case_t;

static const ta15_case_t CASES[] = {
    { "_handle_location_message", ta15_handle_location_message },
    { "_handle_media_message", ta15_handle_media_message },
    { "_handle_model_picker_callback", ta15_handle_model_picker_callback },
    { "_handle_polling_conflict", ta15_handle_polling_conflict },
    { "_handle_polling_network_error", ta15_handle_polling_network_error },
    { "_handle_slash_confirm_callback", ta15_handle_slash_confirm_callback },
    { "_handle_sticker", ta15_handle_sticker },
    { "_handle_text_message", ta15_handle_text_message },
    { "_handle_update_prompt_callback", ta15_handle_update_prompt_callback },
    { "_handoff_polling_fatal_error", ta15_handoff_polling_fatal_error },
    { "_hold_chat_outbound_slot", ta15_hold_chat_outbound_slot },
    { "_initialize_app_with_retries", ta15_initialize_app_with_retries },
    { "_instrument_polling_request", ta15_instrument_polling_request },
    { "_is_callback_user_authorized", ta15_is_callback_user_authorized },
    { "_warn_then", ta15_warn_then },
    { "_set_status_indicator", ta15_set_status_indicator },
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

        const ta15_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ta15_ctx_t ctx;
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
