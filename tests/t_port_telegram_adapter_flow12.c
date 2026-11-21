/* t_port_telegram_adapter_flow12.c -- harness for the Telegram adapter FLOW
 * part 12 port (src/gateway/platforms/telegram_adapter_flow12.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events ("trace"), the final return ("ret"), and the state echo ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the LIVE
 * reference from it).
 */
#include "gateway/platforms/telegram_adapter_flow12.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ta12_fn_t)(ta12_ctx_t *, const json_t *);
typedef struct { const char *func; ta12_fn_t fn; } ta12_case_t;

static const ta12_case_t CASES[] = {
    { "_clear_reactions", ta12_clear_reactions },
    { "_clear_task_attrs_except", ta12_clear_task_attrs_except },
    { "_consume_abandoned_task", ta12_consume_abandoned_task },
    { "_claim_callback_state", ta12_claim_callback_state },
    { "_check_polling_stall", ta12_check_polling_stall },
    { "_collect_live_tasks", ta12_collect_live_tasks },
    { "_create_dm_topic", ta12_create_dm_topic },
    { "_delete_webhook_best_effort", ta12_delete_webhook_best_effort },
    { "_download_observed_media", ta12_download_observed_media },
    { "_drain_general_connections_after_pool_timeout", ta12_drain_general_connections_after_pool_timeout },
    { "_disarm_ptb_retry_loop", ta12_disarm_ptb_retry_loop },
    { "_chat_send_lock", ta12_chat_send_lock },
    { "_compress_image_to_jpeg", ta12_compress_image_to_jpeg },
    { "_normalize_chat_type", ta12_normalize_chat_type },
    { "_env_float_clamped", ta12_env_float_clamped },
    { "_notification_kwargs", ta12_notification_kwargs },
    { "_is_id_like", ta12_is_id_like },
    { "_coerce_bool_extra", ta12_coerce_bool_extra },
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

        const ta12_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ta12_ctx_t ctx;
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
