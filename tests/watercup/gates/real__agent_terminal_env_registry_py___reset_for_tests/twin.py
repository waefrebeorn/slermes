"""twin for agent/terminal_env_registry.py:_reset_for_tests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/terminal_env_registry.py"
    PY_FN = "_reset_for_tests"
    C_FN = "_reset_for_tests"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
