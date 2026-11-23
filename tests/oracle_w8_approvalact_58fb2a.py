#!/usr/bin/env python3
"""Differential oracle for w8_approvalact_58fb2a -> gateway/approval_actor.py.

gateway.slash_access is a late import: a scripted SlashAccessPolicy,
policy_for_chat_type and _coerce_id_list are registered in sys.modules.  Their
behaviour is mirrored by the C port (extra.enabled/admins/commands and the
fixture's admin_ids list).  _refusal direct cases use only normal requester
names / allowed outcomes; may_answer covers the NUL-prefixed sentinels as bools.
"""
import importlib.util
import json
import sys
import types

G = "w8_approvalact_58fb2a"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("gateway")


class SlashAccessPolicy:
    def __init__(self, enabled, admin_user_ids, allowed_commands):
        self.enabled = bool(enabled)
        self.admin_user_ids = frozenset(admin_user_ids)
        self.allowed_commands = frozenset(allowed_commands)

    def can_run(self, actor, command):
        if actor is not None and str(actor) in self.admin_user_ids:
            return True
        return command in self.allowed_commands


def _coerce_id_list(v):
    if v is None:
        return frozenset()
    if isinstance(v, str):
        return frozenset([v])
    try:
        return frozenset(str(x) for x in v)
    except TypeError:
        return frozenset()


def policy_for_chat_type(extra, chat_type):
    extra = extra or {}
    return SlashAccessPolicy(
        bool(extra.get("enabled", False)),
        [str(x) for x in (extra.get("admins") or [])],
        list(extra.get("commands") or []))


_mod("gateway.slash_access", SlashAccessPolicy=SlashAccessPolicy,
     _coerce_id_list=_coerce_id_list, policy_for_chat_type=policy_for_chat_type)

SRC = Q + "/gateway/approval_actor.py"
spec = importlib.util.spec_from_file_location("w8_approvalact_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_approvalact_live"] = live
spec.loader.exec_module(live)


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    cmds = tuple(r.get("commands") or ())
    adm = bool(r.get("admitted", True))
    try:
        if func == "may_answer":
            py = live.may_answer(r.get("authority") or {}, r.get("actor_id"),
                                 r.get("admin_ids") or (), r.get("extra"),
                                 cmds, admitted=adm)
        elif func == "refusal":
            py = live._refusal(r.get("authority") or {}, r.get("actor_id"),
                               r.get("admin_ids") or (), r.get("extra"),
                               cmds, adm)
        elif func == "commands":
            py = list(live.approval_commands(r.get("choice")))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = canonical(r.get("out"))
    cases += 1
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={canonical(py)!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or not cases else 0)
