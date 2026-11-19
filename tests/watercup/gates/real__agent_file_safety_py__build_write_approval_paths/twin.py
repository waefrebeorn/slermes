"""twin for agent/file_safety.py:build_write_approval_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/file_safety.py"
    PY_FN = "build_write_approval_paths"
    C_FN = "build_write_approval_paths"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
