#!/usr/bin/env python3
"""Differential oracle for gateway/run_shutdown_home_notice.py (gate w19_runshutdow_75cb3d).

The mixin and `settle_home_online_notice_debt` are exec'd from the live AST into
stub-filled globals; collaborators (drain marker, restart-loop guard, restart-notice
marker, delivery keys) are stubbed at the module seam with the fixture's values.
"""
import ast
import contextlib
import json
import logging
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/gateway/run_shutdown_home_notice.py"
SHUT = Q + "/gateway/run_shutdown.py"
GATE = "w19_runshutdow_75cb3d"
_ANN = __import__("__future__").annotations.compiler_flag


def _stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


def _exec_node(node, g, path):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, path, "exec", flags=_ANN, dont_inherit=True), g)


# == delivery-key helpers, straight out of gateway/run_shutdown.py ============
stree = ast.parse(open(SHUT, encoding="utf-8").read())
sg = {"__name__": "run_shutdown_live"}
for node in stree.body:
    if isinstance(node, ast.FunctionDef) and node.name in ("_notice_target_key", "_delivery_target_key"):
        _exec_node(node, sg, SHUT)

_mk_gateway = _stub_module("gateway")
_stub_module("gateway.delivery", looks_like_telegram_private_chat_id=lambda cid: _is_private(cid))


def _is_private(chat_id):
    try:
        return int(chat_id) > 0
    except (TypeError, ValueError):
        return False


_stub_module("gateway.run_shutdown", _log_suppressed=None,
             _delivery_target_key=sg["_delivery_target_key"],
             _notice_target_key=sg["_notice_target_key"])
_stub_module("gateway.drain_control", drain_notification_suppressed=lambda: False)
_stub_module("gateway.restart_loop_guard", notice_loop_tripped=lambda *a, **k: False,
             record_notice_delivered=lambda: None)


class _MarkerPath:
    def __init__(self, rec):
        self._rec = rec

    def unlink(self, missing_ok=False):
        self._rec["drop_marker"] = True


_marker_rec = {"drop_marker": False, "write_marker": False}
_stub_module("gateway.restart_notice",
             planned_restart_marker_path=lambda: _MarkerPath(_marker_rec),
             write_planned_restart_marker=lambda: _marker_rec.__setitem__("write_marker", True))

_mk_live = types.ModuleType("home_notice_live")
_mk_live.__package__ = "gateway"
sys.modules["home_notice_live"] = _mk_live
g = _mk_live.__dict__
g.update({"__name__": "home_notice_live", "__package__": "gateway",
          "logging": logging, "nullcontext": contextlib.nullcontext,
          "logger": logging.getLogger("oracle"), "Optional": object})

ltree = ast.parse(open(SRC, encoding="utf-8").read())
for node in ltree.body:
    if isinstance(node, ast.Assign) and any(
            getattr(t, "id", "") in ("HOME_NOTICE_SENT", "HOME_NOTICE_MUTED") for t in node.targets):
        _exec_node(node, g, SRC)
    elif isinstance(node, ast.ClassDef) and node.name == "GatewayHomeShutdownNoticeMixin":
        _exec_node(node, g, SRC)
    elif isinstance(node, ast.FunctionDef) and node.name == "settle_home_online_notice_debt":
        _exec_node(node, g, SRC)

Mixin = g["GatewayHomeShutdownNoticeMixin"]
settle = g["settle_home_online_notice_debt"]
rlg = sys.modules["gateway.restart_loop_guard"]
drain = sys.modules["gateway.drain_control"]
shut = sys.modules["gateway.run_shutdown"]


@contextlib.contextmanager
def _log_suppressed(level, msg, *args, exc_info=False):
    try:
        yield
    except Exception:
        pass


shut._log_suppressed = _log_suppressed


def _mk_runner(**attrs):
    runner = object.__new__(Mixin)
    runner._restart_loop_guard_config = lambda: (3, 600, 120)
    runner._home_shutdown_notice = None
    runner._home_notice_loop_checked = False
    for k, v in attrs.items():
        setattr(runner, k, v)
    return runner


def run_case(r):
    f = r.get("func")
    if f == "broadcast_skip_reason":
        drain.drain_notification_suppressed = lambda: bool(r.get("drain_suppressed"))
        rlg.notice_loop_tripped = lambda *a, **k: bool(r.get("loop_tripped"))
        runner = _mk_runner(
            _restart_requested=bool(r.get("restart_requested")),
            _restart_command_source=r.get("restart_command_source"),
            _planned_takeover_shutdown=bool(r.get("planned_takeover")))
        reason = runner._home_shutdown_broadcast_skip_reason()
        return {"skip_reason": reason, "notice": runner._home_shutdown_notice,
                "loop_checked": bool(runner._home_notice_loop_checked)}
    if f == "notice_muted_by_loop":
        rlg.notice_loop_tripped = lambda *a, **k: bool(r.get("loop_tripped"))
        runner = _mk_runner()
        muted = runner._home_shutdown_notice_muted_by_loop()
        return {"muted": bool(muted), "loop_checked": bool(runner._home_notice_loop_checked)}
    if f == "got_active_session_notice":
        delivered = set()
        for d in r.get("delivered") or []:
            delivered.add(sg["_delivery_target_key"](d.get("platform"), d.get("chat_id"),
                                                     d.get("thread_id"), profile=d.get("profile")))

        def configs():
            for c in r.get("configs") or []:
                home = SimpleNamespace(chat_id=c.get("chat_id"), thread_id=c.get("thread_id"))
                yield (c.get("profile"), SimpleNamespace(value=c.get("platform")),
                       SimpleNamespace(home_channel=home))

        runner = _mk_runner()
        runner._served_home_channel_configs = configs
        return bool(runner._home_got_active_session_notice(delivered))
    if f == "settle_notice_debt":
        _marker_rec["drop_marker"] = False
        _marker_rec["write_marker"] = False
        rec = {"delivered": False}
        rlg.record_notice_delivered = lambda: rec.__setitem__("delivered", True)
        runner = SimpleNamespace(_home_shutdown_notice=r.get("notice"),
                                 _restart_requested=bool(r.get("restart_requested")),
                                 _home_notice_loop_checked=bool(r.get("loop_checked")))
        settle(runner)
        return {"drop_marker": _marker_rec["drop_marker"],
                "write_marker": _marker_rec["write_marker"],
                "record_delivered": rec["delivered"],
                "loop_checked": bool(runner._home_notice_loop_checked)}
    return "SKIP"


FUNCS = {"broadcast_skip_reason", "notice_muted_by_loop", "got_active_session_notice",
         "settle_notice_debt"}


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
        except BaseException as exc:  # noqa: BLE001 - report, never abort the oracle
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
