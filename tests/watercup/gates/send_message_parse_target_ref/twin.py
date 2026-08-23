"""twin for send_message_parse_target_ref.

Traces the LIVE tools/send_message_tool.py::_parse_target_ref with the SAME
rung order as the C harness (C ladder order: telegram, discord, feishu,
slack). The real LIVE result is cross-checked against the traced outcome on
every input (None <-> "" normalization).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_send_message_tool", os.path.join(DEV, "tools", "send_message_tool.py"))
_smt = ilu.module_from_spec(_spec)
sys.modules["wc_live_send_message_tool"] = _smt
_spec.loader.exec_module(_smt)


def trace(platform_name, target_ref):
    c = Cup("parse_target_ref")
    if platform_name == "telegram":
        c.branch(True)
        if target_ref.startswith("@"):
            # C passes any '@' target verbatim; agreed subset = valid usernames,
            # where LIVE's parse_telegram_username_target returns it unchanged.
            c.branch(True)
            c.valstr(target_ref)
            c.valstr("")
            chat, thread, expl = target_ref, "", "1"
        else:
            c.branch(False)
            m = _smt._TELEGRAM_TOPIC_TARGET_RE.fullmatch(target_ref)
            c.branch(bool(m))
            if m:
                chat = m.group(1)
                thread = m.group(2) or ""
                expl = "1"
                c.valstr(chat)
                c.valstr(thread)
                c.valstr(expl)
            else:
                chat, thread, expl = "", "", "0"
                c.valstr("NOMATCH")
                c.valstr(expl)
    elif platform_name == "discord":
        c.branch(False)
        c.branch(True)
        m = _smt._NUMERIC_TOPIC_RE.fullmatch(target_ref)
        c.branch(bool(m))
        if m:
            chat = m.group(1)
            thread = m.group(2) or ""
            expl = "1"
            c.valstr(chat)
            c.valstr(thread)
            c.valstr(expl)
        else:
            chat, thread, expl = "", "", "0"
            c.valstr("NOMATCH")
            c.valstr(expl)
    elif platform_name == "feishu":
        c.branch(False)
        c.branch(False)
        c.branch(True)
        m = _smt._FEISHU_TARGET_RE.fullmatch(target_ref)
        c.branch(bool(m))
        if m:
            chat = m.group(1)
            thread = m.group(2) or ""
            expl = "1"
            c.valstr(chat)
            c.valstr(thread)
            c.valstr(expl)
        else:
            chat, thread, expl = "", "", "0"
            c.valstr("NOMATCH")
            c.valstr(expl)
    elif platform_name == "slack":
        c.branch(False)
        c.branch(False)
        c.branch(False)
        c.branch(True)
        m = _smt._SLACK_THREAD_TARGET_RE.fullmatch(target_ref)
        c.branch(bool(m))
        if m:
            chat = m.group(1)
            thread = m.group(2)
            expl = "1"
            c.valstr(chat)
            c.valstr(thread)
            c.valstr(expl)
        else:
            m2 = _smt._SLACK_TARGET_RE.fullmatch(target_ref)
            c.branch(bool(m2))
            if m2:
                chat = target_ref.strip()
                thread = ""
                expl = "1"
                c.valstr(chat)
                c.valstr(thread)
                c.valstr(expl)
            else:
                chat, thread, expl = "", "", "0"
                c.valstr("NOMATCH")
                c.valstr(expl)
    else:
        c.branch(False)
        c.branch(False)
        c.branch(False)
        c.branch(False)
        c.valstr("UNSUPPORTED")
        c.valstr("0")
        chat, thread, expl = None, None, "0"
        return c.finalize()

    # Cross-check against the LIVE function's real result.
    live_chat, live_thread, live_expl = _smt._parse_target_ref(platform_name, target_ref)
    norm_thread = live_thread if live_thread is not None else ""
    assert (chat, thread, expl) == (
        live_chat if live_chat is not None else "",
        norm_thread, "1" if live_expl else "0"), (
        "drift vs LIVE: (%r,%r,%s) vs (%r,%r,%r)"
        % (chat, thread, expl, live_chat, live_thread, live_expl))
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        platform, target = raw.split("|", 1)
        cups.append(trace(platform, target))
    return cups
