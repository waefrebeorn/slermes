/*
 * t_port_msg_sanitize_surrogates.c — faithful verification harness for
 * agent_message_sanitize_structure_surrogates in
 * src/agent/agent_message_sanitize.c
 * (port of agent/message_sanitization.py:_sanitize_structure_surrogates).
 *
 * Reads a JSON array fixture from argv[1]; each element is a JSON value
 * (tree) to scrub. Emits one JSON line per element:
 *   {"in":<original serialized>,"out":<sanitized serialized>,"found":<bool>}
 * The Python oracle (tests/sta_oracle_msg_sanitize_surrogates.py) recomputes
 * from the LIVE agent/message_sanitization.py; the runner diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declaration — defined in src/agent/agent_message_sanitize.c (port
 * of agent/message_sanitization.py). Avoid pulling hermes.h (libdb chain). */
bool agent_message_sanitize_structure_surrogates(json_t *node);

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

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <trees.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char *err = NULL;
    json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }

    int n = json_array_size(root);
    for (int i = 0; i < n; i++) {
        json_t *tree = json_get(root, i);
        bool found = agent_message_sanitize_structure_surrogates(tree);
        char *out_ser = json_serialize(tree);

        json_t *o = json_new_object();
        /* Emit "out" as a JSON value (parsed back), not an escaped string,
         * so it matches the Python oracle which emits the dict directly. */
        json_t *out_node = json_parse(out_ser, NULL);
        json_set(o, "out", out_node ? out_node : json_null());
        json_set(o, "found", json_bool(found));
        char *ser = json_serialize(o);
        printf("%s\n", ser);
        free(ser);
        json_free(o);
        free(out_ser);
    }
    free(input);
    return 0;
}
