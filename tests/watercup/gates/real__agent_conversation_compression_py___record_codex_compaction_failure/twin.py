"""twin for agent/conversation_compression.py:_record_codex_compaction_failure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_record_codex_compaction_failure"
    C_FN = "_record_codex_compaction_failure"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
