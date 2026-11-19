"""twin for agent/context_references.py:_is_binary_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_references.py"
    PY_FN = "_is_binary_file"
    C_FN = "crf_is_binary_file"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
