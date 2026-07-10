/*
 * t_port_send_message_target.c — oracle harness for send_message_target.
 * Tests display_chat_id + telegram_retry_delay (full 1:1) + parse_target_ref
 * (convergent platforms: telegram/feishu/discord/slack). Includes the module
 * .c directly; links stdlib + math.
 */
#include "send_message_target.c"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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

int main(void)
{
    /* display_chat_id */
    const char *disp[][2] = {
        {"signal", "group:abc123"}, {"signal", "user@signal"},
        {"telegram", "-100123"}, {"slack", "C123"}, {NULL, NULL}
    };
    for (int i = 0; disp[i][0]; i++) {
        char *r = send_message_target_display_chat_id(disp[i][0], disp[i][1]);
        printf("{\"fn\":\"disp\",\"platform\":%s,\"chat\":%s,\"out\":%s}\n",
               js(disp[i][0]), js(disp[i][1]), js(r ? r : ""));
        free(r);
    }

    /* telegram_retry_delay */
    struct { const char *err; int attempt; } retry[] = {
        {"retry_after=30.0 something", 1},
        {"502 Bad Gateway", 2},
        {"429 Too Many Requests", 3},
        {"503 Service Unavailable", 1},
        {"504 Gateway Timeout", 2},
        {"timed out waiting", 1},
        {"some other error", 2},
        {NULL, 0}
    };
    for (int i = 0; retry[i].err; i++) {
        double d = send_message_target_telegram_retry_delay(retry[i].err, retry[i].attempt);
        printf("{\"fn\":\"retry\",\"err\":%s,\"attempt\":%d,\"out\":%.6f}\n",
               js(retry[i].err), retry[i].attempt, d);
    }

    /* parse_target_ref — convergent platforms only */
    struct { const char *plat; const char *target; } ptr[] = {
        {"telegram", "123456:789"},
        {"telegram", "@username"},
        {"telegram", "plainnoat"},
        {"telegram", "abc:def"},
        {"feishu", "ou_abc:thread1"},
        {"feishu", "ou_abc"},
        {"discord", "111:222"},
        {"slack", "C12345678:threadts123"},
        {"slack", "C12345678"},
        {"slack", "C123:threadts"},
        {"unknownplat", "x:y"},
        {NULL, NULL}
    };
    for (int i = 0; ptr[i].plat; i++) {
        char chat[256] = {0}, thread[256] = {0};
        int ex = send_message_target_parse_target_ref(ptr[i].plat, ptr[i].target,
                                                       chat, sizeof(chat), thread, sizeof(thread));
        printf("{\"fn\":\"ptr\",\"platform\":%s,\"target\":%s,\"chat\":%s,\"thread\":%s,\"explicit\":%d}\n",
               js(ptr[i].plat), js(ptr[i].target), js(chat), js(thread), ex);
    }
    return 0;
}
