"""twin for hermes_cli/proxy/adapters/__init__.py:get_adapter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/adapters/__init__.py"
    PY_FN = "get_adapter"
    C_FN = "hcli_proxy_get_adapter"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
