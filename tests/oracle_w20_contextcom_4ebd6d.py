#!/usr/bin/env python3
"""Differential oracle for agent/context_compressor_summary.py (w20_contextcom_4ebd6d).

The live module is imported from the quarry against a stubbed
`agent.auxiliary_client` (its only import-time dependency besides the real
`agent.auxiliary_task_config`, which IS loaded from source because its
AuthoritativeAuxiliaryTask string subclass is the value under test). Each
fixture line describes a callable's inspect.signature() or a compressor
`self`; the oracle rebuilds both and replays the live helper.
"""
import importlib.util
import inspect
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class AuxiliaryExplicitCancellation(Exception):
    pass


if "agent" not in sys.modules:
    _mk("agent")
_mk("agent.auxiliary_client", AuxiliaryExplicitCancellation=AuxiliaryExplicitCancellation)

_aux_sp = importlib.util.spec_from_file_location(
    "agent.auxiliary_task_config", os.path.join(Q, "agent", "auxiliary_task_config.py"))
_aux = importlib.util.module_from_spec(_aux_sp)
sys.modules["agent.auxiliary_task_config"] = _aux
_aux_sp.loader.exec_module(_aux)

NAME = "w20_contextcom_4ebd6d_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "context_compressor_summary.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

KINDS = {
    "pos_only": inspect.Parameter.POSITIONAL_ONLY,
    "pos_or_kw": inspect.Parameter.POSITIONAL_OR_KEYWORD,
    "var_pos": inspect.Parameter.VAR_POSITIONAL,
    "kw_only": inspect.Parameter.KEYWORD_ONLY,
    "var_kw": inspect.Parameter.VAR_KEYWORD,
}


def build_callable(spec):
    if spec is None:
        return 3  # inspect.signature(3) raises TypeError
    params = [inspect.Parameter(p["name"], KINDS[p["kind"]]) for p in (spec.get("params") or [])]
    if spec.get("var_keyword"):
        params.append(inspect.Parameter("kwargs", inspect.Parameter.VAR_KEYWORD))

    def fn():
        return None

    fn.__signature__ = inspect.Signature(params)
    return fn


class Compressor:
    """The compressor attributes `_apply_summary_route` reads off `self`."""


def expected(r):
    f = r.get("func")
    if f == "accepts_keyword":
        target = build_callable(r.get("callable"))
        return live.accepts_keyword_argument(target, r.get("name"))
    if f == "apply_route":
        self_spec = r.get("self")
        me = Compressor()
        if isinstance(self_spec, dict):
            for key, value in self_spec.items():
                setattr(me, key, value)
        kwargs = dict(r.get("call_kwargs") or {})
        live.SummaryDispatchMixin._apply_summary_route(me, kwargs, r.get("pinned"))
        return kwargs
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_contextcom_4ebd6d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
