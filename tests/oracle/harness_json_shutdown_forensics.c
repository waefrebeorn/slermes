/* JSON oracle harness for json_node_t-arg ports (shutdown_forensics + video_generation_tool).
 * Builds by linking the full slermes object closure (has libjson).
 * Usage: harness_json <func> <input.json>
 *   func in {context_as_json, format_context_for_log, normalize_reference_images}
 * Prints canonicalized JSON (or string) output to stdout.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *cli_gateway_shutdown_forensics_context_as_json(json_node_t *ctx);
extern char *cli_gateway_shutdown_forensics_format_context_for_log(json_node_t *ctx);
extern json_node_t *cli_tools_video_generation_tool__normalize_reference_images(json_node_t *value);
extern json_node_t *cli_tools_video_generation_tool__missing_provider_error(const char *configured);
extern json_node_t *cli_tools_yuanbao_tools_get_group_info(const char *group_code);

static char *canon_json(const char *s) {
    char *err = NULL;
    json_node_t *n = json_parse(s, &err);
    if (!n) { free(err); return strdup(""); }
    char *out = json_serialize(n);
    json_free(n);
    return out ? out : strdup("");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <func> <arg>\n", argv[0]); return 2; }
    const char *func = argv[1];
    const char *input = argv[2];
    char *raw = NULL;
    json_node_t *node_ret = NULL;
    if (strcmp(func, "context_as_json") == 0) {
        char *err = NULL;
        json_node_t *ctx = json_parse(input, &err);
        if (!ctx) { fprintf(stderr, "json_parse error: %s\n", err ? err : "?"); free(err); return 3; }
        raw = cli_gateway_shutdown_forensics_context_as_json(ctx);
        json_free(ctx);
    } else if (strcmp(func, "format_context_for_log") == 0) {
        char *err = NULL;
        json_node_t *ctx = json_parse(input, &err);
        if (!ctx) { fprintf(stderr, "json_parse error: %s\n", err ? err : "?"); free(err); return 3; }
        raw = cli_gateway_shutdown_forensics_format_context_for_log(ctx);
        json_free(ctx);
    } else if (strcmp(func, "normalize_reference_images") == 0) {
        char *err = NULL;
        json_node_t *ctx = json_parse(input, &err);
        if (!ctx) { fprintf(stderr, "json_parse error: %s\n", err ? err : "?"); free(err); return 3; }
        node_ret = cli_tools_video_generation_tool__normalize_reference_images(ctx);
        if (node_ret) { raw = json_serialize(node_ret); } else { raw = strdup("null"); }
        json_free(ctx);
    } else if (strcmp(func, "missing_provider_error") == 0) {
        /* input is a plain string (configured provider or empty) */
        node_ret = cli_tools_video_generation_tool__missing_provider_error(input && input[0] ? input : NULL);
        if (node_ret) { raw = json_serialize(node_ret); } else { raw = strdup("null"); }
    } else if (strcmp(func, "get_group_info") == 0) {
        /* input is a plain string (group_code or empty) */
        node_ret = cli_tools_yuanbao_tools_get_group_info(input && input[0] ? input : NULL);
        if (node_ret) { raw = json_serialize(node_ret); } else { raw = strdup("null"); }
    } else {
        fprintf(stderr, "unknown func %s\n", func); return 4;
    }
    if (node_ret) json_free(node_ret);
    if (!raw) { printf("(null)\n"); return 0; }
    if (strcmp(func, "context_as_json") == 0 || strcmp(func, "normalize_reference_images") == 0
        || strcmp(func, "missing_provider_error") == 0 || strcmp(func, "get_group_info") == 0) {
        char *c = canon_json(raw);
        printf("%s\n", c ? c : "");
        free(c);
    } else {
        printf("%s\n", raw);
    }
    free(raw);
    return 0;
}
