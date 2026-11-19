"""twin for agent/file_safety.py:classify_sandbox_mirror_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/file_safety.py"
    PY_FN = "classify_sandbox_mirror_target"
    C_FN = "fsf_classify_sandbox_mirror_target"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
