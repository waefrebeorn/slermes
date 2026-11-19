"""twin for hermes_cli/service_manager.py:_supervised_pid (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/service_manager.py"
    PY_FN = "_supervised_pid"
    C_FN = "hermes_cli_service_manager_u_supervised_pid"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
