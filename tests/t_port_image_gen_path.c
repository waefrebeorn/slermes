/*
 * t_port_image_gen_path.c — oracle harness for image_gen_path.
 * Single verifiable pure fn: looks_like_absolute_file_path. Oracle-verified
 * 1:1 against LIVE tools/image_generation_tool.py:_looks_like_absolute_file_path.
 */
#include "image_gen_path.c"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define JS_CAP 2048
static char g_js[4][JS_CAP];
static int  g_js_cur = 0;
static const char *js(const char *s)
{
    char *b = g_js[g_js_cur];
    g_js_cur = (g_js_cur + 1) % 4;
    if (!s) s = "";
    size_t j = 0;
    b[j++] = '"';
    for (const char *p = s; *p && j < JS_CAP - 4; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { b[j++] = '\\'; b[j++] = (char)c; }
        else if (c == '\n') { b[j++] = '\\'; b[j++] = 'n'; }
        else b[j++] = (char)c;
    }
    b[j++] = '"'; b[j] = '\0';
    return b;
}
static const char *jb(int v) { return v ? "true" : "false"; }

int main(void)
{
    const char *cases[] = {
        "", "http://x.com", "https://x.com", "HTTP://X", "data:image/png;base64,xx",
        "/abs/path/img.png", "C:/Windows/thing", "c:\\win", "D:\\win", "Z:/x",
        "rel/path", "../up", "a/b/c", "file://x", "/", "\\", NULL
    };
    for (int i = 0; cases[i]; i++) {
        int r = image_gen_path_looks_like_absolute_file_path(cases[i]);
        printf("{\"fn\":\"laf\",\"in\":%s,\"out\":%s}\n", js(cases[i]), jb(r));
    }
    return 0;
}
