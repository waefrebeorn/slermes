"""twin for hermes_cli/update_cmd.py:_service_unit_supports_graceful_sigusr1_restart (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_service_unit_supports_graceful_sigusr1_restart"
    C_FN = "_service_unit_supports_graceful_sigusr1_restart"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
