/*
 * t_port_credential_sanitize.c — oracle harness for the DISK-SAFETY boundary
 * sanitize_borrowed_credential_payload() in src/agent/credential_pool_persistence.c
 * (port of agent/credential_persistence.py:sanitize_borrowed_credential_payload).
 *
 * Proves the borrowed-source policy: borrowed runtime secrets are stripped and
 * replaced with a sha256: fingerprint, while Hermes-owned (manual / persistable)
 * sources pass through unchanged. No FS / network — payload is supplied inline.
 *
 * Minimal includes only: we forward-declare the one symbol under test rather
 * than pulling in the credential_pool.h god header.
 */

#include "hermes_json.h"
#include "credential_persistence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The function under test lives in credential_pool_persistence.c (linked via
 * the full slermes object set). json_node_t == json_t. */
json_t *sanitize_borrowed_credential_payload(const json_t *payload, const char *provider_id);

/* Compact JSON string emit. */
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

        /* Format: sani <provider_id> <payload_json>
         * Skip the leading op token, then split provider from payload. */
        const char *q = line;
        while (*q && *q != ' ') q++;
        while (*q == ' ') q++;            /* skip op + spaces */
        const char *p = q;
        while (*p && *p != ' ') p++;      /* end of provider */
        char provider[256];
        size_t pl = (size_t)(p - q);
        if (pl >= sizeof(provider)) pl = sizeof(provider) - 1;
        memcpy(provider, q, pl);
        provider[pl] = '\0';
        while (*p == ' ') p++;
        const char *payload_str = p;

        char *err = NULL;
        json_t *payload = json_parse(payload_str, &err);
        free(err);
        if (!payload || payload->type != JSON_OBJECT) {
            printf("{\"provider\":");
            emit_json_string(provider);
            printf(",\"in\":");
            emit_json_string(payload_str);
            printf(",\"out\":null}\n");
            json_free(payload);
            continue;
        }

        json_t *result = sanitize_borrowed_credential_payload(payload, provider[0] ? provider : NULL);
        char *ser = result ? json_serialize(result) : NULL;

        printf("{\"provider\":");
        emit_json_string(provider);
        printf(",\"in\":");
        emit_json_string(payload_str);
        printf(",\"out\":");
        if (ser) emit_json_string(ser);
        else printf("null");
        printf("}\n");

        free(ser);
        json_free(result);
        json_free(payload);
    }
    return 0;
}
