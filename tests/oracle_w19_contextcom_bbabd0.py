#!/usr/bin/env python3
"""Differential oracle for agent/context_compressor_window.py (gate w19_contextcom_bbabd0).

Extracts the live window-planning helpers from the Python AST: `summary_view`, the four
CompactionWindowMixin methods, and the real `_tool_calls_by_id` they read. The compressor's
collaborators that scan tokens or align on the full transcript (token walk, backward
alignment, progress floor, restart-probe constant) are the seam: the fixture states the
value each one answers, so both sides see the same decisions.
"""
import ast
import json
import sys
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
WINDOW = QUARRY / "agent" / "context_compressor_window.py"
COMPRESSOR = QUARRY / "agent" / "context_compressor.py"
GATE = "w19_contextcom_bbabd0"
_ANN = __future__.annotations.compiler_flag


class _Logger:
    def warning(self, *a, **k):
        pass


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


G = {"__name__": "window_live", "logger": _Logger(), "Any": object, "Optional": object,
     "PreparedCompaction": object, "PreparedSummary": object, "frozenset": frozenset,
     "list": list, "len": len, "min": min, "any": any, "map": map, "zip": zip,
     "range": range}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


_exec(COMPRESSOR, {"_tc_get", "_tool_calls_by_id"}, G)
_exec(WINDOW, {"summary_view", "_restart_probe_extra", "_restart_handoff_probe_bounds",
               "_effective_protect_first_n", "_protect_head_size", "_compress_window"}, G)
if "_SUMMARY_KEEPS_PRUNED_FORM" not in G:
    G["_SUMMARY_KEEPS_PRUNED_FORM"] = frozenset({"clarify"})


class Fake:
    def __init__(self, r):
        self.protect_first_n = int(r.get("protect_first_n") or 0)
        self.compression_count = int(r.get("compression_count") or 0)
        self._previous_summary = r.get("previous_summary")
        self.pin_task_turn = bool(r.get("pin_task_turn"))
        self._floor = int(r.get("floor") or 0)
        self._tail_cut = int(r.get("tail_cut") if r.get("tail_cut") is not None else 0)
        self._last_user = int(r.get("last_user_idx") if r.get("last_user_idx") is not None else -1)
        self._back = int(r.get("aligned_backward") or 0)

    def _is_context_summary_message(self, m):
        return isinstance(m, dict) and bool(m.get("is_summary"))

    def _align_boundary_forward(self, messages, idx):
        while idx < len(messages) and messages[idx].get("role") == "tool":
            idx += 1
        return idx

    def _align_boundary_backward(self, messages, idx):
        return self._back

    def _find_tail_cut_by_tokens(self, messages, start):
        return self._tail_cut

    def _find_last_user_message_idx(self, messages, head_end):
        return self._last_user


Fake._restart_handoff_probe_bounds = G["_restart_handoff_probe_bounds"]
Fake._effective_protect_first_n = G["_effective_protect_first_n"]
Fake._protect_head_size = G["_protect_head_size"]
Fake._compress_window = G["_compress_window"]


def run_case(r):
    fake = Fake(r)
    G["tail_cut_progress_floor"] = lambda self, messages, head_end, cut_idx: self._floor
    stub_module("agent.context_compressor",
                _RESTART_HANDOFF_PROBE_EXTRA_MESSAGES=int(r.get("extra") or 0),
                _tool_calls_by_id=G["_tool_calls_by_id"])
    f = r.get("func")
    if f == "summary_view":
        return G["summary_view"](r.get("plan"), r.get("originals"),
                                 int(r.get("start") or 0), int(r.get("end") or 0))
    if f == "restart_probe_bounds":
        return list(fake._restart_handoff_probe_bounds(r.get("messages")))
    if f == "effective_protect_first_n":
        return fake._effective_protect_first_n(r.get("messages"))
    if f == "protect_head_size":
        return fake._protect_head_size(r.get("messages") or [])
    if f == "compress_window":
        return list(fake._compress_window(r.get("messages") or []))
    return None


def main():
    cases = mism = 0
    wanted = {"summary_view", "restart_probe_bounds", "effective_protect_first_n",
              "protect_head_size", "compress_window"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in wanted:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s args=%r py=raised %r c=%r" % (f, r, exc, r.get("out")),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
