"""twin for hermes_cli/_scan_venv_blockers.py:_is_pausable_gateway (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_scan_venv_blockers.py"
    PY_FN = "_is_pausable_gateway"
    C_FN = "scanvb_is_pausable_gateway"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
