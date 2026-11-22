/* t_port_aux_event_has_content.c — focused harness for the REAL
 * ac_anthropic_event_has_content port (src/agent/port_auxiliary_client.c).
 *
 * Reads JSON-lines: {"event": <json-string>} from stdin, emits
 * {"event": <original>, "out": 0|1|-1} where out is the port verdict
 * (-1 = unparseable event). The oracle replays the live Python
 * _anthropic_event_has_content on the SAME emitted event string.
 *
 * Gate:
 *   bash tests/run_one_oracle.sh aux_event_has_content \
 *     oracle_aux_event_has_content.py "" \
 *     "src/agent/port_auxiliary_client.o lib/libjson/json.o"
 */
#include "port_auxiliary_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

int main(void)
{
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        /* Extract the "event" JSON-string field with a minimal scan. */
        const char *key = strstr(line, "\"event\"");
        if (!key) continue;
        const char *colon = strchr(key, ':');
        if (!colon) continue;
        const char *q = strchr(colon, '"');
        if (!q) continue;
        /* find closing unescaped quote */
        char ev[4096]; size_t n = 0;
        for (const char *p = q + 1; *p && n < sizeof(ev) - 1; p++) {
            if (*p == '"' && (p == q + 1 || *(p - 1) != '\\')) break;
            ev[n++] = *p;
        }
        ev[n] = '\0';
        /* unescape \" and \\ for the C parser */
        char dec[4096]; size_t d = 0;
        for (size_t i = 0; i < n && d < sizeof(dec) - 1; i++) {
            if (ev[i] == '\\' && i + 1 < n && (ev[i + 1] == '"' || ev[i + 1] == '\\')) {
                dec[d++] = ev[++i];
            } else dec[d++] = ev[i];
        }
        dec[d] = '\0';

        json_t *e = json_parse(dec, NULL);
        int out = ac_anthropic_event_has_content(e);
        if (e) json_free(e);
        printf("{\"event\":");
        jstr(dec);
        printf(",\"out\":%d}\n", out);
    }
    return 0;
}
