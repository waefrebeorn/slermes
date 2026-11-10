/*
 * t_port_side_question_request.c — Oracle harness for
 * has_server_side_tools of agent/side_question_request.py. Emits
 * {"in":<ap kwargs json>,"ret":bool} so the oracle replays the live Python on
 * the exact same input. (sq_runtime_key is an agent-object->tuple shape that
 * does not oracle against the flat-field port; left unverified.)
 */
#include "port_side_question_request.h"

#include <stdio.h>

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
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const kwargs[] = {
        "{}",
        "{\"tools\":[{\"type\":\"function\",\"name\":\"read_file\"}]}",
        "{\"tools\":[{\"type\":\"web_search\"}]}",
        "{\"tools\":[{\"name\":\"no_type\"},{\"type\":\"custom\"}]}",
        "{\"tools\":\"nope\"}",
        "{\"web_search_options\":{}}",
        "{\"extra_body\":{\"web_search_options\":null}}",
        "{\"extra_body\":{\"tools\":[{\"type\":\"code_interpreter\"}]}}",
        "{\"plugins\":[{\"id\":\"pareto-router\"}]}",
        "{\"plugins\":[{\"id\":\"web\"}]}",
        "{\"plugins\":[\"bare-string\"]}",
        "{\"plugins\":[]}",
        "{\"extra_body\":{\"plugins\":[{\"id\":\"pareto-router\"},{\"id\":\"web\"}]}}",
        "{\"extra_body\":\"not-an-object\"}",
        "{\"tools\":[7,null,{\"type\":\"function\"}]}",
    };
    for (size_t i = 0; i < sizeof(kwargs) / sizeof(kwargs[0]); i++) {
        printf("{\"in\":");
        jstr(kwargs[i]);
        printf(",\"ret\":%d}\n", sq_has_server_side_tools(kwargs[i]));
    }
    return 0;
}
