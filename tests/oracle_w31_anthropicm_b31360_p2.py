#!/usr/bin/env python3
"""Differential oracle for agent/anthropic_message_convert.py — the pure leaf
surface (gate w31_anthropicm_b31360_p2).

One case per line selects a live helper by name; the same JSON value is handed
to the C port through tests/t_port_w31_anthropicm_b31360_p2.c.  The live module
and the policy modules it routes on (``agent.anthropic_thinking_policy``,
``agent.bedrock_adapter.reasoning_minted_outside_claude``) are loaded from the
quarry checkout by file path so nothing in this repo shadows them.

Seams (not compared): ``rasterize_svg_data_url`` (soft rasterizer, absent on
this host) and Bedrock inference-profile ARN resolution (network) — an
unresolved profile ARN is "unknown issuer" on both sides.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_anthropicm_b31360_p2"

for _p in (str(Path(__file__).resolve().parent), str(QUARRY)):
    if _p not in sys.path:
        sys.path.insert(0, _p)

# ``pm`` is the host's packaged-runtime bootstrap (dependency-install probe on
# import). Stubbed: only install hints come from it, and agent.bedrock_adapter
# imports it.
if "pm" not in sys.modules:
    import types as _types

    _pm = _types.ModuleType("pm")
    _pm.install_hint = lambda *a, **k: ""
    sys.modules["pm"] = _pm

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()


def _load(name, relpath):
    spec = importlib.util.spec_from_file_location(name, QUARRY / relpath)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


live = _load("amc_p2_live", "agent/anthropic_message_convert.py")
policy = _load("anthropic_thinking_policy", "agent/anthropic_thinking_policy.py")
bedrock = _load("bedrock_adapter", "agent/bedrock_adapter.py")


def _blocks(r):
    b = r.get("blocks")
    return b if isinstance(b, list) else []


def _message(r):
    m = r.get("message")
    return m if isinstance(m, dict) else {}


CASES = {
    "model_name": lambda r: live.normalize_model_name(r.get("model"), bool(r.get("preserve_dots"))),
    "tools": lambda r: live.convert_tools_to_anthropic(r.get("tools")),
    "normalize_schema": lambda r: live._normalize_tool_input_schema(r.get("schema")),
    "content_part": lambda r: live._convert_content_part_to_anthropic(r.get("part")),
    "content_list": lambda r: live._convert_content_to_anthropic(r.get("content")),
    "content_parts_blocks": lambda r: live._content_parts_to_anthropic_blocks(r.get("parts")),
    "image_block": lambda r: live._image_block_from_openai_url(r.get("url")),
    "sanitize_replay": lambda r: live._sanitize_replay_block(r.get("block")),
    "split_blank": lambda r: (lambda t: {"kept": t[0], "relocated": t[1], "dropped": t[2]})(
        live._split_blank_text_blocks(_blocks(r))),
    "is_blank": lambda r: live._is_blank_text_block(r.get("block")),
    "safe_text": lambda r: live._safe_text(r.get("text")),
    "sanitize_tool_id": lambda r: live._sanitize_tool_id(r.get("id")),
    "parse_tool_args": lambda r: live._parse_tool_args(r.get("raw")),
    "tool_result_content": lambda r: live._tool_result_content(_message(r)),
    "route": lambda r: policy.anthropic_thinking_route(r.get("base_url"), r.get("model")),
    "replays_prior": lambda r: policy.route_replays_prior_thinking(r.get("route"), r.get("model")),
    "issuer_foreign": lambda r: bedrock.reasoning_minted_outside_claude(_message(r)),
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        func = r.get("func") if isinstance(r, dict) else None
        if func not in CASES:
            continue
        cases += 1
        try:
            py = CASES[func](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (func, r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py =%s\n  c  =%s"
                  % (func, r.get("case"), json.dumps(py, sort_keys=True),
                     json.dumps(got, sort_keys=True)), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
