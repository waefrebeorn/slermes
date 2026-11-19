"""twin for agent/verification_evidence.py:_split_shell_segments (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_evidence.py"
    PY_FN = "_split_shell_segments"
    C_FN = "_split_shell_segments"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
