"""twin for hermes_cli/bundles.py:_cmd_reload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/bundles.py"
    PY_FN = "_cmd_reload"
    C_FN = "cmd_reload"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
