#!/usr/bin/env python3
"""Differential oracle for agent/prompt_cache_lifecycle/replay.py
(gate w24_replay_2d5a10).

Every ported function is called on the LIVE module.  The only adaptation is the
client object ``replay_cap_honoured`` inspects: the wire carries a descriptor
({"kind", "effective", "sdk"}), and this oracle materialises a real object from
it -- an actual ``openai.OpenAI`` (no request is made), a class of a stub
``anthropic`` module installed in ``sys.modules`` (the SDK is not installed
here, and ``_sdk_client_types`` is precisely the seam that asks), or a plain
object exposing ``effective_output_cap(**kwargs)``.

``_supports_adaptive_thinking`` is compared against the live adapter directly,
so the Claude + reasoning branch of ``is_replayable`` is checked against the
upstream predicate rather than against this oracle's opinion.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_replay_2d5a10"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.prompt_cache_lifecycle import replay as live  # noqa: E402
from agent.anthropic_adapter import _supports_adaptive_thinking  # noqa: E402

WANTED = {"markers", "replayable", "cap", "bounded", "honoured", "adaptive", "constants"}


# ------------------------------------------------------------- client seam

def _stub_anthropic():
    mod = sys.modules.get("anthropic")
    if mod is not None and getattr(mod, "_w24_stub", False):
        return mod
    mod = types.ModuleType("anthropic")
    mod._w24_stub = True

    class Anthropic:
        pass

    class AnthropicBedrock:
        pass

    class AnthropicVertex:
        pass

    mod.Anthropic = Anthropic
    mod.AnthropicBedrock = AnthropicBedrock
    mod.AnthropicVertex = AnthropicVertex
    return mod


def _install_anthropic(present):
    if present:
        sys.modules["anthropic"] = _stub_anthropic()
    else:
        sys.modules["anthropic"] = None      # import raises ImportError


_openai_client = None


def _openai():
    global _openai_client
    if _openai_client is None:
        import openai
        _openai_client = openai.OpenAI(api_key="gate-oracle-no-request")
    return _openai_client


def build_client(desc):
    if not isinstance(desc, dict):
        return object()
    effective = desc.get("effective")
    if isinstance(effective, (int, float)) and not isinstance(effective, bool):
        cap = effective

        class _Effective:
            def effective_output_cap(self, **kwargs):
                return cap

        return _Effective()
    kind = desc.get("kind")
    sdk = desc.get("sdk")
    sdk = sdk if isinstance(sdk, dict) else {}
    if kind == "openai":
        return _openai()
    if kind in ("anthropic", "anthropic_bedrock", "anthropic_vertex"):
        _install_anthropic(bool(sdk.get("anthropic", True)))
        mod = sys.modules["anthropic"]
        if mod is None:
            return object()          # no SDK -> no Anthropic client can exist
        return {"anthropic": mod.Anthropic,
                "anthropic_bedrock": mod.AnthropicBedrock,
                "anthropic_vertex": mod.AnthropicVertex}[kind]()
    return object()


# --------------------------------------------------------------- functions

def markers(req):
    kw = req.get("api_kwargs")
    if not isinstance(kw, dict):
        return "ERROR:attribute"
    return live.has_cache_markers(kw)


def replayable(req):
    kw = req.get("api_kwargs")
    if not isinstance(kw, dict):
        return "ERROR:attribute"
    try:
        return live.is_replayable(kw)
    except AttributeError:
        return "ERROR:attribute"


def cap(req):
    model = req.get("model")
    if model is not None and not isinstance(model, str):
        return "ERROR:attribute"
    return live.replay_output_cap(model or "")


def bounded(req):
    kw = req.get("api_kwargs")
    model = req.get("model")
    if not isinstance(kw, dict):
        return "ERROR:attribute"
    if model is not None and not isinstance(model, str):
        return "ERROR:attribute"
    return live.bounded_chat_replay(kw, model or "")


def honoured(req):
    kw = req.get("api_kwargs")
    if not isinstance(kw, dict):
        return "ERROR:attribute"
    model = req.get("model")
    if model is not None and not isinstance(model, str):
        return "ERROR:attribute"
    mode = req.get("api_mode")
    mode = mode if isinstance(mode, str) else ""
    client = build_client(req.get("client"))
    return live.replay_cap_honoured(client, kw, model or "", mode)


def adaptive(req):
    model = req.get("model")
    if model is not None and not isinstance(model, str):
        return False
    return _supports_adaptive_thinking(model)


def constants():
    return {
        "warmable_api_modes": sorted(live.WARMABLE_API_MODES),
        "extra_body_output_caps": list(live._EXTRA_BODY_OUTPUT_CAPS),
        "openai_reasoning_pattern": live._OPENAI_REASONING_MODEL.pattern,
    }


def expected(r):
    f = r.get("func")
    if f == "markers":
        return markers(r)
    if f == "replayable":
        return replayable(r)
    if f == "cap":
        return cap(r)
    if f == "bounded":
        return bounded(r)
    if f == "honoured":
        return honoured(r)
    if f == "adaptive":
        return adaptive(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


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
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
