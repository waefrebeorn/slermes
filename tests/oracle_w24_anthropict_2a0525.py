#!/usr/bin/env python3
"""Differential oracle for agent/anthropic_thinking_policy.py
(gate w24_anthropict_2a0525).

The live module is imported and every pure decision is called on the real
functions: ``claude_family_version``, ``model_preserves_prior_thinking``,
``anthropic_thinking_route``, ``route_replays_prior_thinking`` and
``native_anthropic_preserves_prior_thinking``.  Its endpoint predicates come from
the live ``agent.anthropic_endpoints`` module, so the C route port is compared
against the real matchers, not a copy.

Seam: ``NOUS_INFERENCE_BASE_URL`` (the operator override that makes one extra
host count as Nous Portal) is cleared, matching the C port's "override unset".
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_anthropict_2a0525"

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.pop("NOUS_INFERENCE_BASE_URL", None)   # modelled unset in the C port

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent import anthropic_thinking_policy as live  # noqa: E402

WANTED = {
    "claude_family_version", "model_preserves_prior_thinking", "thinking_route",
    "route_replays_prior_thinking", "native_preserves_prior_thinking", "constants",
}


def as_json(value):
    if isinstance(value, (tuple, list)):
        return [as_json(v) for v in value]
    if isinstance(value, dict):
        return {k: as_json(v) for k, v in value.items()}
    return value


def constants():
    return {
        "signed_replay_routes": sorted(live.SIGNED_REPLAY_ROUTES),
        "keep_all_from": {k: list(v) for k, v in sorted(live._KEEP_ALL_FROM.items())},
        "claude_version_pattern": live._CLAUDE_VERSION_RE.pattern,
        "last_turn_only_pattern": live._LAST_TURN_ONLY_RE.pattern,
    }


def expected(r):
    f = r.get("func")
    if f == "claude_family_version":
        return as_json(live.claude_family_version(r.get("model")))
    if f == "model_preserves_prior_thinking":
        return bool(live.model_preserves_prior_thinking(r.get("model")))
    if f == "thinking_route":
        return live.anthropic_thinking_route(r.get("base_url"), r.get("model"))
    if f == "route_replays_prior_thinking":
        return bool(live.route_replays_prior_thinking(r.get("route"), r.get("model")))
    if f == "native_preserves_prior_thinking":
        return bool(live.native_anthropic_preserves_prior_thinking(
            r.get("base_url"), r.get("model")))
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
