/* t_port_matrix_adapter_flow6.c -- harness for the Matrix-adapter connect /
 * sync / inbound / media / markdown / crypto flow port
 * (src/gateway/platforms/matrix_adapter_flow6.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events {"name","args","ret"} in "trace", the final return ("ret") and the
 * state echo ("state"). The echoed "in" is the ORIGINAL input.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result (plain value, a
 * call-order LIST, or {"__script__":true,"ret"/"raise":...}). A missing or
 * exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (libjson only).
 */
#include "gateway/platforms/matrix_adapter_flow6.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ma6_fn_t)(ma6_ctx_t *, const json_t *);
typedef struct { const char *func; ma6_fn_t fn; } ma6_case_t;

static const ma6_case_t CASES[] = {
    { "_crypto_db_path", ma6_crypto_db_path },
    { "_abort_connect", ma6_abort_connect },
    { "_e2ee_setup_failed", ma6_e2ee_setup_failed },
    { "_connect_initial_sync", ma6_connect_initial_sync },
    { "_extract_reply_context", ma6_extract_reply_context },
    { "_check_e2ee_deps", ma6_check_e2ee_deps },
    { "_create_matrix_session", ma6_create_matrix_session },
    { "_dispatch_sync", ma6_dispatch_sync },
    { "_build_text_message_content", ma6_build_text_message_content },
    { "_absorb_sync", ma6_absorb_sync },
    { "_download_and_cache_media", ma6_download_and_cache_media },
    { "_background_read_receipt", ma6_background_read_receipt },
    { "_markdown_to_html", ma6_markdown_to_html },
    { "_migrate_legacy_crypto_pickle", ma6_migrate_legacy_crypto_pickle },
    { "_build_inbound_event", ma6_build_inbound_event },
    { "_handle_text_message", ma6_handle_text_message },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

/* Set-valued state keys emit as sorted arrays on both sides. */
static void ma6_sort_state_arrays(json_t *state) {
    static const char *KEYS[] = { "_joined_rooms", NULL };
    for (int k = 0; KEYS[k]; k++) {
        json_t *arr = json_obj_get(state, KEYS[k]);
        if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) continue;
        for (size_t i = 1; i < json_len(arr); i++) {
            json_t *v = json_get(arr, i);
            size_t j = i;
            while (j > 0) {
                json_t *prev = json_get(arr, j - 1);
                const char *a = prev && prev->type == JSON_STRING ? prev->str_val : "";
                const char *b = v && v->type == JSON_STRING ? v->str_val : "";
                if (strcmp(a, b) <= 0) break;
                json_array_set(arr, j, prev);
                j--;
            }
            json_array_set(arr, j, v);
        }
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
        json_t *out = json_object();

        const ma6_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ma6_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            ma6_sort_state_arrays(ctx.state);
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
