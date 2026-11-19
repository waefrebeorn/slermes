"""twin for agent/portal_tags.py:_hermes_version (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/portal_tags.py"
    PY_FN = "_hermes_version"
    C_FN = "_hermes_version"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
