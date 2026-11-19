"""twin for agent/compaction_display.py:project_compaction_message_for_display (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/compaction_display.py"
    PY_FN = "project_compaction_message_for_display"
    C_FN = "project_compaction_message_for_display"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
