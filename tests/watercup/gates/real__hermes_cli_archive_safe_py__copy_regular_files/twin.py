"""twin for hermes_cli/archive_safe.py:copy_regular_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/archive_safe.py"
    PY_FN = "copy_regular_files"
    C_FN = "copy_regular_files"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
