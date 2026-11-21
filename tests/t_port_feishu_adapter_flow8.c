/* t_port_feishu_adapter_flow8.c — harness for the Feishu adapter
 * card/payload-builder, lifecycle, batch-dispatch and connection-internals FLOW
 * port (src/gateway/platforms/feishu_adapter_flow8.c).
 *
 * Reads JSON LINES: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
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
#include "gateway/platforms/feishu_adapter_flow8.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*fa8_fn_t)(fa8_ctx_t *, const json_t *);
typedef struct { const char *func; fa8_fn_t fn; } fa8_case_t;
static const fa8_case_t CASES[] = {
    { "_build_update_prompt_card", fa8_build_update_prompt_card },
    { "_build_resolved_approval_card", fa8_build_resolved_approval_card },
    { "_build_resolved_update_prompt_card", fa8_build_resolved_update_prompt_card },
    { "_build_outbound_payload", fa8_build_outbound_payload },
    { "_build_media_post_payload", fa8_build_media_post_payload },
    { "_cancel_pending_tasks", fa8_cancel_pending_tasks },
    { "_disable_websocket_auto_reconnect", fa8_disable_websocket_auto_reconnect },
    { "_clear_webhook_anomaly", fa8_clear_webhook_anomaly },
    { "_dedup_persist_lock_or_create", fa8_dedup_persist_lock_or_create },
    { "_build_event_handler", fa8_build_event_handler },
    { "_dispatch_inbound_event", fa8_dispatch_inbound_event },
    { "_delayed_flush", fa8_delayed_flush },
    { "_dispatch_text_batch", fa8_dispatch_text_batch },
    { "_connect_with_retry", fa8_connect_with_retry },
    { "_connect_websocket", fa8_connect_websocket },
    { "_connect_webhook", fa8_connect_webhook },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *fa8h_read_line(FILE *f) {
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
    while ((line = fa8h_read_line(stdin)) != NULL) {
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

        const fa8_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            fa8_ctx_t ctx;
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
