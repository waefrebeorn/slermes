"""twin for agent/context_references.py:_ensure_reference_path_allowed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_references.py"
    PY_FN = "_ensure_reference_path_allowed"
    C_FN = "crf_ensure_reference_path_allowed"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
