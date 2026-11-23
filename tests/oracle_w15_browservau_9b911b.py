#!/usr/bin/env python3
"""Differential oracle for tools/browser_vault_preview.py (gate w15_browservau_9b911b)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_browservau_9b911b"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# agent parent-package bootstrap + the page-backend base class needed at import time
_ag = _mod("agent")
_ag.__path__ = []


class BrowserPageBackend:
    pass


_mod("agent.browser_page_backend", BrowserPageBackend=BrowserPageBackend)

SRC = QUARRY + "/tools/browser_vault_preview.py"
_spec = importlib.util.spec_from_file_location("browser_vault_preview_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["browser_vault_preview_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "unavailable":
        return live._unavailable()
    if func == "request":
        def callback(payload):
            return rec.get("raw")
        return live._request(callback, "evaluate")
    return None


def main():
    cases = mism = 0
    allowed = ("request", "unavailable")
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in allowed:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
