/* t_port_producer_lifecycle.c — oracle harness for
 * gateway/session_producer_lifecycle.py:_executed_outcome. */
#include "port_producer_lifecycle.h"

#include <stdio.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char *const caps[] = {
        "{\"terminal_result\":{\"interrupted\":true,\"failed\":false}}",
        "{\"result\":{\"failed\":true}}",
        "{\"result\":{\"error\":\"boom\"}}",
        "{\"result\":{}}",
        "{\"result\":{\"interrupted\":false,\"failed\":false}}",
        "{}",
        "{\"terminal_result\":null,\"result\":{\"failed\":true}}",
        "{\"result\":{\"interrupted\":0}}",
        "{\"result\":{\"failed\":\"err\"}}",
        "{\"result\":{\"interrupted\":true,\"failed\":true}}",
        "{\"terminal_result\":{\"error\":\"\"}}",
        "{\"result\":{\"error\":false,\"interrupted\":false,\"failed\":false}}",
    };
    for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); i++) {
        const char *out = spl_executed_outcome(caps[i]);
        printf("{\"family\":\"outcome\",\"case\":%zu,\"cap\":%s,\"out\":\"%s\"}\n",
               i, caps[i], out);
    }
    return 0;
}
