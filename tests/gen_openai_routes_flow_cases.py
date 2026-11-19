#!/usr/bin/env python3
"""Generate tests/fixtures/openai_routes_flow.jsonl — the decision-sequence
cases for t_port_openai_routes_flow / oracle_openai_routes_flow.

Every case carries the SAME scripted seams the C harness and the live-Python
oracle consume (array scripts repeat their last value; null == None;
{"__raise__": cls, "msg": ...} scripts an exception). Run from the repo root:
    python tests/gen_openai_routes_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "openai_routes_flow.jsonl")

CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


ER_BODY = {"status": 400, "body": {"error": {"message": "m", "type": "invalid_request_error"}}}
MVE_BODY = {"status": 400, "body": {"error": {"message": "m", "type": "invalid_request_error"}}}

# ── _result_flags ──
add("result_flags", {"result": None})
add("result_flags", {"result": {}})
add("result_flags", {"result": {"completed": False, "partial": True, "failed": True, "error": "boom"}})
add("result_flags", {"result": {"interrupted": True}})
add("result_flags", {"result": {"completed": True, "interrupted": 1}})
add("result_flags", {"result": {"completed": None, "partial": 0, "failed": "", "error": None}})
add("result_flags", {"result": "text"})
add("result_flags", {"result": {"completed": True, "interrupted": False, "partial": False,
                                "failed": False, "error": {"code": 1}}})

# ── _budget_exhausted ──
add("budget_exhausted", {"result": None})
add("budget_exhausted", {"result": {}})
add("budget_exhausted", {"result": {"turn_exit_reason": "budget_exhausted"}})
add("budget_exhausted", {"result": {"turn_exit_reason": "max_iterations_reached(50)"}})
add("budget_exhausted", {"result": {"turn_exit_reason": "completed"}})
add("budget_exhausted", {"result": {"turn_exit_reason": None}})
add("budget_exhausted", {"result": {"turn_exit_reason": 123}})
add("budget_exhausted", {"result": {"turn_exit_reason": 2.5}})
add("budget_exhausted", {"result": {"turn_exit_reason": "budget_exhausted_extra"}})
add("budget_exhausted", {"result": "str"})

# ── _finish_reason ──
add("finish_reason", {"completed": True, "is_partial": False, "is_failed": False,
                      "err_msg": None, "agent_error": None, "budget_exhausted": False})
add("finish_reason", {"completed": True, "is_partial": True, "is_failed": False,
                      "err_msg": "Output truncated by limit"})
add("finish_reason", {"completed": True, "is_partial": True, "is_failed": False,
                      "err_msg": "TRUNCATED"})
add("finish_reason", {"completed": True, "is_partial": True, "is_failed": False,
                      "err_msg": "other"})
add("finish_reason", {"completed": True, "is_partial": False, "is_failed": True,
                      "err_msg": None})
add("finish_reason", {"completed": True, "is_partial": False, "is_failed": False,
                      "err_msg": None, "agent_error": ""})
add("finish_reason", {"completed": False, "is_partial": False, "is_failed": False,
                      "err_msg": "x"})
add("finish_reason", {"completed": False, "is_partial": False, "is_failed": False,
                      "err_msg": None, "budget_exhausted": False})
add("finish_reason", {"completed": False, "is_partial": False, "is_failed": False,
                      "err_msg": None, "budget_exhausted": True})
add("finish_reason", {"completed": True, "is_partial": True, "is_failed": False,
                      "err_msg": 123})
add("finish_reason", {"completed": True, "is_partial": True, "is_failed": True,
                      "err_msg": "truncat"})
add("finish_reason", {"completed": True, "is_partial": False, "is_failed": False,
                      "err_msg": "truncat"})

# ── _settled ──
add("settled", {"record": None})
add("settled", {"record": {}})
add("settled", {"record": {"response": None}})
add("settled", {"record": {"response": {"id": "r"}}})
add("settled", {"record": {"response": False}})
add("settled", {"record": {"response": ""}})
add("settled", {"record": [1]})
add("settled", {"record": "x"})

# ── _responses_fingerprint_keys ──
add("responses_fingerprint_keys", {"body": {}})
add("responses_fingerprint_keys", {"body": {"truncation": None}})
add("responses_fingerprint_keys", {"body": {"truncation": "auto"}})
add("responses_fingerprint_keys", {"body": {"truncation": False}})
add("responses_fingerprint_keys", {"body": {"truncation": 0}})
add("responses_fingerprint_keys", {"body": None})
add("responses_fingerprint_keys", {"body": []})

# ── _response_status ──
add("response_status", {"result": {}})
add("response_status", {"result": {"interrupted": True}})
add("response_status", {"result": {"interrupted": 1, "failed": True}})
add("response_status", {"result": {"failed": True}})
add("response_status", {"result": {"error": "x"}})
add("response_status", {"result": {"error": ""}})
add("response_status", {"result": {"completed": False}})
add("response_status", {"result": {"completed": 0}})
add("response_status", {"result": {"completed": None}})
add("response_status", {"result": None})

# ── _hermes_extras ──
add("hermes_extras", {"completed": True, "is_partial": False, "is_failed": False,
                      "err_msg": None, "finish_reason": "stop", "budget_exhausted": False})
add("hermes_extras", {"completed": False, "is_partial": True, "is_failed": False,
                      "err_msg": "trunc", "finish_reason": "length", "budget_exhausted": False})
add("hermes_extras", {"completed": False, "is_partial": True, "is_failed": False,
                      "err_msg": "budget", "finish_reason": "length", "budget_exhausted": True})
add("hermes_extras", {"completed": True, "is_partial": False, "is_failed": False,
                      "err_msg": None, "finish_reason": "length", "budget_exhausted": True})
add("hermes_extras", {"completed": False, "is_partial": False, "is_failed": True,
                      "err_msg": "err", "finish_reason": "error", "budget_exhausted": False})
add("hermes_extras", {"completed": True, "is_partial": False, "is_failed": False,
                      "err_msg": None, "finish_reason": "cancelled", "budget_exhausted": False})

# ── _post_stream_transform ──
add("post_stream_transform", {"result": None})
add("post_stream_transform", {"result": {}})
add("post_stream_transform", {"result": {"response_transformed": False}})
add("post_stream_transform", {"result": {"response_transformed": True,
                                         "final_response": "hello world",
                                         "pre_transform_response": "hello"}})
add("post_stream_transform", {"result": {"response_transformed": True,
                                         "final_response": "hello",
                                         "pre_transform_response": "hello"}})
add("post_stream_transform", {"result": {"response_transformed": True,
                                         "final_response": "rewritten",
                                         "pre_transform_response": "hello"}})
add("post_stream_transform", {"result": {"response_transformed": True,
                                         "final_response": "abc",
                                         "pre_transform_response": ""}})
add("post_stream_transform", {"result": {"response_transformed": True,
                                         "final_response": None,
                                         "pre_transform_response": "x"}})
add("post_stream_transform", {"result": {"response_transformed": True,
                                         "final_response": 123,
                                         "pre_transform_response": "x"}})
add("post_stream_transform", {"result": {"response_transformed": True,
                                         "final_response": "abc",
                                         "pre_transform_response": 123}})
add("post_stream_transform", {"result": {"response_transformed": True,
                                         "final_response": 123}})
add("post_stream_transform", {"result": {"response_transformed": 1,
                                         "final_response": "x",
                                         "pre_transform_response": "x"}})

# ── _cap_text ──
add("cap_text", {"text": "abcdef", "keep": 2})
add("cap_text", {"text": "abcdef", "keep": 0})
add("cap_text", {"text": "abcdef", "keep": 99})
add("cap_text", {"text": "abcdef", "keep": -2})
add("cap_text", {"text": "你好世界x", "keep": 2})
add("cap_text", {"text": "", "keep": 3})
add("cap_text", {"text": "abc", "keep": 1.5})
add("cap_text", {"text": 123, "keep": 2})
add("cap_text", {"text": "abcdef", "keep": 6})
add("cap_text", {"text": None, "keep": 2})
add("cap_text", {"text": [1, 2], "keep": 1})

# ── _is_reasoning_input_item ──
add("is_reasoning_input_item", {"item": None})
add("is_reasoning_input_item", {"item": {"type": "reasoning"}})
add("is_reasoning_input_item", {"item": {"type": "message"}})
add("is_reasoning_input_item", {"item": {}})
add("is_reasoning_input_item", {"item": {"type": 123}})
add("is_reasoning_input_item", {"item": "reasoning"})
add("is_reasoning_input_item", {"item": [{"type": "reasoning"}]})

# ── _matching_replay ──
add("matching_replay", {"key": "k", "binding": {"a": 1},
                        "seams": {"store.request_key_matches": False}})
add("matching_replay", {"key": "k", "binding": {"a": 1},
                        "seams": {"store.request_key_matches": True, "store.get": None}})
add("matching_replay", {"key": "k", "binding": {"a": 1},
                        "seams": {"store.request_key_matches": True,
                                  "store.get": {"response": {"id": "r"}}}})
add("matching_replay", {"key": "k", "binding": None,
                        "seams": {"store.request_key_matches": {"__raise__": "OSError",
                                                                "msg": "store down"}}})
add("matching_replay", {"key": "k", "binding": None,
                        "seams": {"store.request_key_matches": True,
                                  "store.get": {"__raise__": "ValueError", "msg": "bad"}}})
add("matching_replay", {"key": None, "binding": None,
                        "seams": {"store.request_key_matches": True, "store.get": None}})
add("matching_replay", {"key": "k", "binding": {"a": 1},
                        "seams": {"store.request_key_matches": 1, "store.get": {"x": 1}}})

# ── _store_nonstream_response ──
add("store_nonstream_response", {"snapshot": None, "replay_record": None})
add("store_nonstream_response", {"snapshot": ["r1", {"x": 1}], "conversation": None,
                                 "replay_record": None})
add("store_nonstream_response", {"snapshot": ["r1", {}], "conversation": "c1",
                                 "terminal_replay": False, "replay_record": None})
add("store_nonstream_response", {"snapshot": ["r1", {}], "conversation": "c1",
                                 "terminal_replay": True, "replay_record": None,
                                 "seams": {"store.get_conversation": None}})
add("store_nonstream_response", {"snapshot": ["r1", {}], "conversation": "c1",
                                 "terminal_replay": True, "replay_record": None,
                                 "seams": {"store.get_conversation": "r1"}})
add("store_nonstream_response", {"snapshot": ["r1", {}], "conversation": "c1",
                                 "terminal_replay": True, "replay_record": None,
                                 "seams": {"store.get_conversation": "other"}})
add("store_nonstream_response", {"snapshot": None,
                                 "replay_record": ["k1", {"fingerprint": "f"}]})
add("store_nonstream_response", {"snapshot": ["r1", {}], "conversation": "",
                                 "terminal_replay": True, "replay_record": None})
add("store_nonstream_response", {"snapshot": ["r1"], "replay_record": None})
add("store_nonstream_response", {"snapshot": ["r1", {}, 3], "replay_record": None})
add("store_nonstream_response", {"snapshot": ["r1", {}], "conversation": "c1",
                                 "terminal_replay": True, "replay_record": None,
                                 "seams": {"store.get_conversation": None,
                                           "store.put": {"__raise__": "OSError",
                                                         "msg": "disk"}}})

# ── _parse_chat_messages ──
add("parse_chat_messages", {"messages": []})
add("parse_chat_messages", {"messages": [{"role": "system", "content": "be nice"}],
                            "seams": {"normalize_chat_content": "be nice"}})
add("parse_chat_messages", {"messages": [{"role": "system", "content": "a"},
                                         {"role": "system", "content": "b"}],
                            "seams": {"normalize_chat_content": ["a", "b"]}})
add("parse_chat_messages", {"messages": [{"role": "user", "content": "hi"},
                                         {"role": "assistant", "content": "yo"}],
                            "seams": {"normalize_multimodal_content": ["hi", "yo"]}})
add("parse_chat_messages", {"messages": [{"role": "system", "content": "s"},
                                         {"role": "user", "content": "u"},
                                         {"role": "tool", "content": "t"}],
                            "seams": {"normalize_chat_content": "s",
                                      "normalize_multimodal_content": "u"}})
add("parse_chat_messages", {"messages": [{"role": "user", "content": "bad"}],
                            "seams": {"normalize_multimodal_content": {"__raise__": "ValueError",
                                                                       "msg": "invalid_image_url:bad"},
                                      "multimodal_validation_error": MVE_BODY}})
add("parse_chat_messages", {"messages": [{"role": "user", "content": "x"}],
                            "seams": {"normalize_multimodal_content": {"__raise__": "TypeError",
                                                                       "msg": "nope"}}})
add("parse_chat_messages", {"messages": None})
add("parse_chat_messages", {"messages": [123]})
add("parse_chat_messages", {"messages": [{"role": "user"}],
                            "seams": {"normalize_multimodal_content": "d"}})
add("parse_chat_messages", {"messages": [{"content": "x"}]})
add("parse_chat_messages", {"messages": [{"role": "system", "content": "a"},
                                         {"role": "system", "content": "b"}],
                            "seams": {"normalize_chat_content": [123, "b"]}})
add("parse_chat_messages", {"messages": [{"role": "system", "content": "a"},
                                         {"role": "system", "content": "b"}],
                            "seams": {"normalize_chat_content": [None, "b"]}})
add("parse_chat_messages", {"messages": [{"role": "assistant", "content": "x"},
                                         {"role": "user", "content": "bad"}],
                            "seams": {"normalize_multimodal_content": ["ok",
                                                                       {"__raise__": "ValueError",
                                                                        "msg": "invalid_content_part:bad"}],
                                      "multimodal_validation_error": MVE_BODY}})

# ── _parse_responses_input ──
add("parse_responses_input", {"input": "hello"})
add("parse_responses_input", {"input": ["a", "b"]})
add("parse_responses_input", {"input": [{"type": "reasoning", "id": "rs_1"}, "x"]})
add("parse_responses_input", {"input": [{"role": "assistant", "content": "hi"}],
                            "seams": {"normalize_multimodal_content": "hi"}})
add("parse_responses_input", {"input": [{"content": "hi"}],
                            "seams": {"normalize_multimodal_content": "hi"}})
add("parse_responses_input", {"input": [123, None, [], {"role": "user", "content": "ok"}],
                            "seams": {"normalize_multimodal_content": "ok"}})
add("parse_responses_input", {"input": 123, "seams": {"error_response": ER_BODY}})
add("parse_responses_input", {"input": None, "seams": {"error_response": ER_BODY}})
add("parse_responses_input", {"input": [{"role": "user", "content": "x"}],
                            "seams": {"normalize_multimodal_content": {"__raise__": "ValueError",
                                                                       "msg": "unsupported_content_type:file"},
                                      "multimodal_validation_error": MVE_BODY}})
add("parse_responses_input", {"input": [{"role": "user", "content": "x"}],
                            "seams": {"normalize_multimodal_content": {"__raise__": "TypeError",
                                                                       "msg": "nope"}}})
add("parse_responses_input", {"input": {"a": 1}, "seams": {"error_response": ER_BODY}})
add("parse_responses_input", {"input": [{"role": 123, "content": "x"}],
                            "seams": {"normalize_multimodal_content": "x"}})
add("parse_responses_input", {"input": [{"role": "user", "content": None}],
                            "seams": {"normalize_multimodal_content": ""}})

# ── _parse_conversation_history ──
add("parse_conversation_history", {"history": []})
add("parse_conversation_history", {"history": [{"role": "user", "content": "hi"}],
                                 "seams": {"normalize_multimodal_content": "hi"}})
add("parse_conversation_history", {"history": [{"role": 123, "content": "x"}],
                                 "seams": {"normalize_multimodal_content": "x"}})
add("parse_conversation_history", {"history": [{"role": None, "content": "x"}],
                                 "seams": {"normalize_multimodal_content": "x"}})
add("parse_conversation_history", {"history": [{"role": True, "content": "x"}],
                                 "seams": {"normalize_multimodal_content": "x"}})
add("parse_conversation_history", {"history": [{"type": "reasoning"}]})
add("parse_conversation_history", {"history": [{"content": "x"}],
                                 "seams": {"error_response": ER_BODY}})
add("parse_conversation_history", {"history": [{"role": "user"}],
                                 "seams": {"error_response": ER_BODY}})
add("parse_conversation_history", {"history": "notlist",
                                 "seams": {"error_response": ER_BODY}})
add("parse_conversation_history", {"history": None,
                                 "seams": {"error_response": ER_BODY}})
add("parse_conversation_history", {"history": [{"role": "user", "content": "x"}],
                                 "seams": {"normalize_multimodal_content": {"__raise__": "ValueError",
                                                                            "msg": "invalid_content_part:bad"},
                                           "multimodal_validation_error": MVE_BODY}})
add("parse_conversation_history", {"history": [123],
                                 "seams": {"error_response": ER_BODY}})
add("parse_conversation_history", {"history": [{"role": "user", "content": "ok"},
                                               {"role": "tool"}],
                                 "seams": {"normalize_multimodal_content": "ok",
                                           "error_response": ER_BODY}})
add("parse_conversation_history", {"history": [{"role": 2.5, "content": "x"}],
                                 "seams": {"normalize_multimodal_content": "x"}})

# ── _select_request_route ──
add("select_request_route", {"body": {"model": "x"}, "model_alias": "x",
                             "model_name": "hermes-agent", "direct_model_requests": True,
                             "session_id": "s1", "gateway_session_key": "k1",
                             "seams": {"resolve_route": {"provider": "openai"},
                                       "request_agent_overrides": {"requested_model": "x"},
                                       "request_route_conflict_error": None}})
add("select_request_route", {"body": {"model": "x"}, "model_alias": "x",
                             "model_name": "hermes-agent", "direct_model_requests": False,
                             "session_id": "s1", "gateway_session_key": "k1",
                             "seams": {"resolve_route": {"provider": "openai"},
                                       "request_agent_overrides": {"requested_model": "x"},
                                       "request_route_conflict_error": "Model route 'x' is pinned.",
                                       "error_response": ER_BODY}})
add("select_request_route", {"body": {}, "model_alias": "x",
                             "model_name": "hermes-agent", "direct_model_requests": True,
                             "session_id": "s1", "gateway_session_key": "k1",
                             "seams": {"resolve_route": {"provider": "openai"},
                                       "request_agent_overrides": {},
                                       "request_route_conflict_error": None}})
add("select_request_route", {"body": {}, "model_alias": "x",
                             "model_name": None, "direct_model_requests": True,
                             "session_id": "s1", "gateway_session_key": "k1",
                             "seams": {"resolve_route": None,
                                       "request_agent_overrides": {"requested_model": "x"},
                                       "request_route_conflict_error": None}})
add("select_request_route", {"body": {}, "model_alias": "x",
                             "model_name": "hermes-agent", "direct_model_requests": True,
                             "session_id": "s1", "gateway_session_key": "k1",
                             "seams": {"resolve_route": {"provider": "openai"},
                                       "request_agent_overrides": None}})
add("select_request_route", {"body": {}, "model_alias": "x",
                             "model_name": "hermes-agent", "direct_model_requests": True,
                             "session_id": "s1", "gateway_session_key": "k1",
                             "seams": {"resolve_route": {"__raise__": "ValueError",
                                                         "msg": "unknown alias"}}})
add("select_request_route", {"body": {"provider": "p"}, "model_alias": "m",
                             "model_name": "hermes-agent", "direct_model_requests": True,
                             "session_id": "s1", "gateway_session_key": "k1",
                             "seams": {"resolve_route": {"provider": "openai"},
                                       "request_agent_overrides": {"requested_provider": "p",
                                                                   "requested_model": "m"},
                                       "request_route_conflict_error": None}})
add("select_request_route", {"body": {}, "model_alias": "x",
                             "model_name": "hermes-agent", "direct_model_requests": True,
                             "session_id": None, "gateway_session_key": None,
                             "seams": {"resolve_route": {"provider": "openai"},
                                       "request_agent_overrides": {"requested_model": "x"},
                                       "request_route_conflict_error": None}})
add("select_request_route", {"body": {}, "model_alias": "x",
                             "model_name": "hermes-agent", "direct_model_requests": True,
                             "session_id": "s1", "gateway_session_key": "k1",
                             "seams": {"resolve_route": {"provider": "openai"},
                                       "request_agent_overrides": {"requested_model": "x"},
                                       "request_route_conflict_error": ""}})

# ── _cap_history_tool_outputs ──
add("cap_history_tool_outputs", {"history": [], "max_chars": 0})
add("cap_history_tool_outputs", {"history": [{"role": "user", "content": "abcdefghij"}],
                                 "max_chars": 0})
add("cap_history_tool_outputs", {"history": [{"role": "tool", "content": "abcdefghij"}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "user", "content": "abcdefghij"}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "assistant", "tool_calls": [
                                     {"function": {"arguments": "{\"q\": \"abcdefghij\"}"}}]}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "assistant", "tool_calls": [
                                     {"function": {"arguments": "{\"a\":1}"}}]}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "assistant", "tool_calls": [
                                     {"function": {"arguments": "not json"}}]}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "assistant", "tool_calls": [
                                     {"function": {"arguments": "[1,2,3]"}}]}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "assistant", "tool_calls": [
                                     123, {"function": "notdict"},
                                     {"function": {"arguments": 42}}]}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "tool", "content": "short"}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [123, "x", None], "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "tool", "content": "你好世界"}],
                                 "max_chars": 2})
add("cap_history_tool_outputs", {"history": None, "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "assistant", "tool_calls": [
                                     {"function": {"arguments": "{\"bad\""}}]}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "assistant", "tool_calls": [
                                     {"function": {"arguments": "{\"a\": {\"b\": \"abcdefghij\"}}"}}]}],
                                 "max_chars": 5})
add("cap_history_tool_outputs", {"history": {"a": 1}, "max_chars": 0})
add("cap_history_tool_outputs", {"history": {"x": 1, "y": 2}, "max_chars": 5})
add("cap_history_tool_outputs", {"history": [{"role": "tool", "content": "abcdefghij",
                                              "extra": "keep"}], "max_chars": 5})

# ── _build_response_conversation_history ──
add("build_response_conversation_history", {"conversation_history": [], "user_message": "hi",
                                            "result": None, "final_response": "ok"})
add("build_response_conversation_history", {"conversation_history": [{"role": "user", "content": "old"}],
                                            "user_message": "hi",
                                            "result": {"messages": []}, "final_response": "ok"})
add("build_response_conversation_history", {"conversation_history": [{"role": "user", "content": "old"}],
                                            "user_message": "hi",
                                            "result": {"messages": [{"role": "assistant", "content": "a"}]},
                                            "final_response": "ok",
                                            "seams": {"turn_start_index": 0}})
add("build_response_conversation_history", {"conversation_history": [{"role": "user", "content": "old"}],
                                            "user_message": "hi",
                                            "result": {"messages": [{"role": "assistant", "content": "a"}]},
                                            "final_response": "ok",
                                            "seams": {"turn_start_index": 2}})
add("build_response_conversation_history", {"conversation_history": [{"role": "user", "content": "old"}],
                                            "user_message": "hi",
                                            "result": {"messages": [{"role": "assistant", "content": "a"}],
                                                       "_compressed": True},
                                            "final_response": "ok",
                                            "seams": {"turn_start_index": 0}})
add("build_response_conversation_history", {"conversation_history": [], "user_message": "hi",
                                            "result": {"_turn_user_row": {"role": "user",
                                                                          "content": "canonical",
                                                                          "api_content": "x"},
                                                       "messages": []},
                                            "final_response": "ok"})
add("build_response_conversation_history", {"conversation_history": [{"role": "user", "content": "old"}],
                                            "user_message": "hi",
                                            "result": {"messages": [{"role": "assistant", "content": "a"}],
                                                       "_turn_user_row": {"role": "user", "content": "c"}},
                                            "final_response": "ok",
                                            "seams": {"turn_start_index": 0}})
add("build_response_conversation_history", {"conversation_history": [], "user_message": "hi",
                                            "result": {"messages": [{"role": "tool", "content": "abcdefghij"}]},
                                            "final_response": "ok", "tool_output_max_chars": 5,
                                            "seams": {"turn_start_index": 0}})
add("build_response_conversation_history", {"conversation_history": None, "user_message": "hi",
                                            "result": None, "final_response": "ok"})
add("build_response_conversation_history", {"conversation_history": [], "user_message": "hi",
                                            "result": "str", "final_response": None})
add("build_response_conversation_history", {"conversation_history": [], "user_message": "hi",
                                            "result": {"messages": [{"role": "assistant", "content": "a"}]},
                                            "final_response": "ok",
                                            "seams": {"turn_start_index": {"__raise__": "OSError",
                                                                           "msg": "db"}}})
add("build_response_conversation_history", {"conversation_history": [], "user_message": None,
                                            "result": None, "final_response": "ok"})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        raise SystemExit("funcs with <5 cases: %s" % thin)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    print("wrote %s: %d cases across %d funcs" % (OUT, len(CASES), len(counts)))
    for k in sorted(counts):
        print("  %-36s %d" % (k, counts[k]))


if __name__ == "__main__":
    main()
