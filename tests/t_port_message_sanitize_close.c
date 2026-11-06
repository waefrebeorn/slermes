/* t_port_message_sanitize_close.c — Oracle harness.
 * Emits the FULL input + result: the pre-mutation messages array, final_response,
 * the port's ret, and the post-mutation array. The oracle replays the live
 * close_interrupted_tool_sequence on the same input and compares both.
 * Line: {"case","in":<json array>,"fr":<json|null>,"ret":bool,"out":<json array>}
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool message_sanitize_close_interrupted(json_t *messages, const char *final_response);

static void run(const char *name, const char *messages_json, const char *fr)
{
    json_t *doc = json_parse(messages_json, NULL);
    json_t *msgs = json_obj_get(doc, "messages");
    char *in = json_serialize(msgs);           /* pre-mutation snapshot */
    int ret = message_sanitize_close_interrupted(msgs, fr);
    char *out = json_serialize(msgs);          /* post-mutation snapshot */
    printf("{\"case\":\"%s\",\"in\":%s,\"fr\":",
           name, in ? in : "[]");
    if (fr) printf("\"%s\"", fr); else printf("null");
    printf(",\"ret\":%s,\"out\":%s}\n", ret ? "true" : "false", out ? out : "[]");
    free(in); free(out);
    json_free(doc);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    run("tool_no_response",
        "{\"messages\":[{\"role\":\"user\",\"content\":\"go\"},{\"role\":\"tool\",\"content\":\"res\"}]}",
        NULL);
    run("tool_with_response",
        "{\"messages\":[{\"role\":\"tool\",\"content\":\"x\"}]}",
        "  partial done  ");
    run("ends_on_user",
        "{\"messages\":[{\"role\":\"tool\",\"content\":\"x\"},{\"role\":\"user\",\"content\":\"hi\"}]}",
        NULL);
    run("empty",
        "{\"messages\":[]}",
        "x");
    run("tool_blank_response",
        "{\"messages\":[{\"role\":\"assistant\",\"content\":\"a\"},{\"role\":\"tool\",\"content\":\"b\"}]}",
        "    ");
    run("placeholder_only",
        "{\"messages\":[{\"role\":\"tool\",\"content\":\"b\"}]}",
        "[interrupt: no assistant output for this turn]");
    run("ends_on_tool_note",
        "{\"messages\":[{\"role\":\"tool\",\"content\":\"b\"}]}",
        "  done  ");
    return 0;
}
