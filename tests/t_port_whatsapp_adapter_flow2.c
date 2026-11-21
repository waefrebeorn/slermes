/* t_port_whatsapp_adapter_flow2.c -- harness for the WhatsApp adapter
 * session/token/auth/env FLOW remainder port
 * (src/gateway/platforms/whatsapp_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and state: method targets materialize args.self into
 * ctx.state["self"] (mutated in place, echoed on success AND raise); module
 * targets emit {}. The echoed "in" is the ORIGINAL input (copied BEFORE
 * dispatch); the oracle re-drives the live reference from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/whatsapp_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*wa2_fn_t)(wa2_ctx_t *, const json_t *);
typedef struct { const char *func; wa2_fn_t fn; int is_method; } wa2_case_t;
static const wa2_case_t CASES[] = {
    { "_session_dir", wa2__session_dir, 0 },
    { "_owner_only_opener", wa2__owner_only_opener, 0 },
    { "_open_bridge_log", wa2__open_bridge_log, 0 },
    { "_terminate_bridge_process", wa2__terminate_bridge_process, 0 },
    { "_is_allowed_bridge_path", wa2__is_allowed_bridge_path, 0 },
    { "_bridge_token_lock", wa2__bridge_token_lock, 0 },
    { "_load_or_create_bridge_token", wa2__load_or_create_bridge_token, 0 },
    { "_import_aiohttp", wa2__import_aiohttp, 0 },
    { "ensure_aiohttp", wa2_ensure_aiohttp, 0 },
    { "_needs_bridge", wa2__needs_bridge, 0 },
    { "_bridge_req", wa2__bridge_req, 1 },
    { "_authenticated_bridge_health", wa2__authenticated_bridge_health, 0 },
    { "_bridge_package_hash", wa2__bridge_package_hash, 1 },
    { "_bridge_env", wa2__bridge_env, 1 },
    { "_adopted_config_matches", wa2__adopted_config_matches, 1 },
    { "_bridge_died", wa2__bridge_died, 1 },
    { "_close_bridge_log", wa2__close_bridge_log, 1 },
    { "_terminate_bridge", wa2__terminate_bridge, 1 },
    { "_check_managed_bridge_exit", wa2__check_managed_bridge_exit, 1 },
    { "_inject_document_text", wa2__inject_document_text, 1 },
    { "_quoted_media", wa2__quoted_media, 1 },
    { "_post_bridge_message", wa2__post_bridge_message, 0 },
    { "_apply_yaml_config", wa2__apply_yaml_config, 0 },
    { "_may_transcribe_at_gate", wa2__may_transcribe_at_gate, 1 },
    { "_log_delivery_status", wa2__log_delivery_status, 1 },
    /* extension wave: send-family + lifecycle leaves */
    { "_bridge_unavailable", wa2__bridge_unavailable, 1 },
    { "_report_bridge_exit", wa2__report_bridge_exit, 1 },
    { "send_image_file", wa2_send_image_file, 1 },
    { "send_video", wa2_send_video, 1 },
    { "send_voice", wa2_send_voice, 1 },
    { "send_document", wa2_send_document, 1 },
    { "send_poll", wa2_send_poll, 1 },
    { "send_location", wa2_send_location, 1 },
    { "edit_message", wa2_edit_message, 1 },
    { "_send_media_to_bridge", wa2__send_media_to_bridge, 1 },
    { "_send_read_receipt", wa2__send_read_receipt, 1 },
    { "get_chat_info", wa2_get_chat_info, 1 },
    /* extension wave 2: unclaimed defs */
    { "_bridge_url", wa2__bridge_url, 1 },
    { "send_clarify", wa2_send_clarify, 1 },
    { "send_image", wa2_send_image, 1 },
    { "_wait_for_bridge", wa2__wait_for_bridge, 1 },
    { "_attach_to_bridge", wa2__attach_to_bridge, 1 },
    { "send_typing", wa2_send_typing, 1 },
    { "_probe_bridge_health", wa2__probe_bridge_health, 1 },
    { "send", wa2_send, 1 },
    { "interactive_setup", wa2_interactive_setup, 0 },
    { "_preflight", wa2__preflight, 1 },
    { "_unidentified_holder_remains", wa2__unidentified_holder_remains, 1 },
    { "_wait_bridge_port_free", wa2__wait_bridge_port_free, 1 },
    /* final whatsapp remainder: 12 more unclaimed defs */
    { "__init__", wa2___init__, 1 },
    { "_build_message_event", wa2__build_message_event, 1 },
    { "_collect_bridge_media", wa2__collect_bridge_media, 1 },
    { "_ensure_bridge_deps", wa2__ensure_bridge_deps, 1 },
    { "_mint_bridge_token", wa2__mint_bridge_token, 1 },
    { "_poll_bridge_health", wa2__poll_bridge_health, 1 },
    { "_poll_messages", wa2__poll_messages, 1 },
    { "_process_inbound", wa2__process_inbound, 1 },
    { "_reuse_running_bridge", wa2__reuse_running_bridge, 1 },
    { "_spoken_mention_clips", wa2__spoken_mention_clips, 1 },
    { "disconnect", wa2_disconnect, 1 },
    { "register", wa2_register, 0 },
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
        /* echo the ORIGINAL input before dispatch */
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const wa2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            wa2_ctx_t ctx;
            ctx.state = json_object();
            if (tc->is_method) {
                const json_t *slf = args ? json_obj_get(args, "self") : NULL;
                json_set(ctx.state, "self",
                         (slf && slf->type == JSON_OBJECT) ? json_copy(slf) : json_object());
            }
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