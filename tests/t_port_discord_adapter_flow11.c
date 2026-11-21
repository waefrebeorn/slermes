/* t_port_discord_adapter_flow11.c — harness for the Discord adapter FLOW11
 * remainder port (src/gateway/platforms/discord_adapter_flow11.c).
 *
 * Reads JSON LINES: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func","in","out"} where out captures the ordered seam
 * events {"name","args","ret"} in "trace", the final return ("ret") and an
 * always-empty "state". The echoed "in" is the ORIGINAL input (copied BEFORE
 * dispatch).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result (plain JSON value,
 * an ordered LIST of results, or {"__script__":true,"ret":... /
 * "raise":{type,message}}). A missing/exhausted script is a fail-closed
 * RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/discord_adapter_flow11.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da11_fn_t)(da11_ctx_t *, const json_t *);
typedef struct { const char *func; da11_fn_t fn; } da11_case_t;
static const da11_case_t CASES[] = {
    { "_edit_overflow_split", da11_edit_overflow_split },
    { "_ensure_missed_message_backfill_task", da11_ensure_missed_message_backfill_task },
    { "_fetch_channel_context", da11_fetch_channel_context },
    { "_finish_recovery_scan", da11_finish_recovery_scan },
    { "_forum_post_file", da11_forum_post_file },
    { "_get_ambient_pcm", da11_get_ambient_pcm },
    { "_handle_bot_task_done", da11_handle_bot_task_done },
    { "_handle_message", da11_handle_message },
    { "_handle_thread_create_slash", da11_handle_thread_create_slash },
    { "_infer_user_for_ssrc", da11_infer_user_for_ssrc },
    { "_install_speaking_hook", da11_install_speaking_hook },
    { "_install_voice_mixer", da11_install_voice_mixer },
    { "_is_allowed_user", da11_is_allowed_user },
    { "_bot_tag_debounce_key", da11_bot_tag_debounce_key },
    { "_command_sync_state_key", da11_command_sync_state_key },
    { "_discord_ready_timeout_seconds", da11_discord_ready_timeout_seconds },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *da11h_read_line(FILE *f) {
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
    while ((line = da11h_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        size_t llen = strlen(line);
        while (llen && (line[llen - 1] == '\r' || line[llen - 1] == '\n')) line[--llen] = '\0';
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const da11_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            da11_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", json_object());
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", echo_in);
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}
