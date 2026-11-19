"""twin for hermes_cli/gateway.py:launch_detached_gateway_restart_by_cmdline (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "launch_detached_gateway_restart_by_cmdline"
    C_FN = "gwc2_launch_detached_gateway_restart_by_cmdline"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
