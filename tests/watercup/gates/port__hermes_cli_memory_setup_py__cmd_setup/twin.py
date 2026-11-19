"""twin for hermes_cli/memory_setup.py:cmd_setup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/memory_setup.py"
    PY_FN = "cmd_setup"
    C_FN = "cmd_setup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
