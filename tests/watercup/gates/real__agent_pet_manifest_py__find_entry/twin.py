"""twin for agent/pet/manifest.py:find_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/manifest.py"
    PY_FN = "find_entry"
    C_FN = "pet_find_entry"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
