"""twin for tools/working_diff.py:_untracked_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/working_diff.py"
    PY_FN = "_untracked_files"
    C_FN = "_untracked_files"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
