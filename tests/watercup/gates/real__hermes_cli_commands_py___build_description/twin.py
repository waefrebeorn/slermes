"""twin for hermes_cli/commands.py:_build_description (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/commands.py"
    PY_FN = "_build_description"
    C_FN = "_build_description"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
