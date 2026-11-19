"""twin for hermes_cli/plugin_index.py:_write_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_index.py"
    PY_FN = "_write_cache"
    C_FN = "_write_cache"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
