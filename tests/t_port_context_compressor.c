/*
 * t_port_context_compressor.c — faithful verification harness for the
 * context_compressor pure helpers in src/agent/context.c
 * (agent/context_compressor.py).
 *
 * Reads a JSON array fixture from argv[1]; each element has an "op" that
 * selects one of the four helper ports:
 *   extract_name_args  -> {"tool_call": <obj>}  -> {"name","args"}
 *   extract_id         -> {"tool_call": <obj>}  -> {"out"}
 *   content_text       -> {"content": <val>}    -> {"out"}
 *   append_text        -> {"content": <val>, "text": <str>, "prepend": <bool>} -> {"out"}
 * The Python oracle (tests/sta_oracle_context_compressor.py) recomputes the
 * same helpers from LIVE agent/context_compressor.py; the runner diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations — these helpers live in src/agent/context.c (port of
 * agent/context_compressor.py). We avoid pulling hermes_agent.h (which drags
 * in libdb) so the oracle harness links with the minimal include set. */
void  context_compressor_extract_name_args(const json_t *tool_call,
                                            char **name_out, char **args_out);
const char *context_compressor_extract_id(const json_t *tool_call);
char  *context_compressor_content_text(const json_t *content);
json_t *context_compressor_append_text(const json_t *content,
                                        const char *text, bool prepend);

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

static json_t *emit_name_args(const json_t *c)
{
    json_t *tc = json_obj_get(c, "tool_call");
    char *name = NULL, *args = NULL;
    context_compressor_extract_name_args(tc, &name, &args);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("extract_name_args"));
    json_set(o, "name", json_string(name ? name : ""));
    json_set(o, "args", json_string(args ? args : ""));
    free(name); free(args);
    return o;
}

static json_t *emit_id(const json_t *c)
{
    json_t *tc = json_obj_get(c, "tool_call");
    char *id = (char *)context_compressor_extract_id(tc);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("extract_id"));
    json_set(o, "out", json_string(id ? id : ""));
    free(id);
    return o;
}

static json_t *emit_content_text(const json_t *c)
{
    json_t *content = json_obj_get(c, "content");
    char *txt = context_compressor_content_text(content);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("content_text"));
    json_set(o, "out", json_string(txt ? txt : ""));
    free(txt);
    return o;
}

static json_t *emit_append_text(const json_t *c)
{
    json_t *content = json_obj_get(c, "content");
    const char *text = json_get_str(c, "text", "");
    bool prepend = json_get_bool(c, "prepend", false);
    json_t *res = context_compressor_append_text(content, text, prepend);
    char *ser = res ? json_serialize(res) : strdup("null");
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("append_text"));
    json_set(o, "out", json_string(ser ? ser : ""));
    free(ser);
    if (res) json_free(res);
    return o;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char *err = NULL;
    json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }

    int n = json_array_size(root);
    for (int i = 0; i < n; i++) {
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "extract_name_args") == 0) o = emit_name_args(c);
        else if (strcmp(op, "extract_id") == 0)        o = emit_id(c);
        else if (strcmp(op, "content_text") == 0)      o = emit_content_text(c);
        else if (strcmp(op, "append_text") == 0)       o = emit_append_text(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }

        char *ser = json_serialize(o);
        printf("%s\n", ser);
        free(ser);
        json_free(o);
    }
    free(input);
    return 0;
}
