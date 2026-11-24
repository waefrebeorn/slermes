#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profile_provision.py (w16_profilepro_1706f1 gate).

Replays the LIVE ProfileCreateRequest.fresh / refusal / message_from /
refusal_from against the C harness output.  The real agent.redact dependency is
used; fixtures deliberately carry only non-secret text, for which its
force=True transform is the identity.
"""
import builtins
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/profile_provision.py"
spec = importlib.util.spec_from_file_location("w16_profilepro_1706f1_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w16_profilepro_1706f1_live"] = live
spec.loader.exec_module(live)

_EXC_TYPES = ("FileExistsError", "FileNotFoundError", "ValueError",
              "RuntimeError", "OSError", "KeyError")


def compute(r):
    func = r.get("func")
    if func == "fresh":
        return live.ProfileCreateRequest(
            name="x",
            clone_from=r.get("clone_from"),
            clone_all=bool(r.get("clone_all", False)),
            clone_config=bool(r.get("clone_config", False)),
        ).fresh
    if func == "refusal":
        name = r.get("type", "RuntimeError")
        cls = getattr(builtins, name) if name in _EXC_TYPES else RuntimeError
        text, data = live.refusal(cls(r.get("text", "")))
        return {"text": text, "type": data["type"], "parts": data["message_parts"]}
    if func == "message_from":
        return live.message_from({"message_parts": r.get("parts", [])})
    if func == "refusal_from":
        exc = live.refusal_from({"type": r.get("type", ""),
                                 "message_parts": r.get("parts", [])})
        if exc is None:
            return None
        return {"type": type(exc).__name__, "message": str(exc)}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w16_profilepro_1706f1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
