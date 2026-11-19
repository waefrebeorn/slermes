"""twin for hermes_cli/debug.py:_best_effort_sweep_expired_pastes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/debug.py"
    PY_FN = "_best_effort_sweep_expired_pastes"
    C_FN = "hermes_cli_debug_u_best_effort_sweep_expired_pastes"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
