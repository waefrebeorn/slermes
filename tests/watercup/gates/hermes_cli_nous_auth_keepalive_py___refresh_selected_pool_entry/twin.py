"""twin for hermes_cli/nous_auth_keepalive.py:_refresh_selected_pool_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_auth_keepalive.py"
    PY_FN = "_refresh_selected_pool_entry"
    C_FN = "hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
