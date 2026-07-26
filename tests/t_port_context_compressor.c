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

/* New v671 cluster (skill-pruned-marker + summary-classification). */
char *context_compressor__skill_pruned_marker(const char *skill_name);
int  context_compressor__extract_pruned_skill_names(const char *text,
                                                    char **out_names, int *out_count,
                                                    int limit);
int  context_compressor__reinject_pruned_skill_markers(const char *summary,
                                                       const char **skill_names,
                                                       int skill_count, char **out);
int  context_compressor__strip_persistence_markers(json_t *messages);
json_t *context_compressor__fresh_compaction_message_copy(const json_t *msg);
int  context_compressor__has_compressed_summary_metadata(const json_t *message);
int  context_compressor__starts_with_summary_prefix(const char *text);
char *context_compressor__classify_summary_content(const char *content);
int  context_compressor__is_context_summary_content(const char *content);
int  context_compressor__is_compaction_summary_message(const json_t *message);

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

/* ── v671 cluster emitters ─────────────────────────────────────────────── */

static json_t *emit_skill_pruned_marker(const json_t *c)
{
    const char *name = json_get_str(c, "name", "");
    char *m = context_compressor__skill_pruned_marker(name);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("skill_pruned_marker"));
    json_set(o, "out", json_string(m ? m : ""));
    free(m);
    return o;
}

static json_t *emit_extract_pruned_skill_names(const json_t *c)
{
    const char *text = json_get_str(c, "text", "");
    char *names[64];
    int count = 0;
    context_compressor__extract_pruned_skill_names(text, names, &count, 64);
    json_t *arr = json_new_array();
    for (int i = 0; i < count; i++) {
        json_array_append(arr, json_string(names[i]));
        free(names[i]);
    }
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("extract_pruned_skill_names"));
    json_set(o, "out", arr);
    return o;
}

static json_t *emit_reinject_pruned_skill_markers(const json_t *c)
{
    const char *summary = json_get_str(c, "summary", "");
    json_t *skills = json_obj_get(c, "skills");
    int n = skills ? (int)json_len(skills) : 0;
    const char **sks = NULL;
    if (n > 0) {
        sks = (const char **)malloc(sizeof(char *) * (size_t)n);
        for (int i = 0; i < n; i++) {
            const json_t *s = json_get(skills, i);
            sks[i] = (s && s->type == JSON_STRING) ? json_string_value(s) : "";
        }
    }
    char *out = NULL;
    context_compressor__reinject_pruned_skill_markers(summary, sks, n, &out);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("reinject_pruned_skill_markers"));
    json_set(o, "out", json_string(out ? out : ""));
    free(out);
    free(sks);
    return o;
}

static json_t *emit_strip_persistence_markers(const json_t *c)
{
    json_t *messages = json_obj_get(c, "messages");
    if (messages) {
        /* deep-ish copy so the harness fixture is not mutated across cases */
        char *ser = json_serialize(messages);
        json_t *copy = ser ? json_parse(ser, NULL) : NULL;
        free(ser);
        /* Python's _strip_persistence_markers is a mutating helper that returns
         * None; mirror that by emitting rc:null. */
        context_compressor__strip_persistence_markers(copy);
        json_t *o = json_new_object();
        json_set(o, "fn", json_string("strip_persistence_markers"));
        json_set(o, "rc", json_null());
        json_set(o, "out", copy ? copy : json_new_array());
        return o;
    }
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("strip_persistence_markers"));
    json_set(o, "rc", json_null());
    json_set(o, "out", json_new_array());
    return o;
}

static json_t *emit_fresh_compaction_message_copy(const json_t *c)
{
    json_t *msg = json_obj_get(c, "message");
    json_t *copy = context_compressor__fresh_compaction_message_copy(msg);
    char *ser = copy ? json_serialize(copy) : strdup("null");
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("fresh_compaction_message_copy"));
    json_set(o, "out", json_string(ser ? ser : ""));
    free(ser);
    if (copy) json_free(copy);
    return o;
}

static json_t *emit_has_compressed_summary_metadata(const json_t *c)
{
    json_t *msg = json_obj_get(c, "message");
    int r = context_compressor__has_compressed_summary_metadata(msg);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("has_compressed_summary_metadata"));
    json_set(o, "out", json_new_bool(r));
    return o;
}

static json_t *emit_starts_with_summary_prefix(const json_t *c)
{
    const char *text = json_get_str(c, "text", "");
    int r = context_compressor__starts_with_summary_prefix(text);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("starts_with_summary_prefix"));
    json_set(o, "out", json_new_bool(r));
    return o;
}

static json_t *emit_classify_summary_content(const json_t *c)
{
    const char *text = json_get_str(c, "text", "");
    char *r = context_compressor__classify_summary_content(text);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("classify_summary_content"));
    json_set(o, "out", json_string(r ? r : "null"));
    free(r);
    return o;
}

static json_t *emit_is_context_summary_content(const json_t *c)
{
    const char *text = json_get_str(c, "text", "");
    int r = context_compressor__is_context_summary_content(text);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("is_context_summary_content"));
    json_set(o, "out", json_new_bool(r));
    return o;
}

static json_t *emit_is_compaction_summary_message(const json_t *c)
{
    json_t *msg = json_obj_get(c, "message");
    int r = context_compressor__is_compaction_summary_message(msg);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("is_compaction_summary_message"));
    json_set(o, "out", json_new_bool(r));
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
        else if (strcmp(op, "skill_pruned_marker") == 0)            o = emit_skill_pruned_marker(c);
        else if (strcmp(op, "extract_pruned_skill_names") == 0)     o = emit_extract_pruned_skill_names(c);
        else if (strcmp(op, "reinject_pruned_skill_markers") == 0)  o = emit_reinject_pruned_skill_markers(c);
        else if (strcmp(op, "strip_persistence_markers") == 0)      o = emit_strip_persistence_markers(c);
        else if (strcmp(op, "fresh_compaction_message_copy") == 0)   o = emit_fresh_compaction_message_copy(c);
        else if (strcmp(op, "has_compressed_summary_metadata") == 0) o = emit_has_compressed_summary_metadata(c);
        else if (strcmp(op, "starts_with_summary_prefix") == 0)      o = emit_starts_with_summary_prefix(c);
        else if (strcmp(op, "classify_summary_content") == 0)        o = emit_classify_summary_content(c);
        else if (strcmp(op, "is_context_summary_content") == 0)      o = emit_is_context_summary_content(c);
        else if (strcmp(op, "is_compaction_summary_message") == 0)   o = emit_is_compaction_summary_message(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }

        char *ser = json_serialize(o);
        printf("%s\n", ser);
        free(ser);
        json_free(o);
    }
    free(input);
    return 0;
}
