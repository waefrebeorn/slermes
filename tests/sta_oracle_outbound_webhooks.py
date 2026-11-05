#!/usr/bin/env python3
"""Oracle for agent/outbound_webhooks.py: _serialize_payload and
_build_delivery (HMAC-SHA256 signature). Both are ast-extracted and run live;
only the profile/timestamp/payload-fields IO is stubbed.
"""
import ast
import hashlib
import hmac
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/agent/outbound_webhooks.py"

_provided = {"_profilename": "default", "_timestamp": "T0", "_extra": {}}


def _ns():
    ns = {"hmac": hmac, "hashlib": hashlib, "json": json}
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in (
                "_serialize_payload", "_build_delivery"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    ns["_utc_now_iso"] = lambda: _provided["_timestamp"]
    ns["_payload_fields"] = lambda kwargs: _provided["_extra"]
    return ns


def _seed_profiles():
    m = types.ModuleType("hermes_cli.profiles")
    m.get_active_profile_name = lambda: _provided["_profilename"]
    sys.modules["hermes_cli"] = types.ModuleType("hermes_cli")
    sys.modules["hermes_cli.profiles"] = m


class _Target:
    def __init__(self, url, label, secret, timeout):
        self.url, self.label, self.secret, self.timeout = url, label, secret, timeout


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    _seed_profiles()
    NS = _ns()
    ser = NS["_serialize_payload"]
    bdel = NS["_build_delivery"]
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE outbound_webhooks: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "ser":
            _provided.update(_profilename=c["prof"], _timestamp=c["ts"],
                             _extra=c["extra"])
            want = json.loads(ser(c["event"], c["extra"], c["did"]).decode())
            got = json.loads(c["out"]) if isinstance(c["out"], str) else None
            if got != want:
                mism += 1
                print(f"MISMATCH ser {c['case']}: C={got} PY={want}")
        elif fam == "del":
            target = _Target(c["url"], c["label"], c["secret"], c["timeout"])
            want = bdel(c["event"], target, c["body"].encode(), c["did"])
            want["body"] = want["body"].decode()
            w = {"url": want["url"], "label": want["label"], "event": want["event"],
                 "body": want["body"], "headers": want["headers"],
                 "timeout": want["timeout"]}
            got = c["out"]
            if got != w:
                mism += 1
                print(f"MISMATCH del {c['case']}: C={got} PY={w}")
    print(f"ORACLE outbound_webhooks: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
