#!/usr/bin/env python3
"""Generate tests/fixtures/bedrock_transport_flow.jsonl for the Bedrock
Converse transport UNCLAIMED-remainder FLOW port
(src/agent/transports/bedrock_transport_flow.c; live defining module
agent/bedrock_adapter.py -- agent/transports/bedrock.py exists but is a thin
delegation shell whose every method is already claimed by
transports_bedrock_ports.c, so the UNCLAIMED data-path functions are ported
from the module it delegates to).

Case shape: {"func": <name>, "in": {...args..., "io": {seam: script},
"state": {module-state seed}}}.
Scripts (shared contract with the C universal seam bt2_call and the oracle):
  - MISSING key            -> fail-closed RuntimeError("no script for <name>")
  - null                   -> None result
  - {"$seq": [v1, v2, ...]}-> one result per call; exhaustion raises
                              RuntimeError("script exhausted: <name>")
  - {"__raise__": cls, "msg": m} -> raises; trace ret is the WHOLE marker
  - any other value        -> reused on every call (a bare ARRAY is a LIST
                              RESULT, not a queue -- wrap queues in $seq)

Seam names are the LIVE collaborator names (underscores kept):
  parse_bedrock_openai_model_id   CLAIMED sibling; result null or 2-array
  _decode_redacted                CLAIMED sibling; result null or bytes marker
  _parse_tool_args                CLAIMED sibling
  _converse_replay_policy         CLAIMED sibling; result policy str
  strip_redacted_reasoning        CLAIMED sibling; {"__same__":true} = identity
  logger_warning / logger_debug   module logger (args stringified)
  _thinking_kwargs                agent.anthropic_adapter (sys.modules stub)
  is_anthropic_bedrock_model      CLAIMED sibling
  _get_bedrock_control_client     CLAIMED sibling; null -> None client
  get_inference_profile           client method
  resolve_bedrock_region          CLAIMED sibling
  list_foundation_models          client method
  list_inference_profiles         client method (nextToken kwarg when set)
  _extract_provider_from_arn      CLAIMED sibling

Fixture domain (documented in the port header):
  - ASCII strings only; non-str scalar raise positions use int/bool/str/list/
    null (no floats -- JSON numbers lose int/float identity for type names).
  - _tool_call_ns input_dict: no floats (json.dumps renders 1.0 vs 1).
  - _nova effort / _model_entry name: str/int/bool/null (str() domain).
  - bytes markers {"__bytes__"/"__bytearray__": [0..255 ints]}.
  - _filter_reasoning keep: the three ported filter names, null, or "bogus".
  - _anthropic_ordered_blocks tool_calls[].id: str only.
  - recover exc: message string (null -> "None").
  - client inputs: null -> None, any other value -> recording double.
  - state keys: _CACHE_POINT_REJECTIONS (model -> placements array),
    _inference_profile_model_cache (arn -> model id).
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "bedrock_transport_flow.jsonl"


def seq(*vals):
    return {"$seq": list(vals)}


def rais(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []


def add(*cases):
    CASES.extend(cases)


ISSUER = "bedrock.native_assistant"
ARN = "arn:aws:bedrock:us-east-1:123456789012:application-inference-profile/abc123"
ARN_NOREGION = "x-application-inference-profile/abc"
MODEL_ARN = "arn:aws:bedrock:us-east-1::foundation-model/anthropic.claude-3-5-sonnet"
CLAUDE = "anthropic.claude-3-5-sonnet-20240620-v1:0"
CLAUDE3 = "anthropic.claude-3-opus-20240229-v1:0"
NOVA = "amazon.nova-2-lite-v1:0"
GPT = "openai.gpt-6-astra"
CP = {"cachePoint": {"type": "default"}}
RC_TEXT = {"reasoningContent": {"reasoningText": {"text": "think", "signature": "sig"}}}
RC_REDACTED = {"reasoningContent": {"redactedContent": {"__bytes__": [1, 2, 3]}}}
TXT = {"text": "hello"}

# ── bedrock_openai_uses_mantle ──────────────────────────────────────────────
add(
    case("bedrock_openai_uses_mantle", model_id=GPT,
         io={"parse_bedrock_openai_model_id": [GPT, ""]}),
    case("bedrock_openai_uses_mantle", model_id="us." + GPT,
         io={"parse_bedrock_openai_model_id": [GPT, "us"]}),
    case("bedrock_openai_uses_mantle", model_id="x",
         io={"parse_bedrock_openai_model_id": None}),
    case("bedrock_openai_uses_mantle", model_id=GPT, io={}),          # fail-closed
    case("bedrock_openai_uses_mantle", model_id=GPT,
         io={"parse_bedrock_openai_model_id": rais("RuntimeError", "table gone")}),
    case("bedrock_openai_uses_mantle", model_id=GPT,
         io={"parse_bedrock_openai_model_id": rais("SystemExit", "bye")}),
    case("bedrock_openai_uses_mantle", model_id=GPT,
         io={"parse_bedrock_openai_model_id": ["only-one"]}),         # IndexError
    case("bedrock_openai_uses_mantle", model_id=None,
         io={"parse_bedrock_openai_model_id": None}),                 # arg dropped
)

# ── _tool_use_block ─────────────────────────────────────────────────────────
add(
    case("_tool_use_block", tool_use_id="t1", name="fn", input_dict={"a": 1}),
    case("_tool_use_block", tool_use_id="", name="", input_dict={}),
    case("_tool_use_block", tool_use_id=None, name=None, input_dict=None),
    case("_tool_use_block", tool_use_id=5, name=True, input_dict=[1, "x"]),
    case("_tool_use_block", tool_use_id="t", name="n",
         input_dict={"nested": {"k": [1, 2]}}),
)

# ── _reasoning_text_block ───────────────────────────────────────────────────
add(
    case("_reasoning_text_block", text="think", signature="sig"),
    case("_reasoning_text_block", text="think", signature=None),
    case("_reasoning_text_block", text=None, signature="sig"),
    case("_reasoning_text_block", text=5, signature=""),
    case("_reasoning_text_block", text="t", signature=5),
    case("_reasoning_text_block", text="", signature="sig"),
)

# ── _redacted_reasoning_block ───────────────────────────────────────────────
add(
    case("_redacted_reasoning_block", encoded="aGVsbG8=",
         io={"_decode_redacted": {"__bytes__": [104, 101, 108, 108, 111]}}),
    case("_redacted_reasoning_block", encoded="!!!", io={"_decode_redacted": None}),
    case("_redacted_reasoning_block", encoded="aGVsbG8=", io={}),     # fail-closed
    case("_redacted_reasoning_block", encoded="x",
         io={"_decode_redacted": rais("ValueError", "bad b64")}),
    case("_redacted_reasoning_block", encoded="x",
         io={"_decode_redacted": rais("KeyboardInterrupt", "int")}),
    case("_redacted_reasoning_block", encoded=None, io={"_decode_redacted": None}),
)

# ── _anthropic_ordered_blocks ───────────────────────────────────────────────
add(
    case("_anthropic_ordered_blocks",
         msg={"tool_calls": [{"id": "c1", "function": {"arguments": '{"x": 1}'}}],
              "anthropic_content_blocks": [
                  {"type": "thinking", "thinking": "t", "signature": "s"},
                  {"type": "tool_use", "id": "c1", "name": "fn"}]},
         io={"_parse_tool_args": {"x": 1}}),
    case("_anthropic_ordered_blocks",
         msg={"anthropic_content_blocks": [
             {"type": "text", "text": "  "},
             {"type": "text", "text": "keep"},
             {"type": "redacted_thinking", "data": "AQI="}]},
         io={"_decode_redacted": {"__bytes__": [1, 2]}}),
    case("_anthropic_ordered_blocks",
         msg={"anthropic_content_blocks": [
             {"type": "image"}, "not-a-dict",
             {"type": "tool_use", "id": "missing", "name": "fn", "input": {"d": 1}}]},
         io={}),                                                    # no seams fire
    case("_anthropic_ordered_blocks", msg=None, io={}),             # AttributeError
    case("_anthropic_ordered_blocks",
         msg={"tool_calls": [{"id": "c1", "function": "oops"}]}, io={}),
    case("_anthropic_ordered_blocks",
         msg={"tool_calls": [{"id": "c1", "function": {}}]}, io={}),  # fail-closed
    case("_anthropic_ordered_blocks",
         msg={"tool_calls": [{"id": "c1", "function": {"arguments": "{}"}},
                             {"id": "c1", "function": {"arguments": "{}"}}],
              "anthropic_content_blocks": [{"type": "tool_use", "id": "c1"}]},
         io={"_parse_tool_args": seq({"first": 1}, {"last": 2})}),   # dup id: last wins
    case("_anthropic_ordered_blocks", msg={"tool_calls": 5}, io={}),  # TypeError
    case("_anthropic_ordered_blocks",
         msg={"anthropic_content_blocks": [{"type": "redacted_thinking", "data": "x"}]},
         io={"_decode_redacted": rais("RuntimeError", "sealed")}),
    case("_anthropic_ordered_blocks",
         msg={"tool_calls": [None, {"id": "c2", "function": None}],
              "anthropic_content_blocks": [{"type": "tool_use", "id": "c2", "name": "f"}]},
         io={"_parse_tool_args": {}}),
)

# ── _issuer ─────────────────────────────────────────────────────────────────
add(
    case("_issuer", msg={"reasoning_details": [{"type": ISSUER, "model": "m1"}]}),
    case("_issuer", msg={"reasoning_details": [{"type": "thinking", "model": "m"}]}),
    case("_issuer", msg={"reasoning_details": [{"type": ISSUER, "model": 5}]}),
    case("_issuer", msg=None),
    case("_issuer", msg={"reasoning_details": 5}),                  # TypeError
    case("_issuer", msg={"reasoning_details": "xy"}),               # chars -> None
    case("_issuer", msg={"reasoning_details": [None, {"type": ISSUER, "model": "m2"}]}),
    case("_issuer", msg={}),
)

# ── _minted_outside_claude ──────────────────────────────────────────────────
add(
    case("_minted_outside_claude", issuer=None, io={}),
    case("_minted_outside_claude", issuer=CLAUDE,
         io={"_converse_replay_policy": "signed"}),
    case("_minted_outside_claude", issuer=GPT,
         io={"_converse_replay_policy": "issuer"}),
    case("_minted_outside_claude", issuer=5, io={}),                # re TypeError
    case("_minted_outside_claude", issuer=ARN, io={},
         state={"_inference_profile_model_cache": {ARN: ARN}}),     # cache hit, same
    case("_minted_outside_claude", issuer=ARN,
         io={"_converse_replay_policy": "unsigned"},
         state={"_inference_profile_model_cache": {ARN: MODEL_ARN}}),
    case("_minted_outside_claude", issuer=ARN,
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": {"models": [{"modelArn": MODEL_ARN}]},
             "_converse_replay_policy": "signed"},
         state={"_inference_profile_model_cache": {}}),
    case("_minted_outside_claude", issuer=CLAUDE,
         io={"_converse_replay_policy": rais("RuntimeError", "policy gone")}),
    case("_minted_outside_claude", issuer=CLAUDE, io={}),           # fail-closed
    case("_minted_outside_claude", issuer=ARN,
         io={"_get_bedrock_control_client": rais("RuntimeError", "no creds"),
             "logger_debug": None},
         state={"_inference_profile_model_cache": {}}),             # caught -> same -> False
    case("_minted_outside_claude", issuer=ARN,
         io={"_get_bedrock_control_client": rais("SystemExit", "bye")},
         state={"_inference_profile_model_cache": {}}),             # trio passthrough
)

# ── reasoning_minted_outside_claude ─────────────────────────────────────────
add(
    case("reasoning_minted_outside_claude",
         msg={"reasoning_details": [{"type": ISSUER, "model": CLAUDE}]},
         io={"_converse_replay_policy": "signed"}),
    case("reasoning_minted_outside_claude",
         msg={"reasoning_details": [{"type": ISSUER, "model": GPT}]},
         io={"_converse_replay_policy": "issuer"}),
    case("reasoning_minted_outside_claude", msg={"reasoning_details": []}, io={}),
    case("reasoning_minted_outside_claude", msg=None, io={}),
    case("reasoning_minted_outside_claude",
         msg={"reasoning_details": [{"type": ISSUER, "model": ARN}]},
         io={"_converse_replay_policy": "unsigned"},
         state={"_inference_profile_model_cache": {ARN: MODEL_ARN}}),
    case("reasoning_minted_outside_claude",
         msg={"reasoning_details": [{"type": ISSUER, "model": CLAUDE}]}, io={}),
)

# ── _unsigned_reasoning ─────────────────────────────────────────────────────
add(
    case("_unsigned_reasoning", block=RC_TEXT),
    case("_unsigned_reasoning",
         block={"reasoningContent": {"reasoningText": {"text": "   "}}}),
    case("_unsigned_reasoning", block=TXT),
    case("_unsigned_reasoning", block={"reasoningContent": "flat"}),
    case("_unsigned_reasoning", block=None),
    case("_unsigned_reasoning", block={"reasoningContent": {"reasoningText": "flat"}}),
    case("_unsigned_reasoning", block={"reasoningContent": {"reasoningText": {"text": 5}}}),
    case("_unsigned_reasoning",
         block={"reasoningContent": {"reasoningText": {"text": " t "}, "redactedContent": "x"}}),
)

# ── _verbatim ───────────────────────────────────────────────────────────────
add(
    case("_verbatim", block=RC_TEXT),
    case("_verbatim", block=None),
    case("_verbatim", block="plain"),
    case("_verbatim", block=[1, 2]),
    case("_verbatim", block=5),
)

# ── _without_reasoning ──────────────────────────────────────────────────────
add(
    case("_without_reasoning", block={"reasoningContent": {}, "text": "x"}),
    case("_without_reasoning", block=TXT),
    case("_without_reasoning", block="xxreasoningContentyy"),
    case("_without_reasoning", block="plain"),
    case("_without_reasoning", block=None),
    case("_without_reasoning", block=5),
    case("_without_reasoning", block=["reasoningContent"]),
    case("_without_reasoning", block=["other"]),
)

# ── _filter_reasoning ───────────────────────────────────────────────────────
add(
    case("_filter_reasoning", blocks=[RC_TEXT, TXT], keep="_without_reasoning"),
    case("_filter_reasoning", blocks=[RC_TEXT, TXT], keep="_verbatim"),
    case("_filter_reasoning", blocks=[RC_TEXT, TXT, RC_REDACTED], keep="_unsigned_reasoning"),
    case("_filter_reasoning", blocks=[TXT], keep=None),
    case("_filter_reasoning", blocks=[TXT], keep="bogus"),
    case("_filter_reasoning", blocks=None, keep="_verbatim"),
    case("_filter_reasoning", blocks=[], keep=None),
    case("_filter_reasoning", blocks=["not-a-dict"], keep="_unsigned_reasoning"),
    case("_filter_reasoning", blocks=[{"reasoningContent": {"reasoningText": {"text": ""}}}],
         keep="_unsigned_reasoning"),
)

# ── _without_cache_points ───────────────────────────────────────────────────
add(
    case("_without_cache_points", blocks=[TXT, CP, TXT]),
    case("_without_cache_points", blocks=[TXT, TXT]),
    case("_without_cache_points", blocks="not-a-list"),
    case("_without_cache_points", blocks=[]),
    case("_without_cache_points", blocks=[{"cachePoint": {"type": "default"}, "text": "x"}]),
    case("_without_cache_points", blocks=None),
)

# ── _without_redacted_reasoning ─────────────────────────────────────────────
add(
    case("_without_redacted_reasoning", blocks=[RC_REDACTED, RC_TEXT, TXT]),
    case("_without_redacted_reasoning", blocks=[RC_TEXT, TXT]),
    case("_without_redacted_reasoning", blocks=None),
    case("_without_redacted_reasoning", blocks=[{"reasoningContent": "flat"}]),
    case("_without_redacted_reasoning", blocks=[]),
    case("_without_redacted_reasoning",
         blocks=[{"reasoningContent": {"redactedContent": None}}]),
)

# ── note_cache_point_rejection ──────────────────────────────────────────────
add(
    case("note_cache_point_rejection", model_id="Anthropic.Claude", placement="tools",
         state={"_CACHE_POINT_REJECTIONS": {}}),
    case("note_cache_point_rejection", model_id="m", placement="other",
         state={"_CACHE_POINT_REJECTIONS": {}}),
    case("note_cache_point_rejection", model_id=None, placement="tools",
         state={"_CACHE_POINT_REJECTIONS": {}}),
    case("note_cache_point_rejection", model_id=None, placement="other",
         state={"_CACHE_POINT_REJECTIONS": {}}),
    case("note_cache_point_rejection", model_id="M", placement="tools",
         state={"_CACHE_POINT_REJECTIONS": {"m": ["tools"]}}),      # dedupe
    case("note_cache_point_rejection", model_id="M", placement="system",
         state={"_CACHE_POINT_REJECTIONS": {"m": ["tools"]}}),      # add second
    case("note_cache_point_rejection", model_id="M", placement="messages",
         state={"_CACHE_POINT_REJECTIONS": {"m": ["system", "tools"]}}),
)

# ── reset_cache_point_rejections ────────────────────────────────────────────
add(
    case("reset_cache_point_rejections",
         state={"_CACHE_POINT_REJECTIONS": {"m": ["tools"]}}),
    case("reset_cache_point_rejections", state={"_CACHE_POINT_REJECTIONS": {}}),
    case("reset_cache_point_rejections"),
    case("reset_cache_point_rejections",
         state={"_CACHE_POINT_REJECTIONS": {"a": ["tools"], "b": ["system"]}}),
    case("reset_cache_point_rejections",
         state={"_CACHE_POINT_REJECTIONS": {"m": ["tools"]},
                "_inference_profile_model_cache": {ARN: MODEL_ARN}}),
)

# ── recover_from_redacted_reasoning_rejection ───────────────────────────────
REJ_MSG = "ValidationException: replayed redacted reasoning is sealed"
KW = {"modelId": "m1", "messages": [{"role": "user", "content": [TXT]}]}
add(
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=KW,
         io={"strip_redacted_reasoning": {"modelId": "m1", "messages": []},
             "logger_warning": None}),
    case("recover_from_redacted_reasoning_rejection", exc="plain error", kwargs=KW, io={}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=KW,
         io={"strip_redacted_reasoning": {"__same__": True}}),      # identity
    case("recover_from_redacted_reasoning_rejection", exc=None, kwargs=KW, io={}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=KW,
         io={"strip_redacted_reasoning": rais("RuntimeError", "strip gone")}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=KW,
         io={"strip_redacted_reasoning": rais("GeneratorExit", "gen")}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=KW, io={}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=None,
         io={"strip_redacted_reasoning": {"__same__": True}}),      # None is None
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=None,
         io={"strip_redacted_reasoning": {"a": 1}}),                # .get AttributeError
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG,
         kwargs={"messages": []},
         io={"strip_redacted_reasoning": {"messages": [TXT]}, "logger_warning": None}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG,
         kwargs={"modelId": None},
         io={"strip_redacted_reasoning": {"x": 1}, "logger_warning": None}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG,
         kwargs={"modelId": 5},
         io={"strip_redacted_reasoning": {"x": 1}, "logger_warning": None}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=KW,
         io={"strip_redacted_reasoning": None, "logger_warning": None}),
    case("recover_from_redacted_reasoning_rejection", exc=REJ_MSG, kwargs=KW,
         io={"strip_redacted_reasoning": {"x": 1},
             "logger_warning": rais("RuntimeError", "log gone")}),
    case("recover_from_redacted_reasoning_rejection",
         exc="validationexception xx encrypt yy", kwargs=KW,
         io={"strip_redacted_reasoning": {"__same__": True}}),
    case("recover_from_redacted_reasoning_rejection",
         exc="redacted before ValidationException", kwargs=KW, io={}),
)

# ── _encode_redacted ────────────────────────────────────────────────────────
add(
    case("_encode_redacted", redacted={"__bytes__": [104, 105]}),
    case("_encode_redacted", redacted={"__bytearray__": [0, 255, 128]}),
    case("_encode_redacted", redacted="already-str"),
    case("_encode_redacted", redacted=None),
    case("_encode_redacted", redacted=5),
    case("_encode_redacted", redacted={"__bytes__": []}),
    case("_encode_redacted", redacted={"other": 1}),
)

# ── _tool_call_ns ───────────────────────────────────────────────────────────
add(
    case("_tool_call_ns", tool_use_id="t1", name="fn", input_dict={"a": 1}),
    case("_tool_call_ns", tool_use_id="", name="", input_dict={}),
    case("_tool_call_ns", tool_use_id="t", name="f", input_dict=[1, "x", None, True]),
    case("_tool_call_ns", tool_use_id="t", name="f", input_dict="str"),
    case("_tool_call_ns", tool_use_id="t", name="f", input_dict=None),
    case("_tool_call_ns", tool_use_id="t", name="f",
         input_dict={"z": 1, "a": {"k": [1, 2]}}),
)

# ── _persisted_reasoning_details ────────────────────────────────────────────
add(
    case("_persisted_reasoning_details",
         ordered_blocks=[{"reasoningContent": {"text": "t", "signature": "s"}}]),
    case("_persisted_reasoning_details",
         ordered_blocks=[{"reasoningContent": {"redactedContentBase64": "AQI="}}]),
    case("_persisted_reasoning_details",
         ordered_blocks=[{"reasoningContent": {"text": "t", "signature": "s",
                                               "redactedContentBase64": "AQI="}}]),
    case("_persisted_reasoning_details", ordered_blocks=[None, TXT, {"reasoningContent": "flat"}]),
    case("_persisted_reasoning_details", ordered_blocks=None),
    case("_persisted_reasoning_details", ordered_blocks="xy"),
    case("_persisted_reasoning_details",
         ordered_blocks=[{"reasoningContent": {"text": "", "signature": "s"}}]),
    case("_persisted_reasoning_details",
         ordered_blocks=[{"reasoningContent": {"text": "t", "signature": 5}}]),
)

# ── _model_entry ────────────────────────────────────────────────────────────
add(
    case("_model_entry", model_id="m1", name=" Name ", provider="p",
         input_mods=["TEXT"], output_mods=["TEXT"]),
    case("_model_entry", model_id="m1", name=None, provider="p",
         input_mods=[], output_mods=[]),
    case("_model_entry", model_id="m1", name="", provider="p",
         input_mods=["TEXT"], output_mods=["TEXT"]),
    case("_model_entry", model_id="m1", name=5, provider="p",
         input_mods=[], output_mods=[]),
    case("_model_entry", model_id=5, name=None, provider="p",
         input_mods=[], output_mods=[]),
    case("_model_entry", model_id="m1", name=False, provider="p",
         input_mods=["TEXT", "IMAGE"], output_mods=["TEXT"]),
)

# ── _nova_converse_reasoning ────────────────────────────────────────────────
add(
    case("_nova_converse_reasoning", model=NOVA, reasoning_config={"enabled": False},
         max_tokens=1000),
    case("_nova_converse_reasoning", model=NOVA, reasoning_config={}, max_tokens=None),
    case("_nova_converse_reasoning", model=NOVA,
         reasoning_config={"effort": "MINIMAL"}, max_tokens=100),
    case("_nova_converse_reasoning", model=NOVA,
         reasoning_config={"effort": "high"}, max_tokens=100),
    case("_nova_converse_reasoning", model=NOVA,
         reasoning_config={"effort": None}, max_tokens=None),
    case("_nova_converse_reasoning", model=NOVA,
         reasoning_config={"effort": 5}, max_tokens=None),
    case("_nova_converse_reasoning", model=NOVA, reasoning_config=None, max_tokens=None),
    case("_nova_converse_reasoning", model=NOVA,
         reasoning_config={"enabled": 0}, max_tokens=None),
    case("_nova_converse_reasoning", model=NOVA,
         reasoning_config={"effort": "low"}, max_tokens=None),
)

# ── _claude_converse_reasoning ──────────────────────────────────────────────
add(
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={"enabled": True},
         max_tokens=4096,
         io={"_thinking_kwargs": {"thinking": {"type": "enabled"},
                                  "temperature": 0.7, "max_tokens": 8000}}),
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={}, max_tokens=100,
         io={"_thinking_kwargs": {}}),
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={}, max_tokens=100,
         io={"_thinking_kwargs": {"thinking": {"type": "disabled"}}}),
    case("_claude_converse_reasoning", model=CLAUDE3, reasoning_config={}, max_tokens=100,
         io={"_thinking_kwargs": {"thinking": {"type": "enabled"}}}),
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={}, max_tokens=None,
         io={"_thinking_kwargs": {"thinking": {"type": "enabled"}, "temperature": 1}}),
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={}, max_tokens=100,
         io={"_thinking_kwargs": None}),                            # TypeError
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={}, max_tokens=100,
         io={}),                                                    # fail-closed
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={}, max_tokens=100,
         io={"_thinking_kwargs": rais("SystemExit", "bye")}),
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={}, max_tokens=100,
         io={"_thinking_kwargs": {"thinking": "flat"}}),            # AttributeError
    case("_claude_converse_reasoning", model=None, reasoning_config={}, max_tokens=100,
         io={"_thinking_kwargs": {"thinking": {"type": "enabled"}}}),
    case("_claude_converse_reasoning", model=CLAUDE, reasoning_config={}, max_tokens=100,
         io={"_thinking_kwargs": {"thinking": {"type": "enabled"},
                                  "output_config": {"effort": "high"}}}),
)

# ── _apply_converse_reasoning ───────────────────────────────────────────────
add(
    case("_apply_converse_reasoning", kwargs={}, inference_config={"maxTokens": 100},
         model=CLAUDE, reasoning_config={"enabled": True}, max_tokens=4096,
         io={"is_anthropic_bedrock_model": True,
             "_thinking_kwargs": {"thinking": {"type": "enabled"}}}),
    case("_apply_converse_reasoning", kwargs={}, inference_config={},
         model=NOVA, reasoning_config={}, max_tokens=None,
         io={"is_anthropic_bedrock_model": False}),
    case("_apply_converse_reasoning", kwargs={"a": 1}, inference_config={"b": 2},
         model="meta.llama", reasoning_config={}, max_tokens=None,
         io={"is_anthropic_bedrock_model": False}),                 # no builder
    case("_apply_converse_reasoning", kwargs={},
         inference_config={"temperature": 0.5, "topP": 0.9, "maxTokens": 100},
         model=NOVA, reasoning_config={"effort": "high"}, max_tokens=100,
         io={"is_anthropic_bedrock_model": False}),                 # pops
    case("_apply_converse_reasoning", kwargs={}, inference_config={},
         model=CLAUDE, reasoning_config={}, max_tokens=None,
         io={"is_anthropic_bedrock_model": rais("RuntimeError", "gate gone")}),
    case("_apply_converse_reasoning", kwargs={}, inference_config={},
         model=CLAUDE, reasoning_config={}, max_tokens=None, io={}),  # fail-closed
    case("_apply_converse_reasoning", kwargs=None, inference_config={},
         model=CLAUDE, reasoning_config={}, max_tokens=100,
         io={"is_anthropic_bedrock_model": True,
             "_thinking_kwargs": {"thinking": {"type": "enabled"}}}),
    case("_apply_converse_reasoning", kwargs={}, inference_config=None,
         model=NOVA, reasoning_config={"effort": "high"}, max_tokens=None,
         io={"is_anthropic_bedrock_model": False}),
    case("_apply_converse_reasoning", kwargs={}, inference_config={},
         model=None, reasoning_config={}, max_tokens=None,
         io={"is_anthropic_bedrock_model": False}),
    case("_apply_converse_reasoning", kwargs={}, inference_config={},
         model=5, reasoning_config={}, max_tokens=None,
         io={"is_anthropic_bedrock_model": False}),
    case("_apply_converse_reasoning", kwargs={}, inference_config={"maxTokens": 100},
         model=NOVA, reasoning_config={"enabled": False}, max_tokens=100,
         io={"is_anthropic_bedrock_model": False}),                 # nova off
)

# ── _resolve_inference_profile_model_id ─────────────────────────────────────
add(
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         state={"_inference_profile_model_cache": {ARN: MODEL_ARN}}, io={}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": {"models": [{"modelArn": MODEL_ARN}]}},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": {"models": []}},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": {}, "get_inference_profile": None,
             "logger_debug": None},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": None, "logger_debug": None},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"logger_debug": None},
         state={"_inference_profile_model_cache": {}}),             # fail-closed caught
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": rais("RuntimeError", "no creds"),
             "logger_debug": None},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": rais("SystemExit", "bye")},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN_NOREGION,
         region="eu-west-1",
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": {"models": [{"modelArn": MODEL_ARN}]}},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN_NOREGION, region="",
         io={"resolve_bedrock_region": "ap-south-1",
             "_get_bedrock_control_client": {},
             "get_inference_profile": {"models": []}},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=[], io={}),
    case("_resolve_inference_profile_model_id", profile_arn=5, io={}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": rais("ValueError", "bad profile"),
             "logger_debug": None},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": {"models": []},
             "logger_debug": rais("RuntimeError", "log gone")},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": {"models": [{"modelArn": None},
                                                  {"modelArn": MODEL_ARN}]}},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": {"models": ["not-a-dict"]},
             "logger_debug": None},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN,
         io={"_get_bedrock_control_client": {},
             "get_inference_profile": {"models": 5},
             "logger_debug": None},
         state={"_inference_profile_model_cache": {}}),
    case("_resolve_inference_profile_model_id", profile_arn=ARN, region=None,
         io={"resolve_bedrock_region": None,
             "_get_bedrock_control_client": {},
             "get_inference_profile": {"models": []}},
         state={"_inference_profile_model_cache": {}}),             # region None dropped
)

# ── _list_foundation_models ─────────────────────────────────────────────────
FM_OK = {"modelId": "anthropic.claude-x", "providerName": "Anthropic",
         "modelName": "Claude X", "modelLifecycle": {"status": "active"},
         "responseStreamingSupported": True,
         "inputModalities": ["TEXT"], "outputModalities": ["TEXT"]}
add(
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": [FM_OK]}}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, modelLifecycle={"status": "LEGACY"})]}}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, responseStreamingSupported=False)]}}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, outputModalities=["IMAGE"])]}}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, modelId="  ")]}}),
    case("_list_foundation_models", client={}, filter_set=["anthropic"], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             FM_OK, dict(FM_OK, modelId="amazon.nova-x", providerName="Amazon")]}}),
    case("_list_foundation_models", client={}, filter_set=["anthropic"], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, providerName="Other")]}}),                 # prefix match
    case("_list_foundation_models", client=None, filter_set=[], models=[], io={}),
    case("_list_foundation_models", client={}, filter_set=[], models=[], io={}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": rais("RuntimeError", "api down")}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": None}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": None}}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, modelLifecycle=None)]}}),
    case("_list_foundation_models", client={}, filter_set=["anthropic"], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, providerName=5)]}}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, modelName=None)]}}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": rais("KeyboardInterrupt", "int")}),
    case("_list_foundation_models", client={}, filter_set=[], models=[],
         io={"list_foundation_models": {"modelSummaries": [
             dict(FM_OK, modelId="nosuffix", providerName="Anthropic")]}}),
)

# ── _list_inference_profiles ────────────────────────────────────────────────
IP_OK = {"inferenceProfileId": "us.anthropic.claude-x", "status": "ACTIVE",
         "inferenceProfileName": "US Claude X",
         "models": [{"modelArn": "arn:aws:bedrock:us-east-1::foundation-model/anthropic.claude-x"}]}
add(
    case("_list_inference_profiles", client={}, filter_set=[], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [IP_OK]}}),
    case("_list_inference_profiles", client={}, filter_set=[], models=[],
         io={"list_inference_profiles": seq(
             {"inferenceProfileSummaries": [IP_OK], "nextToken": "t1"},
             {"inferenceProfileSummaries": [dict(IP_OK, inferenceProfileId="eu.anthropic.claude-x")]})}),
    case("_list_inference_profiles", client={}, filter_set=[], models=[],
         io={"list_inference_profiles": seq(
             {"inferenceProfileSummaries": [], "nextToken": "t1"})}),  # exhausted
    case("_list_inference_profiles", client={}, filter_set=[],
         models=[{"id": "US.ANTHROPIC.CLAUDE-X", "name": "x"}],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [IP_OK]}}),
    case("_list_inference_profiles", client={}, filter_set=[], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [
             dict(IP_OK, status="LEGACY")]}}),
    case("_list_inference_profiles", client={}, filter_set=["anthropic"], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [IP_OK]},
             "_extract_provider_from_arn": "anthropic"}),
    case("_list_inference_profiles", client={}, filter_set=["amazon"], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [IP_OK]},
             "_extract_provider_from_arn": "anthropic"}),
    case("_list_inference_profiles", client={}, filter_set=[], models=[],
         io={"list_inference_profiles": None}),
    case("_list_inference_profiles", client={}, filter_set=[], models=[], io={}),
    case("_list_inference_profiles", client={}, filter_set=[],
         models=[{"name": "no-id"}],
         io={"list_inference_profiles": {"inferenceProfileSummaries": []}}),
    case("_list_inference_profiles", client={}, filter_set=[],
         models=[{"id": 5}],
         io={"list_inference_profiles": {"inferenceProfileSummaries": []}}),
    case("_list_inference_profiles", client=None, filter_set=[], models=[], io={}),
    case("_list_inference_profiles", client={}, filter_set=[], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": ["flat"]}}),
    case("_list_inference_profiles", client={}, filter_set=["anthropic"], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [IP_OK]},
             "_extract_provider_from_arn": rais("RuntimeError", "arn gone")}),
    case("_list_inference_profiles", client={}, filter_set=[], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [IP_OK, IP_OK]}}),
    case("_list_inference_profiles", client={}, filter_set=[], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [IP_OK],
                                         "nextToken": ""}}),
    case("_list_inference_profiles", client={}, filter_set=["anthropic"], models=[],
         io={"list_inference_profiles": {"inferenceProfileSummaries": [
             dict(IP_OK, models=[{"modelArn": "a1"}, {"modelArn": "a2"}])]},
             "_extract_provider_from_arn": seq("amazon", "anthropic")}),  # any() order
)


def main():
    # domain guards: ASCII-only, no floats in str()/dumps positions
    text = json.dumps(CASES, ensure_ascii=True)
    assert text.isascii(), "non-ASCII fixture content"
    per_func = {}
    for c in CASES:
        per_func[c["func"]] = per_func.get(c["func"], 0) + 1
    thin = {f: n for f, n in per_func.items() if n < 5}
    assert not thin, f"funcs with <5 cases: {thin}"
    OUT.write_text("".join(json.dumps(c, ensure_ascii=True) + "\n" for c in CASES),
                   encoding="utf-8")
    print(f"wrote {OUT}: {len(CASES)} cases across {len(per_func)} funcs")


if __name__ == "__main__":
    main()