#!/usr/bin/env python3
"""Differential oracle for gateway/run_turn_runner_approval_settle.py (gate
w21_runturnrun_bdbcf9).

The live module is imported and every function is executed for real:
`register_timeout_notice` is armed against a fake TurnRunner/first-class capture
of the settle hook (only `tools.approval.register_gateway_settle`, a process
global, is replaced by a recorder), the hook is then invoked and the scheduled
coroutine's own frame supplies its arguments. `_timed_out_card_body` and
`_post_timeout_notice` run against a fake adapter whose only job is the I/O the
port models as request fields (`edit_message` / `send` / measure / max length).

Seams, named:
* `approval_timeout_seconds()` -> fixture `fallback_timeout` (reads approvals.timeout
  from config; the port takes the configured value as an argument).
* `ea_default_reason_text()` -> fixture `fallback_reason` (i18n catalogue lookup).
* `tools.approval.register_gateway_settle` -> a recorder (a process-global
  registry; nothing else about it is modelled).
* adapter `edit_message`/`send` -> recorded calls (network).
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "run_turn_runner_approval_settle.py"
GATE = "w21_runturnrun_bdbcf9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("approval_settle_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["approval_settle_live"] = live
_spec.loader.exec_module(live)

import tools.approval as approval_registry  # noqa: E402


def measure_fn(mode):
    def utf16_len(s):
        return len(s.encode("utf-16-le")) // 2

    return utf16_len if mode == "utf16" else len


def closure_vars(fn):
    out = {}
    for name, cell in zip(fn.__code__.co_freevars, fn.__closure__ or ()):
        try:
            out[name] = cell.cell_contents
        except ValueError:
            pass
    return out


class FakeCtx:
    def __init__(self, session_key="", still_current=None, source=None,
                 chat_id="", metadata=None):
        self.session_key = session_key
        self._run_still_current = still_current
        self.source = source
        self._status_chat_id = chat_id
        self._status_thread_metadata = metadata


class FakeRunner:
    def __init__(self, ctx):
        self._ctx = ctx
        self.scheduled = []

    def _schedule(self, aw, label):
        self.scheduled.append((aw, label))


def arm_step(r):
    """Arm the hook and return (captured, runner, ctx)."""
    data = r.get("approval_data")
    if not isinstance(data, dict):
        data = {}
    live.approval_timeout_seconds = lambda: r.get("fallback_timeout", 300)
    live.ea_default_reason_text = lambda: r.get("fallback_reason", "")
    still = r.get("still_current")
    ctx = FakeCtx(session_key=r.get("session_key") or "",
                  still_current=(lambda: still) if isinstance(still, bool) else None)
    runner = FakeRunner(ctx)
    captured = {}
    approval_registry.register_gateway_settle = (
        lambda key, rid, fn: captured.update(key=key, rid=rid, settle=fn))
    live.register_timeout_notice(runner, data, command=r.get("command", ""),
                                 card_message_id=r.get("card_message_id"))
    return captured, runner, data


def do_arm(r):
    captured, _runner, data = arm_step(r)
    if "settle" not in captured:
        return {"armed": False}
    cv = closure_vars(captured["settle"])
    return {
        "armed": True,
        "request_id": captured["rid"],
        "session_key": captured["key"],
        "timeout_s": cv.get("timeout_s"),
        "timeout_from": "params" if data.get("timeout_s") is not None else "fallback",
        "flagged": cv.get("flagged"),
    }


def do_settle(r):
    captured, runner, _data = arm_step(r)
    if "settle" not in captured:
        return {"scheduled": False, "why": "unarmed"}
    if r.get("reason") != "timeout":
        captured["settle"](r.get("reason", ""))
        return {"scheduled": False, "why": "reason"}
    captured["settle"](r.get("reason", ""))
    if not runner.scheduled:
        return {"scheduled": False, "why": "not_current"}
    aw, label = runner.scheduled[0]
    args = dict(aw.cr_frame.f_locals)
    aw.close()
    return {"scheduled": True, "label": label,
            "args": {"command": args["command"], "card_message_id": args["card_message_id"],
                     "timeout_s": args["timeout_s"], "reason": args["reason"]}}


class CardAdapter:
    def __init__(self, mode, max_len):
        self.mode = mode
        self.max_len = max_len

    def message_len_fn_for_chat(self, chat_id):
        return measure_fn(self.mode)

    def max_message_length_for_chat(self, chat_id):
        return self.max_len


def do_card_body(r):
    adapter = CardAdapter(r.get("measure", "len"), r.get("max_len", 4096))
    return live._timed_out_card_body(adapter, "chat", r.get("notice", ""),
                                     r.get("command", ""), r.get("reason", ""))


class Recorder:
    """Adapter double: the measure/max-length answers plus recorded sends/edits."""

    def __init__(self, mode, max_len):
        self.mode = mode
        self.max_len = max_len
        self.calls = {}

    def message_len_fn_for_chat(self, chat_id):
        return measure_fn(self.mode)

    def max_message_length_for_chat(self, chat_id):
        return self.max_len

    async def send(self, chat_id, text, metadata=None):
        self.calls.update(action="sent", chat_id=chat_id, text=text, metadata=metadata)


def do_post_notice(r):
    source = r.get("source")

    class Source:
        pass

    src = Source()
    if isinstance(source, dict) and "is_bot" in source:
        src.is_bot = source["is_bot"]
    adapter = Recorder(r.get("measure", "len"), r.get("max_len", 4096))
    if r.get("has_edit_message"):
        success = bool(r.get("edit_success"))

        async def edit_message(chat_id, message_id, body):
            if not success:
                raise RuntimeError("edit refused")
            adapter.calls.update(action="edited", chat_id=chat_id,
                                 message_id=message_id, body=body)
            return types.SimpleNamespace(success=True)

        adapter.edit_message = edit_message
    ctx = FakeCtx(source=src, chat_id=r.get("status_chat_id", ""),
                  metadata=r.get("thread_metadata"))
    ctx._status_adapter = adapter
    try:
        asyncio.run(live._post_timeout_notice(ctx, r.get("command", ""),
                                              r.get("card_message_id"),
                                              r.get("timeout_s"), r.get("reason", "")))
    except Exception as exc:  # noqa: BLE001 - the port reports raised errors this way
        return {"action": "raised", "type": type(exc).__name__, "message": str(exc)}
    if "action" not in adapter.calls:
        return {"action": "skipped"}
    return adapter.calls


def expected(r):
    f = r.get("func")
    if f == "window":
        from gateway.platforms.base_exec_approval import format_approval_window

        return format_approval_window(r.get("seconds", 0))
    if f == "notice":
        from gateway.platforms.base_exec_approval import format_approval_timed_out_notice

        return format_approval_timed_out_notice(r.get("timeout_s", 0))
    if f == "arm":
        return do_arm(r)
    if f == "settle":
        return do_settle(r)
    if f == "card_body":
        return do_card_body(r)
    if f == "post_notice":
        return do_post_notice(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"window", "notice", "arm", "settle", "card_body", "post_notice"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
