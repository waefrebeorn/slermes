/*
 * t_port_credential_entry_to_json.c — oracle harness for the DISK-SAFE entry
 * serializer credential_entry_to_json() in src/agent/credential_pool_persistence.c
 * (port of agent/credential_pool.py:PooledCredential.to_dict() + sanitize).
 *
 * Reads one JSON object per line: {"provider":.., "entry":{...fields}}.
 * Calls credential_entry_to_json_from_obj() (which builds the entry from JSON
 * and runs sanitize) and emits the result. No FS / network.
 *
 * Minimal includes only: forward-declare the symbol, use slim headers — we do
 * NOT pull in credential_pool.h (god header) here.
 */

#include "hermes_json.h"
#include "credential_persistence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* declared in credential_pool.h; linked via the full slermes object set. */
json_t *credential_entry_to_json_from_obj(const json_t *entry, const char *provider);

static void emit_json_string(const char *s) {
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char *err = NULL;
        json_t *spec = json_parse(line, &err);
        free(err);
        if (!spec || spec->type != JSON_OBJECT) {
            printf("{\"out\":null}\n");
            json_free(spec);
            continue;
        }
        const json_t *entry = json_obj_get(spec, "entry");
        const json_t *prov = json_obj_get(spec, "provider");
        const char *provider = (prov && prov->type == JSON_STRING) ? prov->str_val : "";

        json_t *result = credential_entry_to_json_from_obj(entry, provider[0] ? provider : NULL);
        char *ser = result ? json_serialize(result) : NULL;

        printf("{\"provider\":");
        emit_json_string(provider);
        printf(",\"out\":");
        if (ser) emit_json_string(ser);
        else printf("null");
        printf("}\n");

        free(ser);
        json_free(result);
        json_free(spec);
    }
    return 0;
}
