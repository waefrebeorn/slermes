/*
 * t_port_hermes_state.c — faithful verification harness for the pure,
 * DB-free helpers of agent/hermes_state.py, ported in
 * src/agent/hermes_state/hermes_state_gaps.c.
 *
 * Emits JSON lines consumed by tests/sta_oracle_hermes_state.py, which
 * recomputes the SAME functions from the LIVE hermes_state.py and asserts
 * exact equality.
 *
 * Tested functions (all DB-free / deterministic):
 *   is_disk_full_error(str)
 *   classify_persistence_error(str)
 *   _is_stale_tool_call_marker_message(msg)
 *   _strip_stale_tool_call_markers(messages)
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"

/* hermes_state_gaps.c declares these but has no public header — forward-declare. */
bool hermes_state_is_disk_full_error(const char *err_str);
const char *hermes_state_classify_persistence_error(const char *err_str);
bool hermes_state_is_stale_tool_call_marker(const json_t *msg);
int hermes_state_strip_stale_tool_call_markers(json_t *messages);

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

/* JSON-string escape a C string, returning a static buffer (for embedding in
 * an output line). 4 rotating slots so multiple calls in one printf work. */
static char *json_escaped_str(const char *s) {
    static char buf[4][8192];
    static int cur = 0;
    char *q = buf[cur];
    cur = (cur + 1) % 4;
    char *base = q;
    *q++ = '"';
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { *q++='\\'; *q++=c; }
            else if (c == '\n') { *q++='\\'; *q++='n'; }
            else if (c == '\t') { *q++='\\'; *q++='t'; }
            else if (c == '\r') { *q++='\\'; *q++='r'; }
            else if (c < 32) { *q++='\\'; *q++='u'; *q++='0'; *q++='0';
                *q++= (c>15?'1':'0'); *q++= (c%16<10? c%16+'0' : c%16-10+'a'); }
            else if ((q - base) < 8180) *q++ = c;
        }
    }
    *q++ = '"'; *q = '\0';
    return base;
}

/* Compatibility: print an escaped string as a bare JSON value. */
static void print_json_escaped(const char *s) {
    fputs(json_escaped_str(s), stdout);
}

#define print_json_escaped_str(s) json_escaped_str(s)

/* Serialize a json_t to a JSON string and print it (null-safe). */
static void print_json(const json_t *v) {
    if (!v) { printf("null\n"); return; }
    char *s = json_serialize(v);
    printf("%s\n", s ? s : "null");
    free(s);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: t_port_hermes_state <cases.in>\n");
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
        int arg_is_null = (arg && arg->type == JSON_NULL);

        if (strcmp(op, "is_disk_full_error") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            int out = hermes_state_is_disk_full_error(s) ? 1 : 0;
            printf("{\"fn\":\"is_disk_full_error\",\"arg\":%s,\"out\":%d}\n",
                   arg_is_null ? "null" : print_json_escaped_str(s), out);
        } else if (strcmp(op, "classify_persistence_error") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            const char *out = hermes_state_classify_persistence_error(s);
            printf("{\"fn\":\"classify\",\"arg\":%s,\"out\":%s}\n",
                   arg_is_null ? "null" : print_json_escaped_str(s),
                   print_json_escaped_str(out ? out : "unknown"));
        } else if (strcmp(op, "is_stale_marker") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            json_t *msg = s && *s ? json_parse(s, NULL) : NULL;
            int out = hermes_state_is_stale_tool_call_marker(msg) ? 1 : 0;
            printf("{\"fn\":\"is_stale\",\"arg\":%s,\"out\":%d}\n",
                   print_json_escaped_str(s), out);
            if (msg) json_free(msg);
        } else if (strcmp(op, "strip_stale") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            json_t *msgs = s && *s ? json_parse(s, NULL) : NULL;
            int n = 0;
            if (msgs && msgs->type == JSON_ARRAY) {
                n = hermes_state_strip_stale_tool_call_markers(msgs);
            }
            printf("{\"fn\":\"strip\",\"out_count\":%d,\"out\":", n);
            /* print_json emits "<json>\n"; drop the trailing newline so the
             * object close lands on the same line. */
            {
                char *ser = msgs ? json_serialize(msgs) : NULL;
                printf("%s}\n", ser ? ser : "null");
                free(ser);
            }
            if (msgs) json_free(msgs);
        } else {
            printf("{\"fn\":\"unknown\",\"out\":null}\n");
        }
    }

    json_free(cases);
    return 0;
}
