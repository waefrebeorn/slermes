#!/usr/bin/env python3
"""Differential oracle for agent/prepared_compaction.py (gate w11_preparedco_e759f4).

The pure decision helpers are exercised directly.  The oneshot probe, monotonic
clock and all agent/compressor attributes are injected from the fixture; the
agent package and agent.oneshot_footprint are registered as stubs so the live
module's local import resolves without loading the heavy runtime.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

# Parent-package bootstrap + stubbed external seams.
agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
sys.modules["agent"] = agent_pkg
ONESHOT = {"v": False}
oneshot_mod = types.ModuleType("agent.oneshot_footprint")
oneshot_mod.agent_is_oneshot = lambda agent: ONESHOT["v"]
agent_pkg.oneshot_footprint = oneshot_mod
sys.modules["agent.oneshot_footprint"] = oneshot_mod

SRC = QUARRY + "/agent/prepared_compaction.py"
spec = importlib.util.spec_from_file_location("w11_preparedco_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_preparedco_live"] = live
spec.loader.exec_module(live)

_COMPRESSOR_FIELDS = (
    "_micro_compact_enabled", "awaiting_real_usage_after_compression",
    "_fallback_compression_streak", "last_prompt_tokens", "threshold_tokens",
    "_summary_failure_cooldown_until",
)


def compute(r):
    f = r.get("func")
    ONESHOT["v"] = bool(r.get("oneshot", False))
    if f == "blocked_for_platform":
        agent = SimpleNamespace(
            platform=r.get("platform"),
            _delegate_depth=r.get("_delegate_depth", 0))
        return live._blocked_for_platform(agent, r.get("origin", ""))
    if f == "blocked":
        state = live.PreparedCompaction()
        state._needs_provider_context = bool(r.get("needs_provider_context", False))
        state._retry_after = float(r.get("retry_after", 0))
        ag = r.get("agent") or {}
        cp = r.get("compressor") or {}
        agent = SimpleNamespace(
            compression_enabled=ag.get("compression_enabled", False),
            _persist_disabled=ag.get("_persist_disabled", False),
            api_mode=ag.get("api_mode", ""),
            codex_responses_native_compaction=ag.get("codex_responses_native_compaction", False),
            platform=ag.get("platform"),
            _delegate_depth=ag.get("_delegate_depth", 0))
        kwargs = {k: cp.get(k, 0 if "streak" in k or "tokens" in k else
                            0.0 if "cooldown" in k else False)
                  for k in _COMPRESSOR_FIELDS}
        compressor = SimpleNamespace(**kwargs)
        live.time.monotonic = lambda: r.get("now", 0)
        return state._blocked(agent, compressor, r.get("origin", ""))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_preparedco_e759f4: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_preparedco_e759f4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
