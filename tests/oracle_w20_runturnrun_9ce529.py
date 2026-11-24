#!/usr/bin/env python3
"""Differential oracle for gateway/run_turn_runner_approval_text.py (w20_runturnrun_9ce529).

The live module is imported straight from the quarry.  The localized strings
come from each fixture line (the caller's i18n bundle) by patching ``t`` and the
``base_exec_approval`` accessors, while ``fit_prefix``/``fit_fenced``/
``code_fence`` stay the real implementations; the adapter is reduced to the two
measurement hooks the module calls.  Compared values: the prompt layout for the
default, deny-only and shortened branches, and the fence sizing.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import gateway.platforms.base as platform_base  # noqa: E402
import gateway.platforms.base_exec_approval as bea  # noqa: E402

KEYS = {
    "gateway.exec_approval.header": "header",
    "gateway.exec_approval.reason_label": "reason_label",
    "gateway.exec_approval.deadline": "deadline",
    "gateway.exec_approval.text_choice_joiner": "joiner",
    "gateway.exec_approval.text_choice_last": "last",
    "gateway.exec_approval.text_choice_once": "once",
    "gateway.exec_approval.text_choice_session": "session",
    "gateway.exec_approval.text_choice_always": "always",
    "gateway.exec_approval.text_choice_deny": "deny",
    "gateway.exec_approval.text_prompt_deny_only": "deny_only",
    "gateway.exec_approval.truncated_deny_only": "truncated",
    "gateway.exec_approval.smart_deny_heading": "smart_heading",
}

NAME = "w20_runturnrun_9ce529_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "run_turn_runner_approval_text.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

REAL = {
    "header": bea.ea_header_text,
    "reason_label": bea.ea_reason_label_text,
    "timeout": bea.approval_timeout_seconds,
    "deadline": bea.format_approval_deadline_line,
    "t": live.t,
}


class Adapter:
    def __init__(self, limit):
        self.limit = limit

    def max_message_length_for_chat(self, chat_id):
        return self.limit

    def message_len_fn_for_chat(self, chat_id):
        return len


def _install(bundle):
    def t(key, **kw):
        return bundle[KEYS[key]].format(**kw)

    live.t = t
    bea.ea_header_text = lambda: bundle["header"]
    bea.ea_reason_label_text = lambda: bundle["reason_label"]
    bea.approval_timeout_seconds = lambda: 300
    bea.format_approval_deadline_line = lambda timeout_s: bundle["deadline"]


def expected(r):
    f = r.get("func")
    if f == "code_fence":
        bundle = r.get("i18n") or {}
        _install(bundle) if bundle else None
        return bea.code_fence(r.get("command", ""))
    if f == "cutting_text_prompt":
        bundle = r["i18n"]
        _install(bundle)
        text, deny_only = live._cutting_text_prompt(
            Adapter(r.get("limit", 4096)), r.get("chat_id", "chat"),
            r.get("command", ""), r.get("description", ""), r.get("prefix", "/"),
            allow_permanent=r.get("allow_permanent", True),
            allow_session=r.get("allow_session", True),
            smart_denied=r.get("smart_denied", False))
        return {"text": text, "deny_only": bool(deny_only)}
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
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_runturnrun_9ce529: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
