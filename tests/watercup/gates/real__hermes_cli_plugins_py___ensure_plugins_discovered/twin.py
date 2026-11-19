"""twin for hermes_cli/plugins.py:_ensure_plugins_discovered (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_ensure_plugins_discovered"
    C_FN = "plug_ensure_discovered"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
