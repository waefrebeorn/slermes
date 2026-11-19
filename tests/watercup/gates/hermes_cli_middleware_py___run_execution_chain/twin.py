"""twin for hermes_cli/middleware.py:_run_execution_chain (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/middleware.py"
    PY_FN = "_run_execution_chain"
    C_FN = "hermes_cli_middleware_u_run_execution_chain"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
