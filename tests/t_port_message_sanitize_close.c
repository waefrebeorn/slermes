/*
 * t_port_message_sanitize_close.c — Oracle harness for
 * agent/message_sanitization.py:close_interrupted_tool_sequence
 * (ported to src/agent/agent_message_sanitize.c as
 *  message_sanitize_close_interrupted).
 *
 * Builds a messages JSON array, calls the port, and emits the resulting
 * array + bool return. The Python oracle replays the identical cases against
 * LIVE Python and compares structurally.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool message_sanitize_close_interrupted(json_t *messages, const char *final_response);

static void emit(const char *name, int ret, json_t *arr)
{
    json_t *o = json_object();
    json_set(o, "case", json_string(name));
    json_set(o, "ret", json_bool(ret));
    /* arr is a borrowed ref inside the parent parsed object; copy so that
     * freeing o doesn't also free arr (which the parent still owns). */
    json_set(o, "out", json_copy(arr));
    char *s = json_serialize(o);
    printf("%s\n", s);
    free(s);
    json_free(o);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Case 1: ends on tool, no final_response -> "Operation interrupted." */
    json_t *m1 = json_parse("[[],{\"role\":\"user\",\"content\":\"go\"},{\"role\":\"tool\",\"content\":\"res\"}]", NULL);
    /* NOTE: parser may not accept array-of-arrays at top; use object wrapper */
    json_free(m1);
    m1 = json_parse("{\"messages\":[{\"role\":\"user\",\"content\":\"go\"},{\"role\":\"tool\",\"content\":\"res\"}]}", NULL);
    json_t *arr1 = json_obj_get(m1, "messages");
    int r1 = message_sanitize_close_interrupted(arr1, NULL);
    emit("tool_no_response", r1, arr1);
    json_free(m1);

    /* Case 2: ends on tool, final_response with whitespace -> trimmed */
    json_t *m2 = json_parse("{\"messages\":[{\"role\":\"tool\",\"content\":\"x\"}]}", NULL);
    json_t *arr2 = json_obj_get(m2, "messages");
    int r2 = message_sanitize_close_interrupted(arr2, "  partial done  ");
    emit("tool_with_response", r2, arr2);
    json_free(m2);

    /* Case 3: ends on user -> no change, returns 0 */
    json_t *m3 = json_parse("{\"messages\":[{\"role\":\"tool\",\"content\":\"x\"},{\"role\":\"user\",\"content\":\"hi\"}]}", NULL);
    json_t *arr3 = json_obj_get(m3, "messages");
    int r3 = message_sanitize_close_interrupted(arr3, NULL);
    emit("ends_on_user", r3, arr3);
    json_free(m3);

    /* Case 4: empty messages -> 0 */
    json_t *m4 = json_parse("{\"messages\":[]}", NULL);
    json_t *arr4 = json_obj_get(m4, "messages");
    int r4 = message_sanitize_close_interrupted(arr4, "x");
    emit("empty", r4, arr4);
    json_free(m4);

    /* Case 5: ends on tool, empty final_response -> "Operation interrupted." */
    json_t *m5 = json_parse("{\"messages\":[{\"role\":\"assistant\",\"content\":\"a\"},{\"role\":\"tool\",\"content\":\"b\"}]}", NULL);
    json_t *arr5 = json_obj_get(m5, "messages");
    int r5 = message_sanitize_close_interrupted(arr5, "");
    emit("tool_empty_response", r5, arr5);
    json_free(m5);

    return 0;
}
