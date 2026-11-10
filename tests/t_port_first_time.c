/* t_port_first_time.c — stateful oracle harness for UnauthorizedOwnerNotifier
 * first_time. Replays a call sequence against a C-held LRU set and emits the
 * per-call outcome plus the serialized final set. */
#include "port_first_time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    /* sequence: platform,user_id pairs */
    static const char *const seq[][2] = {
        {"tg", "a"}, {"tg", "b"}, {"tg", "c"}, {"tg", "a"},
        {"tg", "d"}, {"tg", "b"}, {"tg", "e"},
    };
    char *keys[64];
    size_t count = 0;
    for (size_t i = 0; i < 64; i++) keys[i] = NULL;
    for (size_t i = 0; i < sizeof(seq)/sizeof(seq[0]); i++) {
        char key[256];
        snprintf(key, sizeof(key), "%s/%s", seq[i][0], seq[i][1]);
        /* each key needs its own allocation; the set retains the pointer */
        char *owned = strdup(key);
        int out = uin_seen_first_time(keys, &count, 3, owned);
        if (out == 0) free(owned); /* already present: port didn't store it */
        printf("{\"tag\":\"ft\",\"case\":%zu,\"plat\":\"%s\",\"user\":\"%s\",\"out\":%s}\n",
               i, seq[i][0], seq[i][1], out ? "true" : "false");
    }
    /* final set */
    printf("{\"tag\":\"set\",\"keys\":[");
    for (size_t i = 0; i < count; i++) {
        if (i) printf(",");
        jstr(keys[i]);
    }
    printf("]}\n");
    return 0;
}
