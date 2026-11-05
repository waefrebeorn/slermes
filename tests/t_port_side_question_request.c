/*
 * t_port_side_question_request.c — Oracle harness for the pure request-shape
 * checks of agent/side_question_request.py.
 */
#include "port_side_question_request.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        "[]",
        "null",
    };
    for (size_t i = 0; i < sizeof(kwargs) / sizeof(kwargs[0]); i++) {
        printf("{\"family\":\"server_tools\",\"case\":\"k_%zu\",\"ret\":%d}\n", i,
               sq_has_server_side_tools(kwargs[i]));
    }

    {
        static const struct { const char *model, *provider, *base, *mode, *reason; int has_provider; } keys[] = {
            { "m1", "openai", "https://api.openai.com/v1", "chat", NULL, 1 },
            { NULL, NULL, NULL, NULL, NULL, 0 },
            { "m2", "", NULL, "responses", "{\"effort\":\"high\"}", 1 },
        };
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            json_t *key = sq_runtime_key(keys[i].model, keys[i].has_provider,
                                         keys[i].provider, keys[i].base,
                                         keys[i].mode, keys[i].reason);
            printf("{\"family\":\"runtime_key\",\"case\":\"r_%zu\",\"out\":", i);
            if (!key) {
                fputs("null", stdout);
            } else {
                char *s = json_serialize(key);
                fputs(s ? s : "null", stdout);
                free(s);
                json_free(key);
            }
            printf("}\n");
        }
    }
    return 0;
}
