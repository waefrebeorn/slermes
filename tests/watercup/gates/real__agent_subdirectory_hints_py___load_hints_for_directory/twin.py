"""twin for agent/subdirectory_hints.py:_load_hints_for_directory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subdirectory_hints.py"
    PY_FN = "_load_hints_for_directory"
    C_FN = "_load_hints_for_directory"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
