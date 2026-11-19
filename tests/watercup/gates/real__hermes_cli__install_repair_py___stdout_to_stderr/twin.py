"""twin for hermes_cli/_install_repair.py:_stdout_to_stderr (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_install_repair.py"
    PY_FN = "_stdout_to_stderr"
    C_FN = "_stdout_to_stderr"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
