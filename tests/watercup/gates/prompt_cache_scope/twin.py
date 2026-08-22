"""twin for resolve_prompt_cache_scope.

Traces the LIVE agent/prompt_cache_scope.py::resolve_prompt_cache_scope with
the same rung order as the C harness. Probes: "sid|lineage_json|persist|repeat".
The lineage JSON is served by a fake DB whose get_compression_lineage returns
the parsed list for any session.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_MEMO_ATTR = "_prompt_cache_scope_memo"


def _lineage_root(session_id, session_db):
    if session_db is None:
        return None
    try:
        lineage = session_db.get_compression_lineage(session_id)
    except Exception:
        return None
    if isinstance(lineage, (list, tuple)) and lineage:
        root = lineage[0]
        if isinstance(root, str) and root:
            return root
    return None


def _live(agent):
    sid = str(getattr(agent, "session_id", None) or "")
    if not sid:
        return ""
    db = getattr(agent, "_session_db", None)
    key = (sid, db is not None)
    memo = getattr(agent, _MEMO_ATTR, None)
    if isinstance(memo, tuple) and len(memo) == 2 and memo[0] == key:
        return memo[1]
    root = _lineage_root(sid, db) if db is not None else None
    scope = root or sid
    if root is not None or db is None or getattr(agent, "_persist_disabled", False):
        try:
            setattr(agent, _MEMO_ATTR, (key, scope))
        except Exception:
            pass
    return scope


class FakeDB:
    def __init__(self, lineage):
        self._lineage = lineage

    def get_compression_lineage(self, session_id):
        return self._lineage


def run(inputs):
    cups = []
    for probe in inputs:
        fields = (probe.split("|") + ["", "", "", ""])[:4]
        sid, lineage_json, persist_raw, repeat_raw = fields
        lineage = json.loads(lineage_json) if lineage_json else None
        persist_disabled = bool(int(persist_raw or 0))
        repeat = max(1, int(repeat_raw or 1))

        class Agent:
            pass

        agent = Agent()
        agent.session_id = sid
        agent._session_db = FakeDB(lineage) if lineage is not None else None
        agent._persist_disabled = persist_disabled

        c = Cup("resolve_prompt_cache_scope")
        result = ""
        for _ in range(repeat):
            s = str(getattr(agent, "session_id", None) or "")
            if not s:                                   # B1
                c.branch(True)
                result = ""
                break
            c.branch(False)
            db = getattr(agent, "_session_db", None)
            key = (s, db is not None)
            memo = getattr(agent, _MEMO_ATTR, None)
            if isinstance(memo, tuple) and len(memo) == 2 and memo[0] == key:  # B2
                c.branch(True)
                result = memo[1]
                continue
            c.branch(False)                                                     # B3
            root = _lineage_root(s, db) if db is not None else None
            if db is not None:
                c.branch(True)
            else:
                c.branch(False)
            scope = root or s
            if root is not None:                                # B4 found
                c.branch(True)
            else:
                c.branch(False)
            if root is not None or db is None or \
                    getattr(agent, "_persist_disabled", False):  # B5 memoize
                c.branch(True)
                setattr(agent, _MEMO_ATTR, (key, scope))
            else:
                c.branch(False)
            result = scope
        cups.append(c.finalize())
    return cups
