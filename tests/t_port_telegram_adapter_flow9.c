/* t_port_telegram_adapter_flow9.c -- harness for the Telegram adapter FLOW
 * part 9 port (src/gateway/platforms/telegram_adapter_flow9.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events ("trace"), the final return ("ret"), and the state echo ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the LIVE
 * reference from it).
 *
 * Scripted I/O lives in in.io: seam-name -> result (plain value, list consumed
 * per call, or {"__script__":true,"ret":...,"raise":{...}}).  in.state carries
 * the attribute-double fields.
 */
#include "gateway/platforms/telegram_adapter_flow9.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ta9_fn_t)(ta9_ctx_t *, const json_t *);
typedef struct { const char *func; ta9_fn_t fn; } ta9_case_t;

static const ta9_case_t CASES[] = {
    { "_bold_label_html", ta9_bold_label_html },
    { "_append_observed_note", ta9_append_observed_note },
    { "_EA_HEADER", ta9_ea_header },
    { "_EA_REASON_LABEL", ta9_ea_reason_label },
    { "_ea_escape", ta9_ea_escape },
    { "_EA_ACTION_LABELS", ta9_ea_action_labels },
    { "_EA_SMART_DENY_LINE", ta9_ea_smart_deny_line },
    { "_provider_list_text", ta9_provider_list_text },
    { "_provider_button", ta9_provider_button },
    { "_picker_back_cancel_row", ta9_picker_back_cancel_row },
    { "_picker_nav_row", ta9_picker_nav_row },
    { "_teardown_started", ta9_teardown_started },
    { "send_path_degraded", ta9_send_path_degraded },
    { "_callback_ctx", ta9_callback_ctx },
    { "supports_draft_streaming", ta9_supports_draft_streaming },
    { "_fallback_ips", ta9_fallback_ips },
    { "_link_preview_kwargs", ta9_link_preview_kwargs },
    { "_build_adapter", ta9_build_adapter },
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

        const ta9_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ta9_ctx_t ctx;
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
