/* Oracle harness for gateway/runtime_footer.py port.
 * Reads fixture (argv[1]): {"cases":[{
 *     "fn": "build_footer_line"|"format_runtime_footer"|"model_short"|"home_relative_cwd",
 *     "user_config": {...}, "platform_key": "...", "model": "...",
 *     "context_tokens": N, "context_length": N, "cwd": "...",
 *     "fields": [...] }]}
 * Prints one JSON string per case, as a compact JSON array.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Prototypes from src/gateway/runtime_footer.c (avoid pulling the heavy
 * hermes_gateway.h umbrella which transitively needs libdb/db.h). */
char *home_relative_cwd(const char *cwd);
char *model_short(const char *model);
json_node_t *resolve_footer_config(json_node_t *user_config, const char *platform_key);
char *format_runtime_footer(const char *model, int context_tokens, int context_length,
                            const char *cwd, json_node_t *fields);
char *build_footer_line(json_node_t *user_config, const char *platform_key,
                        const char *model, int context_tokens, int context_length,
                        const char *cwd);

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);
    return buf;
}

/* Print a C string as a JSON string literal (minimal escaping). */
static void print_json_string(const char *s) {
    putchar('"');
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"') { putchar('\\'); putchar('"'); }
        else if (c == '\\') { putchar('\\'); putchar('\\'); }
        else if (c == '\n') { putchar('\\'); putchar('n'); }
        else putchar(c);
    }
    putchar('"');
}

static int get_int(json_t *obj, const char *key, int dflt) {
    json_t *v = json_object_get(obj, key);
    return json_is_number(v) ? (int)v->num_val : dflt;
}
static const char *get_str(json_t *obj, const char *key) {
    json_t *v = json_object_get(obj, key);
    return json_is_string(v) ? v->str_val : NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    /* Pin HOME so home_relative_cwd is deterministic and matches the Python
     * oracle (the runner overrides HOME per-command for C only). */
    setenv("HOME", "/home/oracle", 1);
    unsetenv("TERMINAL_CWD");
    char *src = read_file(argv[1]);
    if (!src) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    json_t *root = json_parse(src, NULL);
    if (!root || root->type != JSON_OBJECT) { free(src); return 1; }

    printf("[");
    int first = 1;
    json_t *cases = json_object_get(root, "cases");
    if (cases && cases->type == JSON_ARRAY) {
        for (size_t i = 0; i < cases->c.count; i++) {
            json_t *c = cases->c.items[i];
            if (!c || c->type != JSON_OBJECT) continue;
            const char *fn = get_str(c, "fn");
            if (!fn) continue;
            char *result = NULL;

            if (strcmp(fn, "model_short") == 0) {
                result = model_short(get_str(c, "model"));
            } else if (strcmp(fn, "home_relative_cwd") == 0) {
                result = home_relative_cwd(get_str(c, "cwd"));
            } else if (strcmp(fn, "format_runtime_footer") == 0) {
                json_t *fields = json_object_get(c, "fields");
                result = format_runtime_footer(get_str(c, "model"),
                                               get_int(c, "context_tokens", 0),
                                               get_int(c, "context_length", 0),
                                               get_str(c, "cwd"),
                                               fields);
            } else if (strcmp(fn, "build_footer_line") == 0) {
                json_t *uc = json_object_get(c, "user_config");
                result = build_footer_line(uc, get_str(c, "platform_key"),
                                           get_str(c, "model"),
                                           get_int(c, "context_tokens", 0),
                                           get_int(c, "context_length", 0),
                                           get_str(c, "cwd"));
            }

            if (!first) printf(",");
            print_json_string(result ? result : "");
            free(result);
            first = 0;
        }
    }
    printf("]");
    json_free(root);
    free(src);
    return 0;
}
