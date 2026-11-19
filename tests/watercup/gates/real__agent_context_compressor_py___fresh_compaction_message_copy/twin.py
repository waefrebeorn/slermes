"""twin for agent/context_compressor.py:_fresh_compaction_message_copy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_fresh_compaction_message_copy"
    C_FN = "_fresh_compaction_message_copy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
