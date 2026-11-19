#!/usr/bin/env python3
"""Oracle for the hermes_cli/observability/relay_shared_metrics.py FLOW cores
(relay_shared_metrics_flow.c).

Drives the LIVE functions with SCRIPTED I/O: the functions are AST-extracted
from the quarry (no heavy observability import), their module constants and
the real _ModelCall dataclass are seeded, and every Relay emission / clock /
contract / fields / model-state dependency is replaced by a recording
monkeypatch stub returning the same scripted results the C harness feeds its
seams. Compares the DECISION SEQUENCE (ordered trace of seam calls) plus the
return value / mutated aggregation state, not just a final scalar.

Input: stdin JSON lines from t_port_relay_shared_metrics_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE relay_shared_metrics_flow: N cases, M mismatches. Exit 1 on
any mismatch, and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import asyncio
import contextlib
import copy
import dataclasses
import json
import os
import sys
import types
from collections import OrderedDict
from typing import Any, Callable, Iterator

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "observability", "relay_shared_metrics.py")

WANTED_MODULE = {
    "_text", "_session_pair", "_identities_compatible",
    "_compatible_tool_call_keys", "_absorb_peak", "_terminal_flags",
}
WANTED_METHODS = {
    # targets
    "_admits", "_announce_cold", "_consume_cold", "_count_session_turn",
    "update_model_call",
    # live dependencies the targets compose (run REAL, not scripted)
    "_event_matches_task_turn", "_remember_turn", "_existing_model_call_key",
    "_guarded",
}

CUR = {"func": None, "in": {}, "trace": []}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


class RecLogger:
    def _log(self, level, fmt, *args):
        rec({"ev": "log", "level": level, "msg": fmt % args if args else fmt})

    def debug(self, fmt, *args, **kw):
        self._log("debug", fmt, *args)

    def warning(self, fmt, *args, **kw):
        self._log("warning", fmt, *args)

    def error(self, fmt, *args, **kw):
        self._log("error", fmt, *args)


def exec_node(node, ns, path):
    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "relay_shared_metrics_extracted",
        "logger": RecLogger(), "contextlib": contextlib,
        "OrderedDict": OrderedDict, "dataclass": dataclasses.dataclass,
        "field": dataclasses.field, "monotonic_ns": lambda: cin("now_ns", 0),
        "Any": Any, "Callable": Callable, "Iterator": Iterator,
        "threading": types.SimpleNamespace(RLock=contextlib.nullcontext,
                                           Lock=contextlib.nullcontext),
    }
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            try:
                exec_node(node, ns, SRC)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec_node(node, ns, SRC)
            except Exception:
                pass
    # `logger = logging.getLogger(__name__)` is a module Assign — restore the
    # recording logger AFTER seeding so _guarded warnings reach the trace.
    ns["logger"] = RecLogger()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED_MODULE:
            exec_node(node, ns, SRC)
        if isinstance(node, ast.ClassDef):
            if node.name == "_ModelCall":
                exec_node(node, ns, SRC)   # real dataclass (started_ns factory seeded)
            if node.name == "_Runtime":
                for sub in node.body:
                    if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in WANTED_METHODS:
                        sub = copy.copy(sub)
                        sub.decorator_list = []   # staticmethod wrappers
                        exec_node(sub, ns, SRC)
    return ns


def install_stubs(ns):
    # contract / fields_ sibling modules: scripted recording stubs.
    def fields_stub(event):
        r = copy.deepcopy(cin("fields_ret", {}))
        rec({"ev": "model_call_fields", "ret": r})
        return r

    def error_class_stub(event):
        r = cin("error_class_ret", "none")
        rec({"ev": "model_error_class", "ret": r})
        return r

    def ttft_stub(event):
        r = cin("ttft_ret", "unknown")
        rec({"ev": "ttft_bucket", "ret": r})
        return r

    def tokens_stub(usage, call_role=None, **fields):
        r = cin("tokens_ret")
        rec({"ev": "model_token_fields", "usage": usage, "call_role": call_role,
             "fields": fields, "ret": r})
        return copy.deepcopy(r) if r is not None else None

    contract = types.ModuleType("shared_metrics_contract")
    contract.model_call_fields = fields_stub
    contract.model_error_class = error_class_stub
    # Seed the mark name from the LIVE contract module so a rename upstream
    # surfaces as a mismatch, not a silent drift.
    cpath = os.path.join(QUARRY, "hermes_cli", "observability",
                         "shared_metrics_contract.py")
    ctree = ast.parse(open(cpath, encoding="utf-8").read())
    for node in ctree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "MODEL_TOKENS_MARK"
                for t in node.targets):
            exec_node(node, contract.__dict__, cpath)
    if not hasattr(contract, "MODEL_TOKENS_MARK"):
        raise RuntimeError("MODEL_TOKENS_MARK not found in live contract")
    fields_ = types.ModuleType("shared_metrics_fields")
    fields_.ttft_bucket = ttft_stub
    fields_.model_token_fields = tokens_stub
    ns["contract"] = contract
    ns["fields_"] = fields_


def norm_result(x):
    if isinstance(x, tuple):
        return [norm_result(v) for v in x]
    if isinstance(x, list):
        return [norm_result(v) for v in x]
    if isinstance(x, dict):
        return {k: norm_result(v) for k, v in x.items()}
    return x


def make_session(ns, sj):
    """JSON session model -> attribute double with REAL _ModelCall values."""
    if sj is None:
        return None
    s = types.SimpleNamespace()
    s.session_id = sj.get("session_id", "")
    s.closing = sj.get("closing", False)
    s.replies = sj.get("replies", 0)
    s.lock = contextlib.nullcontext()
    s.model_calls = {}
    for k, mc in (sj.get("model_calls") or {}).items():
        t, r = k.split("\t", 1)
        s.model_calls[(t, r)] = ns["_ModelCall"](
            handle=None, task_id=mc.get("task_id", ""),
            fields=dict(mc.get("fields") or {}),
            error_class=mc.get("error_class", "none"),
            ttft_bucket=mc.get("ttft_bucket", "unknown"),
            started_ns=mc.get("started_ns", 0))
    s.tasks = {}
    for tid, tj in (sj.get("tasks") or {}).items():
        s.tasks[tid] = types.SimpleNamespace(
            task_id=tid,
            cost=types.SimpleNamespace(user_turn=(tj.get("cost") or {}).get("user_turn", False)))

    def observe_call(fields, usage, context_length):
        rec({"ev": "observe_call", "fields": fields, "usage": usage,
             "context_length": context_length})

    def observe_error(fields, error_class):
        rec({"ev": "observe_error", "fields": fields, "error_class": error_class})

    s.model_state = types.SimpleNamespace(observe_call=observe_call,
                                          observe_error=observe_error)
    return s


def session_json(sj, s):
    """Mutated double -> the same JSON shape the C harness emits."""
    out = copy.deepcopy(sj)
    out["replies"] = s.replies
    calls = {}
    for (t, r), mc in s.model_calls.items():
        key = "%s\t%s" % (t, r)
        base = copy.deepcopy((sj.get("model_calls") or {}).get(key, {}))
        base.update(fields=mc.fields, error_class=mc.error_class,
                    ttft_bucket=mc.ttft_bucket)
        calls[key] = base
    out["model_calls"] = calls
    return out


def drive(ns, func, inp):
    if func == "terminal_flags":
        ej = inp.get("error")
        err = None
        if ej is not None:
            name = ej.get("name", "")
            if name == "CancelledError":
                err = asyncio.CancelledError()
            else:
                classes = {"KeyboardInterrupt": KeyboardInterrupt,
                           "InterruptedError": InterruptedError,
                           "TimeoutError": TimeoutError,
                           "ValueError": ValueError,
                           "RuntimeError": RuntimeError}
                err = classes[name]()
        return {"ret": ns["_terminal_flags"](inp.get("result"), err)}

    if func == "session_pair":
        r = ns["_session_pair"](inp.get("event") or {}, inp.get("key", "task_id"))
        return {"ret": norm_result(r)}

    if func == "identities_compatible":
        r = ns["_identities_compatible"](tuple(inp.get("candidate") or []),
                                         tuple(inp.get("observed") or []))
        return {"ret": r}

    if func == "compatible_tool_call_keys":
        session = types.SimpleNamespace(
            tool_calls={tuple(k): None for k in (inp.get("tool_call_keys") or [])})
        r = ns["_compatible_tool_call_keys"](session, inp.get("task_id", ""),
                                             tuple(inp.get("identity") or []))
        return {"ret": norm_result(r)}

    if func == "absorb_peak":
        ij = inp.get("into") or {}
        sj = inp.get("segment") or {}

        def peak(d):
            return types.SimpleNamespace(
                peak_route=d.get("peak_route"), peak_tokens=d.get("peak_tokens", 0),
                peak_window=d.get("peak_window"), limit_hit=d.get("limit_hit", False))

        into = peak(ij)
        ns["_absorb_peak"](into, peak(sj))
        out = dict(ij)
        out.update(peak_route=into.peak_route, peak_tokens=into.peak_tokens,
                   peak_window=into.peak_window, limit_hit=into.limit_hit)
        return {"ret": None, "state": out}

    if func == "admits":
        stj = inp.get("state") or {}
        sej = inp.get("session") or {}
        taj = inp.get("task") or {}
        task = types.SimpleNamespace(
            task_id=taj.get("task_id", ""), ref=taj.get("ref", ""),
            retired_turn_ids=frozenset(taj.get("retired_turn_ids") or []),
            turn_ids=set(taj.get("turn_ids") or []))
        session = types.SimpleNamespace(
            session_id=sej.get("session_id", ""), closing=sej.get("closing", False),
            tasks={})
        for tid, tj in (sej.get("tasks") or {}).items():
            # `is` identity: the stored run IS the argument only when refs match
            if tid == task.task_id and tj.get("ref") == task.ref:
                session.tasks[tid] = task
            else:
                session.tasks[tid] = types.SimpleNamespace(ref=tj.get("ref", ""))
        rt = types.SimpleNamespace(
            _turn_sessions={tuple(k.split("\t", 1)): v
                            for k, v in (stj.get("turn_sessions") or {}).items()},
            _task_sessions_lock=contextlib.nullcontext(),
            _event_matches_task_turn=ns["_event_matches_task_turn"])
        rt._remember_turn = types.MethodType(ns["_remember_turn"], rt)
        r = ns["_admits"](rt, session, task, inp.get("event") or {},
                          current=inp.get("current", False))
        out_state = dict(stj)
        if rt._turn_sessions or "turn_sessions" in stj:
            out_state["turn_sessions"] = {
                "%s\t%s" % k: (v.session_id if hasattr(v, "session_id") else v)
                for k, v in rt._turn_sessions.items()}
        out_task = dict(taj)
        if task.turn_ids or "turn_ids" in taj:
            out_task["turn_ids"] = sorted(task.turn_ids)
        return {"ret": r, "state": out_state, "task": out_task}

    if func in ("announce_cold", "consume_cold"):
        rt = types.SimpleNamespace(
            _cold_expected=OrderedDict((k, None) for k in (inp.get("expected") or [])),
            _cold_lock=contextlib.nullcontext())
        if func == "announce_cold":
            r = ns["_announce_cold"](rt, inp.get("key", ""))
        else:
            r = ns["_consume_cold"](rt, inp.get("key", ""))
        return {"ret": r, "state": list(rt._cold_expected)}

    if func == "count_session_turn":
        sej = inp.get("session") or {}
        taj = inp.get("task") or {}
        session = types.SimpleNamespace(
            start_fields=sej.get("start_fields"),
            turns=sej.get("turns", 0), failed_turns=sej.get("failed_turns", 0),
            last_outcome=sej.get("last_outcome", "unknown"),
            first_turn_ns=sej.get("first_turn_ns", 0),
            last_turn_ns=sej.get("last_turn_ns", 0),
            api_calls=sej.get("api_calls", 0),
            tool_call_total=sej.get("tool_call_total", 0))
        task = types.SimpleNamespace(
            start_fields=dict(taj.get("start_fields") or {}),
            started_ns=taj.get("started_ns", 0),
            model_call_ids=set(taj.get("model_call_ids") or []),
            tool_call_ids=set(taj.get("tool_call_ids") or []),
            unidentified_tool_calls=taj.get("unidentified_tool_calls", 0))
        ns["_count_session_turn"](session, task, inp.get("outcome", "success"))
        out = dict(sej)
        out.update(start_fields=session.start_fields, turns=session.turns,
                   failed_turns=session.failed_turns, last_outcome=session.last_outcome,
                   first_turn_ns=session.first_turn_ns, last_turn_ns=session.last_turn_ns,
                   api_calls=session.api_calls, tool_call_total=session.tool_call_total)
        return {"ret": None, "state": out}

    if func == "update_model_call":
        sj = inp.get("session")
        session = make_session(ns, sj)

        def any_session(event):
            return session   # silent: the C flow takes the session as a param

        def observe_turn_call(sess, task, model_call, event):
            rec({"ev": "observe_turn_call", "task_id": model_call.task_id,
                 "has_task": task is not None})

        def finish_model_call(sess, key, outcome):
            rec({"ev": "finish_model_call", "key": list(key), "outcome": outcome})
            sess.model_calls.pop(key, None)

        def mark(sess, task, name, data):
            e = {"ev": "mark", "name": name, "data": data,
                 "task_id": task.task_id if task is not None else None}
            if cin("mark_raises", False):
                e["raised"] = True
                rec(e)
                raise RuntimeError("store busy")
            rec(e)

        rt = types.SimpleNamespace(
            _any_session=any_session,
            _existing_model_call_key=ns["_existing_model_call_key"],
            _observe_turn_call=observe_turn_call,
            _finish_model_call=finish_model_call,
            _mark=mark,
            _guarded=ns["_guarded"])
        ns["update_model_call"](rt, inp.get("event") or {}, finish=inp.get("finish", False))
        return {"ret": None, "state": session_json(sj, session) if session is not None else None}

    raise ValueError("unknown func: %s" % func)


def canon(x, ordered):
    if isinstance(x, dict):
        items = [(k, canon(v, ordered)) for k, v in x.items()]
        if not ordered:
            items = sorted(items, key=lambda kv: kv[0])
        return ("obj", items)
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v, ordered) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    ns = extract()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        try:
            want = drive(ns, func, inp)
        except Exception as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["trace"] = CUR["trace"]
        ok = canon(want, False) == canon(got, False)
        if not ok:
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE relay_shared_metrics_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())