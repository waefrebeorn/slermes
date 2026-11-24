#!/usr/bin/env python3
"""Oracle for gate w18_auxiliaryc_cbc55f (agent/auxiliary_codex_response.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

# Controlled seam for the heavy shared normalizer: the live module imports
# these lazily from agent.codex_responses_adapter inside _parse_codex_final_response.
CURRENT = {}
def _lower_or_none(v):
    return v.lower() if isinstance(v, str) else None
def _fake_normalize(response, *, issuer_kind=None, issuer_model=None, recover_leaked_tool_call=True):
    n = CURRENT["_norm"]
    return (SimpleNamespace(content=n.get("content"), tool_calls=n.get("tool_calls")),
            n.get("finish_reason", "stop"))
_adapter = types.ModuleType("agent.codex_responses_adapter")
_adapter._lower_or_none = _lower_or_none
_adapter._normalize_codex_response = _fake_normalize
sys.modules["agent.codex_responses_adapter"] = _adapter

live = _load("agent.auxiliary_codex_response", "agent/auxiliary_codex_response.py")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func != "parse":
        continue
    CURRENT.clear()
    CURRENT["_norm"] = r.get("_norm") or {}
    final = SimpleNamespace(**(r.get("final") or {}))
    tp, tc, usage, fr = live._parse_codex_final_response(
        final, issuer_kind=r.get("issuer_kind"), issuer_model=r.get("issuer_model"))
    u = None if usage is None else {
        "prompt_tokens": usage.prompt_tokens,
        "completion_tokens": usage.completion_tokens,
        "total_tokens": usage.total_tokens,
    }
    py = [tp, tc, u, fr]
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_auxiliaryc_cbc55f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
