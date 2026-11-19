"""twin for agent/conversation_compression.py:_codex_compaction_cooldown_remaining (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_codex_compaction_cooldown_remaining"
    C_FN = "_codex_compaction_cooldown_remaining"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
