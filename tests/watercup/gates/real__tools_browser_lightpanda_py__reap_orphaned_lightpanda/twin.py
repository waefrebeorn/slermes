"""twin for tools/browser_lightpanda.py:reap_orphaned_lightpanda (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_lightpanda.py"
    PY_FN = "reap_orphaned_lightpanda"
    C_FN = "reap_orphaned_lightpanda"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
