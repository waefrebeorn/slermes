"""twin for hermes_cli/web_server.py:_safe_plugin_api_relpath (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_safe_plugin_api_relpath"
    C_FN = "_safe_plugin_api_relpath"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
