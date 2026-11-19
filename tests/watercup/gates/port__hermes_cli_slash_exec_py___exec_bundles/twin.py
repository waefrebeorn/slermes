"""twin for hermes_cli/slash_exec.py:_exec_bundles (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/slash_exec.py"
    PY_FN = "_exec_bundles"
    C_FN = "_exec_bundles"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
