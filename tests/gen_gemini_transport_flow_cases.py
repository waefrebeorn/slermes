#!/usr/bin/env python3
"""Generate tests/fixtures/gemini_transport_flow.jsonl for the Gemini native
transport UNCLAIMED-remainder FLOW port
(src/agent/transports/gemini_transport_flow.c; live module
agent/gemini_native_adapter.py -- the task's agent/transports/gemini.py path
does not exist in this quarry snapshot; the Gemini transport IS
agent/gemini_native_adapter.py, "OpenAI-compatible facade over Google AI
Studio's native Gemini API").

Case shape: {"func": <name>, "in": {...args..., "io": {seam: script}}}.
Scripts (shared contract with the C universal seam gem_call and the oracle):
  - MISSING key            -> fail-closed RuntimeError("no script for <name>")
  - null                   -> None result
  - {"$seq": [v1, v2, ...]}-> one result per call; exhaustion raises
                              RuntimeError("script exhausted: <name>")
  - {"__raise__": cls, "msg": m} -> raises; trace ret is the WHOLE marker
  - any other value        -> reused on every call (a bare ARRAY is a LIST
                              RESULT, not a queue -- wrap queues in $seq)

Seam names are the LIVE collaborator names (underscores kept):
  hermes_user_agent                 hermes_cli.version_info (module-top import)
  is_vertex_express_base_url        CLAIMED sibling (gemini_native_adapter_ports.c)
  prepare_gemini_tool_parameters    agent.gemini_schema (module-top import)
  sanitize_gemini_tool_parameters   agent.gemini_schema (module-top import)
  uuid4                             uuid.uuid4() (result = 32-char hex string)
  time_time                         time.time() (result = epoch number)
  logger_debug                      module logger.debug(fmt, arg)
  gemini_http_error                 CLAIMED sibling; result is an EXCEPTION
                                    DESCRIPTOR {"message": m, ...} materialized
                                    to a real GeminiAPIError on the live side
  _effective_gemini_max_output_tokens  CLAIMED sibling (cct_* port)

Fixture domain (documented in the port header):
  - ASCII strings only. str.strip transcribed byte-wise (White_Space incl
    \\x1c-\\x1f); int() strips the NARROWER set (no \\x1c-\\x1f).
  - Non-str scalar AttributeError raise positions use int/bool/str/list/null
    only (no floats -- JSON numbers lose int/float identity for the type name).
  - _dump_call_args / _tool_call_slot args values: str/int/bool/null/list/dict
    only (no floats -- json.dumps renders 1.0 vs 1 differently).
  - _long_decode_cap max_output_tokens budget: int or non-number (isinstance
    int identity); max_tokens int/str/bool/null (int() coercion domain).
  - _usage_from_metadata count values: int/str/bool/null/float (int() truncates
    floats; bad str raises ValueError with the live message).
  - _inline_data_part base64 domain (probe-verified py3.14 non-strict):
    canonical padded (roundtrip identity), junk-only (-> ""), canonical with
    discarded junk/extra padding, missing padding / %4==1 data chars (-> None
    via the except Exception), non-ASCII (-> None). No mid-string padding.
  - _parse_sse_line / _error_object JSON: strict grammar, no NaN/Infinity,
    \\u escapes ASCII-range only, depth <= 8.
  - _tool_call_slot slots carry index/id/last_arguments (the live state model);
    tool_call_indices insertion order is part of the contract.
  - Scripted __raise__ markers carry ONLY cls+msg (no GeminiAPIError fields);
    the real GeminiAPIError raise (raise_on_prompt_block) carries code /
    status_code / retry_after / details on both sides.
  - _response_text response doubles: {"text": s} attribute double, or
    {"__raise__": cls, "msg": m} raising-text double, or a raw non-dict value
    (AttributeError swallowed by except Exception).
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "gemini_transport_flow.jsonl"


def seq(*vals):
    return {"$seq": list(vals)}


def rais(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []


def add(*cases):
    CASES.extend(cases)


STUDIO = "https://generativelanguage.googleapis.com/v1beta"
EXPRESS = "https://aiplatform.googleapis.com/v1beta1/publishers/google"

# ── _api_client ─────────────────────────────────────────────────────────────
add(
    case("_api_client", io={"hermes_user_agent": "hermes-agent/1.2.3"}),
    case("_api_client", io={"hermes_user_agent": ""}),
    case("_api_client", io={"hermes_user_agent": None}),
    case("_api_client", io={"hermes_user_agent": rais("RuntimeError", "version gone")}),
    case("_api_client", io={}),                                   # fail-closed
    case("_api_client", io={"hermes_user_agent": rais("SystemExit", "bye")}),
)

# ── _response_text ──────────────────────────────────────────────────────────
add(
    case("_response_text", response={"text": "body text"}),
    case("_response_text", response={"text": ""}),
    case("_response_text", response={"text": None}),
    case("_response_text", response={}),                          # absent attr -> ""
    case("_response_text", response=None),                        # AttributeError swallowed
    case("_response_text", response="plain"),
    case("_response_text", response={"__raise__": "RuntimeError", "msg": "stream not read"}),
    case("_response_text", response={"__raise__": "ValueError", "msg": "closed"}),
    case("_response_text", response={"__raise__": "KeyboardInterrupt", "msg": "int"}),
    case("_response_text", response={"__raise__": "GeneratorExit", "msg": "gen"}),
)

# ── wrong_gemini_surface_guidance ───────────────────────────────────────────
add(
    case("wrong_gemini_surface_guidance", base_url=EXPRESS, api_key="AQ.x",
         err_status="PERMISSION_DENIED", io={"is_vertex_express_base_url": True}),
    case("wrong_gemini_surface_guidance", base_url=STUDIO, api_key="AQ.abc",
         err_status="PERMISSION_DENIED", io={"is_vertex_express_base_url": False}),
    case("wrong_gemini_surface_guidance", base_url=STUDIO, api_key="AIzaabc",
         err_status="PERMISSION_DENIED", io={"is_vertex_express_base_url": False}),
    case("wrong_gemini_surface_guidance", base_url="  " + STUDIO.upper() + "  ",
         api_key="  aq.Z  ", err_status=" permission_denied ",
         io={"is_vertex_express_base_url": False}),
    case("wrong_gemini_surface_guidance", base_url=STUDIO, api_key="AQ.x",
         err_status="INVALID_ARGUMENT", io={}),                   # no seam fired
    case("wrong_gemini_surface_guidance", base_url=None, api_key=None,
         err_status=None, io={}),
    case("wrong_gemini_surface_guidance", base_url="https://proxy.example/v1beta",
         api_key="AQ.x", err_status="PERMISSION_DENIED",
         io={"is_vertex_express_base_url": False}),
    case("wrong_gemini_surface_guidance", base_url=EXPRESS, api_key="AQ.x",
         err_status="PERMISSION_DENIED", io={}),                  # fail-closed
    case("wrong_gemini_surface_guidance", base_url=EXPRESS, api_key="AQ.x",
         err_status="PERMISSION_DENIED",
         io={"is_vertex_express_base_url": rais("RuntimeError", "table gone")}),
    case("wrong_gemini_surface_guidance", base_url=STUDIO, api_key="AQ.x",
         err_status=5, io={}),                                    # int.strip AttributeError
)

# ── _text_of ────────────────────────────────────────────────────────────────
add(
    case("_text_of", item="plain"),
    case("_text_of", item={"type": "text", "text": "hello"}),
    case("_text_of", item={"type": "text", "text": ""}),
    case("_text_of", item={"type": "text", "text": 5}),           # non-str text -> None
    case("_text_of", item={"type": "image_url", "text": "x"}),
    case("_text_of", item={"text": "x"}),                         # no type
    case("_text_of", item=None),
    case("_text_of", item=5),
    case("_text_of", item=["a"]),
)

# ── _inline_data_part ───────────────────────────────────────────────────────
IMG = {"type": "image_url", "image_url": {"url": "data:image/png;base64,aGVsbG8="}}
add(
    case("_inline_data_part", item=IMG),
    case("_inline_data_part", item={"type": "image_url",
         "image_url": {"url": "data:image/jpeg;name=x,!!!!"}}),   # junk-only -> ""
    case("_inline_data_part", item={"type": "image_url",
         "image_url": {"url": "data:image/png,aG!VsbG8=!!"}}),    # discarded junk
    case("_inline_data_part", item={"type": "image_url",
         "image_url": {"url": "data:image/png,aGVsbG8=="}}),      # extra padding
    case("_inline_data_part", item={"type": "image_url",
         "image_url": {"url": "data:image/png,aGVsbG8"}}),        # missing padding -> None
    case("_inline_data_part", item={"type": "image_url",
         "image_url": {"url": "data:image/png,A"}}),              # %4==1 -> None
    case("_inline_data_part", item={"type": "image_url",
         "image_url": {"url": "data:image/png"}}),                # no comma -> None
    case("_inline_data_part", item={"type": "image_url",
         "image_url": {"url": "data:imagebase64,aGVsbG8="}}),     # no colon -> None
    case("_inline_data_part", item={"type": "image_url",
         "image_url": {"url": "https://x/y.png"}}),               # not data: -> None
    case("_inline_data_part", item={"type": "text", "image_url": {"url": "data:img/x,aGVsbG8="}}),
    case("_inline_data_part", item={"type": "image_url", "image_url": None}),
    case("_inline_data_part", item={"type": "image_url", "image_url": {"url": None}}),
    case("_inline_data_part", item={"type": "image_url", "image_url": "oops"}),  # .get AttributeError
    case("_inline_data_part", item="oops"),                       # item.get AttributeError
    case("_inline_data_part", item=None),
)

# ── _multimodal_part ────────────────────────────────────────────────────────
add(
    case("_multimodal_part", item="plain"),
    case("_multimodal_part", item=""),                            # str -> {"text": ""}
    case("_multimodal_part", item={"type": "text", "text": "hello"}),
    case("_multimodal_part", item={"type": "text", "text": ""}),  # falsy text, dict -> None
    case("_multimodal_part", item=IMG),
    case("_multimodal_part", item={"type": "image_url",
         "image_url": {"url": "data:image/png,A"}}),              # bad b64 -> None
    case("_multimodal_part", item=None),
    case("_multimodal_part", item=5),
    case("_multimodal_part", item=["x"]),
)

# ── _tool_call_id ───────────────────────────────────────────────────────────
add(
    case("_tool_call_id", tool_call={"id": "call_1"}),
    case("_tool_call_id", tool_call={"id": "", "call_id": "c2"}),
    case("_tool_call_id", tool_call={"id": None, "call_id": "c3"}),
    case("_tool_call_id", tool_call={"call_id": "c4"}),
    case("_tool_call_id", tool_call={}),
    case("_tool_call_id", tool_call={"id": 5}),                   # str(5)
    case("_tool_call_id", tool_call={"id": True}),                # str(True)
    case("_tool_call_id", tool_call={"id": 0, "call_id": False}), # falsy -> ""
    case("_tool_call_id", tool_call=None),                        # .get AttributeError
)

# ── _looks_like_json_schema ─────────────────────────────────────────────────
add(
    case("_looks_like_json_schema", node={"$ref": "#/$defs/x"}),
    case("_looks_like_json_schema", node={"$ref": "https://x/y"}),
    case("_looks_like_json_schema", node={"$ref": 5}),
    case("_looks_like_json_schema", node={"a": {"b": [{"$ref": "#/x"}]}}),
    case("_looks_like_json_schema", node=[{"$ref": "#/x"}, 1]),
    case("_looks_like_json_schema", node={"type": "object", "properties": {"a": {"type": "string"}}}),
    case("_looks_like_json_schema", node=[]),
    case("_looks_like_json_schema", node={}),
    case("_looks_like_json_schema", node="text"),
    case("_looks_like_json_schema", node=None),
)

# ── _has_function_response ──────────────────────────────────────────────────
add(
    case("_has_function_response", content={"parts": [{"functionResponse": {"name": "f"}}]}),
    case("_has_function_response", content={"parts": [{"text": "x"}]}),
    case("_has_function_response", content={"parts": []}),
    case("_has_function_response", content={}),                   # default []
    case("_has_function_response", content={"parts": ["str", 5, None]}),
    case("_has_function_response", content={"parts": None}),      # TypeError not iterable
    case("_has_function_response", content=None),                 # .get AttributeError
)

# ── _merge_alternating ──────────────────────────────────────────────────────
U = lambda t: {"role": "user", "parts": [{"text": t}]}
M = lambda t: {"role": "model", "parts": [{"text": t}]}
FR = {"role": "user", "parts": [{"functionResponse": {"name": "f", "response": {}}}]}
add(
    case("_merge_alternating", contents=[]),
    case("_merge_alternating", contents=[U("a"), M("b"), U("c")]),
    case("_merge_alternating", contents=[U("a"), U("b")]),        # same-role merge
    case("_merge_alternating", contents=[M("a"), M("b"), M("c")]),
    case("_merge_alternating", contents=[FR, U("human")]),        # placeholder interposed
    case("_merge_alternating", contents=[U("human"), FR]),        # vice versa
    case("_merge_alternating", contents=[FR, {"role": "user", "parts": [
        {"functionResponse": {"name": "g", "response": {}}}]}]),  # parallel FR merge
    case("_merge_alternating", contents=[U("a"), FR, U("b"), M("c"), U("d")]),
    case("_merge_alternating", contents=[{"parts": [{"text": "x"}]}, U("y")]),  # prev role KeyError
    case("_merge_alternating", contents=[U("a"), {"role": "user"}]),            # content parts KeyError
    case("_merge_alternating", contents=[U("a"), {"parts": []}]),               # content role KeyError
)

# ── _function_declaration ───────────────────────────────────────────────────
TOOL = {"type": "function", "function": {"name": "f", "description": "d",
        "parameters": {"type": "object"}}}
add(
    case("_function_declaration", tool=TOOL, json_schema=False,
         io={"sanitize_gemini_tool_parameters": {"type": "object", "properties": {}}}),
    case("_function_declaration", tool=TOOL, json_schema=True,
         io={"prepare_gemini_tool_parameters": {"type": "object", "$defs": {}}}),
    case("_function_declaration", tool={"function": {"name": "f"}}, json_schema=False, io={}),
    case("_function_declaration", tool={"function": {"name": "f", "description": "",
         "parameters": []}}, json_schema=False, io={}),           # non-dict params
    case("_function_declaration", tool={"function": {"name": ""}}, json_schema=False, io={}),
    case("_function_declaration", tool={"function": {"name": 5}}, json_schema=False, io={}),
    case("_function_declaration", tool={"function": "oops"}, json_schema=False, io={}),
    case("_function_declaration", tool={"function": None}, json_schema=False, io={}),
    case("_function_declaration", tool=None, json_schema=False, io={}),
    case("_function_declaration", tool="oops", json_schema=False, io={}),
    case("_function_declaration", tool=TOOL, json_schema=False, io={}),          # fail-closed
    case("_function_declaration", tool=TOOL, json_schema=True,
         io={"prepare_gemini_tool_parameters": rais("ValueError", "bad schema")}),
    case("_function_declaration", tool=TOOL, json_schema=False,
         io={"sanitize_gemini_tool_parameters": rais("GeneratorExit", "gen")}),
)

# ── _long_decode_cap ────────────────────────────────────────────────────────
add(
    case("_long_decode_cap", max_tokens=100, extra_body={"max_output_tokens": 200}),
    case("_long_decode_cap", max_tokens=300, extra_body={"max_output_tokens": 200}),
    case("_long_decode_cap", max_tokens=None, extra_body={"max_output_tokens": 200}),
    case("_long_decode_cap", max_tokens=100, extra_body={"max_output_tokens": 0}),
    case("_long_decode_cap", max_tokens=100, extra_body={"max_output_tokens": "200"}),
    case("_long_decode_cap", max_tokens=100, extra_body={"max_output_tokens": None}),
    case("_long_decode_cap", max_tokens=100, extra_body={}),
    case("_long_decode_cap", max_tokens="50", extra_body={"max_output_tokens": 60}),
    case("_long_decode_cap", max_tokens="abc", extra_body={"max_output_tokens": 60}),  # ValueError
    case("_long_decode_cap", max_tokens=True, extra_body={"max_output_tokens": 2}),
    case("_long_decode_cap", max_tokens=100, extra_body=None),    # .get AttributeError
)

# ── _translate_response_format ──────────────────────────────────────────────
RF = {"type": "json_schema", "json_schema": {"name": "s", "schema": {"type": "object"}}}
add(
    case("_translate_response_format", response_format={"type": "json_object"},
         json_schema=False, io={}),
    case("_translate_response_format", response_format=RF, json_schema=False,
         io={"sanitize_gemini_tool_parameters": {"type": "object"}}),
    case("_translate_response_format", response_format=RF, json_schema=True,
         io={"prepare_gemini_tool_parameters": {"type": "object", "anyOf": []}}),
    case("_translate_response_format", response_format={"type": "json_schema"},
         json_schema=False, io={}),                               # no spec -> mime only
    case("_translate_response_format", response_format={"type": "json_schema",
         "json_schema": {"name": "s"}}, json_schema=False, io={}),  # no schema key
    case("_translate_response_format", response_format={"type": "json_schema",
         "json_schema": "oops"}, json_schema=False, io={}),
    case("_translate_response_format", response_format={"type": "json_schema",
         "json_schema": {"schema": []}}, json_schema=False, io={}),
    case("_translate_response_format", response_format={"type": "text"}, json_schema=False, io={}),
    case("_translate_response_format", response_format=None, json_schema=False, io={}),
    case("_translate_response_format", response_format="oops", json_schema=False, io={}),
    case("_translate_response_format", response_format=RF, json_schema=False, io={}),  # fail-closed
    case("_translate_response_format", response_format=RF, json_schema=True,
         io={"prepare_gemini_tool_parameters": rais("KeyboardInterrupt", "int")}),
)

# ── _provider_call_id ───────────────────────────────────────────────────────
add(
    case("_provider_call_id", fc={"id": "fc-1"}),
    case("_provider_call_id", fc={"id": ""}),
    case("_provider_call_id", fc={"id": None}),
    case("_provider_call_id", fc={"id": 5}),                      # non-str -> None
    case("_provider_call_id", fc={}),
    case("_provider_call_id", fc=None),                           # .get AttributeError
)

# ── _new_call_id ────────────────────────────────────────────────────────────
add(
    case("_new_call_id", fc={"id": "fc-9"}, io={}),               # echo, no uuid seam
    case("_new_call_id", fc={"id": ""}, io={"uuid4": "0123456789abcdef0123456789abcdef"}),
    case("_new_call_id", fc={}, io={"uuid4": "abcdef0123456789abcdef0123456789"}),
    case("_new_call_id", fc={"id": 5}, io={"uuid4": "ffffffffffffffffffffffffffffffff"}),
    case("_new_call_id", fc={"id": ""}, io={"uuid4": rais("RuntimeError", "entropy gone")}),
    case("_new_call_id", fc={}, io={}),                           # fail-closed
    case("_new_call_id", fc={}, io={"uuid4": None}),              # None.hex AttributeError
    case("_new_call_id", fc={}, io={"uuid4": rais("SystemExit", "bye")}),
    case("_new_call_id", fc={}, io={"uuid4": "short"}),           # hex shorter than 12
)

# ── _dump_call_args ─────────────────────────────────────────────────────────
add(
    case("_dump_call_args", fc={"args": {"b": 1, "a": "x"}}, kwargs={}),
    case("_dump_call_args", fc={"args": {"b": 1, "a": {"y": 2, "x": [True, None]}}},
         kwargs={"sort_keys": True}),
    case("_dump_call_args", fc={"args": None}, kwargs={}),
    case("_dump_call_args", fc={}, kwargs={}),
    case("_dump_call_args", fc={"args": []}, kwargs={}),
    case("_dump_call_args", fc={"args": "raw"}, kwargs={}),
    case("_dump_call_args", fc={"args": {"s": "q\"b\\n\t\x01"}}, kwargs={}),
    case("_dump_call_args", fc={"args": {"a": 1}}, kwargs={"ensure_ascii": False}),  # dup kw -> {}
    case("_dump_call_args", fc={"args": {"a": 1}}, kwargs={"bogus": 1}),             # bad kw -> {}
    case("_dump_call_args", fc={"args": 0}, kwargs={}),           # falsy -> {}
    case("_dump_call_args", fc=None, kwargs={}),                  # .get AttributeError
)

# ── _usage_from_metadata ────────────────────────────────────────────────────
add(
    case("_usage_from_metadata", usage_meta={"promptTokenCount": 10,
         "candidatesTokenCount": 20, "totalTokenCount": 30}),
    case("_usage_from_metadata", usage_meta={"promptTokenCount": 10,
         "candidatesTokenCount": 20, "totalTokenCount": 40, "thoughtsTokenCount": 10,
         "cachedContentTokenCount": 5}),
    case("_usage_from_metadata", usage_meta={"promptTokenCount": "12",
         "candidatesTokenCount": " 3 ", "totalTokenCount": None}),
    case("_usage_from_metadata", usage_meta={"promptTokenCount": 1.9,
         "candidatesTokenCount": -1.9, "totalTokenCount": True}),
    case("_usage_from_metadata", usage_meta={"promptTokenCount": 10,
         "initialPromptTokenCount": 7, "initialCachedContentTokenCount": 2}),
    case("_usage_from_metadata", usage_meta={"initialPromptTokenCount": 0}),
    case("_usage_from_metadata", usage_meta={}),
    case("_usage_from_metadata", usage_meta={"promptTokenCount": "1_0"}),
    case("_usage_from_metadata", usage_meta={"promptTokenCount": "1.5"}),  # ValueError
    case("_usage_from_metadata", usage_meta={"promptTokenCount": "1\x1c"}),  # ValueError
    case("_usage_from_metadata", usage_meta=None),                # .get AttributeError
)

# ── _envelope ───────────────────────────────────────────────────────────────
CHOICE = {"index": 0, "finish_reason": "stop"}
add(
    case("_envelope", model="gemini-3.7-flash", object_="chat.completion",
         choice=CHOICE, usage={"total_tokens": 5},
         io={"uuid4": "0123456789abcdef0123456789abcdef", "time_time": 1700000000.9}),
    case("_envelope", model="m", object_="chat.completion.chunk", choice=CHOICE,
         usage=None, io={"uuid4": "ffffffffffffffffffffffffffffffff", "time_time": 0}),
    case("_envelope", model="m", object_="chat.completion", choice=None, usage=None,
         io={"uuid4": "abcdef0123456789abcdef0123456789", "time_time": -5.5}),
    case("_envelope", model="m", object_="chat.completion", choice=CHOICE, usage=None,
         io={"uuid4": "0123456789abcdef0123456789abcdef",
             "time_time": rais("RuntimeError", "clock gone")}),
    case("_envelope", model="m", object_="chat.completion", choice=CHOICE, usage=None,
         io={"uuid4": "0123456789abcdef0123456789abcdef"}),       # time fail-closed
    case("_envelope", model="m", object_="chat.completion", choice=CHOICE, usage=None,
         io={"time_time": 1}),                                    # uuid fail-closed
    case("_envelope", model="m", object_="chat.completion", choice=CHOICE, usage=None,
         io={"uuid4": rais("GeneratorExit", "gen"), "time_time": 1}),
)

# ── _tool_call_ns ───────────────────────────────────────────────────────────
add(
    case("_tool_call_ns", name="f", arguments="{}", index=0, call_id="c1",
         extra_content={"google": {"thought_signature": "s"}}),
    case("_tool_call_ns", name="f", arguments="{}", index=2, call_id="c2",
         extra_content=None),
    case("_tool_call_ns", name="f", arguments="{}", index=1, call_id="c3",
         extra_content="not-a-dict"),
    case("_tool_call_ns", name="", arguments="", index=0, call_id="", extra_content={}),
    case("_tool_call_ns", name="f", arguments="{}", index=0, call_id="c4",
         extra_content=[]),
)

# ── _part_text ──────────────────────────────────────────────────────────────
add(
    case("_part_text", part={"text": "hello"}),
    case("_part_text", part={"text": "thought", "thought": True}),
    case("_part_text", part={"text": "x", "thought": False}),
    case("_part_text", part={"text": "x", "thought": 1}),         # is True identity
    case("_part_text", part={"text": ""}),
    case("_part_text", part={"text": 5}),
    case("_part_text", part={"thought": True}),
    case("_part_text", part={}),
    case("_part_text", part=None),                                # .get AttributeError
)

# ── _text_part_signature ────────────────────────────────────────────────────
add(
    case("_text_part_signature", part={"thoughtSignature": "sig"}),
    case("_text_part_signature", part={"thoughtSignature": "sig",
         "functionCall": {"name": "f"}}),                         # fc present -> None
    case("_text_part_signature", part={"thoughtSignature": ""}),
    case("_text_part_signature", part={"thoughtSignature": 5}),
    case("_text_part_signature", part={"thoughtSignature": None}),
    case("_text_part_signature", part={}),
    case("_text_part_signature", part=None),                      # .get AttributeError
)

# ── _part_function_call ─────────────────────────────────────────────────────
add(
    case("_part_function_call", part={"functionCall": {"name": "f", "args": {}}}),
    case("_part_function_call", part={"functionCall": {"name": ""}}),
    case("_part_function_call", part={"functionCall": {"args": {}}}),
    case("_part_function_call", part={"functionCall": {"name": 5}}),  # truthy non-str
    case("_part_function_call", part={"functionCall": "oops"}),
    case("_part_function_call", part={"functionCall": None}),
    case("_part_function_call", part={}),
    case("_part_function_call", part=None),                       # .get AttributeError
)

# ── raise_on_prompt_block ───────────────────────────────────────────────────
add(
    case("raise_on_prompt_block", payload={}),
    case("raise_on_prompt_block", payload={"promptFeedback": {}}),
    case("raise_on_prompt_block", payload={"promptFeedback": None}),
    case("raise_on_prompt_block", payload={"promptFeedback": "oops"}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": "SAFETY"}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": "BLOCKLIST"}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": "OTHER"}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": "safety",
         "blockReasonMessage": "  custom reason  "}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": "SAFETY",
         "blockReasonMessage": "   "}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": "SAFETY",
         "blockReasonMessage": 5}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": 5}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": True}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": ""}}),
    case("raise_on_prompt_block", payload={"promptFeedback": {"blockReason": None}}),
)

# ── _parse_sse_line ─────────────────────────────────────────────────────────
add(
    case("_parse_sse_line", line="data: {\"candidates\": []}", io={}),
    case("_parse_sse_line", line="data: [DONE]", io={}),
    case("_parse_sse_line", line="data: [DONE]\r", io={}),
    case("_parse_sse_line", line="event: ping", io={}),
    case("_parse_sse_line", line="data:{\"a\":1}", io={}),        # no space after colon
    case("_parse_sse_line", line="data: null", io={}),
    case("_parse_sse_line", line="data: [1]", io={}),
    case("_parse_sse_line", line="data: 5", io={}),
    case("_parse_sse_line", line="data: ", io={"logger_debug": None}),
    case("_parse_sse_line", line="data: {bad", io={"logger_debug": None}),
    case("_parse_sse_line", line="data: {\"a\":1,}", io={"logger_debug": None}),
    case("_parse_sse_line", line="data: 01", io={"logger_debug": None}),
    case("_parse_sse_line", line="data: {\"a\": \"\\u0041\"}", io={}),
    case("_parse_sse_line", line="data: {bad", io={}),            # logger fail-closed
    case("_parse_sse_line", line="data: {bad",
         io={"logger_debug": rais("RuntimeError", "log gone")}),
    case("_parse_sse_line", line="data: " + "x" * 300, io={"logger_debug": None}),  # [:200]
)

# ── _tool_call_slot ─────────────────────────────────────────────────────────
SLOT = {"index": 0, "id": "c1", "last_arguments": "{\"a\": 1}"}
add(
    case("_tool_call_slot", fc={"id": "fc-1", "name": "f"}, part={}, part_index=0,
         args_str="{}", tool_call_indices={}),
    case("_tool_call_slot", fc={"id": "fc-1", "name": "f"}, part={}, part_index=0,
         args_str="{}", tool_call_indices={'{"provider_call_id": "fc-1"}': SLOT}),
    case("_tool_call_slot", fc={"name": "f"}, part={"thoughtSignature": "sig"},
         part_index=1, args_str='{"a": 1}', tool_call_indices={}),
    case("_tool_call_slot", fc={"name": "f"}, part={}, part_index=0,
         args_str='{"a": 1, "b": 2}',
         tool_call_indices={'{"name": "f", "part_index": 0, "thought_signature": ""}': SLOT}),
    case("_tool_call_slot", fc={"name": "f"}, part={}, part_index=0,
         args_str='{"z": 9}',
         tool_call_indices={'{"name": "f", "part_index": 0, "thought_signature": ""}': SLOT}),
    case("_tool_call_slot", fc={"name": "f"}, part={}, part_index=0,
         args_str='{"a": 1, "b": 2}',
         tool_call_indices={
             '{"name": "f", "part_index": 0, "thought_signature": ""}': SLOT,
             '{"name": "f", "part_index": 0, "thought_signature": ""}#1':
                 {"index": 1, "id": "c2", "last_arguments": '{"a": 1}'}}),
    case("_tool_call_slot", fc={"name": "f"}, part={"thoughtSignature": 5},
         part_index=2, args_str="{}", tool_call_indices={}),      # non-str sig -> ""
    case("_tool_call_slot", fc={"name": "f"}, part={}, part_index=0,
         args_str="{}", tool_call_indices={"k#0": {"index": 0, "id": "c", "last_arguments": ""}}),
    case("_tool_call_slot", fc={}, part={}, part_index=0, args_str="{}",
         tool_call_indices={}),                                   # fc["name"] KeyError
    case("_tool_call_slot", fc={"name": "f"}, part={}, part_index=0,
         args_str="{}", tool_call_indices={
             '{"name": "f", "part_index": 0, "thought_signature": ""}': {"index": 0}}),  # slot last_arguments KeyError
    case("_tool_call_slot", fc=None, part={}, part_index=0, args_str="{}",
         tool_call_indices={}),                                   # .get AttributeError
)

# ── _error_info ─────────────────────────────────────────────────────────────
EI = {"@type": "type.googleapis.com/google.rpc.ErrorInfo", "reason": "RATE_LIMIT",
      "metadata": {"quota": "100"}}
add(
    case("_error_info", err_obj={"details": [EI]}),
    case("_error_info", err_obj={"details": [EI, {"@type": "type.googleapis.com/google.rpc.ErrorInfo",
         "reason": "SECOND", "metadata": {"x": "1"}}]}),
    case("_error_info", err_obj={"details": [{"@type": "type.googleapis.com/google.rpc.ErrorInfo",
         "metadata": {"m": "1"}}]}),                              # no reason str
    case("_error_info", err_obj={"details": [{"@type": "type.googleapis.com/google.rpc.ErrorInfo",
         "reason": 5, "metadata": []}]}),
    case("_error_info", err_obj={"details": [{"@type": "type.googleapis.com/google.rpc.RetryInfo",
         "reason": "X"}]}),
    case("_error_info", err_obj={"details": ["str", 5, None]}),
    case("_error_info", err_obj={"details": None}),
    case("_error_info", err_obj={"details": "oops"}),
    case("_error_info", err_obj={}),
    case("_error_info", err_obj=None),                            # .get AttributeError
)

# ── _error_object ───────────────────────────────────────────────────────────
add(
    case("_error_object", body_text='{"error": {"status": "RESOURCE_EXHAUSTED", "message": "m"}}'),
    case("_error_object", body_text='{"error": "not-a-dict"}'),
    case("_error_object", body_text='{"other": 1}'),
    case("_error_object", body_text='[1]'),
    case("_error_object", body_text='null'),
    case("_error_object", body_text=''),
    case("_error_object", body_text=None),
    case("_error_object", body_text='{bad'),
    case("_error_object", body_text='{"error": {"message": "m"},}'),
    case("_error_object", body_text='  {"error": {}}  '),
)

# ── GeminiNativeClient._auth_headers ────────────────────────────────────────
add(
    case("GeminiNativeClient._auth_headers", **{"self": {"api_key": "k1"}}),
    case("GeminiNativeClient._auth_headers", **{"self": {"api_key": ""}}),
    case("GeminiNativeClient._auth_headers", **{"self": {"api_key": None}}),
    case("GeminiNativeClient._auth_headers", **{"self": {}}),     # absent attr raise
    case("GeminiNativeClient._auth_headers", **{"self": {"api_key": "k", "extra": 1}}),
)

# ── GeminiNativeClient._http_error ──────────────────────────────────────────
RESP = {"status_code": 429, "text": "quota"}
DESC = {"message": "Gemini HTTP 429 (RESOURCE_EXHAUSTED): quota",
        "code": "gemini_rate_limited", "status_code": 429, "retry_after": 1.5,
        "details": {"status": "RESOURCE_EXHAUSTED"}}
add(
    case("GeminiNativeClient._http_error", **{"self": {"api_key": "k", "base_url": STUDIO},
         "response": RESP, "body_text": None, "io": {"gemini_http_error": DESC}}),
    case("GeminiNativeClient._http_error", **{"self": {"api_key": "k", "base_url": STUDIO},
         "response": RESP, "body_text": "custom body", "io": {"gemini_http_error": DESC}}),
    case("GeminiNativeClient._http_error", **{"self": {"api_key": "k", "base_url": STUDIO},
         "response": RESP, "body_text": "", "io": {"gemini_http_error": DESC}}),
    case("GeminiNativeClient._http_error", **{"self": {"api_key": "k", "base_url": STUDIO},
         "response": RESP, "body_text": None, "io": {"gemini_http_error": None}}),
    case("GeminiNativeClient._http_error", **{"self": {"api_key": "k", "base_url": STUDIO},
         "response": RESP, "body_text": None, "io": {}}),         # fail-closed
    case("GeminiNativeClient._http_error", **{"self": {"api_key": "k", "base_url": STUDIO},
         "response": RESP, "body_text": None,
         "io": {"gemini_http_error": rais("SystemExit", "bye")}}),
    case("GeminiNativeClient._http_error", **{"self": {"api_key": "k"},
         "response": RESP, "body_text": None, "io": {"gemini_http_error": DESC}}),  # base_url absent
)

# ── GeminiNativeClient.effective_output_cap ─────────────────────────────────
add(
    case("GeminiNativeClient.effective_output_cap", max_tokens=100, extra_body=None,
         io={"_effective_gemini_max_output_tokens": 100}),
    case("GeminiNativeClient.effective_output_cap", max_tokens=100,
         extra_body={"max_output_tokens": 500},
         io={"_effective_gemini_max_output_tokens": 500}),
    case("GeminiNativeClient.effective_output_cap", max_tokens=None,
         extra_body={"thinking_config": {"thinkingBudget": 0}},
         io={"_effective_gemini_max_output_tokens": 65535}),
    case("GeminiNativeClient.effective_output_cap", max_tokens=64,
         extra_body={"thinkingConfig": {"includeThoughts": False}},
         io={"_effective_gemini_max_output_tokens": 64}),
    case("GeminiNativeClient.effective_output_cap", max_tokens=64,
         extra_body={"thinking_config": None, "thinkingConfig": {"thinkingLevel": "low"}},
         io={"_effective_gemini_max_output_tokens": 65535}),
    case("GeminiNativeClient.effective_output_cap", max_tokens=64, extra_body="oops",
         io={"_effective_gemini_max_output_tokens": 64}),
    case("GeminiNativeClient.effective_output_cap", max_tokens=64, extra_body={},
         io={}),                                                  # fail-closed
    case("GeminiNativeClient.effective_output_cap", max_tokens=64, extra_body={},
         io={"_effective_gemini_max_output_tokens": rais("ValueError", "bad cap")}),
    case("GeminiNativeClient.effective_output_cap", max_tokens="abc", extra_body={},
         io={"_effective_gemini_max_output_tokens": 65535}),      # int() ValueError
    case("GeminiNativeClient.effective_output_cap", max_tokens=64,
         extra_body={"max_output_tokens": 100},
         io={"_effective_gemini_max_output_tokens": seq(100, 200)}),  # single call, cursor 1
)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [json.dumps(c, ensure_ascii=True, separators=(", ", ": ")) for c in CASES]
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    per = {}
    for c in CASES:
        per[c["func"]] = per.get(c["func"], 0) + 1
    thin = {k: v for k, v in per.items() if v < 5}
    print(f"gen_gemini_transport_flow_cases: {len(CASES)} cases, {len(per)} funcs -> {OUT}")
    if thin:
        raise SystemExit(f"FUNCS WITH <5 CASES: {thin}")


if __name__ == "__main__":
    main()