"""twin for hermes_cli/mcp_catalog.py:_catalog_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_catalog.py"
    PY_FN = "_catalog_root"
    C_FN = "hermes_cli_mcp_catalog_u_catalog_root"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
