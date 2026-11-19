"""twin for agent/coding_context.py:coding_compact_skill_categories (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "coding_compact_skill_categories"
    C_FN = "coding_compact_skill_categories"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
