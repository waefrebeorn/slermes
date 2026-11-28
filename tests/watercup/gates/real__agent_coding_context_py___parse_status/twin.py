"""twin: agent/coding_context.py:_parse_status, LIVE (pure string parser)."""
import agent.coding_context as _cc
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "_parse_status"
    C_FN = "_parse_status"
    def call(self, inp, inputs, idx=0):
        branch, counts = _cc._parse_status(inp)
        parts = []
        for k in ("head","upstream","ahead","behind"):
            parts.append("%s=%s" % (k, branch[k] if k in branch else "absent"))
        for k in ("staged","modified","untracked","conflicts"):
            parts.append("%s=%s" % (k, counts[k] if k in counts else "absent"))
        return "&".join(parts)
    def norm(self, c, result):
        c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
