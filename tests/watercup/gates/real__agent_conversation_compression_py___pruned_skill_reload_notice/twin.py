"""twin for agent/conversation_compression.py:_pruned_skill_reload_notice (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_pruned_skill_reload_notice"
    C_FN = "_pruned_skill_reload_notice"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
