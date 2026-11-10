/* t_port_relay_media.c — oracle harness for gateway/relay/media.py pure core:
 * _content_disposition_filename (new port) + media_base_url (existing port). */
#include "port_media_labels.h"

#include <stdio.h>
#include <stdlib.h>

/* Emit a JSON-escaped string (raw bytes, not unicode-escaped: the Python
 * oracle reads utf-8 directly). */
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

static void emit(const char *tag, const char *in_, const char *out)
{
    printf("{\"tag\":\"%s\",\"in\":", tag);
    jstr(in_);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* --- _content_disposition_filename: plain filename --- */
    {
        static const char *const headers[] = {
            "",
            "attachment; filename=\"foo bar.pdf\"",
            "attachment; filename=plain.tar.gz",
            "attachment; FILENAME=UPPER.CASE",
            "inline",
            "attachment; filename=\"a\\\"b.pdf\"",
            "attachment; filename=\"with;semicolon.pdf\"",
            "attachment; filename=caf\xc3\xa9.png",
            "attachment; filename=\"../../etc/passwd\"",
            "attachment; filename=\".hidden\"",
            "attachment; filename=\"..\"",
            "attachment; filename=\"bad\nname.pdf\"",
            "inline; filename=report.xlsx",
            "attachment; filename=",
        };
        for (size_t i = 0; i < sizeof(headers) / sizeof(headers[0]); i++) {
            char *out = ml_content_disposition_filename(headers[i]);
            emit("cdf", headers[i], out);
            free(out);
        }
    }
    /* --- _content_disposition_filename: RFC 5987 extended filename* --- */
    {
        static const char *const headers[] = {
            "attachment; filename*=UTF-8''r%C3%A9sum%C3%A9.pdf",
            "inline; filename*=utf-8''caf%C3%A9.png; filename=\"fallback.png\"",
            "attachment; filename*=UTF-8''%F0%9F%93%84%20test.png",
            "attachment; filename*=ISO-8859-1''r%E9sum%E9.pdf",
            "attachment; filename*=UTF-8''a%20b.txt",
        };
        for (size_t i = 0; i < sizeof(headers) / sizeof(headers[0]); i++) {
            char *out = ml_content_disposition_filename(headers[i]);
            emit("cdf", headers[i], out);
            free(out);
        }
    }
    /* --- _content_disposition_filename: extended wins over plain (either
       order), and a very long filename caps at 255 keeping extension --- */
    {
        static const char *const headers[] = {
            "attachment; filename*=UTF-8''ext.pdf; filename=\"plain.pdf\"",
            "attachment; filename=\"plain.pdf\"; filename*=UTF-8''ext.pdf",
        };
        for (size_t i = 0; i < sizeof(headers) / sizeof(headers[0]); i++) {
            char *out = ml_content_disposition_filename(headers[i]);
            emit("cdf", headers[i], out);
            free(out);
        }
    }
    /* long plain filename -> capped, keeping extension */
    {
        char val[512];
        size_t i;
        size_t n = 0;
        for (i = 0; i < 300; i++) val[n++] = 'x';
        val[n++] = '.';
        val[n++] = 'p';
        val[n++] = 'd';
        val[n++] = 'f';
        val[n] = '\0';
        {
            size_t hl = 0;
            char *h;
            /* "attachment; filename=\"" + val + '"' + NUL */
            hl = 22 + n + 1 + 1;
            h = malloc(hl);
            size_t k = 0;
            h[k++] = 'a'; h[k++] = 't'; h[k++] = 't'; h[k++] = 'a';
            h[k++] = 'c'; h[k++] = 'h'; h[k++] = 'm'; h[k++] = 'e';
            h[k++] = 'n'; h[k++] = 't'; h[k++] = ';'; h[k++] = ' ';
            h[k++] = 'f'; h[k++] = 'i'; h[k++] = 'l'; h[k++] = 'e';
            h[k++] = 'n'; h[k++] = 'a'; h[k++] = 'm'; h[k++] = 'e';
            h[k++] = '='; h[k++] = '"';
            for (i = 0; i < n; i++) h[k++] = val[i];
            h[k++] = '"';
            h[k] = '\0';
            {
                char *out = ml_content_disposition_filename(h);
                emit("cdf", h, out);
                free(out);
            }
            free(h);
        }
    }

    /* --- media_base_url --- */
    {
        static const char *const urls[] = {
            "",
            "wss://relay.example.com/relay",
            "ws://relay.example.com:9000/relay",
            "https://connector.example.com/relay",
            "wss://relay.example.com/relay/",
            "http://relay.example.com",
            "  wss://  spaces.example.com /relay  ",
        };
        for (size_t i = 0; i < sizeof(urls) / sizeof(urls[0]); i++) {
            char *out = ml_media_base_url(urls[i]);
            emit("mbu", urls[i], out);
            free(out);
        }
    }
    return 0;
}
