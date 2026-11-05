/*
 * t_port_vault_login_js.c — Oracle harness for the two JS builders in
 * agent/vault_login_classifier.py (src/cli/port_vault_login_js.c).
 *
 * Deliberately a SMALL link (the two port objects + libjson): the all-objects
 * harness for the classifier prints its 53 cases and then dies in some
 * cleanup path, which hides anything appended after it.
 */
#include "port_vault_login_classifier.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const nonces[] = {
        "abc123", "", "n\"q", "back\\slash", "line\nbreak", "\xc3\xa9-unicode",
    };
    for (size_t i = 0; i < sizeof(nonces) / sizeof(nonces[0]); i++) {
        char *js = vl_build_inspection_js(nonces[i]);
        printf("{\"family\":\"js\",\"kind\":\"inspect\",\"case\":\"insp_%zu\",\"nonce\":", i);
        emit_json_str(nonces[i]);
        printf(",\"out\":");
        emit_json_str(js);
        printf("}\n");
        free(js);
    }

    static const char *const fills[] = {
        "[]",
        "[{\"index\": 3, \"token\": \"current-password\", \"value\": \"hunter2\"}]",
        "[{\"index\": 1, \"value\": \"a\"}, {\"index\": 2, \"token\": \"cc-number\", \"value\": \"4242\"}]",
        "[{\"index\": 0, \"token\": \"one-time-code\", \"value\": \"q\\\"uote\"}]",
        "[{\"index\": 7, \"token\": \"address-level2\", \"value\": \"Z\\u00fcrich\"}]",
    };
    for (size_t i = 0; i < sizeof(fills) / sizeof(fills[0]); i++) {
        char *js = vl_build_fill_js(fills[i], "https://example.test", "n0");
        printf("{\"family\":\"js\",\"kind\":\"fill\",\"case\":\"fill_%zu\",\"fills\":%s,"
               "\"origin\":\"https://example.test\",\"nonce\":\"n0\",\"out\":", i, fills[i]);
        emit_json_str(js);
        printf("}\n");
        free(js);
    }
    return 0;
}
