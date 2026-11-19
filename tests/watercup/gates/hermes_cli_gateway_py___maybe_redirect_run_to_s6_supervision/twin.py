"""twin for hermes_cli/gateway.py:_maybe_redirect_run_to_s6_supervision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_maybe_redirect_run_to_s6_supervision"
    C_FN = "cgw_u_maybe_redirect_run_to_s6_supervision"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
