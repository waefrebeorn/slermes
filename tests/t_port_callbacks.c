/* t_port_callbacks.c — oracle harness for _skipped.
 * Emits {"tag":"skip","case":N,"var":<json>,"reason":<json>,"msg":<json>,
 *         "out":<json string>}. */
#include "port_callbacks.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char *const vars[] = {
        "answer", "API_KEY", "note", "", "with spaces", "quo\"te\\x"
    };
    static const char *const reasons[] = {
        "stored as literal", "user typed it", "skipped", "", "contains \"quotes\" \\", "line\nbreak"
    };
    static const char *const msgs[] = {
        "Saved as a literal.", "Stored as API_KEY.", "Skipped validation.",
        "", "Prompt value kept as typed.", "Echo."
    };
    size_t n = sizeof(vars) / sizeof(vars[0]);
    for (size_t i = 0; i < n; i++) {
        json_t *b = cb_skipped(vars[i], reasons[i], msgs[i]);
        char *s = json_serialize(b);
        printf("{\"tag\":\"skip\",\"case\":%zu,\"var\":", i);
        jstr(vars[i]);
        printf(",\"reason\":");
        jstr(reasons[i]);
        printf(",\"msg\":");
        jstr(msgs[i]);
        printf(",\"out\":");
        jstr(s);
        printf("}\n");
        free(s); json_free(b);
    }
    return 0;
}
