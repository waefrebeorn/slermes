#!/usr/bin/env python3
"""Differential oracle for gateway/webapp_listener.py (gate w9_webappliste_3df92f)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w9_webappliste_3df92f"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


try:
    import gateway  # noqa: F401
except Exception:
    _seed("gateway")

SRC = QUARRY + "/gateway/webapp_listener.py"
_spec = importlib.util.spec_from_file_location("webapp_listener_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["webapp_listener_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "bare_host":
        return live._bare_host(rec.get("host"))
    if func == "is_wildcard":
        return live._is_wildcard(rec.get("host"))
    if func == "is_loopback":
        return live._is_loopback(rec.get("host"))
    if func == "dial_host":
        return live.dial_host(rec.get("host"))
    if func == "dial_origin":
        return live.dial_origin(rec.get("host"), int(rec["port"]))
    if func == "public_url":
        return live._public_url(rec.get("value"))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("bare_host", "is_wildcard", "is_loopback",
                                    "dial_host", "dial_origin", "public_url"):
            continue
        cases += 1
        py = live_out(rec)
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
