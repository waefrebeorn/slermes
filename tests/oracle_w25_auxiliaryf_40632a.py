#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_fallback_recovery.py (gate w25_auxiliaryf_40632a).

Two live surfaces are compared:

  * ``reapply_repairs`` is called for real with the rungs' own strip callables.
    ``with_reasoning_floor`` and ``_without_structured_output_format`` are
    imported from their modules; the three strips defined inside
    ``agent/auxiliary_client.py`` are compiled from that file's own source
    (importing the whole module needs a live home directory and a provider
    catalogue, neither of which this decision depends on);
  * ``_parameter_ladder`` is driven as a generator with the rung body and the
    failure classifier stubbed, so the compared route facts are the ones the
    live function computes (``async_mode``, ``final_model``, ``base_info``) plus
    the ``max_tokens`` it hands the rung.

The tables those strips consult are read from the live modules that own them.
"""
import ast
import importlib.util
import json
import sys
import types
from pathlib import Path
from typing import Optional

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_auxiliaryf_40632a"
MODULE = QUARRY / "agent" / "auxiliary_fallback_recovery.py"
AUX_CLIENT = QUARRY / "agent" / "auxiliary_client.py"


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


profile_gates = load_module("fg_profile_gates", QUARRY / "agent" / "auxiliary_profile_gates.py")
reasoning_floor = load_module("fg_reasoning_floor", QUARRY / "agent" / "auxiliary_reasoning_floor.py")
structured_output = load_module("fg_structured_output", QUARRY / "agent" / "auxiliary_structured_output.py")


def strips_from_aux_client():
    """The three strip helpers defined in agent/auxiliary_client.py, compiled
    from that file's own source so the port is compared to the shipped code."""
    tree = ast.parse(AUX_CLIENT.read_text(encoding="utf-8"))
    wanted = {"_without_temperature", "_without_max_tokens", "_without_reasoning_fields"}
    body = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in wanted]
    if len(body) != len(wanted):
        raise AssertionError("strip helpers not found in %s" % AUX_CLIENT)
    namespace = {"Optional": Optional, "PROFILE_REASONING_KEYS": profile_gates.PROFILE_REASONING_KEYS}
    exec(compile(ast.Module(body=body, type_ignores=[]), str(AUX_CLIENT), "exec"), namespace)
    return namespace


_aux_strips = strips_from_aux_client()

STRIPS = {
    "temperature": _aux_strips["_without_temperature"],
    "max_tokens": _aux_strips["_without_max_tokens"],
    "reasoning": _aux_strips["_without_reasoning_fields"],
    "structured_output": structured_output._without_structured_output_format,
    "reasoning_floor": reasoning_floor.with_reasoning_floor,
}


class _LadderRoute:
    """Stands in for the namedtuple: it accepts the keys the live call passes."""

    _fields = ("client", "task", "tag", "async_mode", "base_info", "resolved_provider",
               "resolved_model", "resolved_base_url", "resolved_api_key", "resolved_api_mode",
               "final_model", "main_runtime", "route_info", "timeout")

    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)


RECORD = {}


def _failure(route, exc, step=None):
    return types.SimpleNamespace(route=route, error=exc)


def _ladder_parameter_rungs(first_err, route, kwargs, max_tokens):
    RECORD["route"] = route
    RECORD["max_tokens"] = max_tokens
    RECORD["kwargs"] = kwargs
    if False:  # pragma: no cover - makes this a generator, as the live rung is
        yield
    return types.SimpleNamespace(response=None, failure=None, repairs=())


agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [str(QUARRY / "agent")]
sys.modules.setdefault("agent", agent_pkg)
aux_client = types.ModuleType("agent.auxiliary_client")
aux_client._failure = _failure
aux_client._LadderRoute = _LadderRoute
aux_client._ladder_parameter_rungs = _ladder_parameter_rungs
sys.modules["agent.auxiliary_client"] = aux_client


def load_live():
    spec = importlib.util.spec_from_file_location("fg_fallback_recovery", MODULE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


live = load_live()

WANTED = {"reapply_repairs", "ladder_route", "constants"}


def reapply_repairs(r):
    kwargs = r.get("kwargs")
    kwargs = dict(kwargs) if isinstance(kwargs, dict) else {}
    names = r.get("repairs")
    repairs = tuple(STRIPS[name] for name in (names if isinstance(names, list) else []) if name in STRIPS)
    return live.reapply_repairs(kwargs, repairs)


def ladder_route(r):
    kwargs = r.get("kwargs")
    kwargs = dict(kwargs) if isinstance(kwargs, dict) else {}
    base_url = r.get("base_url")
    base_url = base_url if isinstance(base_url, str) else ""
    client = types.SimpleNamespace(base_url=r.get("client_base_url"))
    source = live._parameter_ladder(Exception("400 rejected a parameter"), client, kwargs,
                                    task=r.get("task"), tag=r.get("tag", ""),
                                    provider=r.get("provider", ""), base_url=base_url)
    try:
        next(source)
    except StopIteration:
        pass
    route = RECORD["route"]
    return {
        "max_tokens": RECORD["max_tokens"],
        "final_model": route.final_model,
        "base_info": route.base_info,
        "async_mode": route.async_mode,
        "tag": route.tag,
        "task": route.task,
        "resolved_provider": route.resolved_provider,
    }


def constants():
    return {
        "profile_reasoning_keys": sorted(profile_gates.PROFILE_REASONING_KEYS),
        "disabled_efforts": sorted(reasoning_floor._DISABLED_EFFORTS),
        "reasoning_floor_effort": reasoning_floor.REASONING_FLOOR_EFFORT,
    }


def expected(r):
    f = r.get("func")
    if f == "reapply_repairs":
        return reapply_repairs(r)
    if f == "ladder_route":
        return ladder_route(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
