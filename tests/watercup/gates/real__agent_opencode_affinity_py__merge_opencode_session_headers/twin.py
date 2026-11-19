"""twin for agent/opencode_affinity.py:merge_opencode_session_headers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/opencode_affinity.py"
    PY_FN = "merge_opencode_session_headers"
    C_FN = "merge_opencode_session_headers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
