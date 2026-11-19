"""twin for agent/redact.py:_rebuild_prefix_matcher (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/redact.py"
    PY_FN = "_rebuild_prefix_matcher"
    C_FN = "_rebuild_prefix_matcher"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
