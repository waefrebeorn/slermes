"""twin for agent/secret_scope.py:set_secret_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_scope.py"
    PY_FN = "set_secret_scope"
    C_FN = "secret_scope_set_secret_scope"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
