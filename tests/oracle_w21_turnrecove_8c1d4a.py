#!/usr/bin/env python3
"""Differential oracle for agent/turn_recovery_autorecover.py
(gate w21_turnrecove_8c1d4a).

The five module functions are lifted out of the live source and run with:

* `agent.error_classifier.TRANSIENT_REASONS` / `FailoverReason` -- the LIVE enum
  and set, so the eligibility verdict is the real one;
* `agent.retry_utils` -- LIVE, with `provider_retry_after_seconds` bound to the
  fixture's parsed Retry-After and `random.Random.uniform` pinned to 0 so the
  jittered schedule compares exactly;
* `agent.turn_recovery.interruptible_backoff_sleep` -- the fixture's interrupt
  verdict;
* `agent._has_content_after_think_block` -- the LIVE `strip_think_blocks`
  scrubber, and the fixture's `has_content` is checked against it.
"""
import __future__
import ast
import json
import random
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = Path("C:/Users/eman5/hermes-agent")
SRC = QUARRY / "agent" / "turn_recovery_autorecover.py"
GATE = "w21_turnrecove_8c1d4a"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_pkg = types.ModuleType("agent")
_pkg.__path__ = [str(QUARRY / "agent")]
sys.modules["agent"] = _pkg

import agent.agent_runtime_helpers as runtime_helpers
import agent.error_classifier as error_classifier
import agent.retry_utils as retry_utils

CASE = {"retry_after": None}
INTERRUPT = {"value": None}


class _ModuleStub(types.ModuleType):
    pass


_turn_recovery = _ModuleStub("agent.turn_recovery")
_turn_recovery.interruptible_backoff_sleep = lambda *a, **k: INTERRUPT["value"]
sys.modules["agent.turn_recovery"] = _turn_recovery

retry_utils.provider_retry_after_seconds = lambda error: CASE["retry_after"]


class _ZeroRandom:
    """random.Random with the jitter draw pinned to 0 -> the raw backoff."""

    def __init__(self, *args, **kwargs):
        pass

    def uniform(self, a, b):
        return 0.0


random.Random = _ZeroRandom


class _Log:
    def warning(self, *args, **kwargs):
        pass

    def debug(self, *args, **kwargs):
        pass


def _extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    env = {"__name__": "turn_recovery_autorecover_live", "logger": _Log(),
           "TRANSIENT_REASONS": error_classifier.TRANSIENT_REASONS,
           "Any": object, "Dict": object, "Optional": object}
    wanted = {"auto_recovery_cycles", "ladder_wait_seconds", "ladder_eligible",
              "ladder_notice", "auto_recover_after_exhaustion"}
    consts = {"_LADDER_BASE_DELAY_S", "_LADDER_CAP_S", "_RETRY_AFTER_CAP_S", "_STOP_HINTS",
              "_DEFAULT_STOP_HINT"}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id in consts:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), env)
        elif isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), env)
    return env


G = _extract()


def has_content(text):
    return bool(text) and bool(runtime_helpers.strip_think_blocks(None, text).strip())


class _UnknownReason:
    """A verdict the live enum does not carry: never in TRANSIENT_REASONS."""

    def __init__(self, value):
        self.value = value


class _Classified:
    def __init__(self, reason):
        try:
            # FailoverReason is the live str-enum: `.value` is the wire reason and
            # membership in TRANSIENT_REASONS is a real identity test.
            self.reason = error_classifier.FailoverReason(reason)
        except Exception:  # noqa: BLE001 - unknown verdicts are simply not transient
            self.reason = _UnknownReason(reason)


class _Agent:
    def __init__(self, cfg, streamed):
        cfg = cfg if isinstance(cfg, dict) else {}
        self._auto_recovery_cycles = cfg.get("_auto_recovery_cycles")
        self.platform = cfg.get("platform")
        self._current_streamed_assistant_text = streamed or ""
        self.log_prefix = ""
        self.status = []
        self.waits = []

    def _has_content_after_think_block(self, content):
        return has_content(content)

    def _emit_diagnostic_status(self, text):
        self.status.append(text)

    def _emit_diagnostic_wait(self, text):
        self.waits.append(text)

    def _client_log_context(self):
        return ""


class _Retry:
    def __init__(self, cfg):
        cfg = cfg if isinstance(cfg, dict) else {}
        self.auto_recovery_cycles_used = cfg.get("auto_recovery_cycles_used") or 0
        self.restart_with_redirected_messages = bool(
            cfg.get("restart_with_redirected_messages"))


def _envelope(agent, action, result, wait, notice, used):
    return {"action": action, "result": result, "used_cycles": used, "wait": wait,
            "notice": notice, "status": list(agent.status), "wait_lines": list(agent.waits)}


def expected(r):
    func = r.get("func")
    if func == "cycles":
        return G["auto_recovery_cycles"](_Agent(r.get("agent"), ""))
    if func == "wait":
        CASE["retry_after"] = (r.get("api_error") or {}).get("retry_after")
        wait = G["ladder_wait_seconds"](int(r.get("cycle") or 0), None)
        source = "retry_after" if CASE["retry_after"] is not None else "schedule"
        return {"wait": wait, "source": source}
    if func == "eligible":
        streamed = r.get("streamed")
        agent = _Agent(r.get("agent"), streamed)
        verdict = G["ladder_eligible"](agent, _Classified(r.get("reason")))
        if bool(r.get("has_content")) != has_content(streamed):
            raise AssertionError("fixture has_content=%r but live scrubber says %r"
                                 % (r.get("has_content"), has_content(streamed)))
        return bool(verdict)
    if func == "notice":
        agent = _Agent({"platform": r.get("platform")}, "")
        return G["ladder_notice"](agent, wait_s=float(r.get("wait_s") or 0.0),
                                  cycle=int(r.get("cycle") or 0),
                                  total=int(r.get("total") or 0))
    if func == "recover":
        case = r.get("case_data") or {}
        streamed = case.get("streamed")
        if bool(case.get("has_content")) != has_content(streamed):
            raise AssertionError("fixture has_content=%r but live scrubber says %r"
                                 % (case.get("has_content"), has_content(streamed)))
        agent = _Agent(case.get("agent"), streamed)
        retry = _Retry(case.get("retry"))
        CASE["retry_after"] = (case.get("api_error") or {}).get("retry_after")
        INTERRUPT["value"] = case.get("interrupt")
        action = G["auto_recover_after_exhaustion"](
            agent, None, _Classified(case.get("reason")), retry,
            messages=[], conversation_history=[], api_call_count=0)
        if action is None:
            return _envelope(agent, None, None, None, None, retry.auto_recovery_cycles_used)
        # Any completed cycle emitted the notice first, then computed the wait.
        notice = agent.status[-1] if agent.status else None
        wait = G["ladder_wait_seconds"](retry.auto_recovery_cycles_used, None)
        if action.get("action") == "return":
            return _envelope(agent, "return", action.get("result"), wait, notice,
                             retry.auto_recovery_cycles_used)
        return _envelope(agent, action.get("action"), None, wait, notice,
                         retry.auto_recovery_cycles_used)
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    wanted = {"cycles", "wait", "eligible", "notice", "recover"}
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
        except BaseException as exc:  # noqa: BLE001 - reported, never fatal
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
