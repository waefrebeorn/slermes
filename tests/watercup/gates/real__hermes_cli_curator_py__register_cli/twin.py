"""twin for hermes_cli/curator.py:register_cli (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/curator.py"
    PY_FN = "register_cli"
    C_FN = "register_cli"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
