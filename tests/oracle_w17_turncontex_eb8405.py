#!/usr/bin/env python3
"""Differential oracle for agent/turn_context_compaction.py (gate w17_turncontex_eb8405).

The live module is loaded via importlib.util.spec_from_file_location with the
agent package + its two top-level collaborator modules injected as deterministic
stand-ins, so the two pure helpers run their real live source.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
sys.modules["agent"] = agent_pkg

ctx_engine = types.ModuleType("agent.context_engine")
ctx_engine.automatic_compaction_status_message = lambda *a, **k: None
sys.modules["agent.context_engine"] = ctx_engine

cc = types.ModuleType("agent.conversation_compression")
cc.IDLE_COMPACTION_STATUS_TEMPLATE = ""
cc.PREFLIGHT_COMPRESSION_STATUS_TEMPLATE = ""
cc.compression_skipped_due_to_lock = lambda *a, **k: False
cc.conversation_history_after_compression = lambda *a, **k: None
sys.modules["agent.conversation_compression"] = cc

SRC = QUARRY + "/agent/turn_context_compaction.py"
spec = importlib.util.spec_from_file_location("w17_turncontex_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class _Comp:
    def __init__(self, have, should, reason, raises):
        self._should = should
        self._reason = reason
        self._raises = raises
        if have:
            self.should_compress_info = self._info

    def _info(self, tokens):
        if self._raises:
            raise RuntimeError("boom")
        return (self._should, self._reason)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "blocked_reason":
            comp = _Comp(bool(r.get("have_info")), bool(r.get("should")),
                         r.get("reason"), bool(r.get("raise")))
            tok = int(r.get("tokens", "0"))
            spent = r.get("attempts_spent")
            spent = int(spent) if spent is not None else None
            py = live._blocked_compress_reason(comp, tok, spent)
        elif func == "codex_native":
            agent = types.SimpleNamespace()
            if r.get("api_mode") is not None:
                agent.api_mode = r["api_mode"]
            if r.get("codex_auto") is not None:
                agent.codex_app_server_auto_compaction = r["codex_auto"]
            py = live._codex_native_auto_compaction(agent)
        else:
            continue
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s func=%s\n  py=%r\n  c =%r" %
              (r.get("case"), func, py, got), file=sys.stderr)

print("ORACLE w17_turncontex_eb8405: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
