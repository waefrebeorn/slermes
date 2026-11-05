#!/usr/bin/env python3
"""Oracle for gateway/platforms/access_policy_mixin.py: _is_dm_allowed,
_is_dm_intake_allowed, _is_group_allowed (plus the _open_dm_opted_in /
_entry_matches helpers they compose). All ast-extracted and bound onto a Mock
class so the live method calls resolve.
"""
import ast
import json
import sys
from types import SimpleNamespace

SRC = "C:/Users/eman5/hermes-agent/gateway/platforms/access_policy_mixin.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

NS = {}
for n in tree.body:
    if isinstance(n, ast.Assign):
        for t in n.targets:
            if isinstance(t, ast.Name) and t.id == "OPTIN_TRUTHY":
                seg = ast.get_source_segment(code, n)
                exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                        type_ignores=[]), "x", "exec"), NS)
    elif isinstance(n, ast.ClassDef):
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name in (
                    "_open_dm_opted_in", "_entry_matches",
                    "_is_dm_allowed", "_is_dm_intake_allowed",
                    "_is_group_allowed"):
                seg = ast.get_source_segment(code, m)
                exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                        type_ignores=[]), "x", "exec"), NS)


class Mock:
    _open_dm_opted_in = NS["_open_dm_opted_in"]
    _entry_matches = NS["_entry_matches"]
    _is_dm_allowed = NS["_is_dm_allowed"]
    _is_dm_intake_allowed = NS["_is_dm_intake_allowed"]
    _is_group_allowed = NS["_is_group_allowed"]

    def _live_dm_allow_from(self):
        return self._live_allow

    def _allow_all_env_names(self):
        return ("GATEWAY_ALLOW_ALL_USERS", "WHATSAPP_ALLOW_ALL_USERS")


def build(env0, env1, live_allow, dm_policy, group_policy, group_allow):
    me = Mock.__new__(Mock)  # bypass __init__
    me._dm_policy = dm_policy
    me._group_policy = group_policy
    me._live_allow = live_allow
    me._group_allow_from = group_allow
    NS["OPTIN_TRUTHY"] = frozenset({"true", "1", "yes"})
    NS["get_scoped_secret"] = lambda name, d: {
        "GATEWAY_ALLOW_ALL_USERS": env0,
        "WHATSAPP_ALLOW_ALL_USERS": env1}.get(name, d)
    return me


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
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
        print("ORACLE access_policy: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "optin":
            envmap = {"GATEWAY_ALLOW_ALL_USERS": c["e0"],
                      "WHATSAPP_ALLOW_ALL_USERS": c["e1"]}
            NS["OPTIN_TRUTHY"] = frozenset({"true", "1", "yes"})
            NS["get_scoped_secret"] = lambda name, d: envmap.get(name, d)
            me = SimpleNamespace(ALLOW_ALL_ENV_PREFIX="WHATSAPP",
                                 _allow_all_env_names=lambda: envmap.keys())
            want = 1 if NS["_open_dm_opted_in"](me) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH optin {c['case']}: C={c['out']} PY={want}")
        elif fam == "match":
            want = 1 if NS["_entry_matches"](None, list(c["entries"]),
                                             c["target"]) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH match {c['case']}: C={c['out']} PY={want}")
        elif fam == "dma":
            me = build(c["e0"], c["e1"], c["allow"], c["policy"], "open", [])
            want = 1 if me._is_dm_allowed(c["sender"]) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH dma {c['case']}: C={c['out']} PY={want} "
                      f"policy={c['policy']!r} sender={c['sender']!r}")
        elif fam == "dmi":
            me = build(c["e0"], c["e1"], c["allow"], c["policy"], "open", [])
            want = 1 if me._is_dm_intake_allowed(c["sender"]) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH dmi {c['case']}: C={c['out']} PY={want} "
                      f"policy={c['policy']!r} sender={c['sender']!r}")
        elif fam == "grp":
            me = build("", "", [], "open", c["policy"], c["allow"])
            want = 1 if me._is_group_allowed(c["chat"], "") else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH grp {c['case']}: C={c['out']} PY={want}")
    print(f"ORACLE access_policy: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
