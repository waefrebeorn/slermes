/* t_port_discord_adapter_flow12.c — harness for the Discord adapter FLOW12
 * remainder port (src/gateway/platforms/discord_adapter_flow12.c).
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
#include "gateway/platforms/discord_adapter_flow12.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da12_fn_t)(da12_ctx_t *, const json_t *);
typedef struct { const char *func; da12_fn_t fn; } da12_case_t;
static const da12_case_t CASES[] = {
    { "_is_forum_parent", da12_is_forum_parent },
    { "_is_length_overflow_error", da12_is_length_overflow_error },
    { "_iter_channel_and_thread_messages", da12_iter_channel_and_thread_messages },
    { "_iter_missed_message_backfill_candidates", da12_iter_missed_message_backfill_candidates },
    { "_lead_silence_bytes", da12_lead_silence_bytes },
    { "_liveness_loop", da12_liveness_loop },
    { "_load", da12_load },
    { "_load_voice_fx_config", da12_load_voice_fx_config },
    { "_looks_like_nonconversational_history_message", da12_looks_like_nonconversational_history_message },
    { "_message_has_non_down_bot_response", da12_message_has_non_down_bot_response },
    { "_message_reference_from_ids", da12_message_reference_from_ids },
    { "_missed_message_backfill_channels", da12_missed_message_backfill_channels },
    { "_native_slash_commands", da12_native_slash_commands },
    { "_normalize_permissions", da12_normalize_permissions },
    { "_notify_liveness_fatal_error", da12_notify_liveness_fatal_error },
    { "_notify_unauthorized_slash", da12_notify_unauthorized_slash },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *da12h_read_line(FILE *f) {
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
    while ((line = da12h_read_line(stdin)) != NULL) {
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

        const da12_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            da12_ctx_t ctx;
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
