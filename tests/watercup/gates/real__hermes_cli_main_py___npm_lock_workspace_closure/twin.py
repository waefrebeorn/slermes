"""twin for hermes_cli/main.py:_npm_lock_workspace_closure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_npm_lock_workspace_closure"
    C_FN = "_npm_lock_workspace_closure"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
