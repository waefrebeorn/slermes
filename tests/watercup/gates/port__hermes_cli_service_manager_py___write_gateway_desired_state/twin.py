"""twin for hermes_cli/service_manager.py:_write_gateway_desired_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/service_manager.py"
    PY_FN = "_write_gateway_desired_state"
    C_FN = "hermes_cli_service_manager_u_write_gateway_desired_state"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
