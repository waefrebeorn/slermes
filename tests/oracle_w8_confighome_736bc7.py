#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_home.py (gate w8_confighome_736bc7).

Logic under test: config_load_issue(exc).  The exception kind/message are
injected from the fixture; the LIVE function decides which ConfigIssue to
build.  We stub the Python-only exception carrier (ConfigIssue) and the
module-level hermes_dir_acl dependency, then read the returned issue fields.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class ConfigIssue:
    def __init__(self, severity, message, remediation):
        self.severity = severity
        self.message = message
        self.remediation = remediation


_stub("hermes_dir_acl", make_user_dir=lambda *a, **k: None)
_stub("hermes_cli.config_issues", ConfigIssue=ConfigIssue)

LIVE_NAME = "w8_confighome_736bc7_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/config_home.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "config_load_issue_ex":
        continue

    kind = r.get("exc_kind", "")
    msg = r.get("exc_message", "")
    if kind == "home":
        exc = live.HomeInitializationError(msg)
    elif kind == "os":
        exc = OSError(msg)
    else:
        exc = ValueError(msg)
    ci = live.config_load_issue(exc)
    py = {"severity": ci.severity, "message": ci.message, "remediation": ci.remediation}

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH config_load_issue_ex %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_confighome_736bc7: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
