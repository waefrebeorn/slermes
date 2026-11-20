/* t_port_google_chat_adapter_flow2.c — harness for the Google Chat adapter
 * remainder FLOW port (src/gateway/platforms/google_chat_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {...}, "io": {...}}}. Emits JSON LINES: {"func", "in", "out"}
 * where out captures the decision SEQUENCE (ordered seam events
 * {"name","args","ret"} in "trace"), the final return ("ret"), and the
 * mutated adapter / store state ("state"). The echoed "in" is the ORIGINAL
 * input (copied BEFORE dispatch); the oracle re-drives the live reference
 * from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order (wrap list/tuple RESULTS one level deeper), or
 * {"__script__": true, "ret": <value>, "raise": {type,...}}. A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/google_chat_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*gca2_fn_t)(gca2_ctx_t *, const json_t *);
typedef struct { const char *func; gca2_fn_t fn; } gca2_case_t;

static const gca2_case_t CASES[] = {
    { "_load_sa_credentials_from", gca2__load_sa_credentials_from },
    { "store.load", gca2_store_load },
    { "store.get", gca2_store_get },
    { "store.incr", gca2_store_incr },
    { "store.decr", gca2_store_decr },
    { "store._save", gca2_store__save },
    { "_load_sa_credentials", gca2__load_sa_credentials },
    { "_bot_id_cache_path", gca2__bot_id_cache_path },
    { "_save_cached_bot_id", gca2__save_cached_bot_id },
    { "_resolve_bot_user_id", gca2__resolve_bot_user_id },
    { "_load_user_oauth", gca2__load_user_oauth },
    { "_check_subscription", gca2__check_subscription },
    { "_handle_pubsub_message", gca2__handle_pubsub_message },
    { "dispatch_http_event", gca2_dispatch_http_event },
    { "_dispatch_message", gca2__dispatch_message },
    { "_build_message_event", gca2__build_message_event },
    { "_download_attachment", gca2__download_attachment },
    { "send", gca2_send },
    { "send_card", gca2_send_card },
    { "edit_message", gca2_edit_message },
    { "delete_message", gca2_delete_message },
    { "_patch_message", gca2__patch_message },
    { "_create_message", gca2__create_message },
    { "_call_with_retry", gca2__call_with_retry },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

/* setup_case/drive mirror: the oracle seeds the import-time globals into
 * EVERY case's state (state.setdefault(key, {})) and drive() syncs the six
 * adapter attrs (null when absent) for non-store targets. Seed BEFORE
 * dispatch so untouched globals still appear in out.state. */
static void gca2_seed_state(json_t *state, const char *func) {
    static const char *dicts[] = {
        "_counts", "_typing_messages", "_last_sender_by_chat", "_last_inbound_thread"
    };
    for (size_t i = 0; i < sizeof(dicts) / sizeof(dicts[0]); i++)
        if (!json_has(state, dicts[i]))
            json_set(state, dicts[i], json_object());
    if (strncmp(func, "store.", 6) != 0) {
        static const char *attrs[] = {
            "_shutting_down", "_debug_raw", "_bot_user_id",
            "_user_credentials", "_user_chat_api", "_subscriber"
        };
        for (size_t i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++)
            if (!json_has(state, attrs[i]))
                json_set(state, attrs[i], json_null());
    }
}

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
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const gca2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            gca2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            gca2_seed_state(ctx.state, func);
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