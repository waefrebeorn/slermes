"""twin for hermes_cli/tools_config.py:_enable_recently_shipped_toolsets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_enable_recently_shipped_toolsets"
    C_FN = "_enable_recently_shipped_toolsets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
