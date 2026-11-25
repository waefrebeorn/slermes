/*
 * t_port_cch_flow.c -- oracle harness for chat_completion_helpers_flow.c
 *
 * Producer side of the differential gate: emits one JSON line per scripted
 * case, {"func":..., "in":{...}, "out":[...]}, where "out" is the ordered
 * DECISION SEQUENCE produced by the C flow core under the scripted I/O in
 * "in". tests/oracle_cch_flow.py reads these lines, drives the LIVE Python
 * with the same scripted I/O, and compares the sequences.
 *
 * Link: src/agent/chat_completion_helpers_flow.o
 *       src/agent/port_chat_completion_helpers.o lib/libjson/json.o
 *       lib/libregex/hermes_regex.o
 */
#include "hermes_cch_flow.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* cch_status_code_from_value now has a faithful canonical global in
 * src/agent/port_chat_completion_helpers.c (stub placeholder removed
 * from infrastructure_stubs.c); the former link shim is gone. */

static json_t *dispatch(const char *func, const json_t *in)
{
    if (strcmp(func, "tc_acc") == 0) return cch_flow_tc_acc(in);
    if (strcmp(func, "chat_chunk_loop") == 0) return cch_flow_chat_chunk_loop(in);
    if (strcmp(func, "choiceless_chunk") == 0) return cch_flow_choiceless_chunk(in);
    if (strcmp(func, "assemble_tool_calls") == 0) return cch_flow_assemble_tool_calls(in);
    if (strcmp(func, "finish_chat_stream") == 0) return cch_flow_finish_chat_stream(in);
    if (strcmp(func, "provider_stream_error_from_text") == 0) return cch_flow_provider_stream_error_from_text(in);
    if (strcmp(func, "build_partial_stream_stub") == 0) return cch_flow_build_partial_stream_stub(in);
    if (strcmp(func, "partial_stream_stub") == 0) return cch_flow_partial_stream_stub(in);
    if (strcmp(func, "maybe_disable_streaming") == 0) return cch_flow_maybe_disable_streaming(in);
    if (strcmp(func, "retry_after_drop") == 0) return cch_flow_retry_after_drop(in);
    if (strcmp(func, "handle_stream_error") == 0) return cch_flow_handle_stream_error(in);
    if (strcmp(func, "unmask_probe") == 0) return cch_flow_unmask_probe(in);
    if (strcmp(func, "call_retry_loop") == 0) return cch_flow_call_retry_loop(in);
    if (strcmp(func, "run_outcome") == 0) return cch_flow_run_outcome(in);
    if (strcmp(func, "reasoning_config_for_wire") == 0) return cch_flow_reasoning_config_for_wire(in);
    return json_array();
}

static void run_obj(const char *func, json_t *in)
{
    json_t *out = dispatch(func, in);
    char *ins = json_serialize(in);
    char *outs = json_serialize(out);
    printf("{\"func\":\"%s\",\"in\":%s,\"out\":%s}\n", func, ins ? ins : "{}", outs ? outs : "[]");
    free(ins); free(outs);
    json_free(in); json_free(out);
}

static void run(const char *func, const char *in_json)
{
    json_t *in = json_parse(in_json, NULL);
    if (!in) in = json_object();
    run_obj(func, in);
}

/* ---- small builders for text-heavy inputs ---- */
static json_t *obj(void) { return json_object(); }
static json_t *arr(void) { return json_array(); }
static void set(json_t *o, const char *k, json_t *v) { json_set(o, k, v); }
static void sets(json_t *o, const char *k, const char *s) { json_set(o, k, json_string(s)); }
static void setn(json_t *o, const char *k, double n) { json_set(o, k, json_number(n)); }
static void setb(json_t *o, const char *k, int b) { json_set(o, k, json_bool(b != 0)); }
static void app(json_t *a, json_t *v) { json_append(a, v); }

/* a chat chunk with one choice/delta */
static json_t *chunk_delta(json_t *delta, const char *finish_reason)
{
    json_t *choice = obj();
    set(choice, "delta", delta);
    if (finish_reason) sets(choice, "finish_reason", finish_reason);
    else set(choice, "finish_reason", json_null());
    json_t *choices = arr(); app(choices, choice);
    json_t *chunk = obj(); set(chunk, "choices", choices);
    return chunk;
}

