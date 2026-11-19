"""twin for tools/file_tools.py:_check_file_staleness (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "_check_file_staleness"
    C_FN = "file_tools_check_file_staleness"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
