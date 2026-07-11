/*
 * t_port_copilot_acp_toolcall.c — v560 residual-façade oracle harness for
 * agent/copilot_acp_client.py's two pure struct-builder functions:
 *   - copilot_build_openai_tool_call  (Port of _build_openai_tool_call)
 *   - copilot_completion_to_stream_chunks (Port of _completion_to_stream_chunks)
 *
 * Output: JSON objects on stdout, one per case, fed to
 * sta_oracle_copilot_acp_toolcall.py which recomputes against LIVE Python.
 */
#include "copilot_acp_client.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_raw(const char *label, json_t *obj)
{
    char *s = json_serialize(obj);
    printf("{\"fn\":\"%s\",\"out\":%s}\n", label, s ? s : "null");
    free(s);
    json_free(obj);
}

int main(void)
{
    /* ---- _build_openai_tool_call ---- */
    json_t *b1 = copilot_build_openai_tool_call("call_abc", "search", "{\"q\":\"hi\"}");
    emit_raw("build1", b1);
    json_t *b2 = copilot_build_openai_tool_call("c2", "", "");
    emit_raw("build2", b2);

    /* ---- _completion_to_stream_chunks (with 2 tool calls) ---- */
    json_t *comp = json_object();
    json_set(comp, "model", json_string("gpt-4"));
    json_t *usage = json_object();
    json_set(usage, "prompt_tokens", json_number(1));
    json_set(usage, "completion_tokens", json_number(2));
    json_set(comp, "usage", usage);
    json_t *choices = json_array();
    json_t *choice0 = json_object();
    json_set(choice0, "finish_reason", json_string("stop"));
    json_t *msg = json_object();
    json_set(msg, "content", json_string("hello world"));
    json_t *tcs = json_array();
    json_t *tcA = json_object();
    json_set(tcA, "id", json_string("t1"));
    json_set(tcA, "type", json_string("function"));
    json_t *fnA = json_object();
    json_set(fnA, "name", json_string("search"));
    json_set(fnA, "arguments", json_string("{\"q\":1}"));
    json_set(tcA, "function", fnA);
    json_append(tcs, tcA);
    json_t *tcB = json_object();
    json_set(tcB, "id", json_string("t2"));
    json_set(tcB, "type", json_string("function"));
    json_t *fnB = json_object();
    json_set(fnB, "name", json_string("calc"));
    json_set(fnB, "arguments", json_string("{\"x\":2}"));
    json_set(tcB, "function", fnB);
    json_append(tcs, tcB);
    json_set(msg, "tool_calls", tcs);
    json_set(msg, "reasoning_content", json_string("thinking"));
    json_set(msg, "reasoning", json_null());
    json_set(choice0, "message", msg);
    json_append(choices, choice0);
    json_set(comp, "choices", choices);
    json_t *s1 = copilot_completion_to_stream_chunks(comp);
    emit_raw("stream_with_tc", s1);
    json_free(comp);

    /* ---- _completion_to_stream_chunks (no tool calls) ---- */
    json_t *comp2 = json_object();
    json_set(comp2, "model", json_string("m"));
    json_set(comp2, "usage", json_null());
    json_t *choices2 = json_array();
    json_t *choice2 = json_object();
    json_set(choice2, "finish_reason", json_string("stop"));
    json_t *msg2 = json_object();
    json_set(msg2, "content", json_string("hi"));
    json_set(msg2, "tool_calls", json_null());
    json_set(msg2, "reasoning_content", json_null());
    json_set(msg2, "reasoning", json_null());
    json_set(choice2, "message", msg2);
    json_append(choices2, choice2);
    json_set(comp2, "choices", choices2);
    json_t *s2 = copilot_completion_to_stream_chunks(comp2);
    emit_raw("stream_no_tc", s2);
    json_free(comp2);

    return 0;
}
