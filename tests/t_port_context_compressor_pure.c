/*
 * t_port_context_compressor_pure.c — Oracle harness for context_compressor.py pure helpers.
 *
 * Tests all pure functions that are ported, regardless of which .c file they
 * live in. Uses the sibling's API for functions in context_compressor_pure.c
 * and port_context_compressor_ports.c, and the port_context_compressor_pure.c
 * API for the two functions defined there (cc_fresh_compaction_message_copy,
 * cc_template_visible_role).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libjson/json.h"
#include "port_context_compressor_pure.h"
#include "context_compressor_pure.h"

/* Forward declarations for functions in port_context_compressor_ports.c
 * (no public header yet — sibling port). */
char *cc_append_text_to_content(const char *content_json, const char *text, bool prepend);
char *cc_strip_image_parts_from_parts(const char *parts_json);
char *cc_truncate_tool_call_args_json(const char *args, long head_chars);

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static void print_json_escaped(const char *s) {
    /* Output raw JSON-encoded string (for embedding in JSON string context).
     * Escapes only C0 control + quote/backslash; passes UTF-8 through. */
    putchar('"');
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
            else if (c == '\n') fputs("\\n", stdout);
            else if (c == '\t') fputs("\\t", stdout);
            else if (c == '\r') fputs("\\r", stdout);
            else if (c < 32) printf("\\u%04x", c);
            else putchar(c);
        }
    }
    putchar('"');
}

static void print_str_array(char **arr, int count) {
    putchar('[');
    if (arr) {
        for (int i = 0; i < count; i++) {
            if (i > 0) putchar(',');
            print_json_escaped(arr[i]);
        }
    }
    printf("]\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: t_port_context_compressor_pure <cases.in>\n");
        return 1;
    }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err) { free(err); fprintf(stderr, "parse error\n"); return 1; }
    if (!cases || cases->type != JSON_ARRAY) { fprintf(stderr, "not array\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");
        json_t *arg = json_obj_get(c, "arg");
        const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";

        if (strcmp(op, "safe_int") == 0) {
            /* Python oracle passes the RAW arg string to _safe_int, so
             * int("3.14")/int("123abc") raise -> null. Wrap the raw string in
             * a JSON_STRING to replicate exactly; JSON numbers pass through. */
            json_t *v = NULL;
            if (arg && arg->type == JSON_STRING) v = json_string(arg->str_val);
            else v = arg; /* NULL, or a real JSON number/bool from fixture */
            int out;
            if (cc_safe_int(v, &out)) printf("%d\n", out);
            else printf("null\n");
            if (v && v != arg) json_free(v);
        } else if (strcmp(op, "skill_pruned_marker") == 0) {
            char *r = cc_skill_pruned_marker(s);
            print_json_escaped(r); printf("\n");
            free(r);
        } else if (strcmp(op, "extract_pruned_skill_names") == 0) {
            char *names_arr[64];
            int count = 0;
            int ret = cc_extract_pruned_skill_names(s, names_arr, &count, 64);
            (void)ret;
            print_str_array(names_arr, count);
        } else if (strcmp(op, "is_image_part") == 0) {
            json_t *part = s && *s ? json_parse(s, NULL) : NULL;
            printf("%s\n", cc_is_image_part(part) ? "true" : "false");
            if (part) json_free(part);
        } else if (strcmp(op, "content_has_images") == 0) {
            json_t *content = s && *s ? json_parse(s, NULL) : NULL;
            printf("%s\n", cc_content_has_images(content) ? "true" : "false");
            if (content) json_free(content);
        } else if (strcmp(op, "strip_images_from_content") == 0) {
            json_t *content = s && *s ? json_parse(s, NULL) : NULL;
            json_t *r = cc_strip_images_from_content(content);
            char *out = json_serialize(r);
            printf("%s\n", out ? out : "null");
            free(out);
            if (r && r != content) json_free(r);
            if (content) json_free(content);
        } else if (strcmp(op, "strip_image_parts_from_parts") == 0) {
            /* Sibling API: takes JSON string, returns char* */
            char *r = cc_strip_image_parts_from_parts(s);
            if (r) printf("%s\n", r);
            else printf("null\n");
            free(r);
        } else if (strcmp(op, "truncate_tool_call_args_json") == 0) {
            /* Sibling API: takes (args_str, head_chars) */
            json_t *arg_obj = s && *s ? json_parse(s, NULL) : NULL;
            const char *args = "";
            long head = 200;
            if (arg_obj) {
                json_t *args_val = json_obj_get(arg_obj, "args");
                json_t *head_val = json_obj_get(arg_obj, "head");
                if (args_val && args_val->type == JSON_STRING) args = args_val->str_val;
                if (head_val && head_val->type == JSON_NUMBER) head = (long)head_val->num_val;
            }
            char *r = cc_truncate_tool_call_args_json(args, head);
            print_json_escaped(r); printf("\n");
            free(r);
            if (arg_obj) json_free(arg_obj);
        } else if (strcmp(op, "append_text_to_content") == 0) {
            /* Sibling API: takes (content_json_str, text_str, prepend) */
            json_t *arg_obj = s && *s ? json_parse(s, NULL) : NULL;
            const char *content_json = "";
            const char *text = "";
            int prepend = 0;
            if (arg_obj) {
                json_t *content_val = json_obj_get(arg_obj, "content");
                if (content_val) content_json = json_serialize(content_val);
                json_t *text_val = json_obj_get(arg_obj, "text");
                if (text_val && text_val->type == JSON_STRING) text = text_val->str_val;
                json_t *prepend_val = json_obj_get(arg_obj, "prepend");
                if (prepend_val && prepend_val->type == JSON_BOOL && prepend_val->bool_val) prepend = 1;
            }
            char *r = cc_append_text_to_content(content_json, text, prepend);
            printf("%s\n", r);
            free(r);
            if (arg_obj) json_free(arg_obj);
        } else if (strcmp(op, "template_visible_role") == 0) {
            json_t *msg = s && *s ? json_parse(s, NULL) : NULL;
            const char *role = cc_template_visible_role(msg);
            if (role) printf("\"%s\"\n", role);
            else printf("null\n");
            if (msg) json_free(msg);
        } else if (strcmp(op, "fresh_compaction_message_copy") == 0) {
            json_t *msg = s && *s ? json_parse(s, NULL) : NULL;
            json_t *r = cc_fresh_compaction_message_copy(msg);
            char *out = json_serialize(r);
            printf("%s\n", out ? out : "null");
            free(out);
            if (r) json_free(r);
            if (msg) json_free(msg);
        } else {
            printf("UNKNOWN_OP\n");
        }
    }

    json_free(cases);
    return 0;
}
