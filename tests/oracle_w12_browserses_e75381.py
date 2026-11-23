#!/usr/bin/env python3
"""Differential oracle for tools/browser_session_cdp.py (gate w12_browserses_e75381)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_browserses_e75381"
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


for _pkg in ("tools", "agent"):
    try:
        __import__(_pkg)
    except Exception:
        _seed(_pkg)

# The live module imports these at module load; stub them so no network/proxy work happens.
_seed("agent.proxy_bypass", loopback_request_kwargs=lambda url: {})
_seed("agent.redact", redact_cdp_url=lambda url: url)

DISCOVERY = {"payload": None}


class _Resp:
    def __init__(self, payload):
        self._payload = payload

    def raise_for_status(self):
        pass

    def json(self):
        return self._payload


def _fake_get(url, timeout=None, **kwargs):
    if DISCOVERY["payload"] is None:
        raise Exception("discovery failed")
    return _Resp(DISCOVERY["payload"])


_seed("requests", get=_fake_get)

SRC = QUARRY + "/tools/browser_session_cdp.py"
_spec = importlib.util.spec_from_file_location("browser_session_cdp_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["browser_session_cdp_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    if rec.get("func") != "resolve":
        return None
    DISCOVERY["payload"] = rec.get("discovery")
    return live.resolve_cdp_override(rec.get("cdp_url", ""))


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
        if rec.get("func") != "resolve":
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} "
                  f"C={got!r} PY={py!r} input={rec!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
