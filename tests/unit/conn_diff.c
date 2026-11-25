/* Differential harness for the ported connectors validation layer.
 *
 * Cases: [ {"name":..., "args": {...}} ]  -- a manage_connections argument object.
 * Out:   [ {"name":..., "result": {...}} ] -- what w19_tool_6f4666_manage_connections returns.
 *
 * The comparison lives in the Python oracle: only it knows what upstream said.
 */
#include "hermes_json.h"
#include "noc_w19_tool_6f4666.h"

#include <stdio.h>
#include <stdlib.h>

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    return b;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <cases.json> <out.json>\n", argv[0]); return 2; }
    char *text = read_all(argv[1]);
    if (!text) return 2;
    json_t *cases = json_parse(text, NULL);
    free(text);
    if (!cases || cases->type != JSON_ARRAY) return 2;

    json_t *out = json_array();
    for (size_t i = 0; i < json_len(cases); i++) {
        const json_t *c = json_get(cases, i);
        json_t *args = json_copy(json_obj_get(c, "args"));
        json_t *res = w19_tool_6f4666_manage_connections(args ? args : json_object());
        json_t *entry = json_object();
        json_set(entry, "name", json_string(json_get_str(c, "name", "")));
        json_set(entry, "result", res ? res : json_null());
        json_append(out, entry);
        json_free(args);
    }
    char *ser = json_serialize(out);
    json_free(out);
    json_free(cases);
    if (!ser) return 2;
    FILE *f = fopen(argv[2], "wb");
    if (!f) { free(ser); return 2; }
    fputs(ser, f);
    fclose(f);
    free(ser);
    return 0;
}
