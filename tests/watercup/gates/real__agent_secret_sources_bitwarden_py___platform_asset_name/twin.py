"""twin for agent/secret_sources/bitwarden.py:_platform_asset_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"
    PY_FN = "_platform_asset_name"
    C_FN = "_platform_asset_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
