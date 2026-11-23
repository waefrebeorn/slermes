#!/usr/bin/env python3
"""Differential oracle for live gateway/session_commands.py (w10_sessioncom_c9451d)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def cron_read(tokens):
    if not tokens:
        return "overview"
    if tokens[0].lower() == "list" and set(tokens[1:]) <= {"--all", "-a"}:
        return "list-all" if tokens[1:] else "list"
    return None


class AuthorityRefusal(Exception):
    pass


stub("gateway.session_cron_commands", cron_read=cron_read)
stub("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

NAME = "scom_c9451d_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/gateway/session_commands.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "first_word":
        return live._first_word(r.get("arg", ""))
    if f == "first_token":
        try:
            return live._first_token(r.get("arg", ""))
        except AuthorityRefusal:
            return {"__raise__": "AuthorityRefusal", "msg": "invalid_params"}
    if f == "command_verdict":
        return live.command_verdict(r.get("name", ""), r.get("arg", ""))
    if f == "refusal_hint":
        return live.REFUSAL_HINTS.get(r.get("name", ""))
    if f == "text":
        o = r.get("output")
        obj = types.SimpleNamespace(**o) if isinstance(o, dict) else o
        return live._text(obj)
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
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %s: %r" % (r.get("func"), exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_sessioncom_c9451d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
