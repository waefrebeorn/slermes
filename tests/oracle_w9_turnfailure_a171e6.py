#!/usr/bin/env python3
"""Differential oracle for gate w9_turnfailure_a171e6 (agent/turn_failure_copy.py).

AST-extracts the eight ported functions plus their module literals from the quarry,
seeding a real FailoverReason enum and the ExitFailure NamedTuple. The C port mirrors
the same literals. Compares 'out' against the live result.
"""
import __future__
import ast
import builtins
import copy
import enum
import json
import sys
import types
from pathlib import Path
from typing import NamedTuple

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "turn_failure_copy.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "agent/turn_failure_copy.py")
NAME = "w9_turnfailure_a171e6"
PORTED = ["stamp_failure", "untyped_failed_turn_display_kind", "failed_turn_notice",
          "exit_reason_failure", "is_max_iteration_handoff", "failure_cause_gloss",
          "short_detail", "invalid_response_failure_reason"]
CONSTS = ["SITE_FAILURE_CODES", "FAILED_TURN_NOTICE", "PARTIAL_FAILED_TURN_NOTICE",
          "AUTOMATION_INTERRUPTED_NOTICE", "FAILED_TURN_DISPLAY_KIND",
          "_EXIT_REASON_FAILURES", "_INVALID_RESPONSE_CODES", "FAILURE_CAUSE_GLOSS",
          "_NEXT_STEPS_RETRY", "_NEXT_STEPS_LOOP"]

_REASON_VALUES = {
    "auth": "auth", "auth_permanent": "auth_permanent", "billing": "billing",
    "rate_limit": "rate_limit", "upstream_rate_limit": "upstream_rate_limit",
    "upstream_blocked": "upstream_blocked", "overloaded": "overloaded",
    "server_error": "server_error", "timeout": "timeout",
    "ssl_cert_verification": "ssl_cert_verification", "context_overflow": "context_overflow",
    "payload_too_large": "payload_too_large", "model_not_found": "model_not_found",
    "provider_policy_blocked": "provider_policy_blocked", "data_policy_denied": "data_policy_denied",
    "content_policy_blocked": "content_policy_blocked", "guardrail_blocked": "guardrail_blocked",
    "format_error": "format_error", "role_alternation": "role_alternation",
}
FailoverReason = enum.Enum("FailoverReason", _REASON_VALUES)

_TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
_FLAGS = __future__.annotations.compiler_flag


def _compile(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


_CODE, _CCODE = {}, {}
for _n in _TREE.body:
    if isinstance(_n, ast.FunctionDef) and _n.name in PORTED:
        _CODE[_n.name] = _compile(_n)
    elif isinstance(_n, (ast.Assign, ast.AnnAssign)):
        _t = _n.targets[0] if isinstance(_n, ast.Assign) else _n.target
        if isinstance(_t, ast.Name) and _t.id in CONSTS:
            _CCODE[_t.id] = _compile(_n)
_bad = (set(PORTED) - set(_CODE)) | ({c for c in CONSTS if c not in
        {"_NEXT_STEPS_RETRY", "_NEXT_STEPS_LOOP"}} - set(_CCODE))
if _bad:
    print(f"ORACLE {NAME}: FATAL missing: {sorted(_bad)}", file=sys.stderr)
    sys.exit(1)


class ExitFailure(NamedTuple):
    reason: str
    retryable: bool
    fails_turn: bool = True


_MOD = types.ModuleType("w9tfc_live_turn_failure_copy")
_MOD.__dict__.update({"__name__": "w9tfc_live_turn_failure_copy", "__builtins__": builtins,
                      "FailoverReason": FailoverReason, "ExitFailure": ExitFailure,
                      "Any": object, "Dict": dict, "Optional": object, "Tuple": tuple})
sys.modules["w9tfc_live_turn_failure_copy"] = _MOD
_NS = _MOD.__dict__
for _nm, _c in _CCODE.items():
    exec(_c, _NS)
for _nm, _c in _CODE.items():
    exec(_c, _NS)


def norm(v):
    if isinstance(v, ExitFailure):
        return {"reason": v.reason, "retryable": v.retryable, "fails_turn": v.fails_turn}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v


def drive(rec):
    inp, func = rec["in"], rec["func"]
    if func == "stamp":
        return norm(_NS["stamp_failure"](copy.deepcopy(inp.get("result") or {}),
                                         inp.get("reason", ""), inp.get("retryable", False)))
    if func == "display_kind":
        return norm(_NS["untyped_failed_turn_display_kind"](inp.get("role", ""), inp.get("content")))
    if func == "notice":
        return norm(_NS["failed_turn_notice"](inp.get("turn_messages") or []))
    if func == "exit_failure":
        return norm(_NS["exit_reason_failure"](inp.get("reason", "")))
    if func == "handoff":
        return norm(_NS["is_max_iteration_handoff"](inp.get("result")))
    if func == "gloss":
        return norm(_NS["failure_cause_gloss"](inp.get("reason", ""),
                    subject=inp.get("subject", "it"), possessive=inp.get("possessive", "its")))
    if func == "short_detail":
        return norm(_NS["short_detail"](inp.get("text", ""), inp.get("limit", 200)))
    if func == "invalid_reason":
        response = types.SimpleNamespace(error=inp.get("error"))
        return norm(_NS["invalid_response_failure_reason"](response))
    raise KeyError(func)


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            out = drive(rec)
        except BaseException as e:
            out = {"__raise__": type(e).__name__}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
