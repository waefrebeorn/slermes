"""twin for agent/verification_evidence.py:_ensure_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_evidence.py"
    PY_FN = "_ensure_schema"
    C_FN = "vev_u_ensure_schema"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
