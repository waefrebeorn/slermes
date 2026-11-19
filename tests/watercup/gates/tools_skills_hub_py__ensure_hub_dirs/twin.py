"""twin for tools/skills_hub.py:ensure_hub_dirs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_hub.py"
    PY_FN = "ensure_hub_dirs"
    C_FN = "hub_ensure_dirs"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
