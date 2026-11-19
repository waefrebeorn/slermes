"""twin for agent/context_compressor.py:maybe_seed_preflight_display_tokens (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "maybe_seed_preflight_display_tokens"
    C_FN = "maybe_seed_preflight_display_tokens"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
