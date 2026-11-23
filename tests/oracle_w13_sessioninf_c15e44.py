#!/usr/bin/env python3
"""Differential oracle for gate w13_sessioninf_c15e44 (gateway/session_info_reads.py).

The LIVE owner_lines / run_info_read are driven with scripted seams for the
owner-loop offload, the info-line builders and the frozen-route policy. The
join/strip and read dispatch under test are the quarry code itself.
"""
import asyncio
import contextlib
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessioninf_c15e44"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace
CTX = {}


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


for _pkg in ("gateway", "hermes_cli"):
    if _pkg not in sys.modules:
        _seed(_pkg)


@contextlib.contextmanager
def owner_scope(authority):
    yield


_seed("gateway.session_authorities", owner_scope=owner_scope)


def toolsets_lines(items):
    return ["toolset:" + str(t) for t in items]


def platforms_lines():
    return ["platform:telegram"]


_seed("hermes_cli.cli_info_lines", toolsets_lines=toolsets_lines, platforms_lines=platforms_lines)


class _Policy:
    def current_toolsets(self, config):
        return CTX.get("toolsets") or []

    def config(self, authority, secrets=False):
        return {}


def policy_for_source(runner, source):
    return _Policy() if CTX.get("policy") else None


_seed("gateway.session_policy", policy_for_source=policy_for_source)

SRC = QUARRY + "/gateway/session_info_reads.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


async def _sync_offload(fn):
    return fn()  # run the owner build inline, no thread hop


def compute(r):
    CTX.clear()
    CTX.update({"policy": bool(r.get("policy")), "toolsets": r.get("toolsets") or []})
    func = r.get("func")
    if func == "owner_lines":
        lines = list(r.get("lines") or [])
        return asyncio.run(live.owner_lines(NS(), lambda: lines, _sync_offload))
    if func == "run_info_read":
        try:
            return asyncio.run(live.run_info_read(NS(runner=NS()), NS(source=NS()), r.get("name")))
        except KeyError:
            return {"error": "KeyError"}
    return None


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
