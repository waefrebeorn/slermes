/* t_port_feishu_adapter_flow6.c — harness for the Feishu adapter resolution
 * helpers FLOW port (src/gateway/platforms/feishu_adapter_flow6.c).
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
#include "gateway/platforms/feishu_adapter_flow6.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*fa6_fn_t)(fa6_ctx_t *, const json_t *);
typedef struct { const char *func; fa6_fn_t fn; } fa6_case_t;
static const fa6_case_t CASES[] = {
    { "_is_connected", fa6__is_connected },
    { "_normalize_media_type", fa6_normalize_media_type },
    { "_default_image_media_type", fa6_default_image_media_type },
    { "_map_chat_type", fa6_map_chat_type },
    { "_resolve_source_chat_type", fa6_resolve_source_chat_type },
    { "_guess_remote_extension", fa6_guess_remote_extension },
    { "_display_name_from_cached_path", fa6_display_name_from_cached_path },
    { "_get_response_header", fa6_get_response_header },
    { "_extract_text_from_raw_content", fa6_extract_text_from_raw_content },
    { "_guess_extension", fa6_guess_extension },
    { "_guess_document_media_type", fa6_guess_document_media_type },
    { "_guess_media_type_from_filename", fa6_guess_media_type_from_filename },
    { "_derive_remote_filename", fa6_derive_remote_filename },
    { "_log_background_failure", fa6_log_background_failure },
    { "_is_webhook_signature_valid", fa6_is_webhook_signature_valid },
    { "_namespace_from_mapping", fa6_namespace_from_mapping },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *fa6h_read_line(FILE *f) {
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
    while ((line = fa6h_read_line(stdin)) != NULL) {
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

        const fa6_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            fa6_ctx_t ctx;
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
