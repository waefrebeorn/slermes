/* t_port_session_owned.c — oracle harness for session_owned. */
#include "port_session_owned.h"

#include <stdio.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char *const params[] = {
        "{\"owner\":{\"type\":\"session\",\"id\":\"s1\"}}",
        "{\"owner\":{\"type\":\"chat\"}}",
        "{\"owner\":null}",
        "{\"owner\":\"nope\"}",
        "{}",
        NULL,
        "[1,2]",
        "{\"owner\":{\"type\":\"session\"},\"x\":1}",
    };
    for (size_t i = 0; i < sizeof(params)/sizeof(params[0]); i++) {
        int r = sco_session_owned(params[i]);
        printf("{\"tag\":\"owned\",\"case\":%zu,\"in\":%s,\"out\":%s}\n",
               i, params[i] ? params[i] : "null", r ? "true" : "false");
    }
    return 0;
}
