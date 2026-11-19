"""twin for agent/portal_tags.py:nous_portal_tags (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/portal_tags.py"
    PY_FN = "nous_portal_tags"
    C_FN = "nous_portal_tags"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
