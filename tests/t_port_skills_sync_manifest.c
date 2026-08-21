/*
 * t_port_skills_sync_manifest.c — faithful parity harness for the pure
 * sync-manifest helpers in src/tools/skills_sync_client.c
 * (tools/skills_sync_client.py: build_sync_manifest_bytes, parse_sync_manifest,
 * canonical_json_bytes, wire_address).
 *
 * Reads a JSON fixture from argv[1] with an "ops" array. Each op carries a
 * "skills" map (name->enabled) and/or "data" bytes. Emits one JSON line per op
 * mirroring sta_oracle_skills_sync_manifest.py.
 */
#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Prototypes — implemented in src/tools/skills_sync_client.c (wired build). */
char *skills_sync_build_manifest_bytes(const char *const *names,
                                      const bool *enabled, size_t count,
                                      size_t *out_len);
json_t *skills_sync_parse_manifest(const char *data, size_t len);
char *skills_sync_canonical_json_bytes(json_t *obj, size_t *out_len);
char *skills_sync_wire_address(const unsigned char *data, size_t len);

static char *read_all(const char *path) {
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

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_all(argv[1]);
    if (!raw) { fprintf(stderr, "cannot read fixture\n"); return 1; }
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture json\n"); free(raw); return 1; }

    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *op = json_get_str(c, "op", "");
            if (strcmp(op, "manifest_roundtrip") == 0) {
                json_t *sk = json_object_get(c, "skills");
                size_t count = (sk && sk->type == JSON_OBJECT) ? sk->c.count : 0;
                const char **names = NULL;
                bool *enabled = NULL;
                if (count) {
                    names = calloc(count, sizeof(const char *));
                    enabled = calloc(count, sizeof(bool));
                    for (size_t k = 0; k < count; k++) {
                        names[k] = json_object_get_key_at(sk, k);
                        json_t *v = json_object_get_at(sk, k);
                        enabled[k] = v && v->type == JSON_BOOL ? v->bool_val : false;
                    }
                }
                size_t blen = 0;
                char *bytes = skills_sync_build_manifest_bytes(names, enabled, count, &blen);
                /* parse it back */
                json_t *parsed = skills_sync_parse_manifest(bytes ? bytes : "", bytes ? blen : 0);
                char *parsed_ser = parsed ? json_serialize(parsed) : NULL;
                /* wire address of the built bytes */
                char *addr = skills_sync_wire_address((const unsigned char *)(bytes ? bytes : ""), blen);
                printf("{\"op\": \"manifest_roundtrip\", \"built_len\": %zu, \"parsed\": %s, \"wire_address\": \"%s\"}\n",
                       blen, parsed_ser ? parsed_ser : "null", addr ? addr : "null");
                free(bytes); free(addr); free(parsed_ser); free(names); free(enabled);
                if (parsed) json_free(parsed);
            } else if (strcmp(op, "canonical") == 0) {
                json_t *obj = json_object_get(c, "obj");
                size_t olen = 0;
                char *out = skills_sync_canonical_json_bytes(obj, &olen);
                /* Emit the canonical form as a JSON object (no nested string)
                 * so the oracle's normalization compares structurally. */
                printf("{\"op\": \"canonical\", \"out\": %s}\n", out ? out : "null");
                free(out);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
