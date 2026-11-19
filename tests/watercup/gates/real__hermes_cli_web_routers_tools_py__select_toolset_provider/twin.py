"""twin for hermes_cli/web_routers/tools.py:select_toolset_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/tools.py"
    PY_FN = "select_toolset_provider"
    C_FN = "select_toolset_provider"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
