/* t_port_buzz_adapter_flow2.c — harness for the Buzz adapter FLOW remainder
 * port (src/gateway/platforms/buzz_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {...self attrs...}, "io": {...seam scripts...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated self double ("state"."self"). The echoed
 * "in" is the ORIGINAL input (copied BEFORE dispatch, since the flows mutate
 * ctx.state in place); the oracle re-drives the live reference from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State emission flags per case: bit0 = emit ctx.state as out.state.self.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/buzz_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ba2_fn_t)(ba2_ctx_t *, const json_t *);
typedef struct { const char *func; ba2_fn_t fn; int flags; } ba2_case_t;

static const ba2_case_t CASES[] = {
    { "_get_scoped_secret", ba2__get_scoped_secret, 0 },
    { "_escape_unresolved_presentation_mention", ba2__escape_unresolved_presentation_mention, 0 },
    { "_safe_attachment_filename", ba2__safe_attachment_filename, 0 },
    { "_attachment_origin", ba2__attachment_origin, 0 },
    { "_effective_port", ba2__effective_port, 0 },
    { "_is_relay_media_url", ba2__is_relay_media_url, 0 },
    { "_find_relay_media_refs", ba2__find_relay_media_refs, 0 },
    { "_replace_media_refs", ba2__replace_media_refs, 0 },
    { "hex_to_npub", ba2__hex_to_npub, 0 },
    { "_resolve_cli_path", ba2__resolve_cli_path, 0 },
    { "_configured_cli_path", ba2__configured_cli_path, 0 },
    { "_resolve_auth_tag", ba2__resolve_auth_tag, 0 },
    { "_consume_ws_read_task", ba2__consume_ws_read_task, 0 },
    { "_parse_imeta_attachments", ba2__parse_imeta_attachments, 0 },
    { "_imeta_attachments", ba2__imeta_attachments, 0 },
    { "_websocket_url", ba2__websocket_url, 1 },
    { "_send_result", ba2__send_result, 1 },
    { "_connect_failed", ba2__connect_failed, 1 },
    { "_run_cli", ba2__run_cli, 1 },
    { "_cli_json", ba2__cli_json, 1 },
    { "send_reaction", ba2_send_reaction, 1 },
    { "edit_message", ba2_edit_message, 1 },
    { "delete_message", ba2_delete_message, 1 },
    { "get_chat_info", ba2_get_chat_info, 1 },
    { "_resolve_user_name", ba2__resolve_user_name, 1 },
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
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const ba2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *self_in = in ? json_obj_get(in, "state") : NULL;
            ba2_ctx_t ctx;
            ctx.state = (self_in && self_in->type == JSON_OBJECT) ? json_copy(self_in) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            if (tc->flags & 1) ba2_set_copy(state, "self", ctx.state);
            json_set(out, "state", state);
            json_free(ctx.cursors);
            json_free(ctx.state);
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