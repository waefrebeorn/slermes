/*
 * t_port_gateway_signal_format.c — verification harness for
 * port_gateway_signal_format.c. Links the real object. Emits JSON lines:
 *   {"in": <escaped input>, "text": <c text>, "styles": [<c style>, ...]}
 * The oracle sta_oracle_signal_format.py recomputes from LIVE Python and
 * compares text + style list exactly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include the real port directly (it is self-contained: only stdio/stdlib/
 * string/pcre2). This makes the harness a faithful link of the actual code. */
#include "port_gateway_signal_format.c"

static void emit(const char *input)
{
    char *out_text = NULL;
    char *styles[256];
    int n = 0;
    int got = gateway_signal_markdown_to_signal(input, &out_text, styles, 256, &n);
    /* emit JSON */
    printf("{\"in\":");
    /* escape minimal */
    printf("\"");
    for (const char *p = input; *p; p++) {
        if (*p == '"' || *p == '\\') printf("\\%c", *p);
        else if (*p == '\n') printf("\\n");
        else if (*p == '\t') printf("\\t");
        else printf("%c", *p);
    }
    printf("\",\"text\":\"");
    for (const char *p = out_text ? out_text : ""; *p; p++) {
        if (*p == '"' || *p == '\\') printf("\\%c", *p);
        else if (*p == '\n') printf("\\n");
        else if (*p == '\t') printf("\\t");
        else printf("%c", *p);
    }
    printf("\",\"styles\":[");
    for (int i = 0; i < got; i++) {
        if (i) printf(",");
        printf("\"%s\"", styles[i]);
    }
    printf("]}\n");
    free(out_text);
    for (int i = 0; i < got; i++) free(styles[i]);
}

int main(void)
{
    const char *cases[] = {
        "**bold** and _italic_ and `code`",
        "# Heading\n\nnormal text",
        "- item one\n- item two",
        "~~struck~~ text",
        "**bold with *nested* star**",
        "a * b is multiplication, not italic",
        "```\ncode block\n```",
        "before **bold** after",
        "word__under__word",
        "***bolditalic***",
        "line1\n\n\n\n\nline2",
        "![alt](http://x) and **b**",
        "```python\nx = 1\n```\n# Title\ntext **b**",
        "no markup here at all",
        "  * indented bullet",
        "x_y_z single underscores not italic",
        NULL,
    };
    for (int i = 0; cases[i]; i++) emit(cases[i]);
    return 0;
}
