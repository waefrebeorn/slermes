#!/usr/bin/env python3
"""Differential oracle for gateway/slash_commands_session.py (w18_slashcomma_782bf8).

Top-level origin imports are stubbed so the live module loads; the two ported
helpers touch none of them except Platform.TELEGRAM.value.
"""
import enum
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


for pkg in ("gateway", "gateway.platforms", "agent"):
    if pkg not in sys.modules:
        _mk(pkg)

Plat = enum.Enum("Platform", {"LOCAL": "local", "TELEGRAM": "telegram", "SLACK": "slack"})
_mk("gateway.config", Platform=Plat)
_mk("agent.i18n", t=lambda *a, **k: "")
_mk("agent.turn_context", extract_api_content_sidecar=lambda m: None)
_mk("gateway.platforms.base", EphemeralReply=object)
_mk("gateway.platforms.event", MessageEvent=object, MessageType=object)
_mk("gateway.run_topics", TopicLinkedElsewhere=Exception)
_mk("gateway.session", SessionSource=object, build_session_key=lambda *a: "",
    is_shared_multi_user_session=lambda *a: False)
_mk("gateway.session_transcript", TranscriptReadError=Exception)
_mk("gateway.slash_commands_branch_thread", BRANCH_THREAD_PLATFORMS=set(),
    branch_dest_source=lambda *a: None, branch_thread_parent=lambda *a: None,
    format_thread_ref=lambda *a: "", parse_branch_args=lambda *a: [])
_mk("gateway.slash_commands_status", history_unreadable=lambda *a: False)

SRC = Q + "/gateway/slash_commands_session.py"
spec = importlib.util.spec_from_file_location("w18_slashcomma_782bf8_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compat(v):
    if isinstance(v, (tuple, set, frozenset)):
        return [compat(x) for x in (sorted(v, key=repr) if isinstance(v, (set, frozenset)) else v)]
    if isinstance(v, list):
        return [compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): compat(x) for k, x in v.items()}
    return v


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "strip_resume_name":
            py = live._strip_resume_name(r.get("parts", []))
        elif func == "telegram_private_chat_owner":
            py = live._telegram_private_chat_owner(r.get("platform"), r.get("chat_type"),
                                                   r.get("chat_id"), r.get("user_id"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    py = compat(py)
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_slashcomma_782bf8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
