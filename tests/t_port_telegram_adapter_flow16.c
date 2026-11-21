/* t_port_telegram_adapter_flow16.c -- harness for the Telegram adapter FLOW
 * part 16 port (src/gateway/platforms/telegram_adapter_flow16.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events ("trace"), the final return ("ret"), and the state echo ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the LIVE
 * reference from it).
 */
#include "gateway/platforms/telegram_adapter_flow16.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ta16_fn_t)(ta16_ctx_t *, const json_t *);
typedef struct { const char *func; ta16_fn_t fn; } ta16_case_t;

static const ta16_case_t CASES[] = {
    { "_is_user_authorized_from_message", ta16_is_user_authorized_from_message },
    { "_legacy_runner_auth_fn", ta16_legacy_runner_auth_fn },
    { "_log_blocked_user", ta16_log_blocked_user },
    { "_menu_fingerprint", ta16_menu_fingerprint },
    { "_merge_into_pending", ta16_merge_into_pending },
    { "_message_mentions_bot", ta16_message_mentions_bot },
    { "_notify_clarify_expired", ta16_notify_clarify_expired },
    { "_observe_unmentioned_group_message", ta16_observe_unmentioned_group_message },
    { "_on_platform_update", ta16_on_platform_update },
    { "_orphan_and_rebuild_polling_client", ta16_orphan_and_rebuild_polling_client },
    { "_paged_keyboard", ta16_paged_keyboard },
    { "_persist_dm_topic_thread_id", ta16_persist_dm_topic_thread_id },
    { "_photo_batch_key", ta16_photo_batch_key },
    { "_picker_edit", ta16_picker_edit },
    { "_should_attempt_rich_draft", ta16_should_attempt_rich_draft },
    { "_restart_task_attr", ta16_restart_task_attr },
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

        const ta16_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ta16_ctx_t ctx;
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
