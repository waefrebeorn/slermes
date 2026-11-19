"""twin for agent/secret_sources/registry.py:_ordered_enabled_sources (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/registry.py"
    PY_FN = "_ordered_enabled_sources"
    C_FN = "_ordered_enabled_sources"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
