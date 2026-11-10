/* t_port_session_server_requests.c — Oracle harness for is_response_frame.
 * Emits {\"in\":<json frame>,\"ret\":bool} so the oracle replays the live Python
 * on the exact same input. */
#include "port_session_server_requests.h"

#include <stdio.h>

/* escape a JSON frame for embedding as a JSON string value */
static void jstr(const char *s)
{
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout); break;
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    static const char *const frames[] = {
        "{\"jsonrpc\":\"2.0\",\"id\":\"srq-abc123\",\"result\":{\"value\":\"x\"}}",
        "{\"id\":\"srq-abc123\",\"error\":{\"code\":1,\"message\":\"no\"}}",
        "{\"id\":\"srq-abc123\"}",
        "{\"id\":\"srq-abc123\",\"result\":null}",
        "{\"id\":\"other-1\",\"result\":{}}",
        "{\"id\":\"srq-\",\"result\":{}}",
        "{\"id\":\"SRQ-upper\",\"result\":{}}",
        "{\"id\":42,\"result\":{}}",
        "{\"id\":\"srq-abc\",\"result\":{},\"method\":\"x\"}",
        "{\"method\":\"x\",\"id\":\"srq-abc\"}",
        "[]",
        "\"just a string\"",
        "null",
        "{}",
    };
    setvbuf(stdout, NULL, _IONBF, 0);
    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        printf("{\"in\":");
        jstr(frames[i]);
        printf(",\"ret\":%d}\n", ssr_is_response_frame(frames[i]));
    }
    return 0;
}
