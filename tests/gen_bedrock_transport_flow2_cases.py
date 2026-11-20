#!/usr/bin/env python3
"""Generate tests/fixtures/bedrock_transport_flow2.jsonl for the REMAINING-
UNCLAIMED Bedrock adapter FLOW port (src/agent/transports/bedrock_transport_flow2.c;
live defining module agent/bedrock_adapter.py).

Case shape: {"func": <name>, "in": {...args..., "io": {seam: script},
"state": {module-state seed}}}. Class methods use dotted func names
(BedrockOpenAISigV4Auth.__init__, _ResponseParts.build, ...).

Scripts (shared contract with the C universal seam bt3_call and the oracle):
  - MISSING key            -> fail-closed RuntimeError("no script for <name>")
  - null                   -> None result
  - {"$seq": [v1, v2, ...]}-> one result per call; exhaustion raises
                              RuntimeError("script exhausted: <name>")
  - {"__raise__": cls, "msg": m} -> raises; trace ret is the WHOLE marker
  - any other value        -> reused on every call (a bare ARRAY is a LIST
                              RESULT, not a queue -- wrap queues in $seq)

Fixture domain (documented in the port header): ASCII strings; read_timeout /
usage values / timeout int or null; scoped_aws_session_kwargs result a dict;
request headers dict or null; AWSRequest_ctor result null or dict; import_module
result dict or null; agent/parts/block/cache dicts; cache keys "|"-joined
str(component) with read_timeout int or "None"; _longest_key_match result null or
a 2-array. >=5 cases per function.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "bedrock_transport_flow2.jsonl"


def seq(*vals):
    return {"$seq": list(vals)}


def rais(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []


def add(*cases):
    CASES.extend(cases)


B = {"__bytes__": [1, 2, 3]}
ARN = "arn:aws:bedrock:us-east-1:123456789012:application-inference-profile/abc123"

# ── _new_client ─────────────────────────────────────────────────────────────
add(
    case("_new_client", factory="boto3", service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={"botocore_Config": {"id": "c1"}, "factory_client": "client-A"}),
    case("_new_client", factory="boto3", service="bedrock", region="eu-west-1",
         read_timeout=None, io={"botocore_Config": {"id": "c1"}, "factory_client": "client-A"}),
    case("_new_client", factory="boto3", service="bedrock-runtime", region="us-east-1",
         read_timeout=60, io={"botocore_Config": seq({"id": "c1"}, {"id": "c2"}),
                              "config_merge": {"id": "c3"}, "factory_client": "client-B"}),
    case("_new_client", factory="boto3", service="bedrock-runtime", region="us-east-1",
         read_timeout=0, io={"botocore_Config": seq({"id": "c1"}, {"id": "c2"}),
                             "config_merge": {"id": "c3"}, "factory_client": "client-B"}),
    case("_new_client", factory="boto3", service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={}),                                   # fail-closed
    case("_new_client", factory="boto3", service="bedrock-runtime", region="us-east-1",
         read_timeout=60, io={"botocore_Config": None}),              # None.merge AttributeError
    case("_new_client", factory=None, service="bedrock", region="us-east-1",
         read_timeout=None, io={"botocore_Config": {"id": "c1"}, "factory_client": "x"}),  # None.client
    case("_new_client", factory="boto3", service="bedrock", region="us-east-1",
         read_timeout=None, io={"botocore_Config": {"id": "c1"},
                                "factory_client": rais("SystemExit", "bye")}),
    case("_new_client", factory="boto3", service="bedrock-runtime", region="us-east-1",
         read_timeout=60, io={"botocore_Config": seq({"id": "c1"}, {"id": "c2"}),
                              "config_merge": rais("RuntimeError", "merge fail")}),
    case("_new_client", factory="boto3", service="bedrock", region="us-east-1",
         read_timeout=None, io={"botocore_Config": rais("ImportError", "no botocore")}),
)

# ── _cached_client ──────────────────────────────────────────────────────────
add(
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={"get_hermes_home_override": None, "_require_boto3": "boto3",
                                "botocore_Config": {"id": "c1"}, "factory_client": "client-A"}),
    case("_cached_client", cache={"us-east-1|None": "cached"}, service="bedrock-runtime",
         region="us-east-1", read_timeout=None, io={"get_hermes_home_override": None}),
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={"get_hermes_home_override": "/h", "hermes_home_key": "home1",
                                "scoped_aws_session_kwargs": {"aws_access_key_id": "k"},
                                "_require_boto3": "boto3", "boto3_Session": "sess",
                                "botocore_Config": {"id": "c1"}, "factory_client": "client-B"}),
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=None, state={"_bedrock_clients_by_home": {"home1|bedrock-runtime|us-east-1|None": "cached"}},
         io={"get_hermes_home_override": "/h", "hermes_home_key": "home1"}),
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={}),                                   # fail-closed
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={"get_hermes_home_override": rais("SystemExit", "bye")}),
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={"get_hermes_home_override": "/h", "hermes_home_key": "home1",
                                "scoped_aws_session_kwargs": {}, "_require_boto3": None}),  # None.Session
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={"get_hermes_home_override": "/h", "hermes_home_key": "home1",
                                "scoped_aws_session_kwargs": {}, "_require_boto3": "boto3",
                                "boto3_Session": None, "botocore_Config": {"id": "c1"}}),  # None.client
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=60, io={"get_hermes_home_override": None, "_require_boto3": "boto3",
                              "botocore_Config": seq({"id": "c1"}, {"id": "c2"}),
                              "config_merge": {"id": "c3"}, "factory_client": "client-C"}),
    case("_cached_client", cache={}, service="bedrock-runtime", region="us-east-1",
         read_timeout=None, io={"get_hermes_home_override": "/h", "hermes_home_key": "home1",
                                "scoped_aws_session_kwargs": rais("RuntimeError", "refused")}),
)

# ── BedrockOpenAISigV4Auth.__init__ ─────────────────────────────────────────
add(
    case("BedrockOpenAISigV4Auth.__init__", region="us-east-1", io={}),
    case("BedrockOpenAISigV4Auth.__init__", region="  ",
         io={"resolve_bedrock_runtime_region": "eu-west-1"}),
    case("BedrockOpenAISigV4Auth.__init__", region=None,
         io={"resolve_bedrock_runtime_region": "us-west-2"}),
    case("BedrockOpenAISigV4Auth.__init__", region="", service="bedrock-runtime",
         io={"resolve_bedrock_runtime_region": "us-west-2"}),
    case("BedrockOpenAISigV4Auth.__init__", region=5, io={}),         # int.strip AttributeError
    case("BedrockOpenAISigV4Auth.__init__", region="  x  ", io={}),
    case("BedrockOpenAISigV4Auth.__init__", region=None,
         io={"resolve_bedrock_runtime_region": rais("SystemExit", "bye")}),
    case("BedrockOpenAISigV4Auth.__init__", region=None, io={}),      # fail-closed resolve
)

# ── BedrockOpenAISigV4Auth.auth_flow ────────────────────────────────────────
REQ = {"method": "POST", "url": "https://bedrock-mantle.us-east-1.api.aws/openai/v1/responses",
       "content": B, "headers": {"Authorization": "Bearer x", "Content-Type": "application/json",
                                 "X-Amz-Date": "d"}}
AF_OK = {"scoped_aws_session_kwargs": {}, "_require_boto3": "b", "boto3_Session": "s",
         "session_get_credentials": "creds", "AWSRequest_ctor": {"headers": {"Host": "x"}},
         "credentials_get_frozen_credentials": "frozen", "SigV4Auth_ctor": "signer",
         "signer_add_auth": {"Authorization": "SIGV4"}}
add(
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1", service="bedrock",
         request=REQ, io=dict(AF_OK)),
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1", request=REQ,
         io={"scoped_aws_session_kwargs": {}, "_require_boto3": "b", "boto3_Session": "s",
             "session_get_credentials": None}),                       # no creds RuntimeError
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1", request=REQ, io={}),  # fail-closed
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1", request=REQ,
         io={"scoped_aws_session_kwargs": {}, "_require_boto3": None}),  # None.Session
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1",
         request={"method": "POST", "url": "https://x", "content": None, "headers": None},
         io={"scoped_aws_session_kwargs": {}, "_require_boto3": "b", "boto3_Session": "s",
             "session_get_credentials": "creds"}),                    # None.items
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1", request=REQ,
         io={**AF_OK, "SigV4Auth_ctor": None}),                       # None.add_auth
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1", request=REQ,
         io={**AF_OK, "scoped_aws_session_kwargs": rais("SystemExit", "bye")}),
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1", request=REQ,
         io={**AF_OK, "AWSRequest_ctor": None, "signer_add_auth": None}),  # None.headers
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1",
         request={"method": "GET", "url": "https://x", "content": None,
                  "headers": {"Content-Type": "application/json"}},
         io={**AF_OK, "signer_add_auth": None}),                      # data b"", no add_auth change
    case("BedrockOpenAISigV4Auth.auth_flow", region="us-east-1", request=REQ,
         io={**AF_OK, "session_get_credentials": rais("RuntimeError", "creds fail")}),
)

# ── build_bedrock_openai_http_client ────────────────────────────────────────
add(
    case("build_bedrock_openai_http_client", region="us-east-1", timeout=None,
         io={"httpx_Client": "client"}),
    case("build_bedrock_openai_http_client", region="us-east-1", timeout=30,
         io={"httpx_Client": "client"}),
    case("build_bedrock_openai_http_client", region=None, timeout=None,
         io={"resolve_bedrock_runtime_region": "eu-west-1", "httpx_Client": "client"}),
    case("build_bedrock_openai_http_client", region="us-east-1", timeout=True,
         io={"httpx_Client": "client"}),                              # bool excluded
    case("build_bedrock_openai_http_client", region="us-east-1", timeout=0,
         io={"httpx_Client": "client"}),
    case("build_bedrock_openai_http_client", region="us-east-1", timeout=-5,
         io={"httpx_Client": "client"}),
    case("build_bedrock_openai_http_client", region="us-east-1", timeout="30",
         io={"httpx_Client": "client"}),
    case("build_bedrock_openai_http_client", region="us-east-1", timeout=None, io={}),  # fail-closed
    case("build_bedrock_openai_http_client", region=5, timeout=None, io={}),  # int.strip
    case("build_bedrock_openai_http_client", region="us-east-1", timeout=None,
         io={"httpx_Client": rais("SystemExit", "bye")}),
)

# ── _stale_error_types ──────────────────────────────────────────────────────
# import_module fires ONCE PER NAME (inside the comprehension): botocore 2 calls,
# urllib3 3 calls. $seq entries are consumed in call order.
BOTOCORE = {"ConnectionError": "CE", "HTTPClientError": "HCE"}
URLLIB3 = {"ProtocolError": "PE", "NewConnectionError": "NCE", "ConnectionError": "CE2"}
add(
    case("_stale_error_types", io={"import_module": seq(BOTOCORE, BOTOCORE, URLLIB3, URLLIB3, URLLIB3)}),
    case("_stale_error_types", io={"import_module": seq({"ConnectionError": "CE"}, {"ConnectionError": "CE"},
                                                        URLLIB3, URLLIB3, URLLIB3)}),
    case("_stale_error_types", io={"import_module": seq(rais("ImportError", "no mod"), URLLIB3, URLLIB3, URLLIB3)}),
    case("_stale_error_types", io={"import_module": None}),
    case("_stale_error_types", io={}),                                # fail-closed RuntimeError (not suppressed)
    case("_stale_error_types", io={"import_module": rais("AttributeError", "no attr")}),
    case("_stale_error_types", io={"import_module": rais("SystemExit", "bye")}),
    case("_stale_error_types", io={"import_module": {**BOTOCORE, **URLLIB3}}),
    case("_stale_error_types", io={"import_module": seq(BOTOCORE, BOTOCORE, {"ProtocolError": "PE"},
                                                        {"ProtocolError": "PE"}, {"ProtocolError": "PE"})}),
)

# ── _boto3_chain_has_credentials ────────────────────────────────────────────
add(
    case("_boto3_chain_has_credentials", instance_metadata=True,
         io={"botocore_get_session": "s", "session_get_credentials": "c",
             "credentials_get_frozen_credentials": {"access_key": "AK"}}),
    case("_boto3_chain_has_credentials", instance_metadata=False,
         io={"botocore_get_session": "s", "session_set_config_variable": None,
             "session_get_credentials": "c", "credentials_get_frozen_credentials": {"access_key": "AK"}}),
    case("_boto3_chain_has_credentials", io={"botocore_get_session": "s", "session_get_credentials": "c",
                                             "credentials_get_frozen_credentials": {"access_key": ""}}),
    case("_boto3_chain_has_credentials", io={"botocore_get_session": "s", "session_get_credentials": "c",
                                             "credentials_get_frozen_credentials": {}}),
    case("_boto3_chain_has_credentials", io={"botocore_get_session": "s", "session_get_credentials": None}),
    case("_boto3_chain_has_credentials", io={"botocore_get_session": "s", "session_get_credentials": "c",
                                             "credentials_get_frozen_credentials": None}),
    case("_boto3_chain_has_credentials", io={"botocore_get_session": None}),
    case("_boto3_chain_has_credentials", io={}),                      # fail-closed -> suppressed False
    case("_boto3_chain_has_credentials", io={"botocore_get_session": rais("SystemExit", "bye")}),
    case("_boto3_chain_has_credentials", io={"botocore_get_session": "s",
                                             "session_get_credentials": rais("RuntimeError", "x")}),
    case("_boto3_chain_has_credentials", instance_metadata=False,
         io={"botocore_get_session": "s", "session_set_config_variable": rais("RuntimeError", "x"),
             "session_get_credentials": "c", "credentials_get_frozen_credentials": {"access_key": "AK"}}),
)

# ── bind_bedrock_runtime ────────────────────────────────────────────────────
URL = "https://bedrock-runtime.us-east-1.amazonaws.com"
add(
    case("bind_bedrock_runtime", agent={"model": "m"}, base_url=URL, api_mode="bedrock_converse",
         io={"bedrock_region_from_runtime_url": "us-east-1", "bedrock_guardrail_config": None}),
    case("bind_bedrock_runtime", agent={"model": "m"}, base_url=URL, api_mode="anthropic_messages",
         io={"bedrock_region_from_runtime_url": "us-east-1", "bedrock_guardrail_config": {"guardrailIdentifier": "g"},
             "get_provider_request_timeout": 60, "build_anthropic_bedrock_client": "aclient"}),
    case("bind_bedrock_runtime", agent={}, base_url=URL, api_mode="anthropic_messages",
         io={"bedrock_region_from_runtime_url": "us-east-1", "bedrock_guardrail_config": None,
             "get_provider_request_timeout": None, "build_anthropic_bedrock_client": "aclient"}),
    case("bind_bedrock_runtime", agent={"model": "m"}, base_url=URL, api_mode="bedrock_converse", io={}),
    case("bind_bedrock_runtime", agent={"model": "m"}, base_url=URL, api_mode="bedrock_converse",
         io={"bedrock_region_from_runtime_url": rais("SystemExit", "bye")}),
    case("bind_bedrock_runtime", agent=None, base_url=URL, api_mode="bedrock_converse",
         io={"bedrock_region_from_runtime_url": "us-east-1"}),        # setattr AttributeError
    case("bind_bedrock_runtime", agent={"model": "m"}, base_url=URL, api_mode="bedrock_converse",
         io={"bedrock_region_from_runtime_url": "us-east-1",
             "bedrock_guardrail_config": rais("RuntimeError", "cfg fail")}),
    case("bind_bedrock_runtime", agent={"model": "m"}, base_url=URL, api_mode="anthropic_messages",
         io={"bedrock_region_from_runtime_url": "us-east-1", "bedrock_guardrail_config": None,
             "get_provider_request_timeout": 60, "build_anthropic_bedrock_client": rais("RuntimeError", "build fail")}),
    case("bind_bedrock_runtime", agent={"model": "m"}, base_url=URL, api_mode="other",
         io={"bedrock_region_from_runtime_url": "us-east-1", "bedrock_guardrail_config": None}),
    case("bind_bedrock_runtime", agent={"model": "m"}, base_url=URL, api_mode="anthropic_messages",
         io={"bedrock_region_from_runtime_url": "us-east-1", "bedrock_guardrail_config": None,
             "get_provider_request_timeout": rais("SystemExit", "bye")}),
)

# ── _ResponseParts.__init__ ─────────────────────────────────────────────────
add(
    case("_ResponseParts.__init__", io={}),
    case("_ResponseParts.__init__", io={}, state={}),
    case("_ResponseParts.__init__", io={}, state={"_inference_profile_model_cache": {}}),
    case("_ResponseParts.__init__", io={"on_text": None}),
    case("_ResponseParts.__init__", io={"on_text": None}, state={"_bedrock_clients_by_home": {}}),
)

# ── _ResponseParts.absorb_reasoning ─────────────────────────────────────────
add(
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning={"text": "think", "signature": "sig"},
         block={}, on_text=None, io={"_encode_redacted": None}),
    case("_ResponseParts.absorb_reasoning", parts={},
         reasoning={"reasoningText": {"text": "think", "signature": "sig"}},
         block={"text": "pre", "signature": "s0"}, on_text=True, io={"on_text": None, "_encode_redacted": None}),
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning={"redactedContent": B},
         block={}, on_text=None, io={"_encode_redacted": "AQID"}),
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning="notdict", block={}, on_text=None, io={}),
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning={"text": "think"}, block={},
         on_text=True, io={"on_text": rais("SystemExit", "bye")}),
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning={"text": "x"}, block={}, on_text=None, io={}),
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning={"text": 5}, block={}, on_text=None,
         io={"_encode_redacted": None}),
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning={"text": "x"}, block={"text": 5},
         on_text=None, io={"_encode_redacted": None}),                # int + str TypeError
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning={"text": "a", "signature": ""},
         block={}, on_text=None, io={"_encode_redacted": None}),
    case("_ResponseParts.absorb_reasoning", parts={}, reasoning={"redactedContent": "alreadyb64"},
         block={}, on_text=None, io={"_encode_redacted": rais("RuntimeError", "enc fail")}),
    case("_ResponseParts.absorb_reasoning", parts={"reasoning_details": [{"type": "thinking", "thinking": "t"}]},
         reasoning={"redactedContent": B, "text": "think"}, block={"text": "p"}, on_text=None,
         io={"_encode_redacted": "AQID"}),
)

# ── _ResponseParts.build ────────────────────────────────────────────────────
USAGE = {"inputTokens": 10, "cacheReadInputTokens": 2, "cacheWriteInputTokens": 3, "outputTokens": 5}
add(
    case("_ResponseParts.build", parts={"text_parts": ["hello", "world"], "tool_calls": [], "reasoning_details": []},
         ordered_blocks=[{"text": "hello"}], usage_data=USAGE, stop_reason="end_turn", model="m",
         io={"_persisted_reasoning_details": []}),
    case("_ResponseParts.build", parts={"text_parts": ["x"]}, ordered_blocks=[{"text": "x"}],
         usage_data=USAGE, stop_reason="tool_use", model="m", io={"_persisted_reasoning_details": []}),
    case("_ResponseParts.build", parts={"text_parts": [], "tool_calls": [{"id": "t"}], "reasoning_details": []},
         ordered_blocks=[], usage_data=USAGE, stop_reason="end_turn", model="m",
         io={"_persisted_reasoning_details": []}),
    case("_ResponseParts.build", parts={}, ordered_blocks=[{"reasoningContent": {"text": "think"}}],
         usage_data=USAGE, stop_reason="end_turn", model="m",
         io={"_persisted_reasoning_details": [{"type": "thinking", "thinking": "think"}]}),
    case("_ResponseParts.build", parts={},
         ordered_blocks=[{"reasoningContent": {"text": "a"}}, {"reasoningContent": {"text": "b"}}],
         usage_data=USAGE, stop_reason="end_turn", model="m", io={"_persisted_reasoning_details": []}),
    case("_ResponseParts.build", parts={"reasoning_details": [{"type": "redacted_thinking", "data": "x"}]},
         ordered_blocks=[], usage_data=USAGE, stop_reason="end_turn", model="m",
         io={"_persisted_reasoning_details": []}),
    case("_ResponseParts.build", parts={}, ordered_blocks=[], usage_data=USAGE, stop_reason="end_turn",
         model="m", io={}),                                           # fail-closed
    case("_ResponseParts.build", parts={}, ordered_blocks=[], usage_data=USAGE, stop_reason="end_turn",
         model="m", io={"_persisted_reasoning_details": rais("SystemExit", "bye")}),
    case("_ResponseParts.build", parts={}, ordered_blocks=None, usage_data=USAGE, stop_reason="end_turn",
         model="m", io={"_persisted_reasoning_details": []}),         # None not iterable
    case("_ResponseParts.build", parts={}, ordered_blocks=[], usage_data={}, stop_reason="end_turn",
         model="m", io={"_persisted_reasoning_details": []}),
    case("_ResponseParts.build", parts={}, ordered_blocks=[5], usage_data=USAGE, stop_reason="end_turn",
         model="m", io={"_persisted_reasoning_details": []}),         # int.get AttributeError
    case("_ResponseParts.build", parts={"text_parts": []}, ordered_blocks=[], usage_data=USAGE,
         stop_reason="unknown", model="m", io={"_persisted_reasoning_details": []}),
)

# ── bedrock_static_context_length ───────────────────────────────────────────
add(
    case("bedrock_static_context_length", model_id="anthropic.claude-3-5-sonnet-20240620-v1:0", io={}),
    case("bedrock_static_context_length", model_id="anthropic.claude-opus-4-6-20250101", io={}),
    case("bedrock_static_context_length", model_id="us.anthropic.claude-fable-5", io={}),
    case("bedrock_static_context_length", model_id="openai.gpt-6-astra", io={}),
    case("bedrock_static_context_length", model_id="unknown.model", io={}),
    case("bedrock_static_context_length", model_id="anthropic.custom-model",
         io={"_longest_key_match": ["custom-model", 50000]}),
    case("bedrock_static_context_length", model_id="anthropic.custom",
         io={"_longest_key_match": None}),
    case("bedrock_static_context_length", model_id=5, io={}),         # regex TypeError
    case("bedrock_static_context_length", model_id=ARN,
         state={"_inference_profile_model_cache": {ARN: "anthropic.claude-3-opus"}}, io={}),
    case("bedrock_static_context_length", model_id=ARN, state={"_inference_profile_model_cache": {}}, io={}),
    case("bedrock_static_context_length", model_id="anthropic.custom", io={}),  # fail-closed
    case("bedrock_static_context_length", model_id="anthropic.custom",
         io={"_longest_key_match": rais("SystemExit", "bye")}),
    case("bedrock_static_context_length", model_id="anthropic.custom",
         io={"_longest_key_match": ["only"]}),                        # IndexError
    case("bedrock_static_context_length", model_id="xai.grok-4.6", io={}),
    case("bedrock_static_context_length", model_id="anthropic.custom",
         io={"_longest_key_match": ["k", 0]}),
)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print(f"  {fn}: {counts[fn]}{flag}")
    assert all(v >= 5 for v in counts.values()), "every function needs >=5 cases"


if __name__ == "__main__":
    main()