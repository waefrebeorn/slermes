"""twin for agent/context_compressor.py:_emit_init_summary_once (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_emit_init_summary_once"
    C_FN = "cc_micro_compact_emit_init_summary_once"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
