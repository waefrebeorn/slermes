/* t_port_media_labels.c — oracle harness for the pure helpers in
 * gateway/relay/media.py, hermes_cli/platforms.py, browser_credential.py. */
#include "port_media_labels.h"

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
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* media_base_url: ws(s) dial -> http(s) connector base. */
    static const char *const dials[] = {
        "ws://relay.host/relay", "wss://relay.host/relay", "wss://relay.host/relay/",
        "wss://relay.host/base/relay", "ws://x/cb", "http://plain.example/relay",
        "", "  wss://h/relay  ", "wss://h/relayfoo", NULL,
    };
    for (int i = 0; i < 10; i++) {
        char *out = ml_media_base_url(dials[i]);
        printf("{\"family\":\"burl\",\"case\":%d,\"in\":", i);
        jstr(dials[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* is_relay_media_url. */
    static const char *const us[] = {
        "http://x/relay/media/1", "http://x/relay/", "pre/relay/media/post", NULL,
    };
    for (int i = 0; i < 4; i++) {
        int out = ml_is_relay_media_url(us[i]);
        printf("{\"family\":\"url\",\"case\":%d,\"in\":", i);
        jstr(us[i]);
        printf(",\"out\":%d}\n", out);
    }

    /* _plugin_label: (emoji, label). */
    static const char *const pl[][2] = {
        {"⚡", "Bolt"}, {"", "Plain"}, {NULL, "Noname"}, {"x", ""},
    };
    for (int i = 0; i < 4; i++) {
        char *out = ml_plugin_label(pl[i][0], pl[i][1]);
        printf("{\"family\":\"label\",\"case\":%d,\"emoji\":", i);
        jstr(pl[i][0]);
        printf(",\"label\":");
        jstr(pl[i][1]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* generation_of: sha256 hexdigest. */
    static const char *const creds[] = {"secret-token-123", "", "x", "The quick brown fox"};
    for (int i = 0; i < 4; i++) {
        char *out = ml_generation_of(creds[i]);
        printf("{\"family\":\"gen\",\"case\":%d,\"in\":", i);
        jstr(creds[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }
    return 0;
}
