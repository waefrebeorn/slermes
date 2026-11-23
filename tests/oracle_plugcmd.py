#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/plugin_command_dispatch.py."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugin_command_dispatch.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

spec = importlib.util.spec_from_file_location("plugcmd_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["plugcmd_live"] = live
spec.loader.exec_module(live)

_MISSING = object()


def make_handler(params):
    if any(p["kind"] == "NOSIG" for p in params):
        class NoSignature:
            @property
            def __signature__(self):
                raise ValueError("no signature")
            def __call__(self, raw):
                return {"raw_args": raw, "kwargs": {}, "legacy": True}
        return NoSignature()

    pre, pos, kwonly = [], [], []
    varpos = varkw = None
    varpos_i = varkw_i = -1
    for i, p in enumerate(params):
        k, nm = p["kind"], p["name"]
        if k == "POSITIONAL_ONLY": pre.append(nm)
        elif k == "POSITIONAL_OR_KEYWORD": pos.append(nm)
        elif k == "VAR_POSITIONAL": varpos, varpos_i = nm, i
        elif k == "KEYWORD_ONLY": kwonly.append(nm)
        elif k == "VAR_KEYWORD": varkw, varkw_i = nm, i

    parts = []
    if pre:
        parts += [f"{n}=_MISSING" for n in pre] + ["/"]
    parts += [f"{n}=_MISSING" for n in pos]
    if varpos: parts.append("*" + varpos)
    elif kwonly: parts.append("*")
    parts += [f"{n}=_MISSING" for n in kwonly]
    if varkw: parts.append("**" + varkw)

    names = [p["name"] for p in params]
    included = [n for i, n in enumerate(names)
                if i > 0 and i not in (varpos_i, varkw_i)]
    body = f"def handler({', '.join(parts)}):\n"
    body += f"    out = {{'raw_args': {names[0]}, 'kwargs': {{}}, 'legacy': False}}\n"
    for nm in included:
        body += f"    if {nm} is not _MISSING: out['kwargs'][{nm!r}] = {nm}\n"
    if varkw:
        body += f"    out['kwargs'].update({varkw})\n"
    body += "    return out\n"
    ns = {"_MISSING": _MISSING}
    exec(compile(body, "<handler>", "exec"), ns)
    return ns["handler"]


def compute(r):
    if r.get("func") != "invoke":
        return None
    params = r.get("params") or []
    handler = make_handler(params)
    return live.invoke_plugin_command(handler, r.get("raw_args"), **(r.get("context") or {}))


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
        print(f"ORACLE plugcmd: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH plugcmd case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE plugcmd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
