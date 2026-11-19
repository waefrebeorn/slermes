/* t_port_whatsapp_adapter_flow.c — harness for the WhatsApp adapter
 * bridge-lifecycle FLOW port
 * (src/gateway/platforms/whatsapp_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and state (always {} — every observable effect in this
 * cluster rides the seam trace). The echoed "in" is the ORIGINAL input
 * (copied BEFORE dispatch); the oracle re-drives the live reference from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/whatsapp_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*wa_fn_t)(wa_ctx_t *, const json_t *);
typedef struct { const char *func; wa_fn_t fn; } wa_case_t;
static const wa_case_t CASES[] = {
    { "_safe_ints", wa__safe_ints },
    { "_fmt_uptime", wa__fmt_uptime },
    { "_bridge_auth_headers", wa__bridge_auth_headers },
    { "_bridge_media_type", wa__bridge_media_type },
    { "_bridge_auth_proof", wa__bridge_auth_proof },
    { "_bridge_config_fingerprint", wa__bridge_config_fingerprint },
    { "_classify_bridge_message", wa__classify_bridge_message },
    { "_wenv", wa__wenv },
    { "_is_connected", wa__is_connected },
    { "_cache_dirs", wa__cache_dirs },
    { "_unlink_quietly", wa__unlink_quietly },
    { "_tighten_bridge_log", wa__tighten_bridge_log },
    { "_read_bridge_token", wa__read_bridge_token },
    { "_write_bridge_fingerprint", wa__write_bridge_fingerprint },
    { "_write_bridge_pidfile", wa__write_bridge_pidfile },
    { "_read_bridge_pidfile", wa__read_bridge_pidfile },
    { "_bridge_pid_is_ours", wa__bridge_pid_is_ours },
    { "_pids_alive", wa__pids_alive },
    { "_wait_pids_exit", wa__wait_pids_exit },
    { "_pid_looks_like_node_bridge", wa__pid_looks_like_node_bridge },
    { "_windows_listener_pids", wa__windows_listener_pids },
    { "_listener_pids_on_port", wa__listener_pids_on_port },
    { "_port_listener_pids", wa__port_listener_pids },
    { "_kill_port_process", wa__kill_port_process },
    { "_kill_stale_bridge_by_pidfile", wa__kill_stale_bridge_by_pidfile },
    { "check_whatsapp_requirements", wa_check_whatsapp_requirements },
    { "_file_content_hash", wa__file_content_hash },
    { "_bridge_source_hash", wa__bridge_source_hash },
    { "_spawn_bridge_process", wa__spawn_bridge_process },
    { "_normalize_outbound_mentions", wa__normalize_outbound_mentions },
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

        const wa_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            wa_ctx_t ctx;
            ctx.state = json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", json_object());
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
