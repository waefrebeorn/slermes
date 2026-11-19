"""twin for hermes_cli/main.py:_pytest_owns_live_checkout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_pytest_owns_live_checkout"
    C_FN = "main_g_pytest_owns_live_checkout"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
