/*
 * t_port_tts.c — faithful verification harness for tts_resolve_output_format
 * in src/tools/tts.c (agent/tts_provider.py).
 *
 * Reads a JSON array fixture from argv[1]; each element is a string value to
 * clamp. Emits one JSON line per element: {"value":<in>,"out":<resolved>}.
 * The Python oracle (tests/sta_oracle_tts.py) recomputes resolve_output_format
 * from the LIVE agent/tts_provider.py; the runner diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration — defined in src/tools/tts.c (port of
 * agent/tts_provider.py). Avoid pulling tts_provider.h (which drags in
 * hermes_agent.h -> libdb) so the oracle harness links with the minimal set. */
const char *tts_resolve_output_format(const char *value);

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
    if (argc < 2) { fprintf(stderr, "usage: %s <values.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char *err = NULL;
    json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }

    int n = json_array_size(root);
    for (int i = 0; i < n; i++) {
        json_t *v = json_get(root, i);
        const char *value = (v && v->type == JSON_STRING) ? v->str_val : "";
        const char *out = tts_resolve_output_format(value);
        json_t *o = json_new_object();
        json_set(o, "value", json_string(value ? value : ""));
        json_set(o, "out", json_string(out ? out : ""));
        char *ser = json_serialize(o);
        printf("%s\n", ser);
        free(ser);
        json_free(o);
    }
    free(input);
    return 0;
}
