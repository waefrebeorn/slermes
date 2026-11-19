"""twin for agent/subdirectory_hints.py:_extract_paths_from_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subdirectory_hints.py"
    PY_FN = "_extract_paths_from_command"
    C_FN = "_extract_paths_from_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
