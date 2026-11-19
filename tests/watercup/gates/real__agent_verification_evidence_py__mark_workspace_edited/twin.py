"""twin for agent/verification_evidence.py:mark_workspace_edited (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_evidence.py"
    PY_FN = "mark_workspace_edited"
    C_FN = "vev_mark_workspace_edited"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
