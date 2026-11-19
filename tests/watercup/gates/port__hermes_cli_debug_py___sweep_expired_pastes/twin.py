"""twin for hermes_cli/debug.py:_sweep_expired_pastes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/debug.py"
    PY_FN = "_sweep_expired_pastes"
    C_FN = "_sweep_expired_pastes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
