#!/usr/bin/env python3
"""Differential oracle for live gateway/run_turn_hygiene.py (w10_runturnhy_0e89de).

AST-extracts the two static methods plus the fail-closed bounding helper from
the live sources; fixture-injected settings/history drive the comparison.
"""
import ast
import __future__
from contextlib import suppress
import json
import logging
import sys
import types
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
ANN = __future__.annotations.compiler_flag

ns = {"suppress": suppress, "logger": logging.getLogger("rth_oracle"), "Any": object}
mod = types.ModuleType("rthy_0e89de_live")
sys.modules["rthy_0e89de_live"] = mod
mod.__dict__.update(ns)
ns = mod.__dict__

SRC_H = Q + "/gateway/run_turn_hygiene.py"
SRC_R = Q + "/gateway/run_turn.py"
tree_h = ast.parse(open(SRC_H, encoding="utf-8").read())
tree_r = ast.parse(open(SRC_R, encoding="utf-8").read())


def ex(node, src):
    exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec", flags=ANN, dont_inherit=True), ns)


READ = "_hmwa_hygiene_read_config"
BOUND = "_bound_hygiene_payload"
for node in tree_h.body:
    if isinstance(node, ast.ClassDef):
        for sub in node.body:
            if isinstance(sub, ast.FunctionDef) and sub.name in (READ, BOUND):
                sub.decorator_list = []
                ex(sub, SRC_H)
BOUND_FN = "bound_model_input_without_hygiene"
ns["_HYGIENE_SETUP_ROLES"] = ("system", "session_meta")
for node in tree_r.body:
    if isinstance(node, ast.FunctionDef) and node.name == BOUND_FN:
        ex(node, SRC_R)
for fn in (READ, BOUND, BOUND_FN):
    if fn not in ns:
        raise SystemExit("ORACLE w10_runturnhy_0e89de: live def %s not found" % fn)

# _bound_hygiene_payload re-imports the helper from gateway.run_turn at call time;
# bind our AST-extracted copy there so the heavy real module never loads.
if "gateway" not in sys.modules:
    _gw = types.ModuleType("gateway"); _gw.__path__ = [Q + "/gateway"]; sys.modules["gateway"] = _gw
_rt = types.ModuleType("gateway.run_turn")
_rt.bound_model_input_without_hygiene = ns[BOUND_FN]
sys.modules["gateway.run_turn"] = _rt

FIELDS = ["model", "config_context_length", "provider", "base_url", "compression_enabled",
          "hard_msg_limit", "timeout_seconds", "total_ceiling_seconds",
          "max_turn_hold_seconds", "failure_cooldown_seconds"]


def compute(r):
    f = r.get("func")
    if f == "read_config":
        hs = NS(**{k: r["hs"][k] for k in FIELDS})
        ns[READ](hs, r.get("data") or {})
        return {k: getattr(hs, k) for k in FIELDS}
    if f == "bound_payload":
        history = r.get("history") or []
        limit = int(r.get("limit", 0))
        hs = NS(hard_msg_limit=limit)
        return ns[BOUND](history, hs, NS(session_id="x"))
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
print("ORACLE w10_runturnhy_0e89de: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
