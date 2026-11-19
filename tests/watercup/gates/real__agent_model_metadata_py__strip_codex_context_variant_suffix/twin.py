"""twin for agent/model_metadata.py:strip_codex_context_variant_suffix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "strip_codex_context_variant_suffix"
    C_FN = "strip_codex_context_variant_suffix"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