int main(void)
{
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);

    /* ================= tc_acc ================= */
    run("tc_acc", "{\"deltas\":[{\"index\":0,\"id\":\"c1\",\"function\":{\"name\":\"read_file\",\"arguments\":\"{\"}},{\"index\":0,\"function\":{\"arguments\":\"}\"}}],\"materialize\":true}");
    run("tc_acc", "{\"deltas\":[{\"index\":0,\"id\":\"a\",\"function\":{\"name\":\"t1\",\"arguments\":\"{}\"}},{\"index\":0,\"id\":\"b\",\"function\":{\"name\":\"t2\",\"arguments\":\"{}\"}}],\"materialize\":true}");
    run("tc_acc", "{\"deltas\":[{\"index\":0,\"id\":\"a\",\"function\":{\"name\":\"tool\",\"arguments\":\"[1,\"}},{\"index\":0,\"id\":\"b\",\"function\":{\"arguments\":\"2]\"}}],\"materialize\":true}");
    run("tc_acc", "{\"deltas\":[{\"index\":0,\"id\":\"a\",\"function\":{\"name\":\"tool\",\"arguments\":\"{}\"}},{\"index\":0,\"id\":\"b\",\"function\":{\"name\":\"tool\",\"arguments\":\"{}\"}}],\"materialize\":true}");
    run("tc_acc", "{\"deltas\":[{\"index\":0,\"id\":5,\"function\":{\"name\":\"t\",\"arguments\":\"{}\"}}],\"materialize\":true}");
    run("tc_acc", "{\"deltas\":[{\"function\":{\"name\":\"noindex\",\"arguments\":\"{}\"}}],\"materialize\":true}");

    /* ================= choiceless_chunk ================= */
    run("choiceless_chunk", "{\"chunk\":{\"usage\":{\"completion_tokens\":5}},\"finish_reason\":null}");
    run("choiceless_chunk", "{\"chunk\":{\"lastOne\":true},\"finish_reason\":null}");
    run("choiceless_chunk", "{\"chunk\":{\"model_extra\":{\"lastOne\":\"true\"}},\"finish_reason\":null}");
    run("choiceless_chunk", "{\"chunk\":{\"error_type\":\"503\",\"error_message\":\"down\"},\"finish_reason\":null}");
    run("choiceless_chunk", "{\"chunk\":{\"lastOne\":true},\"finish_reason\":\"stop\"}");
    run("choiceless_chunk", "{\"chunk\":{},\"finish_reason\":null}");

    /* ================= assemble_tool_calls ================= */
    run("assemble_tool_calls", "{\"acc\":{\"0\":{\"id\":\"c1\",\"type\":\"function\",\"function\":{\"name\":\"t\",\"arguments\":\"{}\"}}},\"finish_reason\":\"stop\"}");
    run("assemble_tool_calls", "{\"acc\":{\"0\":{\"id\":\"c1\",\"type\":\"function\",\"function\":{\"name\":\"t\",\"arguments\":\"{\"}}},\"finish_reason\":null}");
    run("assemble_tool_calls", "{\"acc\":{\"0\":{\"id\":\"c1\",\"type\":\"function\",\"function\":{\"name\":\"t\",\"arguments\":\"{bad\"}}},\"finish_reason\":\"length\",\"repair_results\":[\"{}\"]}");
    run("assemble_tool_calls", "{\"acc\":{\"0\":{\"id\":\"c1\",\"type\":\"function\",\"function\":{\"name\":\"t\",\"arguments\":\"{bad\"}}},\"finish_reason\":\"length\",\"repair_results\":[\"{\\\"a\\\":1}\"]}");
    run("assemble_tool_calls", "{\"acc\":{\"0\":{\"id\":\"c1\",\"type\":\"function\",\"function\":{\"name\":\"t\",\"arguments\":\"\"}}},\"finish_reason\":null}");
    run("assemble_tool_calls", "{\"acc\":{\"0\":{\"id\":\"c1\",\"type\":\"function\",\"function\":{\"name\":\"t\",\"arguments\":\"{}\"}}},\"finish_reason\":null,\"repetition\":[true]}");

    /* ================= build_partial_stream_stub ================= */
    run("build_partial_stream_stub", "{\"role\":\"assistant\",\"full_content\":\"hi\",\"model_name\":\"m\",\"clean_eof\":true}");
    run("build_partial_stream_stub", "{\"role\":\"assistant\",\"full_content\":null,\"model_name\":\"m\",\"api_mode\":\"anthropic_messages\"}");
    run("build_partial_stream_stub", "{\"role\":\"assistant\",\"model_name\":\"m\",\"overflow_terminal\":true,\"dropped_tool_names\":[\"t\"]}");
    run("build_partial_stream_stub", "{\"role\":\"assistant\",\"model_name\":\"m\",\"dropped_tool_names\":[]}");

    /* ================= partial_stream_stub ================= */
    run("partial_stream_stub", "{\"streamed_text\":\"hello\",\"partial_tool_names\":[],\"classify\":{\"reason\":null},\"model\":\"m\"}");
    run("partial_stream_stub", "{\"streamed_text\":\"\",\"partial_tool_names\":[\"read_file\",\"write_file\"],\"warning_presentation\":true,\"classify\":{\"reason\":null},\"model\":\"m\"}");
    run("partial_stream_stub", "{\"streamed_text\":\"\",\"partial_tool_names\":[\"t\"],\"classify\":{\"reason\":\"context_overflow\"},\"model\":\"m\"}");
    run("partial_stream_stub", "{\"streamed_text\":\"x\",\"partial_tool_names\":[],\"classify\":{\"reason\":\"content_policy_blocked\"},\"model\":\"m\"}");
    run("partial_stream_stub", "{\"streamed_text\":\"\",\"partial_tool_names\":[\"a\",\"b\",\"c\",\"d\",\"e\"],\"classify\":{\"reason\":null},\"model\":\"m\"}");

    /* ================= maybe_disable_streaming ================= */
    run("maybe_disable_streaming", "{\"llamacpp\":true}");
    run("maybe_disable_streaming", "{\"empty_frame\":true}");
    run("maybe_disable_streaming", "{\"stream_unavailable\":false}");
    run("maybe_disable_streaming", "{\"stream_unavailable\":true,\"error\":{\"text\":\"Streaming is not supported for this model\"},\"api_mode\":\"chat_completions\",\"provider\":\"custom\"}");
    run("maybe_disable_streaming", "{\"stream_unavailable\":true,\"error\":{\"text\":\"unexpected event order\"},\"api_mode\":\"anthropic_messages\",\"provider\":\"custom\"}");
    run("maybe_disable_streaming", "{\"stream_unavailable\":true,\"error\":{\"text\":\"unexpected event order\"},\"api_mode\":\"anthropic_messages\",\"provider\":\"bedrock\"}");
    run("maybe_disable_streaming", "{\"stream_unavailable\":true,\"error\":{\"text\":\"InvokeModelWithResponseStream denied\"},\"api_mode\":\"bedrock_converse\",\"provider\":\"bedrock\"}");

    /* ================= retry_after_drop ================= */
    run("retry_after_drop", "{\"attempt\":0,\"max_retries\":2,\"mid_tool_call\":false,\"reason\":\"stream_retry_cleanup\",\"is_parse_error\":false,\"backoff_delay\":1.0}");
    run("retry_after_drop", "{\"attempt\":0,\"max_retries\":2,\"mid_tool_call\":false,\"reason\":\"stream_retry_cleanup\",\"is_parse_error\":true,\"backoff_delay\":1.0}");
    run("retry_after_drop", "{\"attempt\":1,\"max_retries\":2,\"mid_tool_call\":true,\"reason\":\"stream_mid_tool_retry_cleanup\",\"is_parse_error\":false,\"backoff_delay\":2.0}");
    run("retry_after_drop", "{\"attempt\":2,\"max_retries\":2,\"mid_tool_call\":false,\"reason\":\"stream_retry_cleanup\",\"is_parse_error\":false,\"backoff_delay\":4.0}");

    /* ================= handle_stream_error ================= */
    run("handle_stream_error", "{\"request_cancelled\":true,\"error\":{\"kind\":\"ReadError\"}}");
    run("handle_stream_error", "{\"deltas_were_sent\":false,\"stream_options_unsupported\":false,\"rejects_stream_options\":true,\"error\":{\"kind\":\"APIStatusError\"}}");
    run("handle_stream_error", "{\"is_timeout\":true,\"deltas_were_sent\":false,\"attempt\":0,\"max_retries\":2,\"backoff_delay\":1.0,\"error\":{\"kind\":\"ReadTimeout\"}}");
    run("handle_stream_error", "{\"is_timeout\":true,\"deltas_were_sent\":false,\"attempt\":2,\"max_retries\":2,\"error\":{\"kind\":\"ReadTimeout\"}}");
    run("handle_stream_error", "{\"is_empty_stream\":true,\"deltas_were_sent\":false,\"attempt\":2,\"max_retries\":2,\"error\":{\"kind\":\"EmptyStreamError\"}}");
    run("handle_stream_error", "{\"deltas_were_sent\":true,\"partial_tool_names\":[\"t\"],\"is_conn_err\":true,\"attempt\":0,\"max_retries\":2,\"warning_presentation\":true,\"backoff_delay\":1.0,\"error\":{\"kind\":\"ReadError\"}}");
    run("handle_stream_error", "{\"deltas_were_sent\":true,\"streamed_text\":\"hi\",\"partial_tool_names\":[],\"is_conn_err\":true,\"attempt\":0,\"max_retries\":2,\"error\":{\"kind\":\"ReadError\"}}");
    run("handle_stream_error", "{\"deltas_were_sent\":true,\"streamed_text\":\"   \",\"partial_tool_names\":[],\"is_timeout\":true,\"attempt\":0,\"max_retries\":2,\"backoff_delay\":1.0,\"error\":{\"kind\":\"ReadTimeout\"}}");
    run("handle_stream_error", "{\"deltas_were_sent\":false,\"attempt\":0,\"max_retries\":2,\"unmask_handles\":true,\"disable_ctx\":{\"stream_unavailable\":false},\"error\":{\"kind\":\"APIStatusError\"}}");

    /* ================= unmask_probe ================= */
    run("unmask_probe", "{\"interrupt_requested\":true,\"error\":{\"kind\":\"APIStatusError\"}}");
    run("unmask_probe", "{\"status\":500,\"deltas_were_sent\":false,\"api_mode\":\"chat_completions\",\"last_probe_ts\":null,\"now\":100.0,\"probe_outcome\":\"success\",\"replay_ok\":true,\"probe_response\":{\"id\":\"r1\"},\"error\":{\"kind\":\"APIStatusError\"}}");
    run("unmask_probe", "{\"status\":500,\"deltas_were_sent\":false,\"api_mode\":\"chat_completions\",\"last_probe_ts\":null,\"now\":100.0,\"probe_outcome\":\"error\",\"probe_status\":400,\"probe_error\":{\"kind\":\"BadRequest\"},\"error\":{\"kind\":\"APIStatusError\"}}");
    run("unmask_probe", "{\"status\":500,\"deltas_were_sent\":false,\"api_mode\":\"chat_completions\",\"last_probe_ts\":null,\"now\":100.0,\"probe_outcome\":\"error\",\"probe_status\":502,\"error\":{\"kind\":\"APIStatusError\"}}");
    run("unmask_probe", "{\"status\":500,\"deltas_were_sent\":false,\"api_mode\":\"chat_completions\",\"last_probe_ts\":90.0,\"now\":100.0,\"error\":{\"kind\":\"APIStatusError\"}}");
    run("unmask_probe", "{\"status\":400,\"deltas_were_sent\":false,\"api_mode\":\"chat_completions\",\"error\":{\"kind\":\"APIStatusError\"}}");
    run("unmask_probe", "{\"status\":500,\"deltas_were_sent\":false,\"api_mode\":\"chat_completions\",\"last_probe_ts\":null,\"now\":100.0,\"probe_outcome\":\"success\",\"replay_ok\":false,\"error\":{\"kind\":\"APIStatusError\"}}");

    /* ================= call_retry_loop ================= */
    run("call_retry_loop", "{\"max_stream_retries\":2,\"wire\":[{\"outcome\":\"response\",\"response\":{\"id\":\"r1\"}}]}");
    run("call_retry_loop", "{\"max_stream_retries\":2,\"wire\":[{\"outcome\":\"error\",\"error\":{\"kind\":\"ReadError\"}},{\"outcome\":\"response\",\"response\":{\"id\":\"r2\"}}],\"handle_retry\":[true],\"route_switched\":[false]}");
    run("call_retry_loop", "{\"max_stream_retries\":2,\"wire\":[{\"outcome\":\"error\",\"error\":{\"kind\":\"ReadError\"}}],\"handle_retry\":[false]}");
    run("call_retry_loop", "{\"max_stream_retries\":2,\"wire\":[{\"outcome\":\"error\",\"error\":{\"kind\":\"ReadError\"}}],\"handle_retry\":[true],\"route_switched\":[true]}");
    run("call_retry_loop", "{\"max_stream_retries\":2,\"interrupt_before\":[0],\"wire\":[{\"outcome\":\"response\"}]}");
    run("call_retry_loop", "{\"max_stream_retries\":1,\"wire\":[{\"outcome\":\"error\",\"error\":{\"kind\":\"ReadError\"}},{\"outcome\":\"error\",\"error\":{\"kind\":\"ReadError\"}}],\"handle_retry\":[true,true]}");

    /* ================= run_outcome ================= */
    run("run_outcome", "{\"monitor_interrupted\":true}");
    run("run_outcome", "{\"interrupt_requested\":true}");
    run("run_outcome", "{\"result_response\":{\"id\":\"r1\"},\"diag_first_chunk_at\":5.0}");
    run("run_outcome", "{\"result_error\":{\"kind\":\"ReadError\"},\"deltas_were_sent\":false}");
    run("run_outcome", "{\"result_error\":{\"kind\":\"ReadError\"},\"deltas_were_sent\":true,\"partial_stub\":{\"id\":\"partial-stream-stub\"}}");
    run("run_outcome", "{\"abandon_error\":{\"kind\":\"TimeoutError\"},\"deltas_were_sent\":true,\"streamed_text\":\"hi\",\"partial_tool_names\":[],\"partial_stub\":{\"id\":\"partial-stream-stub\"}}");
    run("run_outcome", "{\"abandon_error\":{\"kind\":\"TimeoutError\"},\"deltas_were_sent\":true,\"streamed_text\":\"\",\"partial_tool_names\":[]}");

    /* ================= reasoning_config_for_wire ================= */
    run("reasoning_config_for_wire", "{\"reasoning_config\":{\"enabled\":true,\"effort\":\"high\"}}");
    run("reasoning_config_for_wire", "{\"reasoning_config\":{\"enabled\":true,\"effort\":\"high\"},\"effort_rejected\":true}");
    run("reasoning_config_for_wire", "{\"reasoning_config\":{\"enabled\":true,\"effort\":\"high\"},\"ephemeral_reasoning_off\":true}");
    run("reasoning_config_for_wire", "{\"reasoning_config\":{\"enabled\":false},\"disable_rejected\":true,\"floor_required\":false}");
    run("reasoning_config_for_wire", "{\"reasoning_config\":{\"enabled\":false},\"disable_rejected\":true,\"floor_required\":true,\"floor_effort\":\"low\"}");
    run("reasoning_config_for_wire", "{\"reasoning_config\":null,\"unset_default\":{\"enabled\":true,\"effort\":\"medium\"}}");
    run("reasoning_config_for_wire", "{\"reasoning_config\":{\"enabled\":true,\"effort\":\"high\"},\"disable_rejected\":true}");

    /* ================= provider_stream_error_from_text (text-heavy) ================= */
    {
        json_t *in = obj(); sets(in, "text", ""); sets(in, "finish_reason", "error");
        run_obj("provider_stream_error_from_text", in);
    }
    {
        json_t *in = obj(); sets(in, "text", "event: error\ndata: {}\n\n"); sets(in, "finish_reason", "stop");
        run_obj("provider_stream_error_from_text", in);
    }
    {
        json_t *in = obj(); sets(in, "text", "event: error\ndata: something bad\n\n"); sets(in, "finish_reason", "error");
        run_obj("provider_stream_error_from_text", in);
    }
    {
        json_t *in = obj(); sets(in, "text", "data: {\"message\":\"x\"}\nstatus: 503\n\n"); sets(in, "finish_reason", "error_finish");
        run_obj("provider_stream_error_from_text", in);
    }
    {
        json_t *in = obj(); sets(in, "text", "{\"error\":{\"message\":\"bad\",\"code\":\"E1\"}}"); sets(in, "finish_reason", "error");
        run_obj("provider_stream_error_from_text", in);
    }
    {
        json_t *in = obj(); sets(in, "text", "something went wrong"); sets(in, "finish_reason", "error");
        run_obj("provider_stream_error_from_text", in);
    }

    /* ================= finish_chat_stream ================= */
    {
        json_t *in = obj();
        json_t *cp = arr(); app(cp, json_string("Hello")); set(in, "content_parts", cp);
        set(in, "reasoning_parts", arr()); set(in, "refusal_parts", arr()); set(in, "acc", obj());
        sets(in, "finish_reason", "stop"); sets(in, "model", "m"); sets(in, "role", "assistant");
        run_obj("finish_chat_stream", in);
    }
    {
        json_t *in = obj();
        set(in, "content_parts", arr()); set(in, "reasoning_parts", arr());
        set(in, "refusal_parts", arr()); set(in, "acc", obj());
        set(in, "finish_reason", json_null()); sets(in, "model", "m");
        run_obj("finish_chat_stream", in);
    }
    {
        json_t *in = obj();
        json_t *cp = arr(); app(cp, json_string("partial")); set(in, "content_parts", cp);
        set(in, "reasoning_parts", arr()); set(in, "refusal_parts", arr()); set(in, "acc", obj());
        set(in, "finish_reason", json_null()); sets(in, "model", "m");
        run_obj("finish_chat_stream", in);
    }
    {
        json_t *in = obj();
        set(in, "content_parts", arr()); set(in, "reasoning_parts", arr()); set(in, "refusal_parts", arr());
        json_t *acc = obj(); json_t *e = obj(); sets(e, "id", "c1"); sets(e, "type", "function");
        json_t *fn = obj(); sets(fn, "name", "t"); sets(fn, "arguments", "{"); set(e, "function", fn);
        set(acc, "0", e); set(in, "acc", acc);
        set(in, "finish_reason", json_null()); sets(in, "model", "m");
        run_obj("finish_chat_stream", in);
    }
    {
        json_t *in = obj();
        json_t *cp = arr(); app(cp, json_string("event: error\ndata: something bad\n\n")); set(in, "content_parts", cp);
        set(in, "reasoning_parts", arr()); set(in, "refusal_parts", arr()); set(in, "acc", obj());
        sets(in, "finish_reason", "error"); sets(in, "model", "m");
        run_obj("finish_chat_stream", in);
    }
    {
        json_t *in = obj();
        json_t *cp = arr(); app(cp, json_string("hi")); set(in, "content_parts", cp);
        set(in, "reasoning_parts", arr()); set(in, "refusal_parts", arr()); set(in, "acc", obj());
        set(in, "finish_reason", json_null()); sets(in, "model", "m");
        json_t *usage = obj(); setn(usage, "completion_tokens", 1); set(in, "usage", usage);
        run_obj("finish_chat_stream", in);
    }

    /* ================= chat_chunk_loop ================= */
    {
        json_t *in = obj(); json_t *chunks = arr();
        json_t *d1 = obj(); sets(d1, "content", "Hel");
        app(chunks, chunk_delta(d1, NULL));
        json_t *d2 = obj(); sets(d2, "content", "lo");
        app(chunks, chunk_delta(d2, "stop"));
        set(in, "chunks", chunks);
        json_t *watch = obj(); json_t *wc = arr(); app(wc, json_bool(false)); app(wc, json_bool(false)); set(watch, "content", wc);
        set(in, "watch", watch);
        run_obj("chat_chunk_loop", in);
    }
    {
        json_t *in = obj(); json_t *chunks = arr();
        json_t *d = obj(); sets(d, "reasoning_content", "think"); sets(d, "content", "answer");
        app(chunks, chunk_delta(d, NULL));
        set(in, "chunks", chunks);
        json_t *watch = obj(); json_t *wr = arr(); app(wr, json_bool(false)); set(watch, "reasoning", wr);
        json_t *wc = arr(); app(wc, json_bool(false)); set(watch, "content", wc); set(in, "watch", watch);
        run_obj("chat_chunk_loop", in);
    }
    {
        json_t *in = obj(); json_t *chunks = arr();
        json_t *d = obj(); json_t *tcs = arr(); json_t *tc = obj();
        setn(tc, "index", 0); sets(tc, "id", "c1");
        json_t *fn = obj(); sets(fn, "name", "run"); sets(fn, "arguments", "{}"); set(tc, "function", fn);
        app(tcs, tc); set(d, "tool_calls", tcs);
        app(chunks, chunk_delta(d, NULL));
        set(in, "chunks", chunks); set(in, "watch", obj());
        run_obj("chat_chunk_loop", in);
    }
    {
        /* router-shim hold then flush */
        json_t *in = obj(); json_t *chunks = arr();
        json_t *d1 = obj(); sets(d1, "content", "Connect timeout");
        app(chunks, chunk_delta(d1, NULL));
        json_t *d2 = obj(); sets(d2, "content", ", please try again later.");
        app(chunks, chunk_delta(d2, NULL));
        json_t *d3 = obj(); sets(d3, "content", " Extra");
        app(chunks, chunk_delta(d3, NULL));
        set(in, "chunks", chunks);
        json_t *watch = obj(); json_t *wc = arr();
        app(wc, json_bool(false)); app(wc, json_bool(false)); app(wc, json_bool(false));
        set(watch, "content", wc); set(in, "watch", watch);
        setb(in, "deltas_were_sent", 0);
        run_obj("chat_chunk_loop", in);
    }
    {
        json_t *in = obj(); json_t *chunks = arr();
        json_t *d = obj(); sets(d, "content", "loop");
        app(chunks, chunk_delta(d, NULL));
        set(in, "chunks", chunks);
        json_t *watch = obj(); json_t *wc = arr(); app(wc, json_bool(true)); set(watch, "content", wc);
        set(in, "watch", watch);
        run_obj("chat_chunk_loop", in);
    }
    {
        json_t *in = obj(); json_t *chunks = arr();
        json_t *d1 = obj(); sets(d1, "content", "a"); app(chunks, chunk_delta(d1, NULL));
        json_t *d2 = obj(); sets(d2, "content", "b"); app(chunks, chunk_delta(d2, NULL));
        set(in, "chunks", chunks);
        json_t *ia = arr(); app(ia, json_number(1)); set(in, "interrupt_at", ia);
        json_t *watch = obj(); json_t *wc = arr(); app(wc, json_bool(false)); set(watch, "content", wc);
        set(in, "watch", watch);
        run_obj("chat_chunk_loop", in);
    }
    {
        json_t *in = obj(); json_t *chunks = arr();
        json_t *usage_chunk = obj(); set(usage_chunk, "choices", arr());
        json_t *u = obj(); setn(u, "completion_tokens", 3); set(usage_chunk, "usage", u);
        app(chunks, usage_chunk);
        json_t *d = obj(); sets(d, "content", "x"); app(chunks, chunk_delta(d, "stop"));
        set(in, "chunks", chunks);
        json_t *watch = obj(); json_t *wc = arr(); app(wc, json_bool(false)); set(watch, "content", wc);
        set(in, "watch", watch);
        run_obj("chat_chunk_loop", in);
    }
    {
        json_t *in = obj(); json_t *chunks = arr();
        json_t *prog = obj(); json_t *me = obj(); json_t *pp = obj();
        setn(pp, "processed", 10); setn(pp, "total", 100); set(me, "prompt_progress", pp);
        set(prog, "model_extra", me);
        json_t *pd = obj(); json_t *pc = arr(); json_t *pchoice = obj(); set(pchoice, "delta", pd);
        set(pchoice, "finish_reason", json_null()); app(pc, pchoice); set(prog, "choices", pc);
        app(chunks, prog);
        json_t *d = obj(); sets(d, "content", "hi"); app(chunks, chunk_delta(d, NULL));
        set(in, "chunks", chunks); setb(in, "llamacpp", 1);
        json_t *watch = obj(); json_t *wc = arr(); app(wc, json_bool(false)); set(watch, "content", wc);
        set(in, "watch", watch);
        run_obj("chat_chunk_loop", in);
    }

    return 0;
}