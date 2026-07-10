/*
 * t_port_web_base64_img.c — oracle harness for web_convert_base64_images_to_links
 * Includes the port .c directly; links only stdlib. Compares C output
 * byte-for-byte against LIVE Python tools/web_tools.py:convert_base64_images_to_links.
 */
#include "web_base64_img.c"
#include <stdio.h>
#include <string.h>

/* Single non-rotizing JSON-string escaper into a throwaway buffer. */
static const char *esc(const char *s, char *buf, size_t bufsz)
{
    char *b = buf;
    char *end = buf + bufsz - 8;
    *b++ = '"';
    if (!s) s = "";
    for (const char *p = s; *p && b < end; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *b++ = '\\'; *b++ = (char)c; }
        else if (c == '\n') { *b++ = '\\'; *b++ = 'n'; }
        else if (c == '\t') { *b++ = '\\'; *b++ = 't'; }
        else if (c < 0x20) {
            static const char *hx = "0123456789abcdef";
            *b++ = '\\'; *b++ = 'u'; *b++ = '0'; *b++ = '0';
            *b++ = hx[(c >> 4) & 0xF]; *b++ = hx[c & 0xF];
        } else *b++ = (char)c;
    }
    *b++ = '"';
    *b = '\0';
    return buf;
}

int main(void)
{
    static const char *cases[] = {
        "![alt text](data:image/png;base64,iVBORw0KGgoAAAA)",
        "![](data:image/jpeg;base64,/9j/4AAQSkZJRg==)",
        "see (data:image/png;base64,AAAAbbbcccc)",
        "prefix data:image/gif;base64,abcd+/= suffix",
        "![has space](data:image/png;base64,abc==   ) end",
        "no images here at all, plain text",
        "![real](https://example.com/x.png) kept",
        "![a](data:image/png;base64,AAAA) and ![b](data:image/png;base64,BBBB) and (data:image/png;base64,CCCC)",
        "![x](data:image/png;base64,AAAA)[IMAGE]already",
        "",
        "data:image/png;base64,",
        "![a](data:image/png;base64,AAAA)data:image/png;base64,BBBB",
        NULL
    };
    char inbuf[4096], outbuf[4096];
    for (int i = 0; cases[i]; i++) {
        char *out = web_base64_img_convert(cases[i]);
        printf("{\"fn\":\"web_base64_img_convert\",\"in\":%s,\"out\":%s}\n",
               esc(cases[i], inbuf, sizeof(inbuf)),
               esc(out ? out : "", outbuf, sizeof(outbuf)));
        free(out);
    }
    return 0;
}
