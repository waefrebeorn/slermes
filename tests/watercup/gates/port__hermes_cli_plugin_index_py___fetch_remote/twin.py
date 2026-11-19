"""twin for hermes_cli/plugin_index.py:_fetch_remote (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_index.py"
    PY_FN = "_fetch_remote"
    C_FN = "_fetch_remote"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
