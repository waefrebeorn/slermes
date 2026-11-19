"""twin for hermes_cli/provider_catalog.py:tab_for_auth_type (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/provider_catalog.py"
    PY_FN = "tab_for_auth_type"
    C_FN = "provider_catalog_tab_for_auth_type"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
