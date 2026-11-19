"""twin for agent/secret_scope.py:is_multiplex_active (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_scope.py"
    PY_FN = "is_multiplex_active"
    C_FN = "secret_scope_is_multiplex_active"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
