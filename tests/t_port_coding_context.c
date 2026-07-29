/*
 * t_port_coding_context.c — verification harness for
 * src/agent/coding_context.c (agent/coding_context.py).
 *
 * Exercises each oracle-tested port with a JSON array of cases
 * from argv[1]. The Python oracle (tests/sta_oracle_coding_context.py)
 * recomputes the same logic from the live Python source; the runner
 * diffs the two outputs byte-for-byte.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "coding_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Emit the result of coding_context_detect_profile_name for a
 * given (mode, platform, cwd) triple. Returns a json_t object
 * with "fn" and "out" keys. */
static json_t *emit_detect_profile(const json_t *c)
{
    const char *mode = json_get_str(c, "mode", "auto");
    const char *platform = json_get_str(c, "platform", "");
    const char *cwd = json_get_str(c, "cwd", ".");
    const char *out = coding_context_detect_profile_name(mode, platform, cwd);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("detect_profile_name"));
    json_set(o, "out", json_string(out ? out : ""));
    return o;
}

/* Emit the result of coding_context_resolve_mode for a config path. */
static json_t *emit_resolve_mode(const json_t *c)
{
    const char *config_path = json_get_str(c, "config_path", "");
    hermes_config_t cfg = {0};
    if (config_path[0]) {
        hermes_config_load(&cfg, config_path);
    } else {
        hermes_config_defaults(&cfg);
    }
    const char *out = coding_context_resolve_mode(&cfg);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("resolve_mode"));
    json_set(o, "out", json_string(out ? out : ""));
    return o;
}

/* Emit the result of coding_context_find_git_root for a given cwd. */
static json_t *emit_find_git_root(const json_t *c)
{
    const char *cwd = json_get_str(c, "cwd", ".");
    char out[HERMES_PATH_MAX];
    bool found = coding_context_find_git_root(cwd, out, sizeof(out));
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("find_git_root"));
    json_set(o, "found", json_bool(found));
    json_set(o, "root", json_string(found ? out : ""));
    return o;
}

/* Emit the result of coding_context_find_marker_root for a given cwd. */
static json_t *emit_find_marker_root(const json_t *c)
{
    const char *cwd = json_get_str(c, "cwd", ".");
    char out[HERMES_PATH_MAX];
    bool found = coding_context_find_marker_root(cwd, out, sizeof(out));
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("find_marker_root"));
    json_set(o, "found", json_bool(found));
    json_set(o, "root", json_string(found ? out : ""));
    return o;
}

/* Emit the result of coding_context_coding_system_blocks for a
 * given (platform, cwd, model). */
static json_t *emit_coding_system_blocks(const json_t *c)
{
    const char *platform = json_get_str(c, "platform", "");
    const char *cwd = json_get_str(c, "cwd", ".");
    const char *model = json_get_str(c, "model", "");
    int count = 0;
    char **blocks = coding_context_coding_system_blocks(platform, cwd, NULL, model, &count);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("coding_system_blocks"));
    json_set(o, "count", json_number(count));
    if (blocks && count > 0) {
        json_t *arr = json_new_array();
        for (int i = 0; i < count; i++) {
            json_array_append(arr, json_string(blocks[i] ? blocks[i] : ""));
        }
        json_set(o, "blocks", arr);
        coding_runtime_mode_free_blocks(blocks, count);
    } else {
        json_set(o, "blocks", json_new_array());
    }
    return o;
}

/* Emit the result of coding_context_build_workspace_block for cwd. */
static json_t *emit_build_workspace_block(const json_t *c)
{
    const char *cwd = json_get_str(c, "cwd", ".");
    char *out = coding_context_build_workspace_block(cwd);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("build_workspace_block"));
    json_set(o, "out", json_string(out ? out : ""));
    free(out);
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

        if      (strcmp(op, "detect_profile_name")   == 0) o = emit_detect_profile(c);
        else if (strcmp(op, "resolve_mode")          == 0) o = emit_resolve_mode(c);
        else if (strcmp(op, "find_git_root")         == 0) o = emit_find_git_root(c);
        else if (strcmp(op, "find_marker_root")      == 0) o = emit_find_marker_root(c);
        else if (strcmp(op, "coding_system_blocks")  == 0) o = emit_coding_system_blocks(c);
        else if (strcmp(op, "build_workspace_block") == 0) o = emit_build_workspace_block(c);
        else {
            o = json_new_object();
            json_set(o, "fn", json_string(op));
        }

        char *ser = json_serialize(o);
        printf("%s\n", ser);
        free(ser);
        json_free(o);
    }
    free(input);
    return 0;
}