#!/usr/bin/env python3
"""Generate tests/fixtures/bedrock_adapter_flow.jsonl — decision-sequence
cases for the agent/bedrock_adapter.py FLOW + leaf port. Pure data: no quarry
import. Run: python tests/gen_bedrock_adapter_flow_cases.py

Fixture domain (documented in the port header and the oracle):
- str()/strip()/lower() operands are ASCII strings/null; list-typed carriers
  are lists; tool_calls ids are strings; guardrail id/version are strings.
- base64 payloads: canonical padded form, junk-only (decode to b""), or
  raising forms (utf-8 fallback); no mid-string-padding binascii quirks.
- io scripts: plain value reused; {"__raise__": cls, "msg": ...} raises;
  absent key -> None. resolve_profile: map arn -> resolved (missing -> arn).
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "bedrock_adapter_flow.jsonl"

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


ARN = "arn:aws:bedrock:us-east-1:123456789012:application-inference-profile/abc123"
ARN_CLAUDE = "arn:aws:bedrock:us-east-1:123456789012:foundation-model/anthropic.claude-v2"
ARN_GPT = "arn:aws:bedrock:us-east-1:123456789012:foundation-model/openai.gpt-6-astra"

# ── scoped_aws_session_kwargs: auth decision branches ──
add("scoped_aws_session_kwargs", io={"home_override": None})
add("scoped_aws_session_kwargs", io={"home_override": "/profiles/p1", "secret_scope": None,
                                     "multiplex_active": False})
add("scoped_aws_session_kwargs", io={
    "home_override": "/profiles/p1",
    "secret_scope": {"AWS_ACCESS_KEY_ID": " AKIA123 ", "AWS_SECRET_ACCESS_KEY": " secret "}})
add("scoped_aws_session_kwargs", io={
    "home_override": "/profiles/p1", "secret_scope": {"AWS_PROFILE": " prod "}})
add("scoped_aws_session_kwargs", io={
    "home_override": "/profiles/p1",
    "secret_scope": {"AWS_ACCESS_KEY_ID": "AKIA123", "AWS_SESSION_TOKEN": " tok "},
    "multiplex_active": True})
add("scoped_aws_session_kwargs", io={
    "home_override": "/profiles/p1", "secret_scope": {"AWS_ACCESS_KEY_ID": "AKIA123"},
    "multiplex_active": False})
add("scoped_aws_session_kwargs", io={
    "home_override": "/profiles/p1",
    "secret_scope": {"AWS_SECRET_ACCESS_KEY": "s", "AWS_PROFILE": "p"}})
add("scoped_aws_session_kwargs", io={
    "home_override": "/profiles/p1",
    "secret_scope": {"AWS_ACCESS_KEY_ID": "   ", "AWS_SECRET_ACCESS_KEY": "s"},
    "multiplex_active": False})

# ── resolve_bedrock_runtime_region: config -> env -> botocore chain ──
add("resolve_bedrock_runtime_region", config={"bedrock": {"region": " eu-west-1 "}})
add("resolve_bedrock_runtime_region", config=None,
    io={"load_config": {"bedrock": {"region": "ap-south-1"}}})
add("resolve_bedrock_runtime_region", config=None,
    io={"load_config": {"__raise__": "Exception", "msg": "disk gone"}},
    env={"AWS_REGION": "us-west-2"})
add("resolve_bedrock_runtime_region", config=None, io={"load_config": {}},
    env={"AWS_DEFAULT_REGION": " ca-central-1 "})
add("resolve_bedrock_runtime_region", config={}, env={},
    io={"botocore_region": "eu-north-1"})
add("resolve_bedrock_runtime_region", config={"bedrock": {"region": ""}}, env={},
    io={"botocore_region": None})
add("resolve_bedrock_runtime_region", config={"bedrock": None}, env={},
    io={"botocore_region": {"__raise__": "Exception", "msg": "no botocore"}})
add("resolve_bedrock_runtime_region", config={"bedrock": {"region": "  "}},
    env={"AWS_REGION": " ", "AWS_DEFAULT_REGION": "sa-east-1"})
add("resolve_bedrock_runtime_region", config={"bedrock": {"region": None}}, env={},
    io={"botocore_region": "eu-west-3"})

# ── bedrock_region_from_runtime_url ──
add("bedrock_region_from_runtime_url",
    base_url="https://bedrock-runtime.us-east-1.amazonaws.com/model/x/converse")
add("bedrock_region_from_runtime_url", base_url="https://bedrock-runtime.eu-west-2.amazonaws.com")
add("bedrock_region_from_runtime_url", base_url="https://bedrock-runtime.US-EAST-1.amazonaws.com")
add("bedrock_region_from_runtime_url", base_url=None)
add("bedrock_region_from_runtime_url", base_url="https://bedrock-mantle.us-east-1.api.aws")
add("bedrock_region_from_runtime_url", base_url="bedrock-runtime.ap-south-1.")
add("bedrock_region_from_runtime_url", base_url="https://bedrock-runtime.amazonaws.com")
add("bedrock_region_from_runtime_url", base_url="bedrock-runtime.us-east-1")

# ── parse_bedrock_openai_model_id ──
add("parse_bedrock_openai_model_id", model_id="openai.gpt-5.5")
add("parse_bedrock_openai_model_id", model_id="us.openai.gpt-6-astra")
add("parse_bedrock_openai_model_id", model_id="global.openai.gpt-6.1-sol")
add("parse_bedrock_openai_model_id", model_id="in.openai.gpt-5.6-terra")
add("parse_bedrock_openai_model_id", model_id="  OPENAI.GPT-5.4  ")
add("parse_bedrock_openai_model_id", model_id="openai.gpt-oss-120b")
add("parse_bedrock_openai_model_id", model_id="xx.openai.gpt-5.5")
add("parse_bedrock_openai_model_id", model_id="us.anthropic.claude-opus-4-6")
add("parse_bedrock_openai_model_id", model_id=None)
add("parse_bedrock_openai_model_id", model_id="us.openai.gpt-5.5.extra")

# ── _mantle_url_parts ──
add("_mantle_url_parts", base_url="https://bedrock-mantle.us-east-1.api.aws/openai/v1")
add("_mantle_url_parts", base_url="https://bedrock-mantle.eu-west-1.api.aws")
add("_mantle_url_parts", base_url="HTTPS://BEDROCK-MANTLE.AP-SOUTH-1.API.AWS/OpenAI/V1///")
add("_mantle_url_parts",
    base_url="https://u:***@bedrock-mantle.us-west-2.api.aws:443/openai/")
add("_mantle_url_parts", base_url="bedrock-mantle.us-east-1.api.aws/openai/v1")
add("_mantle_url_parts", base_url=None)
add("_mantle_url_parts", base_url="https://bedrock-mantle..api.aws/openai")
add("_mantle_url_parts", base_url="https://example.com/openai/v1")
add("_mantle_url_parts", base_url="https://bedrock-mantle.us-east-1.api.aws/openai/v1?x=1#f")

# ── bedrock_guardrail_config ──
GR_FULL = {"guardrail_identifier": "g1", "guardrail_version": "1"}
add("bedrock_guardrail_config", config={"bedrock": {"guardrail": dict(GR_FULL)}})
add("bedrock_guardrail_config", config={"bedrock": {"guardrail": {
    **GR_FULL, "stream_processing_mode": "async", "trace": "enabled"}}})
add("bedrock_guardrail_config", config={"bedrock": {"guardrail": {
    "guardrail_identifier": "g1"}}})
add("bedrock_guardrail_config", config={"bedrock": {"guardrail": {}}})
add("bedrock_guardrail_config", config=None, io={"load_config": {"bedrock": {
    "guardrail": {**GR_FULL, "trace": "enabled_full"}}}})
add("bedrock_guardrail_config", config=None,
    io={"load_config": {"__raise__": "Exception", "msg": "no config"}})
add("bedrock_guardrail_config", config={"bedrock": {"guardrail": {
    "guardrail_identifier": "", "guardrail_version": "1"}}})
add("bedrock_guardrail_config", config={"bedrock": {"guardrail": {
    **GR_FULL, "stream_processing_mode": "", "trace": "enabled_full"}}})

# ── bedrock_guardrail_headers ──
add("bedrock_guardrail_headers", config={"bedrock": {"guardrail": {
    **GR_FULL, "trace": "enabled"}}})
add("bedrock_guardrail_headers", config={"bedrock": {"guardrail": dict(GR_FULL)}})
add("bedrock_guardrail_headers", config={"bedrock": {"guardrail": {
    **GR_FULL, "trace": "ENABLED_FULL"}}})
add("bedrock_guardrail_headers", config={"bedrock": {"guardrail": {
    **GR_FULL, "trace": "true"}}})
add("bedrock_guardrail_headers", config={"bedrock": {"guardrail": {
    **GR_FULL, "trace": "disabled"}}})
add("bedrock_guardrail_headers", config={})
add("bedrock_guardrail_headers", config={"bedrock": {"guardrail": {
    **GR_FULL, "trace": None}}})
add("bedrock_guardrail_headers", config={"bedrock": {"guardrail": {
    **GR_FULL, "trace": True}}})

# ── anthropic_response_guardrail_intervened ──
FIELD = "amazon-bedrock-guardrailAction"
add("anthropic_response_guardrail_intervened", response={"model_extra": {FIELD: "INTERVENED"}})
add("anthropic_response_guardrail_intervened", response={"model_extra": {FIELD: "intervened"}})
add("anthropic_response_guardrail_intervened", response={"model_extra": {FIELD: "Intervened"}})
add("anthropic_response_guardrail_intervened", response={"model_extra": {FIELD: "blocked"}})
add("anthropic_response_guardrail_intervened", response={"model_extra": {}})
add("anthropic_response_guardrail_intervened", response={"model_extra": None})
add("anthropic_response_guardrail_intervened", response={})
add("anthropic_response_guardrail_intervened", response=None)
add("anthropic_response_guardrail_intervened", response={"model_extra": {FIELD: None}})

# ── _parse_tool_args ──
add("_parse_tool_args", args='{"city": "Paris", "n": 5}')
add("_parse_tool_args", args="not json")
add("_parse_tool_args", args="")
add("_parse_tool_args", args='{"a":1,}')
add("_parse_tool_args", args=42)
add("_parse_tool_args", args=None)
add("_parse_tool_args", args={"already": "dict"})
add("_parse_tool_args", args="[1, 2]")
add("_parse_tool_args", args="null")
add("_parse_tool_args", args='  {"k": true}  ')

# ── _decode_redacted (strict base64, canonical re-encode projection) ──
add("_decode_redacted", encoded="aGVsbG8=")
add("_decode_redacted", encoded="AB==")
add("_decode_redacted", encoded="")
add("_decode_redacted", encoded=None)
add("_decode_redacted", encoded="aGVsbG8")
add("_decode_redacted", encoded="aGVs bG8=")
add("_decode_redacted", encoded="!!!!")
add("_decode_redacted", encoded=42)
add("_decode_redacted", encoded="Zm9vYmFy")

# ── _image_block_from_data_url (lenient base64, utf-8 fallback) ──
add("_image_block_from_data_url", url="data:image/png;base64,aGVsbG8=")
add("_image_block_from_data_url", url="data:image/jpeg;base64,Zm9vYmFy")
add("_image_block_from_data_url", url="data:;base64,aGVsbG8=")
add("_image_block_from_data_url", url="data:image/svg+xml;base64,PHN2Zz4=")
add("_image_block_from_data_url", url="data:text/plain,hello world")
add("_image_block_from_data_url", url="https://example.com/x.png")
add("_image_block_from_data_url", url="data:image/png;base64,!!!")
add("_image_block_from_data_url", url="data:image/png;base64,héllo")
add("_image_block_from_data_url", url="data:image/webp")
add("_image_block_from_data_url", url="data:image/png;base64,aGVs bG8=")

# ── _system_blocks ──
add("_system_blocks", content="hello")
add("_system_blocks", content="   ")
add("_system_blocks", content=[{"type": "text", "text": "a"},
                               {"type": "text", "text": "  "}, {"type": "image"}])
add("_system_blocks", content=["plain", {"type": "text", "text": "b"}, 5, None])
add("_system_blocks", content=None)
add("_system_blocks", content={"not": "list"})
add("_system_blocks", content=[{"type": "text"}])
add("_system_blocks", content=[{"type": "text", "text": 42}])
add("_system_blocks", content="")

# ── _replay_ordered_blocks ──
add("_replay_ordered_blocks", blocks=[
    {"text": "hi"}, {"toolUse": {"toolUseId": "t1", "name": "f", "input": {"a": 1}}}])
add("_replay_ordered_blocks", blocks=[
    {"reasoningContent": {"text": "think", "signature": "sig"}}])
add("_replay_ordered_blocks", blocks=[
    {"reasoningContent": {"text": "t", "redactedContentBase64": "aGVsbG8="}}])
add("_replay_ordered_blocks", blocks=[
    {"reasoningContent": {"redactedContentBase64": "bad!"}}])
add("_replay_ordered_blocks", blocks=[
    {"text": 5}, "skip", {"toolUse": {"name": "g"}}])
add("_replay_ordered_blocks", blocks=[{"reasoningContent": "notdict"}])
add("_replay_ordered_blocks", blocks=[
    {"reasoningContent": {"text": "", "signature": ""}}])
add("_replay_ordered_blocks", blocks=[
    {"text": "a", "reasoningContent": {"text": "b"}}])
add("_replay_ordered_blocks", blocks=[
    {"toolUse": {"toolUseId": "t", "name": "n", "input": None}}])

# ── _reasoning_details_blocks ──
THINK = {"type": "thinking", "thinking": "t", "signature": "s"}
add("_reasoning_details_blocks", msg={"reasoning_details": [dict(THINK)]}, claude=True)
add("_reasoning_details_blocks", msg={"reasoning_details": [dict(THINK)]}, claude=False)
add("_reasoning_details_blocks", msg={"reasoning_details": [dict(THINK)]},
    claude=True, own=True)
add("_reasoning_details_blocks", msg={"reasoning_details": [
    {"type": "thinking", "thinking": "t"}]}, claude=True)
add("_reasoning_details_blocks", msg={
    "reasoning_details": [{"type": "thinking", "thinking": "  "}],
    "reasoning_content": "fallback"}, claude=False)
add("_reasoning_details_blocks", msg={"reasoning_details": [
    {"type": "redacted_thinking", "data": "aGVsbG8="}]}, claude=True)
add("_reasoning_details_blocks", msg={"reasoning_details": [
    {"type": "redacted_thinking", "redactedContentBase64": "Zm9vYmFy"}]}, claude=True)
add("_reasoning_details_blocks", msg={
    "reasoning_details": [{"type": "redacted_thinking", "data": "bad"}],
    "reasoning_content": "rc"}, claude=False)
add("_reasoning_details_blocks", msg={"reasoning_details": [dict(THINK)]},
    claude=False, own=True)
add("_reasoning_details_blocks", msg={"reasoning_details": [
    5, {"type": "thinking", "thinking": "x"}]}, claude=False)
add("_reasoning_details_blocks", msg={"reasoning_details": [
    {"type": "redacted_thinking", "data": "", "redactedContentBase64": "aGVsbG8="}]},
    claude=True)

# ── _assistant_blocks: carrier chain ──
add("_assistant_blocks", msg={"bedrock_content_blocks": [{"text": "ordered"}]},
    content="ignored")
add("_assistant_blocks", msg={"bedrock_content_blocks": [],
                              "anthropic_content_blocks": [{"type": "text", "text": "a"}]},
    claude=True)
add("_assistant_blocks", msg={
    "anthropic_content_blocks": [
        {"type": "thinking", "thinking": "t", "signature": "s"},
        {"type": "text", "text": "  "},
        {"type": "tool_use", "id": "tu1", "name": "f", "input": {"x": 1}}],
    "tool_calls": [{"id": "tu1", "function": {"name": "f", "arguments": '{"y": 2}'}}]},
    claude=True)
add("_assistant_blocks", msg={}, content="hello", claude=False)
add("_assistant_blocks", msg={}, content=["part", {"type": "text", "text": ""}],
    claude=False)
add("_assistant_blocks", msg={"tool_calls": [
    {"id": "t1", "function": {"name": "fn", "arguments": "bad"}}]},
    content=None, claude=False)
add("_assistant_blocks", msg={"reasoning_details": [
    {"type": "thinking", "thinking": "r"}]}, content="txt", claude=False)
add("_assistant_blocks", msg={"bedrock_content_blocks": [
    {"reasoningContent": {"redactedContentBase64": "bad"}}]}, content=None, claude=False)
add("_assistant_blocks", msg={"anthropic_content_blocks": [
    {"type": "redacted_thinking", "data": "!!"}, {"type": "text", "text": "ok"}]},
    claude=True)
add("_assistant_blocks", msg={}, content=[
    {"type": "image_url", "image_url": {"url": "https://x/i.png"}}], claude=False)
add("_assistant_blocks", msg={"anthropic_content_blocks": [
    {"type": "tool_use", "name": "g", "input": {"z": 3}}]}, claude=True)

# ── _converse_replay_policy ──
add("_converse_replay_policy", model=None)
add("_converse_replay_policy", model="anthropic.claude-sonnet-4-6")
add("_converse_replay_policy", model="us.anthropic.claude-opus-4-6")
add("_converse_replay_policy", model="openai.gpt-6-astra")
add("_converse_replay_policy", model="deepseek.r1")
add("_converse_replay_policy", model="moonshotai.kimi-k3")
add("_converse_replay_policy", model="amazon.nova-pro")
add("_converse_replay_policy", model=ARN, io={"resolve_profile": {ARN: ARN_CLAUDE}})
add("_converse_replay_policy", model=ARN, io={"resolve_profile": {}})
add("_converse_replay_policy", model="meta.llama4-maverick")
add("_converse_replay_policy", model=ARN, io={"resolve_profile": {
    ARN: "arn:aws:bedrock:us-east-1:123456789012:foundation-model/deepseek.r1"}})

# ── _reasoning_filter: decision sequence ──
add("_reasoning_filter", policy="signed", issuer=None, own=False, in_loop=False)
add("_reasoning_filter", policy="signed", issuer=None, own=False, in_loop=True)
add("_reasoning_filter", policy="strip", issuer=None, own=False, in_loop=False)
add("_reasoning_filter", policy="strip", issuer=None, own=False, in_loop=True)
add("_reasoning_filter", policy="unsigned", issuer=None, own=False, in_loop=False)
add("_reasoning_filter", policy="issuer", issuer=None, own=False, in_loop=True)
add("_reasoning_filter", policy="strip", issuer=None, own=True, in_loop=False)
add("_reasoning_filter", policy="unsigned", issuer=None, own=True, in_loop=False)
add("_reasoning_filter", policy="signed", issuer="openai.gpt-6-astra",
    own=False, in_loop=True)
add("_reasoning_filter", policy="signed", issuer="anthropic.claude-x", own=False)
add("_reasoning_filter", policy="signed", issuer=ARN, own=False,
    io={"resolve_profile": {}})
add("_reasoning_filter", policy="signed", issuer=ARN, own=False,
    io={"resolve_profile": {ARN: ARN_GPT}})
add("_reasoning_filter", policy="issuer", issuer="deepseek.r1", own=False, in_loop=False)
add("_reasoning_filter", policy="bogus", issuer=None, own=False, in_loop=False)
add("_reasoning_filter", policy="strip", issuer=None, own=True, in_loop=True)

# ── strip_redacted_reasoning: identity + empty-turn drop ──
REDACT = {"reasoningContent": {"redactedContent": "x"}}
add("strip_redacted_reasoning", kwargs={"messages": [
    {"role": "user", "content": [{"text": "hi"}]}]})
add("strip_redacted_reasoning", kwargs={"messages": [
    {"role": "assistant", "content": [dict(REDACT), {"text": "keep"}]}]})
add("strip_redacted_reasoning", kwargs={"messages": [
    {"role": "assistant", "content": [dict(REDACT)]},
    {"role": "user", "content": [{"text": "u"}]}]})
add("strip_redacted_reasoning", kwargs={"messages": "notlist"})
add("strip_redacted_reasoning", kwargs={})
add("strip_redacted_reasoning", kwargs={"messages": [
    None, {"role": "assistant", "content": [dict(REDACT), {"text": "k"}]}]})
add("strip_redacted_reasoning", kwargs={"messages": [
    {"role": "assistant", "content": [
        {"reasoningContent": "str"}, dict(REDACT)]}]})
add("strip_redacted_reasoning", kwargs={
    "modelId": "m",
    "messages": [{"role": "assistant", "content": [dict(REDACT), {"text": "k"}]}],
    "inferenceConfig": {"maxTokens": 5}})
add("strip_redacted_reasoning", kwargs={"messages": [
    {"role": "user"},
    {"role": "assistant", "content": [dict(REDACT), {"text": "k"}]}]})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    for k in sorted(counts):
        print(f"{k}: {counts[k]}")
    print(f"TOTAL {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    assert all(v >= 5 for v in counts.values()), "every port needs >=5 cases"
    assert 12 <= len(counts) <= 18, "target 12-18 ports"


if __name__ == "__main__":
    main()
