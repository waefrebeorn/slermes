#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_events.py (gate w10_plugineven_e257fc).

Only the pure naming/validation core is compared; broadcast/forwarder side
effects are out of scope and never reached by these cases.
"""
import importlib.util, json, sys, types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
PKG = "w10_plugineven_live_pkg"
_pkg = types.ModuleType(PKG); _pkg.__path__ = [Q + "/hermes_cli"]; sys.modules[PKG] = _pkg
spec = importlib.util.spec_from_file_location(PKG + ".plugin_events", Q + "/hermes_cli/plugin_events.py")
live = importlib.util.module_from_spec(spec); sys.modules[spec.name] = live; spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "event_name":
        try:
            return {"ok": True, "value": live.plugin_event_name(r.get("plugin_id"), r.get("event"))}
        except ValueError:
            return {"ok": False, "error": "ValueError"}
    if func == "session_event_body":
        sid, pl = r.get("session_id"), r.get("payload")
        try:
            if not isinstance(sid, str) or not sid:
                raise ValueError("session")
            if pl is not None and not isinstance(pl, dict):
                raise TypeError("payload")
            name = live.plugin_event_name(r.get("plugin_id"), r.get("event"))
            body = json.loads(json.dumps(pl or {}, allow_nan=False))
            return {"ok": True, "name": name, "body": body}
        except ValueError:
            return {"ok": False, "error": "ValueError"}
        except TypeError:
            return {"ok": False, "error": "TypeError"}
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
        print("MISMATCH %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)
print("ORACLE w10_plugineven_e257fc: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
