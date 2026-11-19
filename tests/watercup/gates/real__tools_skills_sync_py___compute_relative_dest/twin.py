"""twin for tools/skills_sync.py:_compute_relative_dest (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync.py"
    PY_FN = "_compute_relative_dest"
    C_FN = "_compute_relative_dest"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
