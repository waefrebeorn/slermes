/* t_port_feishu_adapter_flow7.c — harness for the Feishu adapter proxy /
 * onboarding / registration / reaction / batch-helper FLOW port
 * (src/gateway/platforms/feishu_adapter_flow7.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
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
#include "gateway/platforms/feishu_adapter_flow7.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*fa7_fn_t)(fa7_ctx_t *, const json_t *);
typedef struct { const char *func; fa7_fn_t fn; } fa7_case_t;
static const fa7_case_t CASES[] = {
    { "__getattr__", fa7__getattr__ },
    { "__repr__", fa7__repr__ },
    { "_accounts_base_url", fa7__accounts_base_url },
    { "_onboard_open_base_url", fa7__onboard_open_base_url },
    { "_parse_bot_response", fa7__parse_bot_response },
    { "_reactions_enabled", fa7_reactions_enabled },
    { "_text_batch_is_compatible", fa7_text_batch_is_compatible },
    { "_begin_registration", fa7__begin_registration },
    { "_init_registration", fa7__init_registration },
    { "_apply_yaml_config", fa7__apply_yaml_config },
    { "_build_onboard_client", fa7__build_onboard_client },
    { "probe_bot", fa7_probe_bot },
    { "_add_reaction", fa7__add_reaction },
    { "_remove_reaction", fa7__remove_reaction },
    { "_media_batch_key", fa7__media_batch_key },
    { "_media_batch_is_compatible", fa7__media_batch_is_compatible },
    { "_get_cached_sender_name", fa7__get_cached_sender_name },
    { "_probe_bot_sdk", fa7__probe_bot_sdk },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *fa7h_read_line(FILE *f) {
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
    while ((line = fa7h_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        /* tolerate CRLF fixture lines */
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

        const fa7_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            fa7_ctx_t ctx;
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
