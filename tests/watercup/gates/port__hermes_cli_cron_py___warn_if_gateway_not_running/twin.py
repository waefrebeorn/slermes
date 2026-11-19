"""twin for hermes_cli/cron.py:_warn_if_gateway_not_running (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cron.py"
    PY_FN = "_warn_if_gateway_not_running"
    C_FN = "hermes_cli_cron_u_warn_if_gateway_not_running"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
