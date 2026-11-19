"""twin for hermes_cli/provider_catalog.py:provider_catalog (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/provider_catalog.py"
    PY_FN = "provider_catalog"
    C_FN = "hermes_cli_provider_catalog_provider_catalog"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
