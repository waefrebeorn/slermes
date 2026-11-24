#!/usr/bin/env python3
"""Differential oracle for persistence/gateway/routing.py (gate w19_routing_b4d479)."""
import ast
import dataclasses
import json
import sys
import types
import typing
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "gateway" / "routing.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_routing_b4d479"

_WANTED_FUNCS = {"route_namespace", "normalize_topic_profile_name", "_route",
                 "gateway_runtime", "resume_route"}
_WANTED_CLASSES = {"GatewayRuntime"}
_WANTED_CONSTS = {"PEER_RESET_END_REASONS", "END_REASON_SUPERSEDED_BY_REPAIR",
                  "ORPHAN_ADOPTION_MAX_GAP_S", "BARE_BILLING_PROVIDERS", "TOPIC_SCHEMA_META_KEY"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("routing_live")
    sys.modules["routing_live"] = mod
    g = mod.__dict__
    g.update(dict(dataclass=dataclasses.dataclass, field=dataclasses.field,
                  json=json,
                  Iterable=typing.Iterable, Mapping=typing.Mapping, Optional=object,
                  Protocol=object, runtime_checkable=lambda c: c, TYPE_CHECKING=False,
                  JsonValue=object, SessionId=str, SessionArtifacts=object, Conflict=ValueError,
                  SessionRecord=object))
    g["_ROUTE_KEYS"] = ("provider", "base_url", "api_mode")
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in _WANTED_FUNCS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in _WANTED_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) \
                and node.target.id in _WANTED_CONSTS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if names and names[0] in _WANTED_CONSTS:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def rt_out(rt):
    return {"provider": rt.provider, "base_url": rt.base_url, "api_mode": rt.api_mode}


def run_case(r):
    f = r.get("func")
    if f == "route_namespace":
        return G["route_namespace"](r.get("session_key"))
    if f == "normalize_topic_profile_name":
        return G["normalize_topic_profile_name"](r.get("profile_name"))
    if f == "route":
        return rt_out(G["_route"](r.get("members") or {}, keep_falsy=bool(r.get("keep_falsy"))))
    if f == "gateway_runtime":
        return rt_out(G["gateway_runtime"](r.get("model_config"), billing_provider=r.get("billing_provider"),
                                           billing_base_url=r.get("billing_base_url")))
    if f == "resume_route":
        model, route = G["resume_route"](r.get("model_config"), model=r.get("model"),
                                         billing_provider=r.get("billing_provider"),
                                         billing_base_url=r.get("billing_base_url"))
        return {"model": model, "route": rt_out(route)}
    if f == "is_empty":
        rt = G["GatewayRuntime"](**(r.get("runtime") or {}))
        return rt.is_empty
    if f == "constants":
        return {"peer_reset_end_reasons": sorted(G["PEER_RESET_END_REASONS"]),
                "end_reason_superseded_by_repair": G["END_REASON_SUPERSEDED_BY_REPAIR"],
                "orphan_adoption_max_gap_s": G["ORPHAN_ADOPTION_MAX_GAP_S"],
                "bare_billing_providers": sorted(G["BARE_BILLING_PROVIDERS"]),
                "topic_schema_meta_key": G["TOPIC_SCHEMA_META_KEY"]}
    return None


FUNCS = {"route_namespace", "normalize_topic_profile_name", "route", "gateway_runtime",
         "resume_route", "is_empty", "constants"}


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
        f = r.get("func")
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
