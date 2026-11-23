#!/usr/bin/env python3
"""Differential oracle for w13_methodsvoi_a3e372 (tui_gateway/methods_voice.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/tui_gateway/methods_voice.py"
sys.path.insert(0, QUARRY)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [QUARRY + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg

spec = importlib.util.spec_from_file_location("tui_gateway.methods_voice", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.methods_voice"] = live
spec.loader.exec_module(live)
# _ok is a server.py global; identity on the wire for this seam.
live._ok = lambda rid, result: result


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False))


def compute(r):
    f = r.get("func")
    if f == "voice_cfg_number":
        return live._voice_cfg_number(r.get("value"), r.get("default"))
    if f == "frame_fields":
        return live._frame_fields(dict(r.get("frame") or {}))
    if f == "fd_barge_params":
        return list(live._fd_barge_params(dict(r.get("cfg") or {})))
    if f == "owner_result":
        return live._owner_result(r.get("rid", ""), r.get("field", ""),
                                  r.get("ok", False), **(r.get("extra") or {}))
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
        py = canon(compute(r))
    except Exception as exc:
        print(f"ORACLE w13_methodsvoi_a3e372: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH w13_methodsvoi_a3e372 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_methodsvoi_a3e372: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
