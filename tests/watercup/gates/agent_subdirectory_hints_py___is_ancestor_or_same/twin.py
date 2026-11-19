"""twin for agent/subdirectory_hints.py:_is_ancestor_or_same (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subdirectory_hints.py"
    PY_FN = "_is_ancestor_or_same"
    C_FN = "shi_is_ancestor_or_same"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
