"""twin for hermes_cli/proxy/adapters/nous_portal.py:is_authenticated (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/adapters/nous_portal.py"
    PY_FN = "is_authenticated"
    C_FN = "cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
