/* Oracle harness: gateway/platforms/helpers.py table helpers
 * Emits one JSON object per line. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gateway/port_gateway_platforms_helpers.h"

static const char *js(const char *s) {
    static char bufs[8][8192];
    static int bi = 0;
    char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 8000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

int main(void) {
    printf("{\"t\":\"is_table_row\",\"in\":%s,\"out\":%d}\n", js("| a | b |"),
           gateway_platforms_helpers_is_table_row("| a | b |")?1:0);
    printf("{\"t\":\"is_table_row\",\"in\":%s,\"out\":%d}\n", js("plain text"),
           gateway_platforms_helpers_is_table_row("plain text")?1:0);
    printf("{\"t\":\"is_table_row\",\"in\":%s,\"out\":%d}\n", js(""),
           gateway_platforms_helpers_is_table_row("")?1:0);
    printf("{\"t\":\"is_table_row\",\"in\":%s,\"out\":%d}\n", js("  col1 | col2"),
           gateway_platforms_helpers_is_table_row("  col1 | col2")?1:0);

    int n = 0;
    char **cells = gateway_platforms_helpers_split_markdown_table_row("| a | b | c |", &n);
    printf("{\"t\":\"split_row\",\"in\":%s,\"cells\":[", js("| a | b | c |"));
    for (int i = 0; i < n; i++) { printf("%s%s", i?",":"", js(cells[i])); free(cells[i]); }
    printf("]}\n");
    free(cells);

    const char *table = "| Name | Role |\n| --- | --- |\n| Alice | Admin |\n| Bob | User |";
    char *conv = gateway_platforms_helpers_convert_table_to_bullets(table);
    printf("{\"t\":\"convert\",\"in\":%s,\"out\":%s}\n", js(table), js(conv));
    free(conv);

    const char *notable = "just some text with | a pipe but no dash";
    char *conv2 = gateway_platforms_helpers_convert_table_to_bullets(notable);
    printf("{\"t\":\"convert_notable\",\"in\":%s,\"out\":%s}\n", js(notable), js(conv2));
    free(conv2);

    return 0;
}
