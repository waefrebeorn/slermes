/* t_port_telegram_adapter_flow11.c -- harness for the Telegram adapter FLOW
 * part 11 port (src/gateway/platforms/telegram_adapter_flow11.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events ("trace"), the final return ("ret"), and the state echo ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the LIVE
 * reference from it).
 */
#include "gateway/platforms/telegram_adapter_flow11.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ta11_fn_t)(ta11_ctx_t *, const json_t *);
typedef struct { const char *func; ta11_fn_t fn; } ta11_case_t;

static const ta11_case_t CASES[] = {
    { "_cache_replied_media", ta11_cache_replied_media },
    { "_cache_dm_topic_from_message", ta11_cache_dm_topic_from_message },
    { "_callback_authorized", ta11_callback_authorized },
    { "_chat_outbound_slot_remaining", ta11_chat_outbound_slot_remaining },
    { "_cancel_task_attr", ta11_cancel_task_attr },
    { "_cache_observed_media", ta11_cache_observed_media },
    { "_apply_yaml_config", ta11_apply_yaml_config },
    { "_caption_1024", ta11_caption_1024 },
    { "_entity_type", ta11_entity_type },
    { "_ext_from_path", ta11_ext_from_path },
    { "_await_cold_start_readiness", ta11_await_cold_start_readiness },
    { "_build_message_event", ta11_build_message_event },
    { "_build_ptb_requests", ta11_build_ptb_requests },
    { "_cache_inbound_av", ta11_cache_inbound_av },
    { "_cache_inbound_document", ta11_cache_inbound_document },
    { "_cancel_pending_delivery_tasks", ta11_cancel_pending_delivery_tasks },
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

        const ta11_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ta11_ctx_t ctx;
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
