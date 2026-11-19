"""twin for hermes_cli/proxy/adapters/nous_portal.py:_save_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/adapters/nous_portal.py"
    PY_FN = "_save_state"
    C_FN = "cli_hermes_cli_proxy_adapters_nous_portal__save_state"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
