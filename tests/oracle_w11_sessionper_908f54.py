#!/usr/bin/env python3
"""Differential oracle for agent/session_persistence.py (gate w11_sessionper_908f54).

Loads the LIVE module with stubbed heavy dependencies; the helpers compared
here only need the two metadata-key constants, which are set to their real
values from the upstream source.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name); m.__path__ = []; sys.modules[name] = m
    return m

agent_pkg = _pkg("agent")

cc = types.ModuleType("agent.context_compressor")
cc.COMPRESSED_SUMMARY_METADATA_KEY = "_compressed_summary"
cc._DB_PERSISTED_MARKER = "_db_persisted"
cc.ContextCompressor = type("ContextCompressor", (), {
    "classify_summary_content": staticmethod(lambda content: "standalone")})
cc._newest_checkpoint_carrier = lambda *a, **k: -1
cc._strip_persistence_markers = lambda *a, **k: None
cc.drop_shadowed_checkpoints = lambda *a, **k: None
sys.modules["agent.context_compressor"] = cc
agent_pkg.context_compressor = cc

turns = types.ModuleType("session_rules.turns")
for _n in ("user_originated_turn_view", "sanitize_context", "message_uid_or_none",
           "_durable_content", "_is_ephemeral_scaffolding"):
    setattr(turns, _n, lambda *a, **k: None)
turns._EPHEMERAL_SCAFFOLDING_FLAGS = set()
turns._IMAGE_PART_TYPES = set()
sys.modules["session_rules.turns"] = turns
_pkg("session_rules")

lf = types.ModuleType("agent.lazy_forward")
lf.forward = lambda *a, **k: (lambda *a2, **k2: None)
lf.forward_static = lambda *a, **k: (lambda *a2, **k2: None)
sys.modules["agent.lazy_forward"] = lf
agent_pkg.lazy_forward = lf

trajectory = types.ModuleType("agent.trajectory")
trajectory.save_trajectory = lambda *a, **k: None
sys.modules["agent.trajectory"] = trajectory
agent_pkg.trajectory = trajectory

mm = types.ModuleType("agent.message_metadata")
for _n in ("ABSORBED_MESSAGE_UIDS", "DB_ROW_SNAPSHOT", "MESSAGE_UID",
           "REPAIR_BOOKKEEPING_FIELDS", "RESTORED_FROM_STORE", "TOOL_CALL_UID", "TOOL_CALL_UIDS"):
    setattr(mm, _n, "_" + _n.lower())
mm.MERGED_TURN_PREFIX = "_merged_turn_prefix"
mm.copy_identity_fields = lambda *a, **k: None
mm.tool_call_uid_from_history = lambda *a, **k: None
sys.modules["agent.message_metadata"] = mm
agent_pkg.message_metadata = mm

repair = types.ModuleType("agent.transcript_repair")
repair.sync_flushed_message_markers = lambda *a, **k: None
sys.modules["agent.transcript_repair"] = repair
agent_pkg.transcript_repair = repair

pcm = types.ModuleType("persistence.conversation.messages")
pcm.NewMessage = type("NewMessage", (), {"from_live": classmethod(lambda cls, x: x)})
pcm.RowDisposition = type("RowDisposition", (), {"REPAIRED": 1})
_pkg("persistence"); _pkg("persistence.conversation")
sys.modules["persistence.conversation.messages"] = pcm

SRC = QUARRY + "/agent/session_persistence.py"
spec = importlib.util.spec_from_file_location("w11_sessionper_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_sessionper_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "override":
        return live._override_replaces_content(r.get("msg") or {}, r.get("content"), r.get("override"))
    if func == "content_override":
        return live._content_with_turn_override(r.get("msg") or {}, r.get("content"), r.get("override"))
    if func == "safe_filename":
        return live._safe_session_filename_component(r.get("session_id"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_sessionper_908f54: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_sessionper_908f54: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
