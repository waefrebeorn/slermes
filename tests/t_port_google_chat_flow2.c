/* t_port_google_chat_flow2.c — harness for the Google Chat adapter third-wave
 * FLOW port (src/gateway/platforms/google_chat_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {...}, "io": {...}}}. Emits JSON LINES: {"func", "in", "out"}
 * where out captures the decision SEQUENCE (ordered seam events
 * {"name","args","ret"} in "trace"), the final return ("ret"), and the
 * mutated adapter state ("state"). The echoed "in" is the ORIGINAL input
 * (copied BEFORE dispatch); the oracle re-drives the live reference from it.
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
#include "gateway/platforms/google_chat_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*gc2_fn_t)(gc2_ctx_t *, const json_t *);
typedef struct { const char *func; gc2_fn_t fn; } gc2_case_t;

static const gc2_case_t CASES[] = {
    { "format_message", gc2_format_message },
    { "_verify_google_id_token", gc2__verify_google_id_token },
    { "_get_google_id_token_request", gc2__get_google_id_token_request },
    { "_load_google_modules", gc2__load_google_modules },
    { "send_animation", gc2_send_animation },
    { "send_document", gc2_send_document },
    { "send_image_file", gc2_send_image_file },
    { "send_video", gc2_send_video },
    { "send_voice", gc2_send_voice },
    { "_send_file_reply", gc2__send_file_reply },
    { "_new_authed_http", gc2__new_authed_http },
    { "_handle_setup_files_command", gc2__handle_setup_files_command },
    { "_refresh_or_none", gc2__refresh_or_none },
    { "_patch_quietly", gc2__patch_quietly },
    { "_on_pubsub_message", gc2__on_pubsub_message },
    { "_post_attachment_fallback", gc2__post_attachment_fallback },
    { "_consume_typing_card_with_text", gc2__consume_typing_card_with_text },
    { "_acquire_user_chat_api", gc2__acquire_user_chat_api },
    { "_load_per_user_chat_api", gc2__load_per_user_chat_api },
    { "_submit_on_loop", gc2__submit_on_loop },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

/* Keys the oracle seeds into EVERY case's state. Must match drive()'s
 * state.setdefault list exactly (null for scalars, {} for dicts). */
static void gc2_seed_state(json_t *state) {
    static const char *dicts[] = {
        "_typing_messages", "_user_chat_api_by_email", "_user_creds_by_email"
    };
    for (size_t i = 0; i < sizeof(dicts) / sizeof(dicts[0]); i++)
        if (!json_has(state, dicts[i]))
            json_set(state, dicts[i], json_object());
    static const char *attrs[] = {
        "_user_chat_api", "_user_credentials", "_credentials", "_scope_ctx",
        "_loop", "_google_id_token_request", "GOOGLE_CHAT_AVAILABLE",
        "_google_modules_loaded", "httplib2", "pubsub_v1", "gax_exceptions",
        "service_account", "AuthorizedHttp", "build_service", "HttpError",
        "MediaFileUpload"
    };
    for (size_t i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++)
        if (!json_has(state, attrs[i]))
            json_set(state, attrs[i], json_null());
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

        const gc2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            gc2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            gc2_seed_state(ctx.state);
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
