#!/usr/bin/env python3
"""Generate tests/fixtures/openai_routes_flow2.jsonl — scripted-seam cases
for the openai_routes_flow2 port (>=5 per func, 23 funcs).

Each case is {"func": <name>, "in": {..., "seams": {<seam>: <script>}}}.
A script is a single result (reused), a bare array (per-call queue, last
value repeats), null (a real None result), or {"__raise__": cls, "msg": ...}
(a scripted exception). A MISSING seam key is fail-closed (RuntimeError).
Domain rules mirror the port header (ASCII hex/strip probes, integral
numbers in str()/dumps positions, 2-item durable_key/events arrays, no
scripted KeyError, request nodes carry only headers).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "openai_routes_flow2.jsonl")

HEX32 = "0123456789abcdef0123456789abcdef"
HEX24 = HEX32[:24]
HEX28 = HEX32[:28]
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def req(headers=None):
    return {"headers": headers or {}}


def gw(sa=True):
    return {"session_authority": {"kind": "sqlite"} if sa else None}


RS_BASE = {
    "durable_key": None,
    "terminal_snapshot_persisted": False,
    "conversation": None,
    "terminal_replay": False,
    "response_id": "resp_1",
    "instructions": None,
    "session_id": "sess_0",
    "conversation_history": [],
    "user_message": "hello",
}

# ── transformed_notice ──
case("transformed_notice", seams={"t": "Output was transformed."})
case("transformed_notice", seams={"t": ""})
case("transformed_notice", seams={"t": None})
case("transformed_notice", seams={})  # fail-closed
case("transformed_notice", seams={"t": {"__raise__": "ValueError", "msg": "bad key"}})
case("transformed_notice", seams={"t": 42})

# ── message_item ──
case("message_item", text="hello")
case("message_item", text="")
case("message_item", text=None)
case("message_item", text=42)
case("message_item", text={"a": 1})

# ── reasoning_item ──
case("reasoning_item", text="thinking", seams={"uuid4_hex": HEX32})
case("reasoning_item", text="t", seams={"uuid4_hex": "abc"})
case("reasoning_item", text="t", seams={"uuid4_hex": None})
case("reasoning_item", text="t", seams={"uuid4_hex": 42})
case("reasoning_item", text="t", seams={})  # fail-closed
case("reasoning_item", text="t", seams={"uuid4_hex": {"__raise__": "SystemExit", "msg": "bye"}})

# ── trim_tool_items ──
LONG600 = "x" * 600
LONG1200 = "y" * 1200
case("trim_tool_items", items=[], seams={})
case("trim_tool_items", items=[
    {"type": "function_call", "arguments": json.dumps({"content": LONG600, "query": "q"})}],
    seams={})
case("trim_tool_items", items=[{"type": "function_call", "arguments": "{bad json"}], seams={})
case("trim_tool_items", items=[
    {"type": "function_call", "arguments": {"content": LONG600, "pattern": "p" * 501}}],
    seams={})
case("trim_tool_items", items=[{"type": "function_call", "arguments": None}], seams={})
case("trim_tool_items", items=[
    {"type": "function_call_output",
     "output": [{"type": "input_text", "text": LONG1200}, {"type": "input_text", "text": "z"}]}],
    seams={"cap_text": "CAPPED"})
case("trim_tool_items", items=[
    {"type": "function_call_output", "output": [{"type": "input_text", "text": "y" * 1000}]}],
    seams={"cap_text": "CAPPED"})
case("trim_tool_items", items=[
    {"type": "function_call_output", "output": [{"type": "input_text", "text": LONG1200}]}],
    seams={})  # cap_text fail-closed propagates (no try in this branch)
case("trim_tool_items", items=[5], seams={})
case("trim_tool_items", items=[
    {"type": "function_call_output", "output": [{"type": "input_text", "text": 42}]}],
    seams={"cap_text": "CAPPED"})
case("trim_tool_items", items=None, seams={})
case("trim_tool_items", items=[
    {"type": "function_call", "arguments": json.dumps({"content": LONG600, "old_string": "o" * 501,
                                                       "new_string": "n" * 501})},
    {"role": "user", "content": "untouched"}], seams={})

# ── turn_start_index ──
CH = [{"role": "user", "content": "a"}]
RES = {"messages": [{"role": "assistant", "content": "b"}]}
case("turn_start_index", conversation_history=CH, user_message="a", result=RES,
     seams={"turn_start_index": 2})
case("turn_start_index", conversation_history=CH, user_message="a", result=RES,
     seams={"turn_start_index": 0})
case("turn_start_index", conversation_history=CH, user_message="a", result=RES,
     seams={"turn_start_index": None})
case("turn_start_index", conversation_history=CH, user_message="a", result=RES, seams={})
case("turn_start_index", conversation_history=CH, user_message="a", result=RES,
     seams={"turn_start_index": {"__raise__": "ValueError", "msg": "no boundary"}})

# ── turn_transcript_messages ──
MSGS = [
    {"role": "assistant", "content": "a"},
    {"role": "tool", "content": "t"},
    {"role": "user", "content": "u"},
    {"role": "assistant", "content": "c"},
]
case("turn_transcript_messages", conversation_history=CH, user_message="a", result=None, seams={})
case("turn_transcript_messages", conversation_history=CH, user_message="a",
     result={"messages": []}, seams={})
case("turn_transcript_messages", conversation_history=CH, user_message="a",
     result={"messages": MSGS},
     seams={"turn_start_index": 0,
            "message_response": [{"display_kind": "text", "content": "a"},
                                 {"display_kind": "hidden"},
                                 {"display_kind": "text", "content": "c"}]})
case("turn_transcript_messages", conversation_history=CH, user_message="a",
     result={"messages": MSGS},
     seams={"turn_start_index": 2,
            "message_response": [{"display_kind": "text", "content": "c"}]})
case("turn_transcript_messages", conversation_history=CH, user_message="a",
     result={"messages": MSGS},
     seams={"turn_start_index": 0, "message_response": None})
case("turn_transcript_messages", conversation_history=CH, user_message="a",
     result={"messages": MSGS}, seams={"turn_start_index": 0})  # message_response fail-closed
case("turn_transcript_messages", conversation_history=CH, user_message="a",
     result={"messages": MSGS},
     seams={"turn_start_index": {"__raise__": "RuntimeError", "msg": "boom"}})
case("turn_transcript_messages", conversation_history=CH, user_message="a",
     result={"messages": [7, {"role": "assistant"}, {"role": "system"}]},
     seams={"turn_start_index": 0, "message_response": {"display_kind": "text"}})

# ── turn_reasoning_text ──
RMSGS = [
    {"role": "assistant", "reasoning": "first"},
    {"role": "assistant", "reasoning": "   "},
    {"role": "assistant", "reasoning": "second"},
    {"role": "user", "reasoning": "ignored"},
    {"role": "assistant", "reasoning": 42},
    {"role": "tool", "reasoning": "tool"},
]
case("turn_reasoning_text", conversation_history=CH, user_message="a", result=None, seams={})
case("turn_reasoning_text", conversation_history=CH, user_message="a",
     result={"messages": "no"}, seams={})
case("turn_reasoning_text", conversation_history=CH, user_message="a",
     result={"messages": RMSGS}, seams={"turn_start_index": 0})
case("turn_reasoning_text", conversation_history=CH, user_message="a",
     result={"messages": RMSGS}, seams={"turn_start_index": 2})
case("turn_reasoning_text", conversation_history=CH, user_message="a",
     result={"messages": [{"role": "assistant", "reasoning": "\n\t"},
                          {"role": "assistant", "reasoning": " \n ok \t"}]},
     seams={"turn_start_index": 0})
case("turn_reasoning_text", conversation_history=CH, user_message="a",
     result={"messages": RMSGS}, seams={"turn_start_index": None})
case("turn_reasoning_text", conversation_history=CH, user_message="a",
     result={"messages": RMSGS}, seams={})  # fail-closed

# ── extract_output_items ──
EX_MSGS = [
    {"role": "assistant", "reasoning": "think",
     "tool_calls": [{"function": {"name": "search", "arguments": "{\"q\":1}"}, "id": "c1"}]},
    {"role": "tool", "tool_call_id": "c1", "content": "result"},
]
case("extract_output_items", result={}, seams={"redact_api_error_text": "ERR"})
case("extract_output_items", result={"final_response": "done"}, seams={})
case("extract_output_items", result={"messages": EX_MSGS, "final_response": "fin"},
     seams={"uuid4_hex": HEX32})
case("extract_output_items", result={"messages": EX_MSGS, "final_response": "fin"},
     start_index=1, seams={"uuid4_hex": HEX32})
case("extract_output_items", result={"final_response": "done"}, response_id="resp_9",
     seams={"identify_output": [[{"id": "identified"}]]})
case("extract_output_items", result=None, seams={})
case("extract_output_items", result={"messages": None}, seams={})
case("extract_output_items", result={}, seams={})  # redact fail-closed
case("extract_output_items", result={"messages": EX_MSGS, "final_response": "f"},
     seams={"uuid4_hex": {"__raise__": "KeyboardInterrupt", "msg": "stop"}})
case("extract_output_items", result={"messages": [{"role": "assistant", "tool_calls": [5]}]},
     seams={"uuid4_hex": HEX32})
case("extract_output_items", result={"final_response": "d"}, start_index="x", seams={})
case("extract_output_items", result={"error": "boom"},
     seams={"redact_api_error_text": "REDACTED"})

# ── chat_request_id ──
case("chat_request_id", request=req(), seams={})
case("chat_request_id", request=req({"Idempotency-Key": ""}), seams={})
case("chat_request_id", request=req({"Idempotency-Key": "k1"}),
     seams={"run_idempotency_scope": "scope1"})
case("chat_request_id", request=req({"Idempotency-Key": "k1"}), seams={})  # fail-closed
case("chat_request_id", request=req({"Idempotency-Key": "k1"}),
     seams={"run_idempotency_scope": None})
case("chat_request_id", request=req({"Idempotency-Key": 42}),
     seams={"run_idempotency_scope": "scope1"})
case("chat_request_id", request=req({"Idempotency-Key": "k1"}),
     seams={"run_idempotency_scope": {"__raise__": "KeyboardInterrupt", "msg": "int"}})

# ── authority_chat_kwargs ──
case("authority_chat_kwargs", request=req({"Idempotency-Key": "k1"}),
     provided_session_id="sid", seams={})  # gateway_runner absent -> AttributeError
case("authority_chat_kwargs", gateway_runner=None, request=req(), provided_session_id=None, seams={})
case("authority_chat_kwargs", gateway_runner={}, request=req(), provided_session_id=None, seams={})
case("authority_chat_kwargs", gateway_runner={"session_authority": None},
     request=req(), provided_session_id=None, seams={})
case("authority_chat_kwargs", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     provided_session_id="sid", seams={"run_idempotency_scope": ["s1", "s2"]})
case("authority_chat_kwargs", gateway_runner=gw(), request=req(),
     provided_session_id=None, seams={"run_idempotency_scope": "s2"})
case("authority_chat_kwargs", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     provided_session_id="", seams={"run_idempotency_scope": ["s1", "s2"]})
case("authority_chat_kwargs", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     provided_session_id="sid",
     seams={"run_idempotency_scope": {"__raise__": "RuntimeError", "msg": "no scope"}})

# ── response_identity ──
case("response_identity", durable_key=None,
     seams={"uuid4_hex": HEX32, "time": 1700000000.9})
case("response_identity", durable_key=["idem:s:k", "fp"],
     seams={"durable_response_id": "resp_d", "off_loop": 123})
case("response_identity", durable_key=None, seams={"uuid4_hex": HEX32, "time": None})
case("response_identity", durable_key=None, seams={"time": 5})  # uuid fail-closed
case("response_identity", durable_key=["idem:s:k", "fp"],
     seams={"durable_response_id": {"__raise__": "ValueError", "msg": "bad"}, "off_loop": 1})
case("response_identity", durable_key=["idem:s:k", "fp"],
     seams={"durable_response_id": "resp_d",
            "off_loop": {"__raise__": "OSError", "msg": "disk"}})
case("response_identity", durable_key=None, seams={"uuid4_hex": HEX32, "time": -5.7})
case("response_identity", durable_key=None, seams={"uuid4_hex": HEX32, "time": 0})

# ── implicit_chat_session ──
IMPL_OK = {"active_authority": {"persistence": "P"},
           "run_idempotency_scope": "s1", "to_thread": "new_sid"}
case("implicit_chat_session", request=req({"Idempotency-Key": "k1"}),
     session_id="sid", seams={})  # gateway_runner absent
case("implicit_chat_session", gateway_runner=None, request=req(), session_id="sid", seams={})
case("implicit_chat_session", gateway_runner={"session_authority": None},
     request=req(), session_id="sid", seams={})
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": None})
case("implicit_chat_session", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     session_id="sid", seams=IMPL_OK)
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": {"persistence": "P"}, "to_thread": "new_sid"})
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": {"persistence": "P"},
            "to_thread": {"__raise__": "AuthorityRefusal", "msg": "refused"},
            "store_error_response": {"error": "refusal"}})
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": {"persistence": "P"},
            "to_thread": {"__raise__": "TransientStoreError", "msg": "busy"},
            "store_error_response": {"error": "503"}})
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": {"persistence": "P"},
            "to_thread": {"__raise__": "OperationalError", "msg": "locked"},
            "store_error_response": {"error": "503"}})
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": {"persistence": "P"},
            "to_thread": {"__raise__": "RuntimeError", "msg": "boom"},
            "logger.error": None, "error_response": {"error": "500"}})
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": {"persistence": "P"},
            "to_thread": {"__raise__": "SystemExit", "msg": "exit"}})
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": {}, "logger.error": None,
            "error_response": {"error": "500"}})
case("implicit_chat_session", gateway_runner=gw(), request=req(), session_id="sid",
     seams={"active_authority": {"__raise__": "RuntimeError", "msg": "probe failed"}})

# ── run_idempotent ──
RESULT = {"final_response": "ok"}
USAGE = {"input_tokens": 1, "output_tokens": 2, "total_tokens": 3}
FK = ["model", "provider", "messages"]
case("run_idempotent", gateway_runner=None, request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, log_label="chat completions", fingerprint_keys=FK, route="chat_completions",
     seams={"run_idempotency_scope": "s1", "make_request_fingerprint": "fp1",
            "idem_get_or_set": [[RESULT, USAGE]]})
case("run_idempotent", gateway_runner=None, request=req(), body={"model": "m"},
     log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"compute": [[RESULT, USAGE]]})
case("run_idempotent", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"compute": [[RESULT, USAGE]]})
case("run_idempotent", gateway_runner=None, request=req(), body={},
     log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"compute": {"__raise__": "AuthorityRefusal", "msg": "refused"},
            "store_error_response": {"error": "refusal"}})
case("run_idempotent", gateway_runner=None, request=req(), body={},
     log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"compute": {"__raise__": "RuntimeError", "msg": "boom"},
            "logger.error": None, "error_response": {"error": "500"}})
case("run_idempotent", gateway_runner=None, request=req(), body={},
     log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"compute": {"__raise__": "RuntimeError", "msg": "muted",
                        "_notification_presentation_suppressed": True},
            "logger.error": None, "error_response": {"error": "500"}})
case("run_idempotent", gateway_runner=None, request=req(), body={},
     log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"compute": {"__raise__": "KeyboardInterrupt", "msg": "int"}})
case("run_idempotent", gateway_runner=None, request=req({"Idempotency-Key": "k1"}),
     body={}, log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"run_idempotency_scope": "s1", "make_request_fingerprint": "fp1",
            "idem_get_or_set": None, "logger.error": None,
            "error_response": {"error": "500"}})
case("run_idempotent", gateway_runner=None, request=req({"Idempotency-Key": "k1"}),
     body={}, log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"run_idempotency_scope": "s1", "make_request_fingerprint": "fp1",
            "idem_get_or_set": [[RESULT]], "logger.error": None,
            "error_response": {"error": "500"}})
case("run_idempotent", gateway_runner=None, request=req({"Idempotency-Key": "k1"}),
     body={}, log_label="responses", fingerprint_keys=FK, route="responses",
     seams={"logger.error": None, "error_response": {"error": "500"}})

# ── prepare_sse_response ──
case("prepare_sse_response", request=req(), session_id="s", gateway_session_key="k",
     seams={"sse_headers": {"Access-Control-Allow-Origin": "*"},
            "stream_response": None, "response.prepare": None})
case("prepare_sse_response", request=req(), session_id=None, gateway_session_key=None,
     seams={"sse_headers": {"A": "1"}, "stream_response": None, "response.prepare": None})
case("prepare_sse_response", request=req(), session_id="s", gateway_session_key=None,
     seams={"sse_headers": None})
case("prepare_sse_response", request=req(), session_id=None, gateway_session_key=None,
     seams={"sse_headers": None, "stream_response": None, "response.prepare": None})
case("prepare_sse_response", request=req(), session_id=None, gateway_session_key=None,
     seams={"sse_headers": {}, "stream_response": {"__raise__": "OSError", "msg": "no"}})
case("prepare_sse_response", request=req(), session_id=None, gateway_session_key=None,
     seams={"sse_headers": {}, "stream_response": None,
            "response.prepare": {"__raise__": "ConnectionResetError", "msg": "gone"}})
case("prepare_sse_response", request=req(), session_id="", gateway_session_key="",
     seams={"sse_headers": {"A": "1"}, "stream_response": None, "response.prepare": None})
case("prepare_sse_response", request=req(), session_id=None, gateway_session_key=None,
     seams={"stream_response": None, "response.prepare": None})  # sse_headers fail-closed

# ── replay_sse_responses ──
PREP = {"sse_headers": {}, "stream_response": None, "response.prepare": None}
ENV = {"id": "resp_1", "object": "response", "created_at": 1700000000,
       "model": "m", "status": "completed"}
case("replay_sse_responses", request=req(),
     replay={"headers": {"X-Hermes-Session-Id": "s", "X-Hermes-Session-Key": "k"},
             "events": [["response.created", {"type": "response.created"}],
                        ["response.completed", {"type": "response.completed"}]]},
     seams=dict(PREP, sse_frame=["f1", "f2"], **{"response.write": None}))
case("replay_sse_responses", request=req(), replay={"response": ENV},
     seams=dict(PREP, sse_frame="frame", **{"response.write": None}))
case("replay_sse_responses", request=req(), replay={}, seams=PREP)
case("replay_sse_responses", request=req(),
     replay={"response": {"id": "r", "object": "response", "created_at": 1, "model": "m"}},
     seams=PREP)
case("replay_sse_responses", request=req(),
     replay={"response": {"id": "r", "object": "response", "created_at": 1, "status": "failed"}},
     seams=PREP)
case("replay_sse_responses", request=req(), replay={"events": [[1]]},
     seams=dict(PREP, sse_frame="f"))
case("replay_sse_responses", request=req(),
     replay={"events": [["response.created", {}]]},
     seams=dict(PREP, sse_frame={"__raise__": "ValueError", "msg": "bad frame"}))
case("replay_sse_responses", request=req(), replay=None, seams={})
case("replay_sse_responses", request=req(), replay={"response": ENV, "events": []},
     seams=dict(PREP, sse_frame="frame", **{"response.write": None}))

# ── responses_durable_replay ──
DR_BASE = {"run_idempotency_scope": "s1",
           "responses_fingerprint_keys": [["model", "input"]],
           "make_request_fingerprint": "fp1",
           "current_response_store": {"__store__": "response"},
           "request_binding": {"principal": "p"}}
DK = ["idem:s1:k1", "fp1"]
REPLAY_REC = {"fingerprint": "fp1", "response": ENV, "headers": {"X-Hermes-Session-Id": "s"}}
case("responses_durable_replay", gateway_runner=None, request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False, seams={})
case("responses_durable_replay", request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False, seams={})  # gateway_runner absent
case("responses_durable_replay", gateway_runner=gw(), request=req(),
     body={"model": "m"}, stream=False, seams={})
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False,
     seams=dict(DR_BASE, off_loop=[[False, None]],
                openai_error={"error": {"code": "admission_conflict"}}, json_response=None))
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False,
     seams=dict(DR_BASE, off_loop=[[True, None]]))
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False,
     seams=dict(DR_BASE, off_loop=[[True, {"fingerprint": "other"}]],
                openai_error={"error": {"code": "admission_conflict"}}, json_response=None))
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False,
     seams=dict(DR_BASE, off_loop=[[True, {"fingerprint": "fp1"}]], settled=False,
                openai_error={"error": {"code": "unknown_execution"}}, json_response=None))
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False,
     seams=dict(DR_BASE, off_loop=[[True, REPLAY_REC]], settled=True, json_response=None))
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=True,
     seams=dict(DR_BASE, off_loop=[[True, dict(REPLAY_REC, events=[["response.completed", {}]])]],
                settled=True, sse_headers={}, stream_response=None,
                **{"response.prepare": None}, sse_frame="frame", **{"response.write": None}))
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False,
     seams=dict(DR_BASE, current_response_store=None))
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False,
     seams={"responses_fingerprint_keys": [["model"]], "make_request_fingerprint": "fp1"})
case("responses_durable_replay", gateway_runner=gw(), request=req({"Idempotency-Key": "k1"}),
     body={"model": "m"}, stream=False,
     seams=dict(DR_BASE, off_loop=[[True, REPLAY_REC]],
                settled={"__raise__": "OSError", "msg": "disk"}))

# ── rs_item_id ──
case("rs_item_id", state={"response_id": "resp_1", "_identity_counts": {}},
     kind="message", seams={"next_item_id": "msg_1"})
case("rs_item_id", state={"response_id": "resp_1", "_identity_counts": {"message": 2}},
     kind="function_call", call_id="c1", commentary=True, seams={"next_item_id": "fc_1"})
case("rs_item_id", state={}, kind="message", seams={"next_item_id": "x"})
case("rs_item_id", state={"response_id": "resp_1"}, kind="message", seams={"next_item_id": "x"})
case("rs_item_id", state={"response_id": "resp_1", "_identity_counts": {}},
     kind="message", seams={})
case("rs_item_id", state={"response_id": "resp_1", "_identity_counts": {}},
     kind="reasoning", seams={"next_item_id": {"__raise__": "ValueError", "msg": "bad"}})

# ── rs_envelope ──
ENV_STATE = {"response_id": "resp_1", "created_at": 1700000000, "model": "m"}
case("rs_envelope", state=ENV_STATE, status="in_progress")
case("rs_envelope", state=ENV_STATE, status=None)
case("rs_envelope", state={"response_id": "r", "created_at": 1}, status="completed")
case("rs_envelope", state={}, status="completed")
case("rs_envelope", state={"response_id": "r", "created_at": 0, "model": ""}, status="failed")

# ── rs_terminal_envelope ──
TE_STATE = dict(ENV_STATE, durable_key=None, usage={"input_tokens": 0, "output_tokens": 0, "total_tokens": 0})
case("rs_terminal_envelope", state=TE_STATE, status="completed", output=[],
     seams={"responses_usage_payload": {"input_tokens": 1}})
case("rs_terminal_envelope", state=TE_STATE, status="failed", output=[{"type": "message"}],
     error="boom", seams={"responses_usage_payload": {}})
case("rs_terminal_envelope", state=dict(TE_STATE, durable_key=DK), status="failed",
     output=[], error="boom", seams={"responses_usage_payload": {}})
case("rs_terminal_envelope", state=TE_STATE, status="completed", output=[], seams={})
case("rs_terminal_envelope", state=dict(ENV_STATE, durable_key=None), status="completed",
     output=[], seams={"responses_usage_payload": {}})  # usage attr absent
case("rs_terminal_envelope", state=TE_STATE, status="incomplete", output=None,
     seams={"responses_usage_payload": {}})
case("rs_terminal_envelope", state=TE_STATE, status="completed", output=[], error=None,
     seams={"responses_usage_payload": {"__raise__": "OSError", "msg": "disk"}})

# ── rs_history_with_user ──
case("rs_history_with_user", state={"conversation_history": [{"role": "user", "content": "a"}],
                                    "user_message": "b"})
case("rs_history_with_user", state={"conversation_history": [], "user_message": "b"})
case("rs_history_with_user", state={"conversation_history": None, "user_message": "b"})
case("rs_history_with_user", state={"conversation_history": []})
case("rs_history_with_user", state={"conversation_history": [{"role": "assistant", "content": "x"}],
                                    "user_message": None})

# ── rs_persist_snapshot ──
def ps_state(**kw):
    return dict(RS_BASE, **kw)


case("rs_persist_snapshot", state=ps_state(), response_env={"status": "completed"},
     seams={"store.put": None})
case("rs_persist_snapshot", state=ps_state(durable_key=DK),
     response_env={"status": "completed"},
     seams={"store.get": {"response": ENV}, "settled": True})
case("rs_persist_snapshot", state=ps_state(durable_key=DK),
     response_env={"status": "completed"},
     seams={"store.get": None, "settled": False, "store.put": None})
case("rs_persist_snapshot", state=ps_state(terminal_snapshot_persisted=True),
     response_env={"status": "incomplete"}, seams={})
case("rs_persist_snapshot", state=ps_state(), response_env={"status": "in_progress"},
     seams={"store.put": None})
case("rs_persist_snapshot", state=ps_state(conversation="c1"),
     response_env={"status": "completed"},
     seams={"store.put": None, "store.set_conversation": None})
case("rs_persist_snapshot", state=ps_state(conversation="c1", terminal_replay=True),
     response_env={"status": "completed"},
     seams={"store.get_conversation": None, "store.put": None, "store.set_conversation": None})
case("rs_persist_snapshot", state=ps_state(conversation="c1", terminal_replay=True),
     response_env={"status": "completed"},
     seams={"store.get_conversation": "other", "store.put": None})
case("rs_persist_snapshot", state=ps_state(conversation="c1", terminal_replay=True),
     response_env={"status": "completed"},
     seams={"store.get_conversation": "resp_1", "store.put": None, "store.set_conversation": None})
case("rs_persist_snapshot", state=ps_state(), response_env={"status": "completed"},
     history=[{"role": "user", "content": "h"}], session_id="sX",
     seams={"store.put": None})
case("rs_persist_snapshot", state=ps_state(), response_env=None, seams={})
case("rs_persist_snapshot", state=ps_state(), response_env={"status": "completed"},
     seams={"store.put": {"__raise__": "OSError", "msg": "disk"}})

# ── rs_commit_terminal ──
CT_STATE = dict(RS_BASE, store=True)
case("rs_commit_terminal", state=CT_STATE, env={"status": "completed"},
     full_history=[], seams={"store.put": None})
case("rs_commit_terminal", state=dict(RS_BASE, store=False, durable_key=DK),
     env={"status": "completed"}, full_history=[], record={"fingerprint": "fp1"},
     seams={"store.put": None, "store.get": {"response": ENV}})
case("rs_commit_terminal", state=dict(RS_BASE, store=False), env={"status": "completed"},
     full_history=[], seams={})
case("rs_commit_terminal", state=dict(RS_BASE, store=False, durable_key=DK),
     env={"status": "completed"}, full_history=[], record=None,
     seams={"store.get": None})
case("rs_commit_terminal", state=dict(RS_BASE, store=False, durable_key=DK),
     env={"status": "completed"}, full_history=[], record={"fingerprint": "fp1"},
     seams={"store.put": None, "store.get": {"__raise__": "OSError", "msg": "disk"}})
case("rs_commit_terminal", state=dict(RS_BASE, store=True, durable_key=DK),
     env={"status": "completed"}, full_history=[], record={"fingerprint": "fp1"},
     seams={"store.put": [None, None], "store.get": {"response": ENV}})
case("rs_commit_terminal", state=dict(RS_BASE), env={"status": "completed"},
     full_history=[], seams={})  # store attr present (RS_BASE lacks store) -> AttributeError

# ── rs_write_event ──
WE_STATE = {"durable_key": None, "sequence_number": 0, "recorded_events": [],
            "_published_output_ids": [], "_suppress_incremental": False}
case("rs_write_event", state=WE_STATE, event_type="response.created",
     data={"type": "response.created"},
     seams={"sse_frame": "frame", "response.write": None})
case("rs_write_event", state=dict(WE_STATE, durable_key=DK),
     event_type="response.output_item.added",
     data={"type": "response.output_item.added", "output_index": 0, "item": {"id": "m1"}},
     seams={"sse_frame": "frame", "response.write": None})
case("rs_write_event", state=dict(WE_STATE, durable_key=DK),
     event_type="response.output_item.added",
     data={"type": "response.output_item.added", "output_index": 1, "item": {"id": "m1"}},
     seams={})
case("rs_write_event", state=dict(WE_STATE, durable_key=DK, _suppress_incremental=True),
     event_type="response.output_item.added",
     data={"output_index": 0, "item": {"id": "m1"}}, seams={})
case("rs_write_event", state=dict(WE_STATE, durable_key=DK, _published_output_ids=["m1"]),
     event_type="response.output_text.delta",
     data={"output_index": 0, "item_id": "m1", "delta": "hi"},
     seams={"sse_frame": "frame", "response.write": None})
case("rs_write_event", state=dict(WE_STATE, durable_key=DK, _published_output_ids=["other"]),
     event_type="response.output_text.delta",
     data={"output_index": 0, "item_id": "m1"}, seams={})
case("rs_write_event", state=WE_STATE, event_type="response.created",
     data={"type": "response.created", "sequence_number": 5},
     seams={"sse_frame": "frame", "response.write": None})
case("rs_write_event", state={"durable_key": None, "sequence_number": 3},
     event_type="hermes.status", data={"kind": "wait"},
     seams={"sse_frame": "frame", "response.write": None})
case("rs_write_event", state=dict(WE_STATE, durable_key=DK),
     event_type="response.created", data={"type": "response.created"},
     seams={"response.write": None})  # sse_frame fail-closed; partial state echo
case("rs_write_event", state=WE_STATE, event_type="response.created", data=None, seams={})
case("rs_write_event", state=dict(WE_STATE, durable_key=DK),
     event_type="response.output_item.added", data={"output_index": 0}, seams={})
case("rs_write_event", state=dict(WE_STATE, durable_key=DK),
     event_type="response.created", data={"type": "response.created"},
     seams={"sse_frame": "frame", "response.write": {"__raise__": "ConnectionResetError", "msg": "gone"}})
case("rs_write_event", state=dict(WE_STATE, sequence_number="x"),
     event_type="response.created", data={"type": "response.created"}, seams={})
case("rs_write_event", state=dict(WE_STATE, sequence_number=[1]),
     event_type="response.created", data={"type": "response.created"}, seams={})
case("rs_write_event", state=dict(WE_STATE, sequence_number=None),
     event_type="response.created", data={"type": "response.created"}, seams={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        raise SystemExit("FATAL funcs with <5 cases: %s" % short)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %s: %d cases across %d funcs" % (OUT, len(CASES), len(counts)))
    for fn in sorted(counts):
        print("  %-28s %d" % (fn, counts[fn]))


if __name__ == "__main__":
    main()